// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iterator>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <type_traits>

#include "logging.hpp"
#include "thread_utils.hpp"
#include "vxsdr_imp.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_transport.hpp"
#include "vxsdr_threads.hpp"

/*! @file pcie_command_transport.cpp
    @brief Constructor, destructor, and utility functions for the @p vxsdr_pcie command transport classes.
*/

pcie_command_transport::pcie_command_transport(const std::map<std::string, int64_t>& settings, std::shared_ptr<pcie_dma_interface> pcie_iface) {
    LOG_DEBUG("pcie command transport constructor entered");

    auto config = packet_transport::apply_transport_settings(settings, default_settings);

    pcie_if = pcie_iface;

    rx_state        = TRANSPORT_STARTING;
    receiver_thread = vxsdr_thread([this] { command_receive(); });

    tx_state      = TRANSPORT_STARTING;
    sender_thread = vxsdr_thread([this] { command_send(); });

    auto start_time = std::chrono::steady_clock::now();
    while (tx_state != TRANSPORT_READY or rx_state != TRANSPORT_READY) {
        std::this_thread::sleep_for(pcie_ready_wait);
        if ((std::chrono::steady_clock::now() - start_time) > pcie_ready_timeout) {
            LOG_ERROR("timeout waiting for transport ready in pcie command transport constructor");
            throw std::runtime_error("timeout waiting for transport ready in pcie command transport constructor");
        }
    }
}

pcie_command_transport::~pcie_command_transport() noexcept {
    LOG_DEBUG("pcie command transport destructor entered");

    rx_state = TRANSPORT_SHUTDOWN;

    receiver_thread_stop_flag = true;

    LOG_DEBUG("joining pcie command receiver thread");
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }
    LOG_DEBUG("joining pcie command sender thread");
    tx_state = TRANSPORT_SHUTDOWN;
    sender_thread_stop_flag = true;
    if (sender_thread.joinable()) {
        sender_thread.join();
    }

    if (log_stats_on_exit) {
        log_stats();
    }
    LOG_DEBUG("pcie command transport destructor complete");
}

