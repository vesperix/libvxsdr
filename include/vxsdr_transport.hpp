// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <complex>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>
#include <ratio>
#include <string>
#include <thread>
#include <vector>
using namespace std::chrono_literals;

#include "logging.hpp"
#include "vxsdr_net.hpp"
#include "vxsdr_pcie.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_queues.hpp"
#include "vxsdr_threads.hpp"

#include "vxsdr.hpp"

class packet_transport {
  protected:
    // statistics
    uint64_t send_errors  = 0;
    uint64_t packets_sent = 0;
    uint64_t bytes_sent   = 0;

    uint64_t sequence_errors  = 0;
    uint64_t packets_received = 0;
    uint64_t bytes_received   = 0;

    std::array<uint64_t, (1UL << VXSDR_PACKET_TYPE_BITS)> packet_types_sent     = {0};
    std::array<uint64_t, (1UL << VXSDR_PACKET_TYPE_BITS)> packet_types_received = {0};

    // control of behavior
    std::atomic<bool> throw_on_tx_error  {false};
    std::atomic<bool> throw_on_rx_error  {false};
    std::atomic<bool> log_stats_on_exit  {true};

  public:
    packet_transport() = default;
    packet_transport(const std::string& local_address,
              const std::string& device_address,
              const std::map<std::string, int64_t>& settings);

    virtual ~packet_transport()                          = default;
    packet_transport(const packet_transport&)            = delete;
    packet_transport& operator=(const packet_transport&) = delete;
    packet_transport(packet_transport&&)                 = delete;
    packet_transport& operator=(packet_transport&&)      = delete;

    // flags used for orderly shutdown
    std::atomic<bool> sender_thread_stop_flag   {false};
    std::atomic<bool> receiver_thread_stop_flag {false};

    using transport_state = enum { TRANSPORT_UNINITIALIZED, TRANSPORT_STARTING, TRANSPORT_READY, TRANSPORT_SHUTDOWN, TRANSPORT_ERROR };
    // state of transport in each direction
    std::atomic<transport_state> tx_state {TRANSPORT_UNINITIALIZED};
    std::atomic<transport_state> rx_state {TRANSPORT_UNINITIALIZED};

