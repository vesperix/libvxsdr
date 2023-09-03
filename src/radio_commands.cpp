// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <complex>
#include <vector>

#include "logging.hpp"
#include "vxsdr.hpp"
#include "vxsdr_imp.hpp"

/*! @file radio_commands.cpp
    @brief Radio command functions for the @p vxsdr class.
*/

bool vxsdr::imp::tx_start(const vxsdr::time_point& t,
                                           const uint64_t n,
                                           const uint8_t subdev) {
    if (not vxsdr::imp::get_tx_enabled(subdev)) {
        LOG_ERROR("tx is not enabled in tx_start()");
        return false;
    }
    auto res = vxsdr::imp::get_tx_stream_state(subdev);
    if (not res) {
        LOG_ERROR("unable to get tx stream state in tx_start()");
        return false;
    }
    if (res.value() != STREAM_STOPPED) {
        LOG_ERROR("tx stream state is {:s} in tx_start()", vxsdr::imp::stream_state_to_string(res.value()));
        return false;
    }
    vxsdr::imp::data_tport->reset_tx_stream(n);
    cmd_or_rsp_packet_time_samples p{};
    p.hdr = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_START, FLAGS_TIME_PRESENT, subdev, 0, sizeof(p), 0};
    vxsdr::imp::time_point_to_time_spec_t(t, p.time);
    p.n_samples  = n;
    auto resp_ok = vxsdr::imp::send_packet_and_check_response(p, "tx_start()");
    if (resp_ok) {
        return true;
    }
    return false;
}

bool vxsdr::imp::rx_start(const vxsdr::time_point& t,
                                           const uint64_t n,
                                           const uint8_t subdev) {
    if (not vxsdr::imp::get_rx_enabled(subdev)) {
        LOG_ERROR("rx is not enabled in rx_start()");
        return false;
    }
    auto res = vxsdr::imp::get_rx_stream_state(subdev);
    if (not res) {
        LOG_ERROR("unable to get rx stream state in rx_start()");
        return false;
    }
    if (res.value() != STREAM_STOPPED) {
        LOG_ERROR("rx stream state is {:s} in rx_start()",
                              vxsdr::imp::stream_state_to_string(res.value()));
        return false;
    }
    vxsdr::imp::data_tport->reset_rx_stream(n);
    cmd_or_rsp_packet_time_samples p{};
    p.hdr = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_START, FLAGS_TIME_PRESENT, subdev, 0, sizeof(p), 0};
    vxsdr::imp::time_point_to_time_spec_t(t, p.time);
    p.n_samples  = n;
    auto resp_ok = vxsdr::imp::send_packet_and_check_response(p, "rx_start()");
    if (resp_ok) {
        return true;
    }
    return false;
}

bool vxsdr::imp::tx_loop(const vxsdr::time_point& t,
                                        const uint64_t n,
                                        const vxsdr::duration& t_delay,
                                        const uint32_t n_repeat,
                                        const uint8_t subdev)
{
    if (not vxsdr::imp::get_tx_enabled(subdev)) {
        LOG_ERROR("tx is not enabled in tx_loop()");
        return false;
    }
    auto res = vxsdr::imp::get_tx_stream_state(subdev);
    if (not res) {
        LOG_ERROR("unable to get tx stream state in tx_loop()");
        return false;
    }
    if (res.value() != STREAM_STOPPED) {
        LOG_ERROR("tx stream state is {:s} in tx_loop()",
                              vxsdr::imp::stream_state_to_string(res.value()));
        return false;
    }
    vxsdr::imp::data_tport->reset_tx_stream(0);
    loop_packet p{};
    p.hdr = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_LOOP, FLAGS_TIME_PRESENT, subdev, 0, sizeof(p), 0};
    vxsdr::imp::time_point_to_time_spec_t(t, p.time);
    p.n_samples = n;
    vxsdr::imp::duration_to_time_spec_t(t_delay, p.t_delay);
    p.n_repeat = n_repeat;
    auto resp_ok = vxsdr::imp::send_packet_and_check_response(p, "tx_loop()");
    if (resp_ok) {
        return true;
    }
    return false;
}

