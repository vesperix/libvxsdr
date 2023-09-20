// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vxsdr.hpp"
#include "vxsdr_imp.hpp"

/*! @file vxsdr.cpp
    @brief Constructor, destructor, and utility functions for the @p vxsdr class.
*/

vxsdr::vxsdr(const std::string& local_address,
             const std::string& device_address,
             const std::map<std::string, int64_t>& settings) {
    p_imp = std::make_unique<imp>(local_address,
                                  device_address,
                                  settings);
}

vxsdr::~vxsdr() noexcept = default;

uint32_t vxsdr::get_library_version() {
    return p_imp->get_library_version();
}

uint32_t vxsdr::get_library_packet_version() {
    return p_imp->get_library_packet_version();
}

std::vector<std::string> vxsdr::get_library_details() {
    return p_imp->get_library_details();
}

size_t vxsdr::get_rx_data(std::vector<std::complex<int16_t>>& data, const size_t n_requested, const uint8_t subdev,
    const double timeout_s) {
    return p_imp->get_rx_data<int16_t>(data, n_requested, subdev, timeout_s);
}

size_t vxsdr::get_rx_data(std::vector<std::complex<float>>& data, const size_t n_requested, const uint8_t subdev,
    const double timeout_s) {
    return p_imp->get_rx_data<float>(data, n_requested, subdev, timeout_s);
}

size_t vxsdr::put_tx_data(const std::vector<std::complex<int16_t>> &data, const size_t n_requested, const uint8_t subdev, const double timeout_s) {
    return p_imp->put_tx_data<int16_t>(data, n_requested, subdev, timeout_s);
}

size_t vxsdr::put_tx_data(const std::vector<std::complex<float>> &data, const size_t n_requested, const uint8_t subdev, const double timeout_s) {
    return p_imp->put_tx_data<float>(data, n_requested, subdev, timeout_s);
}

bool vxsdr::set_host_command_timeout(const double timeout_s) {
    return p_imp->set_host_command_timeout(timeout_s);
}

std::optional<std::array<uint32_t, 6>> vxsdr::hello() {
    return p_imp->hello();
}

bool vxsdr::reset() {
    return p_imp->reset();
}

bool vxsdr::clear_status(const uint8_t subdev) {
    return p_imp->clear_status(subdev);
}

std::optional<std::array<uint32_t, 8>> vxsdr::get_status(const uint8_t subdev) {
    return p_imp->get_status(subdev);
}

bool vxsdr::set_time_now(const vxsdr::time_point& t) {
    return p_imp->set_time_now(t);
}

bool vxsdr::set_time_next_pps(const vxsdr::time_point& t) {
    return p_imp->set_time_next_pps(t);
}

std::optional<vxsdr::time_point> vxsdr::get_time_now() {
    return p_imp->get_time_now();
}

std::optional<std::array<uint32_t, 2>> vxsdr::get_buffer_info(const uint8_t subdev) {
    return p_imp->get_buffer_info(subdev);
}

std::optional<std::array<uint32_t, 2>> vxsdr::get_buffer_use(const uint8_t subdev) {
    return p_imp->get_buffer_use(subdev);
}

std::optional<std::array<bool, 3>> vxsdr::get_timing_status() {
    return p_imp->get_timing_status();
}

std::optional<double> vxsdr::get_timing_resolution() {
    return p_imp->get_timing_resolution();
}

double vxsdr::get_host_command_timeout() const {
    return p_imp->get_host_command_timeout();
}

std::vector<std::string> vxsdr::discover_ipv4_addresses(const std::string& local_addr,
                                                               const std::string& broadcast_addr,
                                                               const double timeout_s) {
    return p_imp->discover_ipv4_addresses(local_addr, broadcast_addr, timeout_s);
}

bool vxsdr::set_ipv4_address(const std::string& device_address) {
    return p_imp->set_ipv4_address(device_address);
}

