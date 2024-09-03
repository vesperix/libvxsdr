// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef VXSDR_ENABLE_UDP

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <compare>
#include <map>
#include <string>
#include <stdexcept>
#include <system_error>
#include <thread>

#include "logging.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_net.hpp"
#include "vxsdr_transport.hpp"

/*! @file udp_command_transport.cpp
    @brief Constructor, destructor, and utility functions for the @p vxsdr_udp command transport classes.
*/

udp_command_transport::udp_command_transport(const std::map<std::string, int64_t>& settings)
    : sender_socket(context, net::ip::udp::v4()), receiver_socket(context, net::ip::udp::v4()) {
    LOG_DEBUG("udp command transport constructor entered");

    auto config = packet_transport::apply_transport_settings(settings, default_settings);

    // apply the convenience settings if specific settings are absent
    if (config.count("udp_transport:local_address") != 0 and config.count("udp_command_transport:local_address") == 0) {
        config["udp_command_transport:local_address"] = config["udp_transport:local_address"];
    }
    if (config.count("udp_transport:device_address") != 0 and config.count("udp_command_transport:device_address") == 0) {
        config["udp_command_transport:device_address"] = config["udp_transport:device_address"];
    }

    if (config.count("udp_command_transport:local_address") == 0 or config.count("udp_command_transport:device_address") == 0) {
        LOG_ERROR("udp command transport settings must include udp_command_transport:local_address and udp_command_transport:device_address");
        throw std::invalid_argument("udp command transport settings must include local address and device address");
    }

    net::ip::address_v4 local_ip  = net::ip::address_v4(config["udp_command_transport:local_address"]);
    net::ip::address_v4 device_ip = net::ip::address_v4(config["udp_command_transport:device_address"]);
    // FIXME: need to add cross-platform method to find local IPs and pick the most likely one
    //  so that command-line specification is no longer necessary
    //  e.g. getifaddrs() (Linux/MacOS) GetAdapterAddrs (Windows)

    net_error_code err;

    LOG_DEBUG("setting udp command sender socket to non-blocking");
    sender_socket.non_blocking(false, err);
    if (err) {
        LOG_ERROR("error setting udp command sender socket to non-blocking ({:s})", err.message());
        throw std::runtime_error("error setting udp command sender socket to non-blocking");
    }

    LOG_DEBUG("setting udp command receiver socket to non-blocking");
    receiver_socket.non_blocking(false, err);
    if (err) {
        LOG_ERROR("error setting udp command receiver socket to non-blocking ({:s})", err.message());
        throw std::runtime_error("error setting udp command receiver socket to non-blocking");
    }

    LOG_DEBUG("binding udp command sender socket to address {:s} port {:d}", local_ip.to_string(), udp_host_cmd_send_port);
    sender_socket.bind(net::ip::udp::endpoint(local_ip, udp_host_cmd_send_port), err);
    if (err) {
        LOG_ERROR("error binding udp command sender socket on local address {:s}; check that network interface is up ({:s})",
                  local_ip.to_string(), err.message());
        throw std::runtime_error("error binding udp command sender socket on local address " + local_ip.to_string() +
                                 "; check that network interface is up");
    }

    LOG_DEBUG("binding udp command receiver socket to address {:s} port {:d}", local_ip.to_string(), udp_host_cmd_receive_port);
    receiver_socket.bind(net::ip::udp::endpoint(local_ip, udp_host_cmd_receive_port), err);
    if (err) {
        LOG_ERROR("error binding udp command receiver socket on local address {:s}; check that network interface is up ({:s})",
                  local_ip.to_string(), err.message());
        throw std::runtime_error("error binding udp command receiver socket on local address " + local_ip.to_string() +
                                 "; check that network interface is up");
    }

    LOG_DEBUG("connecting udp command sender socket to address {:s} port {:d}", device_ip.to_string(), udp_device_cmd_receive_port);
    sender_socket.connect(net::ip::udp::endpoint(device_ip, udp_device_cmd_receive_port), err);
    if (err) {
        LOG_ERROR("error connecting udp command sender socket to device address {:s} ({:s})", device_ip.to_string(), err.message());
        throw std::runtime_error("error connecting udp command sender socket to device address " + device_ip.to_string());
    }

    LOG_DEBUG("connecting udp command receiver socket to address {:s} port {:d}", device_ip.to_string(), udp_device_cmd_send_port);
    receiver_socket.connect(net::ip::udp::endpoint(device_ip, udp_device_cmd_send_port), err);
    if (err) {
        LOG_ERROR("error connecting udp command receiver socket to device address {:s} ({:s})", device_ip.to_string(), err.message());
        throw std::runtime_error("error connecting udp command receiver socket to device address " + device_ip.to_string());
    }

    rx_state        = TRANSPORT_STARTING;
    receiver_thread = vxsdr_thread([this] { command_receive(); });

    tx_state      = TRANSPORT_STARTING;
    sender_thread = vxsdr_thread([this] { command_send(); });

    auto start_time = std::chrono::steady_clock::now();
    while (tx_state != TRANSPORT_READY or rx_state != TRANSPORT_READY) {
        std::this_thread::sleep_for(udp_ready_wait);
        if ((std::chrono::steady_clock::now() - start_time) > udp_ready_timeout) {
            LOG_ERROR("timeout waiting for transport ready in udp command transport constructor");
            throw std::runtime_error("timeout waiting for transport ready in udp command transport constructor");
        }
    }
}

udp_command_transport::~udp_command_transport() noexcept {
    LOG_DEBUG("udp command transport destructor entered");
    rx_state = TRANSPORT_SHUTDOWN;
    receiver_thread_stop_flag = true;
    net_error_code err;
    // use shutdown() to terminate the blocking read
    LOG_DEBUG("shutting down udp command receiver socket");
    receiver_socket.shutdown(net::ip::udp::socket::shutdown_receive, err);
    if (err and err != std::errc::not_connected) {
        // the not connected error is expected since it's a UDP socket
        // (even though it's been connected)
        LOG_ERROR("udp command receiver socket shutdown: {:s}", err.message());
    }
    LOG_DEBUG("closing udp command receiver socket");
    receiver_socket.close(err);
    if (err) {
        LOG_ERROR("udp command receiver socket close: {:s}", err.message());
    }
    LOG_DEBUG("joining udp command receiver thread");
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }
    LOG_DEBUG("joining udp command sender thread");
    tx_state = TRANSPORT_SHUTDOWN;
    sender_thread_stop_flag = true;
    if (sender_thread.joinable()) {
        sender_thread.join();
    }
    sender_socket.close(err);
    if (err) {
        LOG_ERROR("udp command sender socket close: {:s}", err.message());
    }
    if (log_stats_on_exit) {
        log_stats();
    }
    LOG_DEBUG("udp command transport destructor complete");
}

#ifdef VXSDR_TARGET_MACOS
#define UDP_SEND_DOES_NOT_BLOCK_ON_FULL_BUFFER
#endif

size_t udp_command_transport::packet_send(const packet& packet, int& error_code) {
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

size_t udp_command_transport::packet_receive(command_queue_element& packet, int& error_code) {
    net::socket_base::message_flags flags = 0;
    net_error_code err;
    packet.hdr = { 0, 0, 0, 0, 0, 0, 0 };
    size_t bytes = receiver_socket.receive(net::buffer(&packet, sizeof(packet)), flags, err);
    error_code = err.value();
    return bytes;
}

#endif // #ifdef VXSDR_ENABLE_UDP