bool vxsdr::imp::rx_loop(const vxsdr::time_point& t,
                                        const uint64_t n,
                                        const vxsdr::duration& t_delay,
                                        const uint32_t n_repeat,
                                        const uint8_t subdev)
{
    if (not vxsdr::imp::get_rx_enabled(subdev)) {
        LOG_ERROR("rx is not enabled in rx_loop()");
        return false;
    }
    auto res = vxsdr::imp::get_rx_stream_state(subdev);
    if (not res) {
        LOG_ERROR("unable to get rx stream state in rx_loop()");
        return false;
    }
    if (res.value() != STREAM_STOPPED) {
        LOG_ERROR("rx stream state is {:s} in rx_loop()", vxsdr::imp::stream_state_to_string(res.value()));
        return false;
    }
    vxsdr::imp::data_tport->reset_rx_stream(0);
    loop_packet p{};
    p.hdr = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_LOOP, FLAGS_TIME_PRESENT, subdev, 0, sizeof(p), 0};
    vxsdr::imp::time_point_to_time_spec_t(t, p.time);
    p.n_samples = n;
    vxsdr::imp::duration_to_time_spec_t(t_delay, p.t_delay);
    p.n_repeat = n_repeat;
    auto resp_ok = vxsdr::imp::send_packet_and_check_response(p, "rx_loop()");
    if (resp_ok) {
        return true;
    }
    return false;
}


bool vxsdr::imp::tx_stop_now(const uint8_t subdev) {
    header_only_packet p;
    p.hdr        = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_STOP_NOW, 0, subdev, 0, sizeof(p), 0};
    auto resp_ok = vxsdr::imp::send_packet_and_check_response(p, "tx_stop_now()");
    if (resp_ok) {
        return true;
    }
    return false;
}

bool vxsdr::imp::rx_stop_now(const uint8_t subdev) {
    header_only_packet p;
    p.hdr        = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_STOP_NOW, 0, subdev, 0, sizeof(p), 0};
    auto resp_ok = vxsdr::imp::send_packet_and_check_response(p, "rx_stop_now()");
    if (resp_ok) {
        return true;
    }
    return false;
}

bool vxsdr::imp::set_tx_iq_bias(const std::array<double, 2> bias, const uint8_t subdev, const uint8_t channel) {
    two_double_packet p;
    p.hdr     = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_SET_IQ_BIAS, 0, subdev, channel, sizeof(p), 0};
    p.value1  = std::min(1.0, std::max(-1.0, bias.at(0)));
    p.value2  = std::min(1.0, std::max(-1.0, bias.at(1)));
    return vxsdr::imp::send_packet_and_check_response(p, "set_tx_iq_bias()");
}

bool vxsdr::imp::set_rx_iq_bias(const std::array<double, 2> bias, const uint8_t subdev, const uint8_t channel) {
    two_double_packet p;
    p.hdr     = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_SET_IQ_BIAS, 0, subdev, channel, sizeof(p), 0};
    p.value1  = std::min(1.0, std::max(-1.0, bias.at(0)));
    p.value2  = std::min(1.0, std::max(-1.0, bias.at(1)));
    return vxsdr::imp::send_packet_and_check_response(p, "set_rx_iq_bias()");
}

std::optional<std::array<double, 2>> vxsdr::imp::get_tx_iq_bias(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_IQ_BIAS, 0, subdev, channel, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_tx_iq_bias()");
    if (res) {
        auto q                    = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r                   = reinterpret_cast<two_double_packet*>(&q);
        std::array<double, 2> res = {r->value1, r->value2};
        return res;
    }
    return std::nullopt;
}

