// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <complex>
#include <cstddef>
#include <cstdint>

#include "logging.hpp"
#include "socket_utils.hpp"
#include "thread_utils.hpp"
#include "vxsdr_imp.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_transport.hpp"
#include "vxsdr_threads.hpp"

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
        throw std::runtime_error("udp data transport settings must include local address and device address");
    }

    net::ip::address_v4 local_ip  = net::ip::address_v4(config["udp_data_transport:local_address"]);
    net::ip::address_v4 device_ip = net::ip::address_v4(config["udp_data_transport:device_address"]);
    // FIXME: need to add cross-platform method to find local IPs and pick the most likely one
    //  so that command-line specification is no longer necessary
    //  e.g. getifaddrs() (Linux/MacOS) GetAdapterAddrs (Windows)

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
    auto mtu_est = get_socket_mtu(sender_socket);
    if (mtu_est < 0) {
        LOG_ERROR("error getting mtu for udp data sender socket");
        throw std::runtime_error("error getting mtu for udp data sender socket");
    }
    if (mtu_est < (int)(MAX_DATA_PACKET_BYTES + 20)) { // 20 is a typical IP header size
        LOG_ERROR("mtu too small for udp data sender socket");
        throw std::runtime_error("mtu too small for udp data sender socket");
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

    for (unsigned i = 0; i < num_subdevs; i++) {
        rx_data_queue.push_back(
            std::make_unique<spsc_queue<data_queue_element>>(config["udp_data_transport:rx_data_queue_packets"]));
        rx_sample_queue.push_back(
            std::make_unique<spsc_queue<vxsdr::wire_sample>>(MAX_DATA_LENGTH_SAMPLES));
    }

    LOG_DEBUG("using {:d} receive data buffers of {:d} packets", num_subdevs, config["udp_data_transport:rx_data_queue_packets"]);
    LOG_DEBUG("using {:d} receive sample buffers of {:d} samples", num_subdevs, MAX_DATA_LENGTH_SAMPLES);

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

void udp_data_transport::data_receive() {
    LOG_DEBUG("udp data rx started");
    uint16_t last_seq = 0;
    bytes_received    = 0;
    samples_received  = 0;
    packets_received  = 0;
    sequence_errors   = 0;

    if (rx_data_queue.empty()) {
        rx_state = TRANSPORT_SHUTDOWN;
        LOG_FATAL("queues not initialized in udp data rx");
        throw(std::runtime_error("queues not initialized in udp data rx"));
        return;
    }

    rx_state = TRANSPORT_READY;
    LOG_DEBUG("udp data rx in READY state");

    while ((rx_state == TRANSPORT_READY or rx_state == TRANSPORT_ERROR) and not receiver_thread_stop_flag) {
        static data_queue_element recv_buffer;
        net::socket_base::message_flags flags = 0;
        net_error_code err;
        size_t bytes_in_packet = 0;

        // sync receive
        bytes_in_packet = receiver_socket.receive(net::buffer(&recv_buffer, sizeof(recv_buffer)), flags, err);

        if (not receiver_thread_stop_flag) {
            if (err) {
                rx_state = TRANSPORT_ERROR;
                LOG_ERROR("udp data rx packet error: {:s}", err.message());
                if (throw_on_rx_error) {
                    throw(std::runtime_error("udp data receive error"));
                }
            } else if (bytes_in_packet > 0) {
                // check size and discard unless packet size agrees with header
                if (recv_buffer.hdr.packet_size != bytes_in_packet) {
                    rx_state = TRANSPORT_ERROR;
                    LOG_ERROR("packet size error in udp data rx (header {:d}, packet {:d})",
                            (uint16_t)recv_buffer.hdr.packet_size, bytes_in_packet);
                    if (throw_on_rx_error) {
                        throw(std::runtime_error("packet size error in udp data rx"));
                    }
                } else {
                    // update stats
                    packets_received++;
                    packet_types_received.at(recv_buffer.hdr.packet_type)++;
                    bytes_received += bytes_in_packet;

                    // check sequence and update sequence counter
                    if (packets_received > 1 and recv_buffer.hdr.sequence_counter != (uint16_t)(last_seq + 1)) {
                        rx_state = TRANSPORT_ERROR;
                        uint16_t received = recv_buffer.hdr.sequence_counter;
                        LOG_ERROR("sequence error in udp data rx (expected {:d}, received {:d})", (uint16_t)(last_seq + 1), received);
                        sequence_errors++;
                        sequence_errors_current_stream++;
                        if (throw_on_rx_error) {
                            throw(std::runtime_error("sequence error in udp data rx"));
                        }
                    }
                    last_seq = recv_buffer.hdr.sequence_counter;

                    if (recv_buffer.hdr.packet_type == PACKET_TYPE_RX_SIGNAL_DATA) {
                        // check subdevice
                        if (recv_buffer.hdr.subdevice < num_subdevs) {
                            uint16_t preamble_size = get_packet_preamble_size(recv_buffer.hdr);
                            // update sample stats
                            size_t n_samps = (recv_buffer.hdr.packet_size - preamble_size) / sizeof(vxsdr::wire_sample);
                            samples_received += n_samps;
                            samples_received_current_stream += n_samps;
                            if (samples_expected_rx_stream > 0 and samples_received_current_stream > samples_expected_rx_stream ) {
                                LOG_WARN("more samples received than expected in udp data rx (received {:d} expected {:d})",
                                          samples_received_current_stream, samples_expected_rx_stream);
                            }
                            if (not rx_data_queue[recv_buffer.hdr.subdevice]->push(recv_buffer)) {
                                rx_state = TRANSPORT_ERROR;
                                LOG_ERROR("error pushing to data queue in udp data rx (subdevice {:d} sample {:d})",
                                            recv_buffer.hdr.subdevice, samples_received);
                                if (throw_on_rx_error) {
                                    throw(std::runtime_error("error pushing to data queue in udp data rx"));
                                }
                            }
                        } else {
                            LOG_WARN("udp data rx discarded rx data packet from unknown subdevice {:d}", recv_buffer.hdr.subdevice);
                        }
                    } else if (recv_buffer.hdr.packet_type == PACKET_TYPE_TX_SIGNAL_DATA_ACK) {
                        auto* r = std::bit_cast<six_uint32_packet*>(&recv_buffer);
                        tx_buffer_used_bytes = r->value3;
                        tx_buffer_size_bytes = r->value4;
                        tx_packet_oos_count  = r->value5;
                        if (tx_buffer_size_bytes > 0) {
                            tx_buffer_fill_percent = (unsigned)std::min(100ULL, (100ULL * tx_buffer_used_bytes) / tx_buffer_size_bytes);
                        } else {
                            tx_buffer_fill_percent = 0;
                        }
                    } else {
                        LOG_WARN("udp data rx discarded incorrect packet (type {:d})", (int)recv_buffer.hdr.packet_type);
                    }
                }
            }
        }
    }

    rx_state = TRANSPORT_SHUTDOWN;

    LOG_DEBUG("udp data rx exiting");
}

void udp_data_transport::data_send() {
    LOG_DEBUG("udp data tx started");
    enum throttling_state { NO_THROTTLING = 0, NORMAL_THROTTLING = 1, HARD_THROTTLING = 2 };
    static constexpr unsigned data_buffer_size = 256;
    static std::array<data_queue_element, data_buffer_size> data_buffer;

    uint64_t data_packets_processed = 0;
    uint64_t last_check             = 0;
    // Note: all of these must be less than or equal to data_buffer_size
    //       since they are used for unchecked indexing into data_buffer!
    unsigned buffer_low_packets_to_send      = data_buffer_size;
    unsigned buffer_normal_packets_to_send   = data_buffer_size;
    unsigned buffer_check_default_packets    = data_buffer_size;
    unsigned buffer_check_throttling_packets = data_buffer_size / 2;

    throttling_state throttling_state = NO_THROTTLING;
    unsigned buffer_check_interval = buffer_check_default_packets;
    unsigned max_packets_to_send   = buffer_normal_packets_to_send;

    if (tx_data_queue == nullptr) {
        tx_state = TRANSPORT_SHUTDOWN;
        LOG_FATAL("queue not initialized in udp data tx");
        throw(std::runtime_error("queue not initialized in udp data tx"));
        return;
    }


    tx_state = TRANSPORT_READY;
    LOG_DEBUG("udp data tx in READY state");

    while (not sender_thread_stop_flag) {
        if (use_tx_throttling) {
            // There are 3 throttling states: no throttling, normal throttling, and hard throttling;
            // transitions are shown in the state machine below.
            // Note the hysteresis in entering and exiting normal throttling (throttle_off_percent < throttle_on_percent),
            // which reduces bouncing between states.
            if (throttling_state == NO_THROTTLING) {
                if (tx_buffer_fill_percent >= throttle_hard_percent) {
                    throttling_state = HARD_THROTTLING;
                    LOG_TRACE("udp data tx entering throttling state HARD from NONE ({:2d}% full)",
                                (int)tx_buffer_fill_percent);
                } else if (tx_buffer_fill_percent >= throttle_on_percent) {
                    throttling_state = NORMAL_THROTTLING;
                    LOG_TRACE("udp data tx entering throttling state NRML from NONE ({:2d}% full)",
                                (int)tx_buffer_fill_percent);
                }
            } else if (throttling_state == NORMAL_THROTTLING) {
                if (tx_buffer_fill_percent >= throttle_hard_percent) {
                    throttling_state = HARD_THROTTLING;
                    LOG_TRACE("udp data tx entering throttling state HARD from NRML ({:2d}% full)",
                                (int)tx_buffer_fill_percent);
                } else if (tx_buffer_fill_percent < throttle_off_percent) {
                    throttling_state = NO_THROTTLING;
                    LOG_TRACE("udp data tx entering throttling state NONE from NRML ({:2d}% full)",
                                (int)tx_buffer_fill_percent);
                }
            } else {  // current_state == HARD_THROTTLING
                if (tx_buffer_fill_percent < throttle_off_percent) {
                    throttling_state = NO_THROTTLING;
                    LOG_TRACE("udp data tx entering throttling state NONE from HARD ({:2d}% full)",
                                (int)tx_buffer_fill_percent);
                } else if (tx_buffer_fill_percent < throttle_hard_percent) {
                    throttling_state = NORMAL_THROTTLING;
                    LOG_TRACE("udp data tx entering throttling state NRML from HARD ({:2d}% full)",
                                (int)tx_buffer_fill_percent);
                }
            }
            // In no throttling and normal throttling, two control variables are set:
            //    buffer_check_interval_packets = the number of packets to send between requesting an update on device buffer fill
            //    max_packets_to_send           = the maximum number of packets to send in a burst
            if (throttling_state == NORMAL_THROTTLING) {
                buffer_check_interval = buffer_check_throttling_packets;
                max_packets_to_send   = buffer_normal_packets_to_send;

            } else if (throttling_state == NO_THROTTLING) {
                buffer_check_interval = buffer_check_default_packets;
                max_packets_to_send   = buffer_low_packets_to_send;
            }
        } else {
            throttling_state = NO_THROTTLING;
        }

        if (throttling_state == HARD_THROTTLING) {
            // when hard throttling, send one empty data packet and request ack to update buffer use
            data_buffer[0].hdr = {PACKET_TYPE_TX_SIGNAL_DATA, 0, FLAGS_REQUEST_ACK, 0, 0, sizeof(header_only_packet), 0};
            send_packet(data_buffer[0]);
            last_check = data_packets_processed;
            std::this_thread::sleep_for(std::chrono::microseconds(send_thread_sleep_us));
        } else {
            // when not hard throttling, send at most max_packets_to_send packets and update buffer fills
            // every buffer_check_interval packets
            unsigned n_popped = tx_data_queue->pop_or_timeout(&data_buffer.front(), max_packets_to_send, send_thread_wait_us, send_thread_sleep_us);
            for (unsigned i = 0; i < n_popped; i++) {
                if (use_tx_throttling and (data_packets_processed == 0 or data_packets_processed - last_check >= buffer_check_interval)) {
                    // request ack to update buffer use
                    data_buffer[i].hdr.flags |= FLAGS_REQUEST_ACK;
                    last_check = data_packets_processed;
                }
                if (send_packet(data_buffer[i])) {
                    data_packets_processed++;
                }
                if (use_tx_throttling and throttling_state != NO_THROTTLING) {
                    // if we are throttling, pause between each packet
                    std::this_thread::sleep_for(std::chrono::microseconds(throttle_amount_us));
                }
            }

        }
    }

    if (rx_state == TRANSPORT_READY or rx_state == TRANSPORT_ERROR) {
        // send a last empty packet with an ack request so that the stats are updated
        data_buffer[0].hdr = {PACKET_TYPE_TX_SIGNAL_DATA, 0, FLAGS_REQUEST_ACK, 0, 0, sizeof(header_only_packet), 0};
        send_packet(data_buffer[0]);
        // wait for the response to be received by the data rx
        std::this_thread::sleep_for(20ms);
    } else {
        LOG_WARN("udp data rx unavailable at udp tx shutdown: stats will not be updated");
    }

    tx_state = TRANSPORT_SHUTDOWN;

    LOG_DEBUG("udp data tx exiting");
}

bool udp_data_transport::send_packet(packet& packet) {
    packet.hdr.sequence_counter = (uint16_t)(packets_sent++ % (UINT16_MAX + 1UL));
    packet_types_sent.at(packet.hdr.packet_type)++;

    net_error_code err;
    size_t bytes = blocking_packet_send(packet, err);

    bytes_sent += bytes;

    if (bytes != packet.hdr.packet_size) {
        tx_state = TRANSPORT_ERROR;
        LOG_ERROR("send error in udp data tx (size incorrect)");
        send_errors++;
        if(throw_on_tx_error) {
            throw(std::runtime_error("send error in udp data tx (size incorrect)"));
        }
        return false;
    } else if (err) {
        tx_state = TRANSPORT_ERROR;
        LOG_ERROR("send error in udp data tx: {:s}", err.message());
        send_errors++;
        if(throw_on_tx_error) {
            throw(std::runtime_error("send error in udp data tx"));
        }
        return false;
    }

    auto preamble_size = get_packet_preamble_size(packet.hdr);
    if (packet.hdr.packet_type == PACKET_TYPE_TX_SIGNAL_DATA and bytes > preamble_size) {
        samples_sent += (bytes - preamble_size) / sizeof(vxsdr::wire_sample);
        samples_sent_current_stream += (bytes - preamble_size) / sizeof(vxsdr::wire_sample);
        if (samples_expected_tx_stream > 0 and samples_sent_current_stream > samples_expected_tx_stream) {
            LOG_WARN("more samples sent than expected in udp data tx (sent {:d} expected {:d})",
                        samples_sent_current_stream, samples_expected_tx_stream);
        }
    }

    return true;
}

#ifdef VXSDR_TARGET_MACOS
#define UDP_SEND_DOES_NOT_BLOCK_ON_FULL_BUFFER
#endif

size_t udp_data_transport::blocking_packet_send(const packet& packet, net_error_code& err) {
    net::socket_base::message_flags flags = 0;
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
    return bytes;
}