    std::map<std::string, int64_t> apply_transport_settings(const std::map<std::string, int64_t>& settings,
                                                            const std::map<std::string, int64_t>& default_settings) const {
        std::map<std::string, int64_t> config = default_settings;
        for (const auto& s : settings) {
            if (config.count(s.first) > 0) {
                if (config[s.first] != s.second) {
                    config[s.first] = s.second;
                }
            } else {
                config[s.first] = s.second;
            }
        }
        return config;
    };
    void set_log_stats_on_exit(const bool value) {
        log_stats_on_exit = value;
    }
    void set_throw_on_error(const bool value) {
        throw_on_tx_error = value;
        throw_on_rx_error = value;
    }
    bool rx_usable() {
        if (rx_state != TRANSPORT_READY and
            rx_state != TRANSPORT_ERROR) {
            return false;
        }
        return true;
    }
    bool tx_usable() {
        if (tx_state != TRANSPORT_READY and
            tx_state != TRANSPORT_ERROR) {
            return false;
        }
        return true;
    }
    bool tx_rx_usable() {
        return tx_usable() and rx_usable();
    }
    virtual bool reset_rx() {
        if (rx_state == TRANSPORT_UNINITIALIZED or
            rx_state == TRANSPORT_SHUTDOWN) {
            LOG_ERROR("reset rx failed: state is {:s}", transport_state_to_string(rx_state));
            return false;
        }
        rx_state = TRANSPORT_READY;
        sequence_errors                 = 0;
        packets_received                = 0;
        bytes_received                  = 0;
        for (unsigned j = 0; j < (1UL << VXSDR_PACKET_TYPE_BITS); j++) {
            packet_types_received[j] = 0;
        }
        return true;
    }
    virtual bool reset_tx() {
        if (tx_state == TRANSPORT_UNINITIALIZED or
            tx_state == TRANSPORT_SHUTDOWN) {
            LOG_ERROR("reset tx failed: state is {:s}", transport_state_to_string(tx_state));
            return false;
        }
        tx_state = TRANSPORT_READY;
        send_errors                 = 0;
        packets_sent                = 0;
        bytes_sent                  = 0;
        for (unsigned j = 0; j < (1UL << VXSDR_PACKET_TYPE_BITS); j++) {
            packet_types_sent[j] = 0;
        }
        return true;
    }
    size_t get_packet_preamble_size(const packet_header& hdr) const {
        size_t preamble_size = sizeof(packet_header);
        if ((bool)(hdr.flags & FLAGS_TIME_PRESENT)) {
            preamble_size += sizeof(time_spec_t);
        }
        if ((bool)(hdr.flags & FLAGS_STREAM_ID_PRESENT)) {
            preamble_size += sizeof(stream_spec_t);
        }
        return preamble_size;
    };
    std::map<std::string, int64_t> default_settings = {};
    std::string packet_type_to_string(const uint8_t number) const {
        switch (number) {
            case PACKET_TYPE_TX_SIGNAL_DATA:
                return "TX_SIGNAL_DATA";
            case PACKET_TYPE_RX_SIGNAL_DATA:
                return "RX_SIGNAL_DATA";
            case PACKET_TYPE_DEVICE_CMD:
                return "DEVICE_CMD";
            case PACKET_TYPE_TX_RADIO_CMD:
                return "TX_RADIO_CMD";
            case PACKET_TYPE_RX_RADIO_CMD:
                return "RX_RADIO_CMD";
            case PACKET_TYPE_ASYNC_MSG:
                return "ASYNC_MSG";
            case PACKET_TYPE_DEVICE_CMD_ERR:
                return "DEVICE_CMD_ERR";
            case PACKET_TYPE_TX_RADIO_CMD_ERR:
                return "TX_RADIO_CMD_ERR";
            case PACKET_TYPE_RX_RADIO_CMD_ERR:
                return "RX_RADIO_CMD_ERR";
            case PACKET_TYPE_DEVICE_CMD_RSP:
                return "DEVICE_CMD_RSP";
            case PACKET_TYPE_TX_RADIO_CMD_RSP:
                return "TX_RADIO_CMD_RSP";
            case PACKET_TYPE_RX_RADIO_CMD_RSP:
                return "RX_RADIO_CMD_RSP";
            case PACKET_TYPE_TX_SIGNAL_DATA_ACK:
                return "TX_SIGNAL_DATA_ACK";
            case PACKET_TYPE_RX_SIGNAL_DATA_ACK:
                return "RX_SIGNAL_DATA_ACK";
            default:
                return "UNKNOWN_PACKET_TYPE";
        }
    }
    std::string transport_state_to_string(const transport_state state) const {
        switch (state) {
            case TRANSPORT_UNINITIALIZED:
                return "UNINITIALIZED";
            case TRANSPORT_STARTING:
                return "STARTING";
            case TRANSPORT_READY:
                return "READY";
            case TRANSPORT_SHUTDOWN:
                return "SHUTDOWN";
            case TRANSPORT_ERROR:
                return "ERROR";
            default:
                return "UNKNOWN";
        }
    }
};

class command_transport : public packet_transport {
  protected:
    std::string transport_type = "unknown";
    static constexpr unsigned send_thread_wait_us   = 10'000;
    static constexpr unsigned send_thread_sleep_us  =    200;
    static constexpr unsigned queue_push_timeout_us = 10'000;
    static constexpr unsigned queue_push_wait_us    =  1'000;

    // because every command has a response, the command and
    // response queues are just used as interprocess comms mechanisms;
    // there will never be more than one command or response in flight
    static constexpr size_t command_queue_length  = 1;
    static constexpr size_t response_queue_length = 1;
    // async messages, however, can come fast
    static constexpr size_t async_msg_queue_length = 1024;

  public:
    command_transport() = default;
    virtual ~command_transport() = default;

    mpmc_queue<command_queue_element> command_queue{command_queue_length};
    mpmc_queue<command_queue_element> response_queue{response_queue_length};
    mpmc_queue<command_queue_element> async_msg_queue{async_msg_queue_length};

    virtual size_t packet_send(const packet& packet, int& error_code);
    virtual size_t packet_receive(command_queue_element& packet, int& error_code);