bool vxsdr::save_ipv4_address(const std::string& device_address) {
    return p_imp->save_ipv4_address(device_address);
}

std::optional<unsigned> vxsdr::get_max_payload_bytes() {
    return p_imp->get_max_payload_bytes();
}

bool vxsdr::set_max_payload_bytes(const unsigned max_payload_bytes) {
    return p_imp->set_max_payload_bytes(max_payload_bytes);
}

std::optional<vxsdr::stream_state> vxsdr::get_tx_stream_state(const uint8_t subdev) {
    return p_imp->get_tx_stream_state(subdev);
}

std::optional<vxsdr::stream_state> vxsdr::get_rx_stream_state(const uint8_t subdev) {
    return p_imp->get_rx_stream_state(subdev);
}

bool vxsdr::tx_start(const vxsdr::time_point& t, const uint64_t n, const uint8_t subdev) {
    return p_imp->tx_start(t, n, subdev);
}

bool vxsdr::rx_start(const vxsdr::time_point& t, const uint64_t n, const uint8_t subdev) {
    return p_imp->rx_start(t, n, subdev);
}

bool vxsdr::tx_loop(const vxsdr::time_point& t, const uint64_t n, const vxsdr::duration& t_delay,
                                        const uint32_t n_repeat, const uint8_t subdev) {
    return p_imp->tx_loop(t, n, t_delay, n_repeat, subdev);
}

bool vxsdr::rx_loop(const vxsdr::time_point& t, const uint64_t n, const vxsdr::duration& t_delay,
                                        const uint32_t n_repeat, const uint8_t subdev) {
    return p_imp->rx_loop(t, n, t_delay, n_repeat, subdev);
}

bool vxsdr::tx_stop_now(const uint8_t subdev) {
    return p_imp->tx_stop_now(subdev);
}

bool vxsdr::rx_stop_now(const uint8_t subdev) {
    return p_imp->rx_stop_now(subdev);
}

bool vxsdr::set_tx_iq_bias(const std::array<double, 2> bias, const uint8_t subdev, const uint8_t channel) {
    return p_imp->set_tx_iq_bias(bias, subdev, channel);
}

bool vxsdr::set_rx_iq_bias(const std::array<double, 2> bias, const uint8_t subdev, const uint8_t channel) {
    return p_imp->set_rx_iq_bias(bias, subdev, channel);
}

std::optional<std::array<double, 2>> vxsdr::get_tx_iq_bias(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_tx_iq_bias(subdev, channel);
}

std::optional<std::array<double, 2>> vxsdr::get_rx_iq_bias(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_rx_iq_bias(subdev, channel);
}

bool vxsdr::set_tx_iq_corr(const std::array<double, 4> corr, const uint8_t subdev, const uint8_t channel) {
    return p_imp->set_tx_iq_corr(corr, subdev, channel);
}

bool vxsdr::set_rx_iq_corr(const std::array<double, 4> corr, const uint8_t subdev, const uint8_t channel) {
    return p_imp->set_rx_iq_corr(corr, subdev, channel);
}

std::optional<std::array<double, 4>> vxsdr::get_tx_iq_corr(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_tx_iq_corr(subdev, channel);
}

std::optional<std::array<double, 4>> vxsdr::get_rx_iq_corr(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_rx_iq_corr(subdev, channel);
}

std::optional<std::array<double, 2>> vxsdr::get_tx_freq_range(const uint8_t subdev) {
    return p_imp->get_tx_freq_range(subdev);
}

std::optional<std::array<double, 2>> vxsdr::get_rx_freq_range(const uint8_t subdev) {
    return p_imp->get_rx_freq_range(subdev);
}

bool vxsdr::set_tx_freq(const double freq_hz, const uint8_t subdev) {
    return p_imp->set_tx_freq(freq_hz, subdev);
}

bool vxsdr::set_rx_freq(const double freq_hz, const uint8_t subdev) {
    return p_imp->set_rx_freq(freq_hz, subdev);
}