std::optional<std::array<double, 2>> vxsdr::imp::get_rx_iq_bias(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_IQ_BIAS, 0, subdev, channel, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_rx_iq_bias()");
    if (res) {
        auto q                    = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r                   = reinterpret_cast<two_double_packet*>(&q);
        std::array<double, 2> res = {r->value1, r->value2};
        return res;
    }
    return std::nullopt;
}

bool vxsdr::imp::set_tx_iq_corr(const std::array<double, 4> corr, const uint8_t subdev, const uint8_t channel) {
    four_double_packet p;
    p.hdr    = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_SET_IQ_CORR, 0, subdev, channel, sizeof(p), 0};
    p.value1 = corr.at(0);
    p.value2 = corr.at(1);
    p.value3 = corr.at(2);
    p.value4 = corr.at(3);
    return vxsdr::imp::send_packet_and_check_response(p, "set_tx_iq_corr()");
}

bool vxsdr::imp::set_rx_iq_corr(const std::array<double, 4> corr, const uint8_t subdev, const uint8_t channel) {
    four_double_packet p;
    p.hdr    = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_SET_IQ_CORR, 0, subdev, channel, sizeof(p), 0};
    p.value1  = corr.at(0);
    p.value2  = corr.at(1);
    p.value3  = corr.at(2);
    p.value4  = corr.at(3);
    return vxsdr::imp::send_packet_and_check_response(p, "set_rx_iq_corr()");
}

std::optional<std::array<double, 4>> vxsdr::imp::get_tx_iq_corr(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_IQ_CORR, 0, subdev, channel, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_tx_iq_corr()");
    if (res) {
        auto q                     = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r                    = reinterpret_cast<four_double_packet*>(&q);
        std::array<double, 4> corr = {r->value1, r->value2, r->value3, r->value4};
        return corr;
    }
    return std::nullopt;
}

std::optional<std::array<double, 4>> vxsdr::imp::get_rx_iq_corr(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_IQ_CORR, 0, subdev, channel, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_rx_iq_corr()");
    if (res) {
        auto q                     = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r                    = reinterpret_cast<four_double_packet*>(&q);
        std::array<double, 4> corr = {r->value1, r->value2, r->value3, r->value4};
        return corr;
    }
    return std::nullopt;
}

std::optional<std::array<double, 2>> vxsdr::imp::get_tx_freq_range(const uint8_t subdev) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_RF_FREQ_RANGE, 0, subdev, 0, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_tx_freq_range()");
    if (res) {
        auto q                    = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r                   = reinterpret_cast<two_double_packet*>(&q);
        std::array<double, 2> res = {r->value1, r->value2};
        return res;
    }
    return std::nullopt;
}

std::optional<std::array<double, 2>> vxsdr::imp::get_rx_freq_range(const uint8_t subdev) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_RF_FREQ_RANGE, 0, subdev, 0, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_rx_freq_range()");
    if (res) {
        auto q                    = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r                   = reinterpret_cast<two_double_packet*>(&q);
        std::array<double, 2> res = {r->value1, r->value2};
        return res;
    }
    return std::nullopt;
}

bool vxsdr::imp::set_tx_freq(const double freq_hz, const uint8_t subdev) {
    two_double_packet p;
    p.hdr    = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_SET_RF_FREQ, 0, subdev, 0, sizeof(p), 0};
    p.value1 = freq_hz;
    p.value2 = 1e-9;
    return vxsdr::imp::send_packet_and_check_response(p, "set_tx_freq()");
}

bool vxsdr::imp::set_rx_freq(const double freq_hz, const uint8_t subdev) {
    two_double_packet p;
    p.hdr    = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_SET_RF_FREQ, 0, subdev, 0, sizeof(p), 0};
    p.value1 = freq_hz;
    p.value2 = 1e-9;
    return vxsdr::imp::send_packet_and_check_response(p, "set_rx_freq()");
}