    void command_receive() {
        LOG_DEBUG("{:s} command rx started", transport_type);
        uint16_t last_seq = 0;
        bytes_received    = 0;
        packets_received  = 0;
        sequence_errors   = 0;

        rx_state = TRANSPORT_READY;
        LOG_DEBUG("{:s} command rx in READY state", transport_type);

        while ((rx_state == TRANSPORT_READY or rx_state == TRANSPORT_ERROR) and not receiver_thread_stop_flag) {
            static command_queue_element recv_buffer;
            int err = 0;
            size_t bytes_in_packet = 0;

            // sync receive
            bytes_in_packet = packet_receive(recv_buffer, err);

            if (not receiver_thread_stop_flag) {
                if (err != 0 and err != ETIMEDOUT) { // timeouts are ignored
                    rx_state = TRANSPORT_ERROR;
                    LOG_ERROR("{:s} command rx error: {:s}", transport_type, std::strerror(err));
                    if (throw_on_rx_error) {
                        throw(std::runtime_error(transport_type + " command rx error"));
                    }
                } else if (bytes_in_packet > 0) {
                    // check size and discard unless packet size agrees with header
                    if (recv_buffer.hdr.packet_size != bytes_in_packet) {
                        rx_state = TRANSPORT_ERROR;
                        LOG_ERROR("packet size error in {:s} command rx (header {:d}, packet {:d})",
                                transport_type, (uint16_t)recv_buffer.hdr.packet_size, bytes_in_packet);
                        if (throw_on_rx_error) {
                            throw(std::runtime_error("packet size error in " + transport_type + " command rx"));
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
                            LOG_ERROR("sequence error in {:s} command rx (expected {:d}, received {:d})",
                                transport_type, (uint16_t)(last_seq + 1), received);
                            sequence_errors++;
                            if (throw_on_rx_error) {
                                throw(std::runtime_error("sequence error in " + transport_type + " command rx"));
                            }
                        }
                        last_seq = recv_buffer.hdr.sequence_counter;

                        switch (recv_buffer.hdr.packet_type) {
                            case PACKET_TYPE_ASYNC_MSG:
                                if (not async_msg_queue.push_or_timeout(recv_buffer, queue_push_timeout_us, queue_push_wait_us)) {
                                    rx_state = TRANSPORT_ERROR;
                                    LOG_ERROR("timeout pushing to async message queue in {:s} command rx", transport_type);
                                    if (throw_on_rx_error) {
                                        throw(std::runtime_error("timeout pushing to async message queue in " + transport_type + " command rx"));
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
                                    LOG_ERROR("timeout pushing to command response queue in {:s} command rx", transport_type);
                                    if (throw_on_rx_error) {
                                        throw(std::runtime_error("timeout pushing to command response queue in " + transport_type + " command rx"));
                                    }
                                }
                                break;

                            default:
                                LOG_WARN("{:s} command rx discarded incorrect packet (type {:d})", transport_type, (int)recv_buffer.hdr.packet_type);
                                break;
                        }
                    }
                }
            }
        }

        rx_state = TRANSPORT_SHUTDOWN;

        LOG_DEBUG("{:s} command rx exiting", transport_type);
    }

    void command_send() {
        LOG_DEBUG("{:s} command tx started", transport_type);
        static command_queue_element packet_buffer;

        tx_state = TRANSPORT_READY;
        LOG_DEBUG("{:s} command tx in READY state", transport_type);

        while (not sender_thread_stop_flag) {
            if (command_queue.pop_or_timeout(packet_buffer, send_thread_wait_us, send_thread_sleep_us)) {
                send_packet(packet_buffer);
            }
        }

        tx_state = TRANSPORT_SHUTDOWN;

        LOG_DEBUG("{:s} command tx exiting", transport_type);
    }

    bool send_packet(packet& packet) {
        packet.hdr.sequence_counter = (uint16_t)(packets_sent++ % (UINT16_MAX + 1));
        packet_types_sent.at(packet.hdr.packet_type)++;

        int err;
        size_t bytes = packet_send(packet, err);

        bytes_sent += bytes;

        if (bytes != packet.hdr.packet_size) {
            tx_state = TRANSPORT_ERROR;
            LOG_ERROR("send error in {:s} command tx (size incorrect)", transport_type);
            send_errors++;
            if(throw_on_tx_error) {
                throw(std::runtime_error("send error in " + transport_type + " command tx (size incorrect)"));
            }
            return false;
        } else if (err) {
            tx_state = TRANSPORT_ERROR;
            LOG_ERROR("send error in {:s} command tx: {:s}", transport_type, std::strerror(err));
            send_errors++;
            if(throw_on_tx_error) {
                throw(std::runtime_error("send error in " + transport_type + " command tx"));
            }
            return false;
        }

        return true;
    }

    void log_stats() {
        LOG_INFO("{:s} command transport:", transport_type);
        LOG_INFO("       rx state is {:s}", transport_state_to_string(rx_state));
        LOG_INFO("   {:15d} packets received", packets_received);
        for (unsigned i = 0; i < packet_types_received.size(); i++) {
            if (packet_types_received.at(i) > 0) {
                LOG_INFO("   {:15d} {:20s} ({:d})", packet_types_received.at(i), packet_type_to_string(i), i);
            }
        }
        LOG_INFO("   {:15d} bytes received", bytes_received);
        if(sequence_errors == 0) {
            LOG_INFO("   {:15d} sequence errors", sequence_errors);
        } else {
            LOG_WARN("   {:15d} sequence errors", sequence_errors);
        }
        LOG_INFO("       tx state is {:s}", transport_state_to_string(tx_state));
        LOG_INFO("   {:15d} packets sent", packets_sent);
        for (unsigned i = 0; i < packet_types_sent.size(); i++) {
            if (packet_types_sent.at(i) > 0) {
                LOG_INFO("   {:15d} {:20s} ({:d})", packet_types_sent.at(i), packet_type_to_string(i), i);
            }
        }
        LOG_INFO("   {:15d} bytes sent", bytes_sent);
        if(send_errors == 0) {
            LOG_INFO("   {:15d} send errors", send_errors);
        } else {
            LOG_WARN("   {:15d} send errors", send_errors);
        }
    }
    bool reset_rx() {
        if (not packet_transport::reset_rx()) {
            return false;
        }
        response_queue.reset();
        async_msg_queue.reset();
        return true;
    }
    bool reset_tx() {
        if (not packet_transport::reset_tx()) {
            return false;
        }
        command_queue.reset();
        return true;
    }
};

class data_transport : public packet_transport {
  protected:
    std::string transport_type = "unknown";
    unsigned sample_granularity;
    unsigned num_rx_subdevs;
    unsigned max_samples_per_packet;