std::optional<double> vxsdr::get_tx_freq(const uint8_t subdev) {
    return p_imp->get_tx_freq(subdev);
}

std::optional<double> vxsdr::get_rx_freq(const uint8_t subdev) {
    return p_imp->get_rx_freq(subdev);
}

std::optional<std::array<double, 2>> vxsdr::get_tx_gain_range(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_tx_gain_range(subdev, channel);
}

std::optional<std::array<double, 2>> vxsdr::get_rx_gain_range(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_rx_gain_range(subdev, channel);
}

bool vxsdr::set_tx_gain(const double gain_db, const uint8_t subdev, const uint8_t channel) {
    return p_imp->set_tx_gain(gain_db, subdev, channel);
}

bool vxsdr::set_rx_gain(const double gain_db, const uint8_t subdev, const uint8_t channel) {
    return p_imp->set_rx_gain(gain_db, subdev, channel);
}

std::optional<double> vxsdr::get_tx_gain(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_tx_gain(subdev, channel);
}

std::optional<double> vxsdr::get_rx_gain(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_rx_gain(subdev, channel);
}

std::optional<std::array<double, 2>> vxsdr::get_tx_rate_range(const uint8_t subdev) {
    return p_imp->get_tx_rate_range(subdev);
}

std::optional<std::array<double, 2>> vxsdr::get_rx_rate_range(const uint8_t subdev) {
    return p_imp->get_rx_rate_range(subdev);
}

bool vxsdr::set_tx_rate(const double rate_samples_sec, const uint8_t subdev) {
    return p_imp->set_tx_rate(rate_samples_sec, subdev);
}

bool vxsdr::set_rx_rate(const double rate_samples_sec, const uint8_t subdev) {
    return p_imp->set_rx_rate(rate_samples_sec, subdev);
}

std::optional<double> vxsdr::get_tx_rate(const uint8_t subdev) {
    return p_imp->get_tx_rate(subdev);
}

std::optional<double> vxsdr::get_rx_rate(const uint8_t subdev) {
    return p_imp->get_rx_rate(subdev);
}

bool vxsdr::set_tx_filter_enabled(const bool enabled, const uint8_t subdev) {
    return p_imp->set_tx_filter_enabled(enabled, subdev);
}

bool vxsdr::set_rx_filter_enabled(const bool enabled, const uint8_t subdev) {
    return p_imp->set_rx_filter_enabled(enabled, subdev);
}

std::optional<std::vector<std::complex<int16_t>>> vxsdr::get_tx_filter_coeffs(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_tx_filter_coeffs(subdev, channel);
}

std::optional<std::vector<std::complex<int16_t>>> vxsdr::get_rx_filter_coeffs(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_rx_filter_coeffs(subdev, channel);
}

bool vxsdr::set_tx_filter_coeffs(const std::vector<std::complex<int16_t>>& coeffs, const uint8_t subdev, const uint8_t channel) {
    return p_imp->set_tx_filter_coeffs(coeffs, subdev, channel);
}

bool vxsdr::set_rx_filter_coeffs(const std::vector<std::complex<int16_t>>& coeffs, const uint8_t subdev, const uint8_t channel) {
    return p_imp->set_rx_filter_coeffs(coeffs, subdev, channel);
}

std::optional<unsigned> vxsdr::get_tx_filter_length(const uint8_t subdev) {
    return p_imp->get_tx_filter_length(subdev);
}

std::optional<unsigned> vxsdr::get_rx_filter_length(const uint8_t subdev) {
    return p_imp->get_rx_filter_length(subdev);
}

std::optional<unsigned> vxsdr::get_tx_num_subdevs() {
    return p_imp->get_tx_num_subdevs();
}

std::optional<unsigned> vxsdr::get_rx_num_subdevs() {
    return p_imp->get_rx_num_subdevs();
}