std::optional<double> vxsdr::imp::get_tx_freq(const uint8_t subdev) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_RF_FREQ, 0, subdev, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_tx_freq()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_double_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<double> vxsdr::imp::get_rx_freq(const uint8_t subdev) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_RF_FREQ, 0, subdev, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_rx_freq()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_double_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<std::array<double, 2>> vxsdr::imp::get_tx_gain_range(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_RF_GAIN_RANGE, 0, subdev, channel, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_tx_gain_range()");
    if (res) {
        auto q                    = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r                   = reinterpret_cast<two_double_packet*>(&q);
        std::array<double, 2> res = {r->value1, r->value2};
        return res;
    }
    return std::nullopt;
}

std::optional<std::array<double, 2>> vxsdr::imp::get_rx_gain_range(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_RF_GAIN_RANGE, 0, subdev, channel, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_rx_gain_range()");
    if (res) {
        auto q                    = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r                   = reinterpret_cast<two_double_packet*>(&q);
        std::array<double, 2> res = {r->value1, r->value2};
        return res;
    }
    return std::nullopt;
}

bool vxsdr::imp::set_tx_gain(const double gain_db, const uint8_t subdev, const uint8_t channel) {
    one_double_packet p;
    p.hdr    = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_SET_RF_GAIN, 0, subdev, channel, sizeof(p), 0};
    p.value1 = gain_db;
    return vxsdr::imp::send_packet_and_check_response(p, "set_tx_gain()");
}

bool vxsdr::imp::set_rx_gain(const double gain_db, const uint8_t subdev, const uint8_t channel) {
    one_double_packet p;
    p.hdr    = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_SET_RF_GAIN, 0, subdev, channel, sizeof(p), 0};
    p.value1 = gain_db;
    return vxsdr::imp::send_packet_and_check_response(p, "set_rx_gain()");
}

std::optional<double> vxsdr::imp::get_tx_gain(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_RF_GAIN, 0, subdev, channel, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_tx_gain()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_double_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<double> vxsdr::imp::get_rx_gain(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_RF_GAIN, 0, subdev, channel, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_rx_gain()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_double_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<std::array<double, 2>> vxsdr::imp::get_tx_rate_range(const uint8_t subdev) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_SAMPLE_RATE_RANGE, 0, subdev, 0, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_tx_rate_range()");
    if (res) {
        auto q                    = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r                   = reinterpret_cast<two_double_packet*>(&q);
        std::array<double, 2> res = {r->value1, r->value2};
        return res;
    }
    return std::nullopt;
}

std::optional<std::array<double, 2>> vxsdr::imp::get_rx_rate_range(const uint8_t subdev) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_SAMPLE_RATE_RANGE, 0, subdev, 0, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_rx_rate_range()");
    if (res) {
        auto q                    = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r                   = reinterpret_cast<two_double_packet*>(&q);
        std::array<double, 2> res = {r->value1, r->value2};
        return res;
    }
    return std::nullopt;
}

bool vxsdr::imp::set_tx_rate(const double rate_samples_sec, const uint8_t subdev) {
    one_double_packet p;
    p.hdr    = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_SET_SAMPLE_RATE, 0, subdev, 0, sizeof(p), 0};
    p.value1 = rate_samples_sec;
    return vxsdr::imp::send_packet_and_check_response(p, "set_tx_rate()");
}

bool vxsdr::imp::set_rx_rate(const double rate_samples_sec, const uint8_t subdev) {
    one_double_packet p;
    p.hdr    = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_SET_SAMPLE_RATE, 0, subdev, 0, sizeof(p), 0};
    p.value1 = rate_samples_sec;
    return vxsdr::imp::send_packet_and_check_response(p, "set_rx_rate()");
}

std::optional<double> vxsdr::imp::get_tx_rate(const uint8_t subdev) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_SAMPLE_RATE, 0, subdev, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_tx_rate()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_double_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<double> vxsdr::imp::get_rx_rate(const uint8_t subdev) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_SAMPLE_RATE, 0, subdev, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_rx_rate()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_double_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