    // control over throttling for transports that use it
    static constexpr bool use_tx_throttling = false;
    static constexpr unsigned throttle_hard_percent = 100;
    static constexpr unsigned throttle_on_percent   = 100;
    static constexpr unsigned throttle_off_percent  = 100;
    static constexpr unsigned throttle_amount_us    = 0;  // minimum is 60 us on most Linux systems

    static constexpr unsigned send_thread_wait_us   = 10'000;
    static constexpr unsigned send_thread_sleep_us  =    100;

    // parameters used to monitor status of the radio's internal buffers
    // (on the other end of the network connection) for throttling
    std::atomic<unsigned> tx_buffer_size_bytes   {0};
    std::atomic<unsigned> tx_buffer_used_bytes   {0};
    std::atomic<unsigned> tx_buffer_fill_percent {0};

    // number of samples in current stream (0 if continuous)
    uint64_t samples_expected_tx_stream = 0;
    uint64_t samples_expected_rx_stream = 0;

    // statistics in addition to those in packet_transport
    uint64_t samples_sent                = 0;
    uint64_t send_errors_current_stream  = 0;
    uint64_t samples_sent_current_stream = 0;

    uint64_t samples_received                = 0;
    uint64_t sequence_errors_current_stream  = 0;
    uint64_t samples_received_current_stream = 0;

    std::atomic<unsigned> tx_packet_oos_count {0};

  public:

    data_transport(const unsigned granularity, const unsigned n_rx_subdevs, const unsigned max_samps_per_packet) :
                sample_granularity(granularity), num_rx_subdevs(n_rx_subdevs) {
        max_samples_per_packet = sample_granularity * (max_samps_per_packet / sample_granularity);
    };

    virtual ~data_transport() = default;

    // single SPSC data queue for TX (since the device handles sending to the right subdevice)
    std::unique_ptr<spsc_queue<data_queue_element>> tx_data_queue;
    // vector of unique_ptrs to SPSC rx data queues, one for each subdevice
    // (required since SPSC queue is not moveable)
    std::vector<std::unique_ptr<spsc_queue<data_queue_element>>> rx_data_queue;
    // sample queues for each device hold samples left over when the requested data
    // size is less than a full packet
    std::vector<std::unique_ptr<spsc_queue<vxsdr::wire_sample>>> rx_sample_queue;

    virtual size_t packet_send(const packet& packet, int& error_code);
    virtual size_t packet_receive(data_queue_element& packet, int& error_code);


