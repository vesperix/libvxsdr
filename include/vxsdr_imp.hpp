// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <complex>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>
#include <vector>
#include <span>
#include <ratio>
#include <string>
#include <array>
#include <chrono>
using namespace std::chrono_literals;

#include "vxsdr_packets.hpp"
#include "vxsdr_net.hpp"
#include "vxsdr_queues.hpp"
#include "vxsdr_transport.hpp"
#include "vxsdr_threads.hpp"

#include "vxsdr.hpp"

class vxsdr::imp {
  private:
    std::map<std::string, int64_t> default_settings = {
        {"command_transport",             vxsdr::TRANSPORT_TYPE_UDP},
        {"data_transport",                vxsdr::TRANSPORT_TYPE_UDP},
    };
    // waits between tries for data queue push/pop
    static constexpr unsigned tx_data_queue_wait_us   = 200;
    static constexpr unsigned rx_data_queue_wait_us   = 200;


    // timeout and wait between checks for responses from device
    unsigned device_response_timeout_us               = 1'000'000;
    static constexpr unsigned device_response_wait_us =     1'000;

    // timeout and wait between checks for transport to become ready
    static constexpr vxsdr::duration transport_ready_timeout = 1s;
    static constexpr vxsdr::duration transport_ready_wait    = 1ms;

    // how often to check the async queue
    static constexpr vxsdr::duration async_queue_wait        = 1ms;
    // thread to check the queue
    vxsdr_thread async_handler_thread;
    // flag for shutdown of async handler
    std::atomic<bool> async_handler_stop_flag = false;

    std::unique_ptr<command_transport> command_tport{};
    std::unique_ptr<data_transport>    data_tport{};

  public:
    explicit imp(const std::string& local_address,
          const std::string& device_address,
          const std::map<std::string, int64_t>& settings);

    ~imp() noexcept;
    imp(const imp&)            = delete;
    imp& operator=(const imp&) = delete;
    imp(imp&&)                 = delete;
    imp& operator=(imp&&)      = delete;