bool vxsdr::imp::set_tx_filter_enabled(const bool enabled, const uint8_t subdev) {
    one_uint32_packet p;
    p.hdr = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_SET_FILTER_ENABLED, 0, subdev, 0, sizeof(p), 0};
    if (enabled) {
        p.value1 = 1;
    } else {
        p.value1 = 0;
    }
    return vxsdr::imp::send_packet_and_check_response(p, "set_tx_filter_enabled()");
}

bool vxsdr::imp::set_rx_filter_enabled(const bool enabled, const uint8_t subdev) {
    one_uint32_packet p;
    p.hdr = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_SET_FILTER_ENABLED, 0, subdev, 0, sizeof(p), 0};
    if (enabled) {
        p.value1 = 1;
    } else {
        p.value1 = 0;
    }
    return vxsdr::imp::send_packet_and_check_response(p, "set_rx_filter_enabled()");
}

bool vxsdr::imp::set_tx_filter_coeffs(const std::vector<std::complex<int16_t>>& coeffs, const uint8_t subdev, const uint8_t channel) {
    filter_coeff_packet p;
    p.hdr = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_SET_FILTER_COEFFS, 0, subdev, channel, sizeof(p), 0};
    if (coeffs.size() > MAX_FRONTEND_FILTER_LENGTH) {
        return false;
    }
    p.length = coeffs.size();
    for (unsigned i = 0; i < p.length; i++) {
        p.coeffs[i] = coeffs[i];
    }
    for (unsigned i = p.length; i < MAX_FRONTEND_FILTER_LENGTH; i++) {
        p.coeffs[i] = std::complex<int16_t>(0, 0);
    }
    return vxsdr::imp::send_packet_and_check_response(p, "set_tx_filter_coeffs()");
}

bool vxsdr::imp::set_rx_filter_coeffs(const std::vector<std::complex<int16_t>>& coeffs, const uint8_t subdev, const uint8_t channel) {
    filter_coeff_packet p;
    p.hdr = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_SET_FILTER_COEFFS, 0, subdev, channel, sizeof(p), 0};
    if (coeffs.size() > MAX_FRONTEND_FILTER_LENGTH) {
        return false;
    }
    p.length = coeffs.size();
    for (unsigned i = 0; i < p.length; i++) {
        p.coeffs[i] = coeffs[i];
    }
    for (unsigned i = p.length; i < MAX_FRONTEND_FILTER_LENGTH; i++) {
        p.coeffs[i] = std::complex<int16_t>(0, 0);
    }
    return vxsdr::imp::send_packet_and_check_response(p, "set_rx_filter_coeffs()");
}

std::optional<std::vector<std::complex<int16_t>>> vxsdr::imp::get_tx_filter_coeffs(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_FILTER_COEFFS, 0, subdev, channel, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_tx_filter_coeffs()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<filter_coeff_packet*>(&q);
        std::vector<std::complex<int16_t>> coeffs;
        for (unsigned i = 0; i < r->length; i++) {
            coeffs.push_back(r->coeffs[i]);
        }
        return coeffs;
    }
    return std::nullopt;
}

std::optional<std::vector<std::complex<int16_t>>> vxsdr::imp::get_rx_filter_coeffs(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_FILTER_COEFFS, 0, subdev, channel, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_rx_filter_coeffs()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<filter_coeff_packet*>(&q);
        std::vector<std::complex<int16_t>> coeffs;
        for (unsigned i = 0; i < r->length; i++) {
            coeffs.push_back(r->coeffs[i]);
        }
        return coeffs;
    }
    return std::nullopt;
}

