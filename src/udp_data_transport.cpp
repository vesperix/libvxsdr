// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef VXSDR_ENABLE_UDP

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <compare>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <stdexcept>
#include <vector>

#include "logging.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_queues.hpp"
#include "vxsdr_net.hpp"
#include "vxsdr_threads.hpp"
#include "vxsdr_transport.hpp"


/*! @file udp_data_transport.cpp
    @brief Constructor, destructor, and utility functions for the @p vxsdr_udp data transport classes.
*/

udp_data_transport::udp_data_transport(const std::map<std::string, int64_t>& settings,
                                       const unsigned granularity,
                                       const unsigned n_subdevs,
                                       const unsigned max_samps_per_packet)
        : data_transport(granularity, n_subdevs, max_samps_per_packet),
          sender_socket(context, net::ip::udp::v4()),
          receiver_socket(context, net::ip::udp::v4()) {
    LOG_DEBUG("udp data transport constructor entered");

    auto config = packet_transport::apply_transport_settings(settings, default_settings);

    // apply the convenience settings if specific settings are absent
    if (config.count("udp_transport:local_address") != 0 and config.count("udp_data_transport:local_address") == 0) {
        config["udp_data_transport:local_address"] = config["udp_transport:local_address"];
    }
    if (config.count("udp_transport:device_address") != 0 and config.count("udp_data_transport:device_address") == 0) {
        config["udp_data_transport:device_address"] = config["udp_transport:device_address"];
    }

    if (config.count("udp_data_transport:local_address") == 0 or config.count("udp_data_transport:device_address") == 0) {
        LOG_ERROR("udp data transport settings must include udp_data_transport:local_address and udp_data_transport:device_address");
        throw std::invalid_argument("udp data transport settings must include local address and device address");
    }

    net::ip::address_v4 local_ip  = net::ip::address_v4(config["udp_data_transport:local_address"]);
    net::ip::address_v4 device_ip = net::ip::address_v4(config["udp_data_transport:device_address"]);

    net_error_code err;

    LOG_DEBUG("setting udp data sender socket to non-blocking");
    sender_socket.non_blocking(false, err);
    if (err) {
        LOG_ERROR("error setting udp data sender socket to non-blocking ({:s})", err.message());
        throw std::runtime_error("error setting udp data sender socket to non-blocking");
    }

    LOG_DEBUG("setting udp data receiver socket to non-blocking");
    receiver_socket.non_blocking(false, err);
    if (err) {
        LOG_ERROR("error setting udp data receiver socket to non-blocking ({:s})", err.message());
        throw std::runtime_error("error setting udp data receiver socket to non-blocking");
    }

    LOG_DEBUG("binding udp data sender socket to address {:s} port {:d}", local_ip.to_string(), udp_host_data_send_port);
    sender_socket.bind(net::ip::udp::endpoint(local_ip, udp_host_data_send_port), err);
    if (err) {
        LOG_ERROR("error binding udp data sender socket on local address {:s}; check that network interface is up ({:s})",
                  local_ip.to_string(), err.message());
        throw std::runtime_error("error binding udp data sender socket on local address " + local_ip.to_string() +
                                 "; check that network interface is up");
    }

    LOG_DEBUG("binding udp data receiver socket to address {:s} port {:d}", local_ip.to_string(), udp_host_data_receive_port);
    receiver_socket.bind(net::ip::udp::endpoint(local_ip, udp_host_data_receive_port), err);
    if (err) {
        LOG_ERROR("error binding udp data receiver socket on local address {:s}; check that network interface is up ({:s})",
                  local_ip.to_string(), err.message());
        throw std::runtime_error("error binding udp data receiver socket on local address " + local_ip.to_string() +
                                 "; check that network interface is up");
    }

    LOG_DEBUG("connecting udp data sender socket to address {:s} port {:d}", device_ip.to_string(), udp_device_data_receive_port);
    sender_socket.connect(net::ip::udp::endpoint(device_ip, udp_device_data_receive_port), err);
    if (err) {
        LOG_ERROR("error connecting udp data sender socket to device address {:s} ({:s})", device_ip.to_string(), err.message());
        throw std::runtime_error("error connecting udp data sender socket to device address " + device_ip.to_string());
    }

    LOG_DEBUG("setting do-not-fragment flag for udp data sender socket");
    if (set_socket_dontfrag(sender_socket)) {
        LOG_ERROR("error setting do-not-fragment flag for udp data sender socket");
        throw std::runtime_error("error setting do-not-fragment flag for udp data sender socket");
    }

    LOG_DEBUG("connecting udp data receiver socket to address {:s} port {:d}", device_ip.to_string(), udp_device_data_send_port);
    receiver_socket.connect(net::ip::udp::endpoint(device_ip, udp_device_data_send_port), err);
    if (err) {
        LOG_ERROR("error connecting udp data receiver socket to device address {:s} ({:s})", device_ip.to_string(), err.message());
        throw std::runtime_error("error connecting udp data receiver socket to device address " + device_ip.to_string());
    }

    LOG_DEBUG("checking mtu for udp data sender socket");
    // the size returned is an estimate, so this check is not a guarantee
    auto mtu_est = get_socket_mtu(sender_socket);
    if (mtu_est < 0) {
        LOG_ERROR("error getting mtu for udp data sender socket");
        throw std::runtime_error("error getting mtu for udp data sender socket");
    }

    if (config.count("udp_data_transport:mtu_bytes") > 0) {
        if (mtu_est < config["udp_data_transport:mtu_bytes"]) {
            LOG_ERROR("socket mtu is less than udp_data_transport:mtu_bytes");
            throw std::runtime_error("socket mtu is less than udp_data_transport:mtu_bytes");
        }
    }
    constexpr unsigned minimum_ip_udp_header_bytes = 28;
    if (mtu_est < (int)(max_samples_per_packet * sizeof(vxsdr::wire_sample) + sizeof(packet_header) + sizeof(stream_spec_t) + sizeof(time_spec_t) + minimum_ip_udp_header_bytes)) {
        LOG_ERROR("mtu too small for requested max_samples_per_packet on udp data sender socket");
        throw std::runtime_error("mtu too small for requested max_samples_per_packet on udp data sender socket");
    }

    size_t network_send_buffer_bytes    = config["udp_data_transport:network_send_buffer_bytes"];
    size_t network_receive_buffer_bytes = config["udp_data_transport:network_receive_buffer_bytes"];

    sender_socket.set_option(net::socket_base::send_buffer_size((int)network_send_buffer_bytes), err);
    if (err) {
        LOG_ERROR("cannot set network send buffer size to {:d} ({:s})", network_send_buffer_bytes, err.message());
    }
    net::socket_base::send_buffer_size sb_option;
    sender_socket.get_option(sb_option, err);
    if (err) {
        LOG_ERROR("cannot get network send buffer size ({:s})", err.message());
    } else {
        unsigned network_buffer_size = sb_option.value();

        if (network_buffer_size != network_send_buffer_bytes) {
            LOG_ERROR("cannot set network send buffer size to {:d} (got {:d})", network_send_buffer_bytes, network_buffer_size);
        } else {
            LOG_DEBUG("network send buffer size set to {:d}", network_buffer_size);
        }
    }
// FIXME: verify if this is needed -- multicast?
    receiver_socket.set_option(net::ip::udp::socket::reuse_address(true), err);
    if (err) {
        LOG_ERROR("cannot set reuse address option on receive socket ({:s})", err.message());
    }

    receiver_socket.set_option(net::socket_base::receive_buffer_size((int)network_receive_buffer_bytes), err);
    if (err) {
        LOG_ERROR("cannot set network receive buffer size to {:d} ({:s})", network_receive_buffer_bytes, err.message());
    }
    net::socket_base::receive_buffer_size rb_option;
    receiver_socket.get_option(rb_option, err);
    if (err) {
        LOG_ERROR("cannot get network receive buffer size ({:s})", err.message());
    } else {
        unsigned network_buffer_size = rb_option.value();
        if (network_buffer_size != network_receive_buffer_bytes) {
            LOG_ERROR("cannot set network receive buffer size to {:d} (got {:d})", network_receive_buffer_bytes,
                      network_buffer_size);
        } else {
            LOG_DEBUG("network receive buffer size set to {:d}", network_buffer_size);
        }
    }

    LOG_DEBUG("using transmit data buffer of {:d} packets", config["udp_data_transport:tx_data_queue_packets"]);
    tx_data_queue = std::make_unique<spsc_queue<data_queue_element>>(config["udp_data_transport:tx_data_queue_packets"]);

    for (unsigned i = 0; i < num_rx_subdevs; i++) {
        rx_data_queue.push_back(
            std::make_unique<spsc_queue<data_queue_element>>(config["udp_data_transport:rx_data_queue_packets"]));
        rx_sample_queue.push_back(
            std::make_unique<spsc_queue<vxsdr::wire_sample>>(MAX_DATA_LENGTH_SAMPLES));
    }

    LOG_DEBUG("using {:d} receive data buffers of {:d} packets", num_rx_subdevs, config["udp_data_transport:rx_data_queue_packets"]);
    LOG_DEBUG("using {:d} receive sample buffers of {:d} samples", num_rx_subdevs, MAX_DATA_LENGTH_SAMPLES);

    rx_state        = TRANSPORT_STARTING;
    receiver_thread = vxsdr_thread([this] { data_receive(); });

    if (config["udp_data_transport:thread_affinity_offset"] >= 0 and config["udp_data_transport:receiver_thread_affinity"] >= 0) {
        auto desired_affinity =
            config["udp_data_transport:thread_affinity_offset"] + config["udp_data_transport:receiver_thread_affinity"];
        if (set_thread_affinity(receiver_thread, desired_affinity) != 0) {
            LOG_ERROR("unable to set udp data receiver thread affinity in udp data transport constructor");
            throw std::runtime_error("unable to set udp data receiver thread affinity in udp data transport constructor");
        }
        LOG_DEBUG("udp data receiver thread affinity set to cpu {:d}", desired_affinity);
    }
    if (config["udp_data_transport:thread_priority"] >= 0) {
        if (set_thread_priority_realtime(receiver_thread, (int)config["udp_data_transport:thread_priority"]) != 0) {
            LOG_ERROR("unable to set udp data receiver thread realtime priority in udp data transport constructor");
            throw std::runtime_error("unable to set udp data receiver thread realtime priority in udp data transport constructor");
        }
        LOG_DEBUG("udp data receiver thread priority set to {:d}", config["udp_data_transport:thread_priority"]);
    }

    tx_state      = TRANSPORT_STARTING;
    sender_thread = vxsdr_thread([this] { data_send(); });

    if (config["udp_data_transport:thread_affinity_offset"] >= 0 and config["udp_data_transport:sender_thread_affinity"] >= 0) {
        auto desired_affinity =
            config["udp_data_transport:thread_affinity_offset"] + config["udp_data_transport:sender_thread_affinity"];
        if (set_thread_affinity(sender_thread, desired_affinity) != 0) {
            LOG_ERROR("unable to set udp data sender thread affinity in udp data transport constructor");
            throw std::runtime_error("unable to set udp data sender thread affinity in udp data transport constructor");
        }
        LOG_DEBUG("udp data sender thread affinity set to cpu {:d}", desired_affinity);
    }
    if (config["udp_data_transport:thread_priority"] >= 0) {
        if (set_thread_priority_realtime(sender_thread, (int)config["udp_data_transport:thread_priority"]) != 0) {
            LOG_ERROR("unable to set udp data sender thread realtime priority in udp data transport constructor");
            throw std::runtime_error("unable to set udp data sender thread realtime priority in udp data transport constructor");
        }
        LOG_DEBUG("udp data sender thread priority set to {:d}", config["udp_data_transport:thread_priority"]);
    }

    auto start_time = std::chrono::steady_clock::now();
    while (tx_state != TRANSPORT_READY or rx_state != TRANSPORT_READY) {
        std::this_thread::sleep_for(udp_ready_wait);
        if ((std::chrono::steady_clock::now() - start_time) > udp_ready_timeout) {
            LOG_ERROR("timeout waiting for transport ready in udp data transport constructor");
            throw std::runtime_error("timeout waiting for transport ready in udp data transport constructor");
        }
    }

    LOG_DEBUG("udp data transport constructor complete");
}

