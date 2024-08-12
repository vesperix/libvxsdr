// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cerrno>

#include "logging.hpp"
#include "thread_utils.hpp"
#include "vxsdr_imp.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_transport.hpp"
#include "vxsdr_threads.hpp"

/*! @file pcie_data_transport.cpp
    @brief Constructor, destructor, and utility functions for the @p vxsdr_pcie data transport classes.
*/

pcie_data_transport::pcie_data_transport(const std::map<std::string, int64_t>& settings,
                                         std::shared_ptr<pcie_dma_interface> pcie_iface,
                                         const unsigned granularity,
                                         const unsigned n_rx_subdevs,
                                         const unsigned max_samps_per_packet)
            : data_transport(granularity, n_rx_subdevs, max_samps_per_packet) {
    LOG_DEBUG("pcie data transport constructor entered");
    num_rx_subdevs = n_rx_subdevs;

    auto config = packet_transport::apply_transport_settings(settings, default_settings);

    pcie_if = pcie_iface;

    LOG_DEBUG("using transmit data buffer of {:d} packets", config["pcie_data_transport:tx_data_queue_packets"]);
    tx_data_queue = std::make_unique<spsc_queue<data_queue_element>>(config["pcie_data_transport:tx_data_queue_packets"]);

    for (unsigned i = 0; i < num_rx_subdevs; i++) {
        rx_data_queue.push_back(
            std::make_unique<spsc_queue<data_queue_element>>(config["pcie_data_transport:rx_data_queue_packets"]));
        rx_sample_queue.push_back(
            std::make_unique<spsc_queue<std::complex<int16_t>>>(MAX_DATA_LENGTH_SAMPLES));
    }

    LOG_DEBUG("using {:d} receive data buffers of {:d} packets", num_rx_subdevs, config["pcie_data_transport:rx_data_queue_packets"]);
    LOG_DEBUG("using {:d} receive sample buffers of {:d} samples", num_rx_subdevs, MAX_DATA_LENGTH_SAMPLES);

    rx_state        = TRANSPORT_STARTING;
    receiver_thread = vxsdr_thread([this] { data_receive(); });

    if (config["pcie_data_transport:thread_affinity_offset"] >= 0 and config["pcie_data_transport:receiver_thread_affinity"] >= 0) {
        auto desired_affinity =
            config["pcie_data_transport:thread_affinity_offset"] + config["pcie_data_transport:receiver_thread_affinity"];
        if (set_thread_affinity(receiver_thread, desired_affinity) != 0) {
            LOG_ERROR("unable to set pcie data receiver thread affinity in pcie data transport constructor");
            throw std::runtime_error("unable to set pcie data receiver thread affinity in pcie data transport constructor");
        }
        LOG_DEBUG("pcie data receiver thread affinity set to cpu {:d}", desired_affinity);
    }
    if (config["pcie_data_transport:thread_priority"] >= 0) {
        if (set_thread_priority_realtime(receiver_thread, (int)config["pcie_data_transport:thread_priority"]) != 0) {
            LOG_ERROR("unable to set pcie data receiver thread realtime priority in pcie data transport constructor");
            throw std::runtime_error("unable to set pcie data receiver thread realtime priority in pcie data transport constructor");
        }
        LOG_DEBUG("pcie data receiver thread priority set to {:d}", config["pcie_data_transport:thread_priority"]);
    }

    tx_state      = TRANSPORT_STARTING;
    sender_thread = vxsdr_thread([this] { data_send(); });

    if (config["pcie_data_transport:thread_affinity_offset"] >= 0 and config["pcie_data_transport:sender_thread_affinity"] >= 0) {
        auto desired_affinity =
            config["pcie_data_transport:thread_affinity_offset"] + config["pcie_data_transport:sender_thread_affinity"];
        if (set_thread_affinity(sender_thread, desired_affinity) != 0) {
            LOG_ERROR("unable to set pcie data sender thread affinity in pcie data transport constructor");
            throw std::runtime_error("unable to set pcie data sender thread affinity in pcie data transport constructor");
        }
        LOG_DEBUG("pcie data sender thread affinity set to cpu {:d}", desired_affinity);
    }
    if (config["pcie_data_transport:thread_priority"] >= 0) {
        if (set_thread_priority_realtime(sender_thread, (int)config["pcie_data_transport:thread_priority"]) != 0) {
            LOG_ERROR("unable to set pcie data sender thread realtime priority in pcie data transport constructor");
            throw std::runtime_error("unable to set pcie data sender thread realtime priority in pcie data transport constructor");
        }
        LOG_DEBUG("pcie data sender thread priority set to {:d}", config["pcie_data_transport:thread_priority"]);
    }

    auto start_time = std::chrono::steady_clock::now();
    while (tx_state != TRANSPORT_READY or rx_state != TRANSPORT_READY) {
        std::this_thread::sleep_for(pcie_ready_wait);
        if ((std::chrono::steady_clock::now() - start_time) > pcie_ready_timeout) {
            LOG_ERROR("timeout waiting for transport ready in pcie data transport constructor");
            throw std::runtime_error("timeout waiting for transport ready in pcie data transport constructor");
        }
    }

    LOG_DEBUG("pcie data transport constructor complete");
}