void pcie_command_transport::command_receive() {
    LOG_DEBUG("pcie command rx started");
    uint16_t last_seq = 0;
    bytes_received    = 0;
    packets_received  = 0;
    sequence_errors   = 0;

    rx_state = TRANSPORT_READY;
    LOG_DEBUG("pcie command rx in READY state");

    while ((rx_state == TRANSPORT_READY or rx_state == TRANSPORT_ERROR) and not receiver_thread_stop_flag) {
        static command_queue_element recv_buffer;
        int err = 0;
        size_t bytes_in_packet = 0;

        // clear buffer header and sync receive
        recv_buffer.hdr = { 0, 0, 0, 0, 0, 0, 0 };
        bytes_in_packet = pcie_if->command_receive(&recv_buffer, sizeof(recv_buffer), err);
/*
        LOG_DEBUG("CMD  RECV type {:2d} cmd {:2d} flags {:4b} subdev {:3d} chan {:3d} size {:5d} seq {:5d}",
                                                            (unsigned)recv_buffer.hdr.packet_type,
                                                            (unsigned)recv_buffer.hdr.command,
                                                            (unsigned)recv_buffer.hdr.flags,
                                                            (unsigned)recv_buffer.hdr.subdevice,
                                                            (unsigned)recv_buffer.hdr.channel,
                                                            (unsigned)recv_buffer.hdr.packet_size,
                                                            (unsigned)recv_buffer.hdr.sequence_counter);
*/
        if (not receiver_thread_stop_flag) {
            if (err != 0 and err != ETIMEDOUT) { // timeouts are ignored
                rx_state = TRANSPORT_ERROR;
                LOG_ERROR("pcie command rx error: {:s}", std::strerror(err));
                if (throw_on_rx_error) {
                    throw(std::runtime_error("pcie command rx error"));
                }
            } else if (bytes_in_packet > 0) {
                // check size and discard unless packet size agrees with header
                if (recv_buffer.hdr.packet_size != bytes_in_packet) {
                    rx_state = TRANSPORT_ERROR;
                    LOG_ERROR("packet size error in pcie command rx (header {:d}, packet {:d})",
                            (uint16_t)recv_buffer.hdr.packet_size, bytes_in_packet);
                    if (throw_on_rx_error) {
                        throw(std::runtime_error("packet size error in pcie command rx"));
                    }
                } else {
                    // update stats
                    packets_received++;
                    packet_types_received.at(recv_buffer.hdr.packet_type)++;
                    bytes_received += bytes_in_packet;

                    // check sequence and update sequence counter
                    if (packets_received > 1 and recv_buffer.hdr.sequence_counter != (uint16_t)(last_seq + 1)) {
                        uint16_t received = recv_buffer.hdr.sequence_counter;
                        rx_state = TRANSPORT_ERROR;
                        LOG_ERROR("sequence error in pcie command rx (expected {:d}, received {:d})", (uint16_t)(last_seq + 1), received);
                        sequence_errors++;
                        if (throw_on_rx_error) {
                            throw(std::runtime_error("sequence error in pcie command rx"));
                        }
                    }
                    last_seq = recv_buffer.hdr.sequence_counter;

                    switch (recv_buffer.hdr.packet_type) {
                        case PACKET_TYPE_ASYNC_MSG:
                            if (not async_msg_queue.push_or_timeout(recv_buffer, queue_push_timeout_us, queue_push_wait_us)) {
                                rx_state = TRANSPORT_ERROR;
                                LOG_ERROR("timeout pushing to async message queue in pcie command rx");
                                if (throw_on_rx_error) {
                                    throw(std::runtime_error("timeout pushing to async message queue in pcie command rx"));
                                }
                            }
                            break;

                        case PACKET_TYPE_DEVICE_CMD_RSP:
                        case PACKET_TYPE_TX_RADIO_CMD_RSP:
                        case PACKET_TYPE_RX_RADIO_CMD_RSP:
                        case PACKET_TYPE_DEVICE_CMD_ERR:
                        case PACKET_TYPE_TX_RADIO_CMD_ERR:
                        case PACKET_TYPE_RX_RADIO_CMD_ERR:
                            if (not response_queue.push_or_timeout(recv_buffer, queue_push_timeout_us, queue_push_wait_us)) {
                                rx_state = TRANSPORT_ERROR;
                                LOG_ERROR("timeout pushing to command response queue in pcie command rx");
                                if (throw_on_rx_error) {
                                    throw(std::runtime_error("timeout pushing to command response queue in pcie command rx"));
                                }
                            }
                            break;

                        default:
                            LOG_WARN("pcie command rx discarded incorrect packet (type {:d})", (int)recv_buffer.hdr.packet_type);
                            break;
                    }
                }
            }
        }
    }

    rx_state = TRANSPORT_SHUTDOWN;

    LOG_DEBUG("pcie command rx exiting");
}

void pcie_command_transport::command_send() {
    LOG_DEBUG("pcie command tx started");
    static command_queue_element packet_buffer;

    tx_state = TRANSPORT_READY;
    LOG_DEBUG("pcie command tx in READY state");

    while (not sender_thread_stop_flag) {
        if (command_queue.pop_or_timeout(packet_buffer, send_thread_wait_us, send_thread_sleep_us)) {
            send_packet(packet_buffer);
        }
    }

    tx_state = TRANSPORT_SHUTDOWN;

    LOG_DEBUG("pcie command tx exiting");
}

bool pcie_command_transport::send_packet(packet& packet) {
    packet.hdr.sequence_counter = (uint16_t)(packets_sent++ % (UINT16_MAX + 1));
    packet_types_sent.at(packet.hdr.packet_type)++;

    int err = 0;
    size_t bytes = blocking_packet_send(packet, err);

    bytes_sent += bytes;

    if (bytes != packet.hdr.packet_size) {
        tx_state = TRANSPORT_ERROR;
        LOG_ERROR("send error in pcie command tx (size incorrect)");
        send_errors++;
        if(throw_on_tx_error) {
            throw(std::runtime_error("send error in pcie command tx (size incorrect)"));
        }
        return false;
    } else if (err != 0) {
        tx_state = TRANSPORT_ERROR;
        LOG_ERROR("send error in pcie command tx: {:s}", std::strerror(err));
        send_errors++;
        if(throw_on_tx_error) {
            throw(std::runtime_error("send error in pcie command tx"));
        }
        return false;
    }

    return true;
}

size_t pcie_command_transport::blocking_packet_send(const packet& packet, int& err) {
    size_t bytes = pcie_if->command_send(&packet, packet.hdr.packet_size, err);
    return bytes;
}