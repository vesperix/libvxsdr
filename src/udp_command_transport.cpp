// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include "logging.hpp"
#include "thread_utils.hpp"
#include "vxsdr_imp.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_transport.hpp"
#include "vxsdr_threads.hpp"

/*! @file udp_command_transport.cpp
    @brief Constructor, destructor, and utility functions for the @p vxsdr_udp command transport classes.
*/

udp_command_transport::udp_command_transport(const std::string& local_address,
                                             const std::string& device_address,
                                             const std::map<std::string, int64_t>& settings)
    : sender_socket(context, net::ip::udp::v4()), receiver_socket(context, net::ip::udp::v4()) {
    LOG_DEBUG("udp command transport constructor entered");

    auto config = packet_transport::apply_transport_settings(settings, default_settings);

    net::ip::address_v4 local_ip  = net::ip::address_v4::from_string(local_address);
    net::ip::address_v4 device_ip = net::ip::address_v4::from_string(device_address);
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

void udp_command_transport::command_receive() {
    LOG_DEBUG("udp command rx started");
    uint16_t last_seq = 0;
    bytes_received    = 0;
    packets_received  = 0;
    sequence_errors   = 0;

    rx_state = TRANSPORT_READY;
    LOG_DEBUG("udp command rx in READY state");

    while (rx_state == TRANSPORT_READY and not receiver_thread_stop_flag) {
        static command_queue_element recv_buffer;
        net::socket_base::message_flags flags = 0;
        net_error_code err;
        size_t bytes_in_packet = 0;

        // sync receive
        bytes_in_packet = receiver_socket.receive(net::buffer(&recv_buffer, sizeof(recv_buffer)), flags, err);
        if (not receiver_thread_stop_flag) {
            if (err) {
                LOG_ERROR("udp command rx error: {:s}", err.message());
                rx_state = TRANSPORT_ERROR;
            } else if (bytes_in_packet > 0) {
                // check size and discard unless packet size agrees with header
                if (recv_buffer.hdr.packet_size != bytes_in_packet) {
                    LOG_ERROR("udp command rx discarded packet with incorrect size in header (header {:d}, packet {:d})",
                            (uint16_t)recv_buffer.hdr.packet_size, bytes_in_packet);
                    rx_state = TRANSPORT_ERROR;
                } else {
                    // update stats
                    packets_received++;
                    packet_types_received.at(recv_buffer.hdr.packet_type)++;
                    bytes_received += bytes_in_packet;

                    // check sequence and update sequence counter
                    if (packets_received > 1 and recv_buffer.hdr.sequence_counter != (uint16_t)(last_seq + 1)) {
                        uint16_t received = recv_buffer.hdr.sequence_counter;
                        LOG_ERROR("udp command rx sequence error (expected {:d}, received {:d})", (uint16_t)(last_seq + 1), received);
                        sequence_errors++;
                        rx_state = TRANSPORT_ERROR;
                    }
                    last_seq = recv_buffer.hdr.sequence_counter;

                    switch (recv_buffer.hdr.packet_type) {
                        case PACKET_TYPE_ASYNC_MSG:
                            if (not async_msg_queue.push_or_timeout(recv_buffer, queue_push_timeout_us, queue_push_wait_us)) {
                                LOG_ERROR("timeout pushing to async message queue in udp command rx");
                                rx_state = TRANSPORT_ERROR;
                            }
                            break;

                        case PACKET_TYPE_DEVICE_CMD_RSP:
                        case PACKET_TYPE_TX_RADIO_CMD_RSP:
                        case PACKET_TYPE_RX_RADIO_CMD_RSP:
                        case PACKET_TYPE_DEVICE_CMD_ERR:
                        case PACKET_TYPE_TX_RADIO_CMD_ERR:
                        case PACKET_TYPE_RX_RADIO_CMD_ERR:
                            if (not response_queue.push_or_timeout(recv_buffer, queue_push_timeout_us, queue_push_wait_us)) {
                                LOG_ERROR("timeout pushing to command response queue in udp command rx; stopping");
                                rx_state = TRANSPORT_ERROR;
                            }
                            break;
                        case PACKET_TYPE_DEVICE_CMD_ACK:
                        case PACKET_TYPE_TX_RADIO_CMD_ACK:
                        case PACKET_TYPE_RX_RADIO_CMD_ACK:
                            // command acks are obsolete since all commands
                            // return responses or errors; any acks received
                            // are silently ignored
                            break;
                        default:
                            LOG_WARN("udp command rx discarded incorrect packet (type {:d})", (int)recv_buffer.hdr.packet_type);
                            break;
                    }
                }
            }
        }
    }

    rx_state = TRANSPORT_SHUTDOWN;

    LOG_DEBUG("udp command rx exiting");
}

void udp_command_transport::command_send() {
    LOG_DEBUG("udp command tx started");
    static command_queue_element packet_buffer;

    tx_state = TRANSPORT_READY;
    LOG_DEBUG("udp command tx in READY state");

    while (not sender_thread_stop_flag) {
        if (command_queue.pop_or_timeout(packet_buffer, send_thread_wait_us, send_thread_sleep_us)) {
            send_packet(packet_buffer);
        }
    }

    tx_state = TRANSPORT_SHUTDOWN;

    LOG_DEBUG("udp command tx exiting");
}

bool udp_command_transport::send_packet(packet& packet) {
    bool send_ok                = true;
    packet.hdr.sequence_counter = (uint16_t)(packets_sent++ % (UINT16_MAX + 1));
    packet_types_sent.at(packet.hdr.packet_type)++;

    net_error_code err;
    size_t bytes = blocking_packet_send(packet, err);

    bytes_sent += bytes;

    if (bytes != packet.hdr.packet_size) {
        LOG_ERROR("send error in udp command tx: size incorrect");
        send_errors++;
        send_ok = false;
    } else if (err) {
        LOG_ERROR("send error in udp command tx: {:s}", err.message());
        send_errors++;
        send_ok = false;
    }

    if (not send_ok) {
        tx_state = TRANSPORT_ERROR;
    }

    return send_ok;
}

#ifdef VXSDR_TARGET_MACOS
#define UDP_SEND_DOES_NOT_BLOCK_ON_FULL_BUFFER
#endif

size_t udp_command_transport::blocking_packet_send(const packet& packet, net_error_code& err) {
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