    void data_receive() {
        LOG_DEBUG("{:s} data rx started", transport_type);
        uint16_t last_seq = 0;
        bytes_received    = 0;
        samples_received  = 0;
        packets_received  = 0;
        sequence_errors   = 0;

        if (rx_data_queue.empty()) {
            rx_state = TRANSPORT_SHUTDOWN;
            LOG_FATAL("queues not initialized in {:s} data rx", transport_type);
            throw(std::runtime_error("queues not initialized in " + transport_type + " data rx"));
            return;
        }

        rx_state = TRANSPORT_READY;
        LOG_DEBUG("{:s} data rx in READY state", transport_type);

        while ((rx_state == TRANSPORT_READY or rx_state == TRANSPORT_ERROR) and not receiver_thread_stop_flag) {
            static data_queue_element recv_buffer;
            int err = 0;
            size_t bytes_in_packet = 0;

            // sync receive
            bytes_in_packet = packet_receive(recv_buffer, err);

            if (not receiver_thread_stop_flag) {
                if (err != 0 and err != ETIMEDOUT) {
                    rx_state = TRANSPORT_ERROR;
                    LOG_ERROR("{:s} data receive error: {:s}", transport_type, std::strerror(err));
                    if (throw_on_rx_error) {
                        throw(std::runtime_error(transport_type + " data receive error"));
                    }
                } else if (bytes_in_packet > 0) {
                    // check size and discard unless packet size agrees with header
                    if (recv_buffer.hdr.packet_size != bytes_in_packet) {
                        rx_state = TRANSPORT_ERROR;
                        LOG_ERROR("packet size error in {:s} data rx (header {:d}, packet {:d})",
                                transport_type, (uint16_t)recv_buffer.hdr.packet_size, bytes_in_packet);
                        if (throw_on_rx_error) {
                            throw(std::runtime_error("packet size error in " + transport_type + " data rx"));
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
                            LOG_ERROR("sequence error in {:s} data rx (expected {:d}, received {:d})",
                                    transport_type, (uint16_t)(last_seq + 1), received);
                            sequence_errors++;
                            sequence_errors_current_stream++;
                            if (throw_on_rx_error) {
                                throw(std::runtime_error("sequence error in " + transport_type + " data rx"));
                            }
                        }
                        last_seq = recv_buffer.hdr.sequence_counter;

                        if (recv_buffer.hdr.packet_type == PACKET_TYPE_RX_SIGNAL_DATA) {
                            // check subdevice
                            if (recv_buffer.hdr.subdevice < num_rx_subdevs) {
                                uint16_t preamble_size = get_packet_preamble_size(recv_buffer.hdr);
                                // update sample stats
                                size_t n_samps = (recv_buffer.hdr.packet_size - preamble_size) / sizeof(vxsdr::wire_sample);
                                samples_received += n_samps;
                                samples_received_current_stream += n_samps;
                                if (not rx_data_queue[recv_buffer.hdr.subdevice]->push(recv_buffer)) {
                                    rx_state = TRANSPORT_ERROR;
                                    LOG_ERROR("error pushing to data queue in {:s} data rx (subdevice {:d} sample {:d})",
                                            transport_type, recv_buffer.hdr.subdevice, samples_received);
                                    if (throw_on_rx_error) {
                                        throw(std::runtime_error("error pushing to data queue in " + transport_type + " data rx"));
                                    }
                                }
                            } else {
                                LOG_WARN("{:s} data rx discarded rx data packet from unknown subdevice {:d}",
                                        transport_type, recv_buffer.hdr.subdevice);
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
                            LOG_WARN("{:s} data rx discarded incorrect packet (type {:d})", transport_type, (int)recv_buffer.hdr.packet_type);
                        }
                    }
                }
            }
        }

        rx_state = TRANSPORT_SHUTDOWN;

        LOG_DEBUG("{:s} data rx exiting", transport_type);
    }

    void data_send() {
        LOG_DEBUG("{:s} data tx started", transport_type);
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
            LOG_FATAL("queue not initialized in {:s} data tx", transport_type);
            throw(std::runtime_error("queue not initialized in " + transport_type + " data tx"));
            return;
        }

        tx_state = TRANSPORT_READY;
        LOG_DEBUG("{:s} data tx in READY state", transport_type);

        while (not sender_thread_stop_flag) {
            if constexpr (use_tx_throttling) {
                // There are 3 throttling states: no throttling, normal throttling, and hard throttling;
                // transitions are shown in the state machine below.
                // Note the hysteresis in entering and exiting normal throttling (throttle_off_percent < throttle_on_percent),
                // which reduces bouncing between states.
                if (throttling_state == NO_THROTTLING) {
                    if (tx_buffer_fill_percent >= throttle_hard_percent) {
                        throttling_state = HARD_THROTTLING;
                        LOG_TRACE("{:s} data tx entering throttling state HARD from NONE ({:2d}% full)",
                                    transport_type, (int)tx_buffer_fill_percent);
                    } else if (tx_buffer_fill_percent >= throttle_on_percent) {
                        throttling_state = NORMAL_THROTTLING;
                        LOG_TRACE("{:s} data tx entering throttling state NRML from NONE ({:2d}% full)",
                                    transport_type, (int)tx_buffer_fill_percent);
                    }
                } else if (throttling_state == NORMAL_THROTTLING) {
                    if (tx_buffer_fill_percent >= throttle_hard_percent) {
                        throttling_state = HARD_THROTTLING;
                        LOG_TRACE("{:s} data tx entering throttling state HARD from NRML ({:2d}% full)",
                                    transport_type, (int)tx_buffer_fill_percent);
                    } else if (tx_buffer_fill_percent < throttle_off_percent) {
                        throttling_state = NO_THROTTLING;
                        LOG_TRACE("{:s} data tx entering throttling state NONE from NRML ({:2d}% full)",
                                    transport_type, (int)tx_buffer_fill_percent);
                    }
                } else {  // current_state == HARD_THROTTLING
                    if (tx_buffer_fill_percent < throttle_off_percent) {
                        throttling_state = NO_THROTTLING;
                        LOG_TRACE("{:s} data tx entering throttling state NONE from HARD ({:2d}% full)",
                                    transport_type, (int)tx_buffer_fill_percent);
                    } else if (tx_buffer_fill_percent < throttle_hard_percent) {
                        throttling_state = NORMAL_THROTTLING;
                        LOG_TRACE("{:s} data tx entering throttling state NRML from HARD ({:2d}% full)",
                                    transport_type, (int)tx_buffer_fill_percent);
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
            LOG_WARN("{:s} data rx unavailable at tx shutdown: stats will not be updated", transport_type);
        }

        tx_state = TRANSPORT_SHUTDOWN;

        LOG_DEBUG("{:s} data tx exiting", transport_type);
    }

    bool send_packet(packet& packet) {
        packet.hdr.sequence_counter = (uint16_t)(packets_sent++ % (UINT16_MAX + 1));
        packet_types_sent.at(packet.hdr.packet_type)++;

        int err = 0;
        size_t bytes = packet_send(packet, err);

        if (err != 0) {
            tx_state = TRANSPORT_ERROR;
            LOG_ERROR("send error in {:s} data tx: {:s}", transport_type, strerror(err));
            send_errors++;
            if(throw_on_tx_error) {
                throw(std::runtime_error("send error in " + transport_type + " data tx"));
            }
            return false;
        } else if (bytes != packet.hdr.packet_size) {
            tx_state = TRANSPORT_ERROR;
            LOG_ERROR("send error in {:s} data tx (size incorrect)", transport_type);
            send_errors++;
            if(throw_on_tx_error) {
                throw(std::runtime_error("send error in " + transport_type + " data tx (size incorrect)"));
            }
            return false;
        }
        bytes_sent += bytes;

        auto header_size = get_packet_preamble_size(packet.hdr);
        if (packet.hdr.packet_type == PACKET_TYPE_TX_SIGNAL_DATA and bytes > header_size) {
            samples_sent += (bytes - header_size) / sizeof(vxsdr::wire_sample);
            samples_sent_current_stream += (bytes - header_size) / sizeof(vxsdr::wire_sample);
        }

        return true;
    }

    void log_stats() {
        LOG_INFO("{:s} data transport:", transport_type);
        LOG_INFO("       rx state is {:s}", transport_state_to_string(rx_state));
        LOG_INFO("   {:15d} packets received", packets_received);
        for (unsigned i = 0; i < packet_types_received.size(); i++) {
            if (packet_types_received.at(i) > 0) {
                LOG_INFO("   {:15d} {:20s} ({:d})", packet_types_received.at(i), packet_type_to_string(i), i);
            }
        }
        LOG_INFO("   {:15d} samples received", samples_received);
        LOG_INFO("   {:15d} bytes received", bytes_received);
        if(sequence_errors == 0) {
            LOG_INFO("   {:15d} sequence errors", sequence_errors);
        } else {
            LOG_WARN("   {:15d} sequence errors", sequence_errors);
        }
        LOG_INFO("       tx state is {:s}", transport_state_to_string(tx_state));
        LOG_INFO("   {:15d} packets sent", packets_sent);
        for (unsigned i = 0; i < packet_types_sent.size(); i++) {
            if (packet_types_sent.at(i) > 0) {
                LOG_INFO("   {:15d} {:20s} ({:d})", packet_types_sent.at(i), packet_type_to_string(i), i);
            }
        }
        LOG_INFO("   {:15d} samples sent", samples_sent);
        LOG_INFO("   {:15d} bytes sent", bytes_sent);
        if(tx_packet_oos_count == 0) {
            LOG_INFO("   {:15d} packets out of sequence at device", (unsigned)tx_packet_oos_count);
        } else {
            LOG_WARN("   {:15d} packets out of sequence at device", (unsigned)tx_packet_oos_count);
        }
        if(send_errors == 0) {
            LOG_INFO("   {:15d} send errors", send_errors);
        } else {
            LOG_WARN("   {:15d} send errors", send_errors);
        }
    }

    bool reset_rx() {
        if (not packet_transport::reset_rx()) {
            return false;
        }
        samples_received                = 0;
        sequence_errors_current_stream  = 0;
        samples_received_current_stream = 0;
        for (unsigned i = 0; i < num_rx_subdevs; i++) {
            rx_data_queue[i]->reset();
            rx_sample_queue[i]->reset();
        }
        return true;
    }

    bool reset_tx() {
        if (not packet_transport::reset_tx()) {
            return false;
        }
        samples_sent                = 0;
        send_errors_current_stream  = 0;
        samples_sent_current_stream = 0;
        tx_data_queue->reset();
        return true;
    }

    bool reset_rx_stream(const uint64_t n_samples_expected) {
        samples_expected_rx_stream      = n_samples_expected;
        sequence_errors_current_stream  = 0;
        samples_received_current_stream = 0;
        for (unsigned i = 0; i < num_rx_subdevs; i++) {
            rx_data_queue[i]->reset();
            rx_sample_queue[i]->reset();
        }
        return true;
    }

    bool reset_tx_stream(const uint64_t n_samples_expected) {
        samples_expected_tx_stream  = n_samples_expected;
        send_errors_current_stream  = 0;
        samples_sent_current_stream = 0;
        tx_data_queue->reset();
        return true;
    }

    unsigned get_max_samples_per_packet() {
        return max_samples_per_packet;
    }

    bool set_max_samples_per_packet(const unsigned n_samples) {
        if (n_samples > 0 and n_samples <= MAX_DATA_LENGTH_SAMPLES) {
            max_samples_per_packet = sample_granularity * (n_samples / sample_granularity);
            return true;
        }
        return false;
    }
};

class udp_command_transport : public command_transport {
  protected:
    std::string transport_type = "udp";

    // timeouts for the UDP transport to reach ready state
    static constexpr auto udp_ready_timeout = 100'000us;
    static constexpr auto udp_ready_wait    =   1'000us;

    // net context and sockets
    static constexpr unsigned udp_host_cmd_receive_port   =  1030;
    static constexpr unsigned udp_device_cmd_receive_port =  1030;
    static constexpr unsigned udp_host_cmd_send_port      = 55123;
    static constexpr unsigned udp_device_cmd_send_port    =  1030;

    net::io_context context;
    net::ip::udp::socket sender_socket;
    net::ip::udp::socket receiver_socket;

    // threads used for sending and receiving
    vxsdr_thread sender_thread;
    vxsdr_thread receiver_thread;

  public:
    explicit udp_command_transport(const std::map<std::string, int64_t>& settings);
    ~udp_command_transport() noexcept;

  protected:
    void command_receive();
    void command_send();
    bool send_packet(packet& packet);
    size_t packet_send(const packet& packet, int& error_code);
    size_t packet_receive(command_queue_element& packet, int& error_code);
};

class udp_data_transport : public data_transport {
  protected:
    std::string transport_type = "udp";
    std::map<std::string, int64_t> default_settings = {{"udp_data_transport:tx_data_queue_packets",              511},
                                                       {"udp_data_transport:rx_data_queue_packets",          262'143},
                                                       {"udp_data_transport:mtu_bytes",                        9'000},
                                                       {"udp_data_transport:network_send_buffer_bytes",      262'144},
                                                       {"udp_data_transport:network_receive_buffer_bytes", 8'388'608},
                                                       {"udp_data_transport:thread_priority",                      1},
                                                       {"udp_data_transport:thread_affinity_offset",               0},
                                                       {"udp_data_transport:sender_thread_affinity",               0},
                                                       {"udp_data_transport:receiver_thread_affinity",             1}};

    // timeouts for the UDP transport to reach ready state
    static constexpr auto udp_ready_timeout = 100'000us;
    static constexpr auto udp_ready_wait    =   1'000us;

    static constexpr unsigned udp_host_data_receive_port   =  1031;
    static constexpr unsigned udp_device_data_receive_port =  1031;
    static constexpr unsigned udp_host_data_send_port      = 55124;
    static constexpr unsigned udp_device_data_send_port    =  1031;

    // net context and sockets
    net::io_context context;
    net::ip::udp::socket sender_socket;
    net::ip::udp::socket receiver_socket;

    // threads used for sending and receiving
    vxsdr_thread sender_thread;
    vxsdr_thread receiver_thread;

    // transmit throttling settings
    static constexpr bool use_tx_throttling = true;
    static constexpr unsigned throttle_hard_percent = 90;
    static constexpr unsigned throttle_on_percent   = 80;
    static constexpr unsigned throttle_off_percent  = 60;
    static constexpr unsigned throttle_amount_us    = 50;  // will actually be around 60 us on most Linux systems

  public:
    explicit udp_data_transport(const std::map<std::string, int64_t>& settings,
                                const unsigned granularity,
                                const unsigned n_subdevs,
                                const unsigned max_samps_per_packet);
    ~udp_data_transport() noexcept;

  protected:
    void data_send();
    void data_receive();
    bool send_packet(packet& packet);
    size_t packet_send(const packet& packet, int& error_code);
    size_t packet_receive(data_queue_element& packet, int& error_code);
};

class pcie_command_transport : public command_transport {
  protected:
    std::string transport_type = "pcie";

    // timeouts for the PCIe transport to reach ready state
    static constexpr auto pcie_ready_timeout = 100'000us;
    static constexpr auto pcie_ready_wait    =   1'000us;

    // threads used for sending and receiving
    vxsdr_thread sender_thread;
    vxsdr_thread receiver_thread;

    std::shared_ptr<pcie_dma_interface> pcie_if = nullptr;

  public:
    explicit pcie_command_transport(const std::map<std::string, int64_t>& settings, std::shared_ptr<pcie_dma_interface> pcie_iface);
    ~pcie_command_transport() noexcept;

  protected:
    void command_receive();
    void command_send();
    bool send_packet(packet& packet);
    size_t packet_send(const packet& packet, int& error_code);
    size_t packet_receive(command_queue_element& packet, int& error_code);
};

class pcie_data_transport : public data_transport {
  protected:
    std::string transport_type = "pcie";
    std::map<std::string, int64_t> default_settings = {{"pcie_data_transport:tx_data_queue_packets",              511},
                                                       {"pcie_data_transport:rx_data_queue_packets",          262'143},
                                                       {"pcie_data_transport:thread_priority",                      1},
                                                       {"pcie_data_transport:thread_affinity_offset",               0},
                                                       {"pcie_data_transport:sender_thread_affinity",               0},
                                                       {"pcie_data_transport:receiver_thread_affinity",             1}};

    // timeouts for the PCIe transport to reach ready state
    static constexpr auto pcie_ready_timeout = 100'000us;
    static constexpr auto pcie_ready_wait    =   1'000us;

    // threads used for sending and receiving
    vxsdr_thread sender_thread;
    vxsdr_thread receiver_thread;

    static constexpr bool use_tx_throttling = false;
    static constexpr unsigned throttle_hard_percent = 95;
    static constexpr unsigned throttle_on_percent   = 90;
    static constexpr unsigned throttle_off_percent  = 85;
    static constexpr unsigned throttle_amount_us    = 50;  // will actually be around 60 us on most Linux systems

    std::shared_ptr<pcie_dma_interface> pcie_if = nullptr;

  public:
    explicit pcie_data_transport(const std::map<std::string, int64_t>& settings,
                                 std::shared_ptr<pcie_dma_interface> pcie_iface,
                                 const unsigned granularity,
                                 const unsigned n_rx_subdevs,
                                 const unsigned max_samps_per_packet);
    ~pcie_data_transport() noexcept;

  protected:
    void data_send();
    void data_receive();
    bool send_packet(packet& packet);
    size_t packet_send(const packet& packet, int& error_code);
    size_t packet_receive(data_queue_element& packet, int& error_code);
};