std::optional<unsigned> vxsdr::imp::get_tx_filter_length(const uint8_t subdev) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_FILTER_LENGTH, 0, subdev, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_tx_filter_length()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<unsigned> vxsdr::imp::get_rx_filter_length(const uint8_t subdev) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_FILTER_LENGTH, 0, subdev, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_rx_filter_length()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<unsigned> vxsdr::imp::get_tx_num_subdevs() {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_NUM_SUBDEVS, 0, 0, 0, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_tx_num_subdevs()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<unsigned> vxsdr::imp::get_rx_num_subdevs() {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_NUM_SUBDEVS, 0, 0, 0, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_rx_num_ports()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

bool vxsdr::imp::get_tx_external_lo_enabled(const uint8_t subdev) {
    header_only_packet p = {};
    p.hdr = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_LO_INPUT, 0, subdev, 0, sizeof(p), 0};
    auto res    = vxsdr::imp::send_packet_and_return_response(p, "get_tx_external_lo_enabled()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1 > 0;
    }
    return false;
}

bool vxsdr::imp::get_rx_external_lo_enabled(const uint8_t subdev) {
    header_only_packet p = {};
    p.hdr = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_LO_INPUT, 0, subdev, 0, sizeof(p), 0};
    auto res    = vxsdr::imp::send_packet_and_return_response(p, "get_rx_external_lo_enabled()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1 > 0;
    }
    return false;
}

bool vxsdr::imp::set_tx_external_lo_enabled(const bool enabled, const uint8_t subdev) {
    one_uint32_packet p;
    p.hdr = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_SET_LO_INPUT, 0, subdev, 0, sizeof(p), 0};
    if (enabled) {
        p.value1 = 1;
    } else {
        p.value1 = 0;
    }
    return vxsdr::imp::send_packet_and_check_response(p, "set_tx_external_lo_enabled()");
}

bool vxsdr::imp::set_rx_external_lo_enabled(const bool enabled, const uint8_t subdev) {
    one_uint32_packet p;
    p.hdr = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_SET_LO_INPUT, 0, subdev, 0, sizeof(p), 0};
    if (enabled) {
        p.value1 = 1;
    } else {
        p.value1 = 0;
    }
    return vxsdr::imp::send_packet_and_check_response(p, "set_rx_external_lo_enabled()");
}

bool vxsdr::imp::get_tx_lo_locked(const uint8_t subdev) {
    header_only_packet p;
    p.hdr = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_LOCK_STATUS, 0, subdev, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_tx_lo_locked()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1 > 0;
    }
    return false;
}

bool vxsdr::imp::get_rx_lo_locked(const uint8_t subdev) {
    header_only_packet p;
    p.hdr = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_LOCK_STATUS, 0, subdev, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_rx_lo_locked()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1 > 0;
    }
    return false;
}

std::optional<unsigned> vxsdr::imp::get_tx_num_ports(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_NUM_RF_PORTS, 0, subdev, channel, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_tx_num_ports()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<unsigned> vxsdr::imp::get_rx_num_ports(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_NUM_RF_PORTS, 0, subdev, channel, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_rx_num_ports()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<unsigned> vxsdr::imp::get_tx_num_channels(const uint8_t subdev) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_NUM_CHANNELS, 0, subdev, 0, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_tx_num_channels()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<unsigned> vxsdr::imp::get_rx_num_channels(const uint8_t subdev) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_NUM_CHANNELS, 0, subdev, 0, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_rx_num_channels()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<std::string> vxsdr::imp::get_tx_port_name(const unsigned port_num, const uint8_t subdev, const uint8_t channel) {
    one_uint32_packet p = {};
    p.hdr               = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_RF_PORT_NAME, 0, subdev, channel, sizeof(p), 0};
    p.value1            = port_num;
    auto res            = vxsdr::imp::send_packet_and_return_response(p, "get_tx_port_name()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<name_packet*>(&q);
        return std::string((char*)r->name, std::min(strlen((char*)r->name), (size_t)(MAX_PAYLOAD_LENGTH_BYTES - 1)));
    }
    return std::nullopt;
}

std::optional<std::string> vxsdr::imp::get_rx_port_name(const unsigned port_num, const uint8_t subdev, const uint8_t channel) {
    one_uint32_packet p = {};
    p.hdr               = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_RF_PORT_NAME, 0, subdev, channel, sizeof(p), 0};
    p.value1            = port_num;
    auto res            = vxsdr::imp::send_packet_and_return_response(p, "get_rx_port_name()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<name_packet*>(&q);
        return std::string((char*)r->name, std::min(strlen((char*)r->name), (size_t)(MAX_PAYLOAD_LENGTH_BYTES - 1)));
    }
    return std::nullopt;
}