pcie_data_transport::~pcie_data_transport() noexcept {
    LOG_DEBUG("pcie data transport destructor entered");

    tx_state = TRANSPORT_SHUTDOWN;
    sender_thread_stop_flag = true;

    // tx must shut down before rx since tx sends a final ack request to update stats
    LOG_DEBUG("joining pcie data sender thread");
    if (sender_thread.joinable()) {
        sender_thread.join();
    }

    rx_state = TRANSPORT_SHUTDOWN;
    receiver_thread_stop_flag = true;

    LOG_DEBUG("joining pcie data receiver thread");
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }

    if (log_stats_on_exit) {
        log_stats();
    }
    LOG_DEBUG("pcie data transport destructor complete");
}

void pcie_data_transport::data_receive() {
    LOG_DEBUG("pcie data rx started");
    uint16_t last_seq = 0;
    bytes_received    = 0;
    samples_received  = 0;
    packets_received  = 0;
    sequence_errors   = 0;

    if (rx_data_queue.empty()) {
        rx_state = TRANSPORT_SHUTDOWN;
        LOG_FATAL("queues not initialized in pcie data rx");
        throw(std::runtime_error("queues not initialized in pcie data rx"));
        return;
    }

    rx_state = TRANSPORT_READY;
    LOG_DEBUG("pcie data rx in READY state");

    while ((rx_state == TRANSPORT_READY or rx_state == TRANSPORT_ERROR) and not receiver_thread_stop_flag) {
        static data_queue_element recv_buffer;
        int err = 0;
        size_t bytes_in_packet = 0;

        // sync receive
        bytes_in_packet = pcie_if->data_receive(&recv_buffer, sizeof(recv_buffer), err);

        if (not receiver_thread_stop_flag) {
            if (err != 0 and err != ETIMEDOUT) {
                rx_state = TRANSPORT_ERROR;
                LOG_ERROR("pcie data receive error: {:s} ({:d})", std::strerror(err), err);
                if (throw_on_rx_error) {
                    throw(std::runtime_error("pcie data receive error"));
                }
            } else if (bytes_in_packet > 0) {
                // check size and discard unless packet size agrees with header
                if (recv_buffer.hdr.packet_size != bytes_in_packet) {
                    rx_state = TRANSPORT_ERROR;
                    LOG_ERROR("packet size error in pcie data rx (header {:d}, packet {:d})",
                            (uint16_t)recv_buffer.hdr.packet_size, bytes_in_packet);
                    if (throw_on_rx_error) {
                        throw(std::runtime_error("packet size error in pcie data rx"));
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
                        LOG_ERROR("sequence error in pcie data rx (expected {:d}, received {:d})", (uint16_t)(last_seq + 1), received);
                        sequence_errors++;
                        sequence_errors_current_stream++;
                        if (throw_on_rx_error) {
                            throw(std::runtime_error("sequence error in pcie data rx"));
                        }
                    }
                    last_seq = recv_buffer.hdr.sequence_counter;

                    if (recv_buffer.hdr.packet_type == PACKET_TYPE_RX_SIGNAL_DATA) {
                        // check subdevice
                        if (recv_buffer.hdr.subdevice < num_rx_subdevs) {
                            uint16_t preamble_size = get_packet_preamble_size(recv_buffer.hdr);
                            // update sample stats
                            size_t n_samps = (recv_buffer.hdr.packet_size - preamble_size) / sizeof(std::complex<int16_t>);
                            samples_received += n_samps;
                            samples_received_current_stream += n_samps;
                            if (not rx_data_queue[recv_buffer.hdr.subdevice]->push(recv_buffer)) {
                                rx_state = TRANSPORT_ERROR;
                                LOG_ERROR("error pushing to data queue in pcie data rx (subdevice {:d} sample {:d})",
                                            recv_buffer.hdr.subdevice, samples_received);
                                if (throw_on_rx_error) {
                                    throw(std::runtime_error("error pushing to data queue in pcie data rx"));
                                }
                            }
                        } else {
                            LOG_WARN("pcie data rx discarded rx data packet from unknown subdevice {:d}", recv_buffer.hdr.subdevice);
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
                        LOG_WARN("pcie data rx discarded incorrect packet (type {:d})", (int)recv_buffer.hdr.packet_type);
                    }
                }
            }
        }
    }

    rx_state = TRANSPORT_SHUTDOWN;

    LOG_DEBUG("pcie data rx exiting");
}

void pcie_data_transport::data_send() {
    LOG_DEBUG("pcie data tx started");
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
        LOG_FATAL("queue not initialized in pcie data tx");
        throw(std::runtime_error("queue not initialized in pcie data tx"));
        return;
    }


    tx_state = TRANSPORT_READY;
    LOG_DEBUG("pcie data tx in READY state");

    while (not sender_thread_stop_flag) {
        if (use_tx_throttling) {
            // There are 3 throttling states: no throttling, normal throttling, and hard throttling;
            // transitions are shown in the state machine below.
            // Note the hysteresis in entering and exiting normal throttling (throttle_off_percent < throttle_on_percent),
            // which reduces bouncing between states.
            if (throttling_state == NO_THROTTLING) {
                if (tx_buffer_fill_percent >= throttle_hard_percent) {
                    throttling_state = HARD_THROTTLING;
                    LOG_TRACE("pcie data tx entering throttling state HARD from NONE ({:2d}% full)",
                                (int)tx_buffer_fill_percent);
                } else if (tx_buffer_fill_percent >= throttle_on_percent) {
                    throttling_state = NORMAL_THROTTLING;
                    LOG_TRACE("pcie data tx entering throttling state NRML from NONE ({:2d}% full)",
                                (int)tx_buffer_fill_percent);
                }
            } else if (throttling_state == NORMAL_THROTTLING) {
                if (tx_buffer_fill_percent >= throttle_hard_percent) {
                    throttling_state = HARD_THROTTLING;
                    LOG_TRACE("pcie data tx entering throttling state HARD from NRML ({:2d}% full)",
                                (int)tx_buffer_fill_percent);
                } else if (tx_buffer_fill_percent < throttle_off_percent) {
                    throttling_state = NO_THROTTLING;
                    LOG_TRACE("pcie data tx entering throttling state NONE from NRML ({:2d}% full)",
                                (int)tx_buffer_fill_percent);
                }
            } else {  // current_state == HARD_THROTTLING
                if (tx_buffer_fill_percent < throttle_off_percent) {
                    throttling_state = NO_THROTTLING;
                    LOG_TRACE("pcie data tx entering throttling state NONE from HARD ({:2d}% full)",
                                (int)tx_buffer_fill_percent);
                } else if (tx_buffer_fill_percent < throttle_hard_percent) {
                    throttling_state = NORMAL_THROTTLING;
                    LOG_TRACE("pcie data tx entering throttling state NRML from HARD ({:2d}% full)",
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
        if (use_tx_throttling and throttling_state == HARD_THROTTLING) {
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
        LOG_WARN("pcie data rx unavailable at pcie tx shutdown: stats will not be updated");
    }

    tx_state = TRANSPORT_SHUTDOWN;

    LOG_DEBUG("pcie data tx exiting");
}

bool pcie_data_transport::send_packet(packet& packet) {
    packet.hdr.sequence_counter = (uint16_t)(packets_sent++ % (UINT16_MAX + 1));
    packet_types_sent.at(packet.hdr.packet_type)++;

    int err = 0;
    size_t bytes = pcie_if->data_send(&packet, packet.hdr.packet_size, err);

    if (err != 0) {
        tx_state = TRANSPORT_ERROR;
        LOG_ERROR("send error in pcie data tx: {:s}", strerror(err));
        send_errors++;
        if(throw_on_tx_error) {
            throw(std::runtime_error("send error in pcie data tx"));
        }
        return false;
    } else if (bytes != packet.hdr.packet_size) {
        tx_state = TRANSPORT_ERROR;
        LOG_ERROR("send error in pcie data tx (size incorrect)");
        send_errors++;
        if(throw_on_tx_error) {
            throw(std::runtime_error("send error in pcie data tx (size incorrect)"));
        }
        return false;
    }
    bytes_sent += bytes;

    auto header_size = get_packet_preamble_size(packet.hdr);
    if (packet.hdr.packet_type == PACKET_TYPE_TX_SIGNAL_DATA and bytes > header_size) {
        samples_sent += (bytes - header_size) / sizeof(std::complex<int16_t>);
        samples_sent_current_stream += (bytes - header_size) / sizeof(std::complex<int16_t>);
    }

    return true;
}