bool vxsdr::get_tx_external_lo_enabled(const uint8_t subdev) {
    return p_imp->get_tx_external_lo_enabled(subdev);
}

bool vxsdr::get_rx_external_lo_enabled(const uint8_t subdev) {
    return p_imp->get_rx_external_lo_enabled(subdev);
}

bool vxsdr::set_tx_external_lo_enabled(const bool enabled, const uint8_t subdev) {
    return p_imp->set_tx_external_lo_enabled(enabled, subdev);
}

bool vxsdr::set_rx_external_lo_enabled(const bool enabled, const uint8_t subdev) {
    return p_imp->set_rx_external_lo_enabled(enabled, subdev);
}

bool vxsdr::get_tx_lo_locked(const uint8_t subdev) {
    return p_imp->get_tx_lo_locked(subdev);
}

bool vxsdr::get_rx_lo_locked(const uint8_t subdev) {
    return p_imp->get_rx_lo_locked(subdev);
}

std::optional<unsigned> vxsdr::get_tx_num_channels(const uint8_t subdev) {
    return p_imp->get_tx_num_channels(subdev);
}

std::optional<unsigned> vxsdr::get_rx_num_channels(const uint8_t subdev) {
    return p_imp->get_rx_num_channels(subdev);
}

std::optional<unsigned> vxsdr::get_tx_num_ports(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_tx_num_ports(subdev, channel);
}

std::optional<unsigned> vxsdr::get_rx_num_ports(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_rx_num_ports(subdev, channel);
}

std::optional<std::string> vxsdr::get_tx_port_name(const unsigned port_num, const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_tx_port_name(port_num, subdev, channel);
}

std::optional<std::string> vxsdr::get_rx_port_name(const unsigned port_num, const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_rx_port_name(port_num, subdev, channel);
}

bool vxsdr::set_tx_port(const unsigned port_num, const uint8_t subdev, const uint8_t channel) {
    return p_imp->set_tx_port(port_num, subdev, channel);
}

bool vxsdr::set_tx_port_by_name(const std::string& port_name, const uint8_t subdev, const uint8_t channel) {
    return p_imp->set_tx_port_by_name(port_name, subdev, channel);
}

bool vxsdr::set_rx_port(const unsigned port_num, const uint8_t subdev, const uint8_t channel) {
    return p_imp->set_rx_port(port_num, subdev, channel);
}

bool vxsdr::set_rx_port_by_name(const std::string& port_name, const uint8_t subdev, const uint8_t channel) {
    return p_imp->set_rx_port_by_name(port_name, subdev, channel);
}

std::optional<unsigned> vxsdr::get_tx_port(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_tx_port(subdev, channel);
}

std::optional<unsigned> vxsdr::get_rx_port(const uint8_t subdev, const uint8_t channel) {
    return p_imp->get_rx_port(subdev, channel);
}

std::optional<unsigned> vxsdr::get_num_sensors(const uint8_t subdev) {
    return p_imp->get_num_sensors(subdev);
}

std::vector<std::string> vxsdr::get_sensor_names(const uint8_t subdev) {
    return p_imp->get_sensor_names(subdev);
}

std::optional<double> vxsdr::get_sensor_reading(const std::string& sensor_name, const uint8_t subdev) {
    return p_imp->get_sensor_reading(sensor_name, subdev);
}

bool vxsdr::set_tx_enabled(const bool enabled, const uint8_t subdev) {
    return p_imp->set_tx_enabled(enabled, subdev);
}

bool vxsdr::set_rx_enabled(const bool enabled, const uint8_t subdev) {
    return p_imp->set_rx_enabled(enabled, subdev);
}

bool vxsdr::get_tx_enabled(const uint8_t subdev) {
    return p_imp->get_tx_enabled(subdev);
}

bool vxsdr::get_rx_enabled(const uint8_t subdev) {
    return p_imp->get_rx_enabled(subdev);
}