    uint32_t get_library_version();
    uint32_t get_library_packet_version();
    std::vector<std::string> get_library_details();
    template <typename T> size_t get_rx_data(std::vector<std::complex<T>>& data,
                       size_t n_requested,
                       const uint8_t subdev,
                       const double timeout_s);
    template <typename T> size_t put_tx_data(const std::vector<std::complex<T>>& data,
                       const uint8_t subdev,
                       const double timeout_s);
    std::optional<std::array<uint32_t, 6>> hello();
    bool reset();
    bool clear_status(const uint8_t subdev = 0);
    std::optional<std::array<uint32_t, 8>> get_status(const uint8_t subdev = 0);
    bool set_time_now(const vxsdr::time_point& t);
    bool set_time_next_pps(const vxsdr::time_point& t);
    std::optional<vxsdr::time_point> get_time_now();
    std::optional<vxsdr::stream_state> get_tx_stream_state(const uint8_t subdev = 0);
    std::optional<vxsdr::stream_state> get_rx_stream_state(const uint8_t subdev = 0);
    bool tx_start(const vxsdr::time_point& t,
                                        const uint64_t n,
                                        const uint8_t subdev = 0);
    bool rx_start(const vxsdr::time_point& t,
                                        const uint64_t n,
                                        const uint8_t subdev = 0);
    bool tx_loop(const vxsdr::time_point& t,
                                        const uint64_t n,
                                        const vxsdr::duration& t_delay = vxsdr::duration::zero(),
                                        const uint32_t n_repeat = 0,
                                        const uint8_t subdev = 0);
    bool rx_loop(const vxsdr::time_point& t,
                                        const uint64_t n,
                                        const vxsdr::duration& t_delay = vxsdr::duration::zero(),
                                        const uint32_t n_repeat = 0,
                                        const uint8_t subdev = 0);
    bool tx_stop_now(const uint8_t subdev = 0);
    bool rx_stop_now(const uint8_t subdev = 0);
    std::optional<std::array<double, 2>> get_tx_freq_range(const uint8_t subdev = 0);
    std::optional<std::array<double, 2>> get_rx_freq_range(const uint8_t subdev = 0);
    bool set_tx_freq(const double freq_hz, const uint8_t subdev = 0);
    bool set_rx_freq(const double freq_hz, const uint8_t subdev = 0);
    std::optional<double> get_tx_freq(const uint8_t subdev = 0);
    std::optional<double> get_rx_freq(const uint8_t subdev = 0);
    std::optional<std::array<double, 2>> get_tx_gain_range(const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<std::array<double, 2>> get_rx_gain_range(const uint8_t subdev = 0, const uint8_t channel = 0);
    bool set_tx_gain(const double gain_db, const uint8_t subdev = 0, const uint8_t channel = 0);
    bool set_rx_gain(const double gain_db, const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<double> get_tx_gain(const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<double> get_rx_gain(const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<std::array<double, 2>> get_tx_rate_range(const uint8_t subdev = 0);
    std::optional<std::array<double, 2>> get_rx_rate_range(const uint8_t subdev = 0);
    bool set_tx_rate(const double rate_samples_sec, const uint8_t subdev = 0);
    bool set_rx_rate(const double rate_samples_sec, const uint8_t subdev = 0);
    std::optional<double> get_tx_rate(const uint8_t subdev = 0);
    std::optional<double> get_rx_rate(const uint8_t subdev = 0);
    bool set_tx_filter_enabled(const bool enabled, const uint8_t subdev = 0);
    bool set_rx_filter_enabled(const bool enabled, const uint8_t subdev = 0);
    std::optional<std::vector<std::complex<int16_t>>> get_tx_filter_coeffs(const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<std::vector<std::complex<int16_t>>> get_rx_filter_coeffs(const uint8_t subdev = 0, const uint8_t channel = 0);
    bool set_tx_filter_coeffs(const std::vector<std::complex<int16_t>>& coeffs, const uint8_t subdev = 0, const uint8_t channel = 0);
    bool set_rx_filter_coeffs(const std::vector<std::complex<int16_t>>& coeffs, const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<unsigned> get_tx_filter_length(const uint8_t subdev = 0);
    std::optional<unsigned> get_rx_filter_length(const uint8_t subdev = 0);
    std::optional<unsigned> get_tx_num_subdevs();
    std::optional<unsigned> get_rx_num_subdevs();
    bool get_tx_external_lo_enabled(const uint8_t subdev = 0);
    bool get_rx_external_lo_enabled(const uint8_t subdev = 0);
    bool set_tx_external_lo_enabled(const bool enabled, const uint8_t subdev = 0);
    bool set_rx_external_lo_enabled(const bool enabled, const uint8_t subdev = 0);
    bool get_tx_lo_locked(const uint8_t subdev = 0);
    bool get_rx_lo_locked(const uint8_t subdev = 0);
    std::optional<unsigned> get_tx_num_ports(const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<unsigned> get_rx_num_ports(const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<unsigned> get_tx_num_channels(const uint8_t subdev = 0);
    std::optional<unsigned> get_rx_num_channels(const uint8_t subdev = 0);
    std::optional<std::string> get_tx_port_name(const unsigned port_num, const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<std::string> get_rx_port_name(const unsigned port_num, const uint8_t subdev = 0, const uint8_t channel = 0);
    bool set_tx_port(const unsigned port_num, const uint8_t subdev = 0, const uint8_t channel = 0);
    bool set_tx_port_by_name(const std::string& port_name, const uint8_t subdev = 0, const uint8_t channel = 0);
    bool set_rx_port(const unsigned port_num, const uint8_t subdev = 0, const uint8_t channel = 0);
    bool set_rx_port_by_name(const std::string& port_name, const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<unsigned> get_tx_port(const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<unsigned> get_rx_port(const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<unsigned> get_num_sensors(const uint8_t subdev);
    std::vector<std::string> get_sensor_names(const uint8_t subdev);
    std::optional<double> get_sensor_reading(const std::string& sensor_name, const uint8_t subdev);
    bool set_tx_enabled(const bool enabled, const uint8_t subdev = 0);
    bool set_rx_enabled(const bool enabled, const uint8_t subdev = 0);
    bool get_tx_enabled(const uint8_t subdev = 0);
    bool get_rx_enabled(const uint8_t subdev = 0);
    bool set_tx_iq_bias(const std::array<double, 2> bias, const uint8_t subdev = 0, const uint8_t channel = 0);
    bool set_rx_iq_bias(const std::array<double, 2> bias, const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<std::array<double, 2>> get_tx_iq_bias(const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<std::array<double, 2>> get_rx_iq_bias(const uint8_t subdev = 0, const uint8_t channel = 0);
    bool set_tx_iq_corr(const std::array<double, 4> corr, const uint8_t subdev = 0, const uint8_t channel = 0);
    bool set_rx_iq_corr(const std::array<double, 4> corr, const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<std::array<double, 4>> get_tx_iq_corr(const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<std::array<double, 4>> get_rx_iq_corr(const uint8_t subdev = 0, const uint8_t channel = 0);
    std::optional<std::array<uint32_t, 2>> get_buffer_info(const uint8_t subdev = 0);
    std::optional<std::array<uint32_t, 2>> get_buffer_use(const uint8_t subdev = 0);
    std::optional<std::array<bool, 3>> get_timing_status();
    std::optional<double> get_timing_resolution();
    bool set_host_command_timeout(const double timeout_s);
    [[nodiscard]] double get_host_command_timeout() const;
    std::vector<std::string> discover_ipv4_addresses(const std::string& local_addr,
                                                            const std::string& broadcast_addr,
                                                            const double timeout_s);
    bool set_ipv4_address(const std::string& device_address);
    bool save_ipv4_address(const std::string& device_address);
    std::optional<unsigned> get_max_payload_bytes();
    bool set_max_payload_bytes(const unsigned max_payload_bytes);
    // logging utilities
    std::string stream_state_to_string(const vxsdr::stream_state state) const;
    std::string error_to_string(const uint32_t reason) const;
    std::string version_number_to_string(const uint32_t version) const;
    std::string packet_type_to_name(const uint8_t number) const;
    std::string device_cmd_to_name(const uint8_t cmd) const;
    std::string cmd_error_to_name(const uint32_t reason) const;
    std::string radio_cmd_to_name(const uint8_t cmd) const;
    std::string async_msg_to_name(const uint8_t msg) const;
  private:
    bool send_packet_and_check_response(packet& p, const std::string& cmd_name = "unknown");
    std::optional<command_queue_element> send_packet_and_return_response(packet& p, const std::string& cmd_name = "unknown");
    [[nodiscard]] bool cmd_queue_push_check(packet& p, const std::string& cmd_name = "unknown");
    void async_handler();
    void time_point_to_time_spec_t(const vxsdr::time_point& t, time_spec_t& ts);
    void duration_to_time_spec_t(const vxsdr::duration& d, time_spec_t& ts);
    void simple_async_message_handler(const command_queue_element& a);
    size_t get_packet_header_size(const packet_header& hdr) const;
    std::span<std::complex<int16_t>> get_packet_data_span(packet& q) const;
    std::map<std::string, int64_t> apply_settings(const std::map<std::string, int64_t>& settings) const;
};