bool vxsdr::imp::set_tx_port(const unsigned port_num, const uint8_t subdev, const uint8_t channel) {
    one_uint32_packet p = {};
    p.hdr               = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_SET_RF_PORT, 0, subdev, channel, sizeof(p), 0};
    p.value1            = port_num;
    return vxsdr::imp::send_packet_and_check_response(p, "set_tx_port()");
}

bool vxsdr::imp::set_tx_port_by_name(const std::string& port_name, const uint8_t subdev, const uint8_t channel) {
    name_packet p = {};
    p.hdr         = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_SET_RF_PORT_BY_NAME, 0, subdev, channel, sizeof(p), 0};
    strncpy((char*)p.name, port_name.c_str(), MAX_PAYLOAD_LENGTH_BYTES - 1);
    p.name[MAX_PAYLOAD_LENGTH_BYTES - 1] = 0;
    return vxsdr::imp::send_packet_and_check_response(p, "set_tx_port_by_name()");
}

bool vxsdr::imp::set_rx_port(const unsigned port_num, const uint8_t subdev, const uint8_t channel) {
    one_uint32_packet p = {};
    p.hdr               = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_SET_RF_PORT, 0, subdev, channel, sizeof(p), 0};
    p.value1            = port_num;
    return vxsdr::imp::send_packet_and_check_response(p, "set_rx_port()");
}

bool vxsdr::imp::set_rx_port_by_name(const std::string& port_name, const uint8_t subdev, const uint8_t channel) {
    name_packet p = {};
    p.hdr         = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_SET_RF_PORT_BY_NAME, 0, subdev, channel, sizeof(p), 0};
    strncpy((char*)p.name, port_name.c_str(), MAX_PAYLOAD_LENGTH_BYTES - 1);
    p.name[MAX_PAYLOAD_LENGTH_BYTES - 1] = 0;
    return vxsdr::imp::send_packet_and_check_response(p, "set_rx_port_by_name()");
}

std::optional<unsigned> vxsdr::imp::get_tx_port(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_RF_PORT, 0, subdev, channel, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_tx_port()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::optional<unsigned> vxsdr::imp::get_rx_port(const uint8_t subdev, const uint8_t channel) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_RF_PORT, 0, subdev, channel, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_rx_port()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

bool vxsdr::imp::set_tx_enabled(const bool enabled, const uint8_t subdev) {
    one_uint32_packet p;
    p.hdr = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_SET_RF_ENABLED, 0, subdev, 0, sizeof(p), 0};
    if (enabled) {
        p.value1 = 1;
    } else {
        p.value1 = 0;
    }
    return vxsdr::imp::send_packet_and_check_response(p, "set_tx_enabled()");
}

bool vxsdr::imp::set_rx_enabled(const bool enabled, const uint8_t subdev) {
    one_uint32_packet p;
    p.hdr = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_SET_RF_ENABLED, 0, subdev, 0, sizeof(p), 0};
    if (enabled) {
        p.value1 = 1;
    } else {
        p.value1 = 0;
    }
    return vxsdr::imp::send_packet_and_check_response(p, "set_rx_enabled()");
}

bool vxsdr::imp::get_tx_enabled(const uint8_t subdev) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_TX_RADIO_CMD, RADIO_CMD_GET_RF_ENABLED, 0, subdev, 0, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_tx_enabled()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1 > 0;
    }
    return false;
}

bool vxsdr::imp::get_rx_enabled(const uint8_t subdev) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_RX_RADIO_CMD, RADIO_CMD_GET_RF_ENABLED, 0, subdev, 0, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_rx_enabled()");
    if (res) {
        auto q  = res.value();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* r = reinterpret_cast<one_uint32_packet*>(&q);
        return r->value1 > 0;
    }
    return false;
}