udp_data_transport::~udp_data_transport() noexcept {
    LOG_DEBUG("udp data transport destructor entered");
    // tx must shut down before rx since tx sends a final ack request to update stats
    LOG_DEBUG("joining udp data sender thread");
    tx_state = TRANSPORT_SHUTDOWN;
    sender_thread_stop_flag = true;
    if (sender_thread.joinable()) {
        sender_thread.join();
    }
    LOG_DEBUG("joining udp data receiver thread");
    rx_state = TRANSPORT_SHUTDOWN;
    receiver_thread_stop_flag = true;
    net_error_code err;
    // use shutdown() to terminate the blocking read
    LOG_DEBUG("shutting down udp data receiver socket");
    receiver_socket.shutdown(net::ip::udp::socket::shutdown_receive, err);
    if (err and err != std::errc::not_connected) {
        // the not connected error is expected since it's a UDP socket
        // (even though it's been connected)
        LOG_ERROR("udp data receiver socket shutdown: {:s}", err.message());
    }
    LOG_DEBUG("closing udp data receiver socket");
    receiver_socket.close(err);
    if (err) {
        LOG_ERROR("udp data receiver socket close: {:s}", err.message());
    }
    LOG_DEBUG("joining udp data receiver thread");
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }
    sender_socket.close(err);
    if (err) {
        LOG_ERROR("udp data sender socket close: {:s}", err.message());
    }
    if (log_stats_on_exit) {
        log_stats();
    }
    LOG_DEBUG("udp data transport destructor complete");
}

