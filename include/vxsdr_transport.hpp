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
    std::atomic<bool> stop_on_tx_error  {false};
    std::atomic<bool> stop_on_rx_error  {false};
    std::atomic<bool> log_stats_on_exit {true};

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
                    LOG_DEBUG("changed setting {:s} = {:d}", s.first, config[s.first]);
                }
            } else {
                config[s.first] = s.second;
                LOG_DEBUG("added setting {:s} = {:d}", s.first, config[s.first]);
            }
        }
        return config;
    };
    void log_stats(const bool value) {
        log_stats_on_exit = value;
    }
    void stop_on_error(const bool value) {
        stop_on_tx_error = value;
        stop_on_rx_error = value;
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
    bool reset_rx() {
        if (rx_state == TRANSPORT_UNINITIALIZED or
            rx_state == TRANSPORT_SHUTDOWN) {
            LOG_ERROR("reset rx stream failed: state is {:s}", transport_state_to_string(rx_state));
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
    bool reset_tx() {
        if (tx_state == TRANSPORT_UNINITIALIZED or
            tx_state == TRANSPORT_SHUTDOWN) {
            LOG_ERROR("reset tx stream failed: state is {:s}", transport_state_to_string(tx_state));
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
    size_t get_packet_header_size(const packet_header& hdr) const {
        size_t header_size = sizeof(packet_header);
        if ((bool)(hdr.flags & FLAGS_TIME_PRESENT)) {
            header_size += sizeof(time_spec_t);
        }
        if ((bool)(hdr.flags & FLAGS_STREAM_ID_PRESENT)) {
            header_size += sizeof(stream_spec_t);
        }
        return header_size;
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

    void log_stats() {
        LOG_INFO("command transport:");
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
    unsigned num_rx_subdevs = 0;
    // number of samples in current stream (0 if continuous)
    uint64_t desired_tx_stream_samples = 0;
    uint64_t desired_rx_stream_samples = 0;

    // statistics in addition to those in packet_transport
    uint64_t samples_sent                = 0;
    uint64_t send_errors_current_stream  = 0;
    uint64_t samples_sent_current_stream = 0;

    uint64_t samples_received                = 0;
    uint64_t sequence_errors_current_stream  = 0;
    uint64_t samples_received_current_stream = 0;

    std::atomic<unsigned> tx_packet_oos_count {0};

    // transmit throttling settings
    static constexpr unsigned throttle_hard_percent = 90;
    static constexpr unsigned throttle_on_percent   = 80;
    static constexpr unsigned throttle_off_percent  = 60;
    static constexpr unsigned throttle_amount_us    = 50;  // will actually be around 60 us on most Linux systems

  public:

    data_transport(const unsigned n_rx_subdevs = 0) : num_rx_subdevs(n_rx_subdevs) {};

    virtual ~data_transport() = default;

    // single SPSC data queue for TX (since the device handles sending to the right subdevice)
    std::unique_ptr<spsc_queue<data_queue_element>> tx_data_queue;
    // vector of unique_ptrs to SPSC rx data queues, one for each subdevice
    // (required since SPSC queue is not moveable)
    std::vector<std::unique_ptr<spsc_queue<data_queue_element>>> rx_data_queue;

    void log_stats() {
        LOG_INFO("data transport:");
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
        }
        return true;
    }

    bool reset_tx(const uint64_t n_samples_desired) {
        if (not packet_transport::reset_tx()) {
            return false;
        }
        samples_sent                = 0;
        send_errors_current_stream  = 0;
        samples_sent_current_stream = 0;
        tx_data_queue->reset();
        return true;
    }

    bool reset_rx_stream(const uint64_t n_samples_desired) {
        LOG_DEBUG("reset rx stream started");
        desired_rx_stream_samples       = n_samples_desired;
        sequence_errors_current_stream  = 0;
        samples_received_current_stream = 0;
        for (unsigned i = 0; i < num_rx_subdevs; i++) {
            rx_data_queue[i]->reset();
        }
        LOG_DEBUG("reset rx stream finished");
        return true;
    }

    bool reset_tx_stream(const uint64_t n_samples_desired) {
        LOG_DEBUG("reset tx stream started");
        desired_tx_stream_samples   = n_samples_desired;
        send_errors_current_stream  = 0;
        samples_sent_current_stream = 0;
        tx_data_queue->reset();
        LOG_DEBUG("reset tx stream finished");
        return true;
    }
};

class udp_command_transport : public command_transport {
  protected:
    static constexpr unsigned send_thread_wait_us  = 10'000;
    static constexpr unsigned send_thread_sleep_us =    200;

    // timeouts for the UDP transport to reach ready state
    static constexpr auto udp_ready_timeout = 100'000us;
    static constexpr auto udp_ready_wait    =   1'000us;

    static constexpr unsigned queue_push_timeout_us = 10'000;
    static constexpr unsigned queue_push_wait_us    =  1'000;

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
    explicit udp_command_transport(const std::string& local_address,
                                   const std::string& device_address,
                                   const std::map<std::string, int64_t>& settings);
    ~udp_command_transport() noexcept;

  protected:
    void command_receive();
    void command_send();
    bool send_packet(packet& packet);
    size_t blocking_packet_send(const packet& packet, net_error_code& err);
};

class udp_data_transport : public data_transport {
  protected:
    std::map<std::string, int64_t> default_settings = {{"udp_data_transport:tx_data_queue_packets",              511},
                                                       {"udp_data_transport:rx_data_queue_packets",          262'143},
                                                       {"udp_data_transport:network_send_buffer_bytes",      262'144},
                                                       {"udp_data_transport:network_receive_buffer_bytes", 8'388'608},
                                                       {"udp_data_transport:net_thread_priority",                  1},
                                                       {"udp_data_transport:thread_affinity_offset",               0},
                                                       {"udp_data_transport:sender_thread_affinity",               0},
                                                       {"udp_data_transport:receiver_thread_affinity",             1}};

    static constexpr unsigned send_thread_wait_us   = 10'000;
    static constexpr unsigned send_thread_sleep_us  =    100;

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

    // parameters used to monitor status of the radio's internal buffers
    // (on the other end of the network connection) for throttling
    std::atomic<unsigned> tx_buffer_size_bytes   {0};
    std::atomic<unsigned> tx_buffer_used_bytes   {0};
    std::atomic<unsigned> tx_buffer_fill_percent {0};

  public:
    explicit udp_data_transport(const std::string& local_address,
                                const std::string& device_address,
                                const std::map<std::string, int64_t>& settings,
                                const unsigned n_rx_subdevs = 0);
    ~udp_data_transport() noexcept;

  protected:
    void data_send();
    void data_receive();
    bool send_packet(packet& packet);
    size_t blocking_packet_send(const packet& packet, net_error_code& err);
};