#ifdef VXSDR_TARGET_MACOS
#define UDP_SEND_DOES_NOT_BLOCK_ON_FULL_BUFFER
#endif

size_t udp_data_transport::packet_send(const packet& packet, int& error_code) {
    net::socket_base::message_flags flags = 0;
    net_error_code err;
#ifdef UDP_SEND_DOES_NOT_BLOCK_ON_FULL_BUFFER
    size_t bytes = 0;
    while (true) {
        bytes = sender_socket.send(net::buffer(&packet, packet.hdr.packet_size), flags, err);
        if (bytes == 0 and err == std::errc::no_buffer_space) {
            std::this_thread::sleep_for(std::chrono::microseconds(send_thread_sleep_us));
        } else {
            break;
        }
    }
#else
    size_t bytes = sender_socket.send(net::buffer(&packet, packet.hdr.packet_size), flags, err);
#endif
    error_code = err.value();
    return bytes;
}

size_t udp_data_transport::packet_receive(data_queue_element& packet, int& error_code) {
    net::socket_base::message_flags flags = 0;
    net_error_code err;
    packet.hdr = { 0, 0, 0, 0, 0, 0, 0 };
    size_t bytes = receiver_socket.receive(net::buffer(&packet, sizeof(packet)), flags, err);
    error_code = err.value();
    return bytes;
}

#endif // #ifdef VXSDR_ENABLE_UDP