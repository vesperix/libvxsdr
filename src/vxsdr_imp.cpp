// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <ctime>
#include <algorithm>
#include <iomanip>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <memory>

#include "logging.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_transport.hpp"
#include "vxsdr_threads.hpp"
#include "vxsdr_imp.hpp"


/*! @file vxsdr_imp.cpp
    @brief Constructor, destructor, and utility functions for the @p vxsdr_imp class.
*/

vxsdr::imp::imp(const std::map<std::string, int64_t>& input_config) {
    LOG_INIT();
    LOG_DEBUG("vxsdr constructor entered");

    auto det = get_library_details();
    LOG_INFO("library info:");
    for (auto &str : det) {
        LOG_INFO("    {:s}", str);
    }

    auto config = vxsdr::imp::apply_config(input_config);

    if (config["data_transport"] != vxsdr::TRANSPORT_TYPE_UDP or config["command_transport"] != vxsdr::TRANSPORT_TYPE_UDP) {
        LOG_ERROR("the transport specified is not supported");
        throw std::runtime_error("the transport specified is not supported in vxsdr constructor");
    }

    // Make the transport objects here
    command_tport = std::make_unique<udp_command_transport>(config);

    auto start_time = std::chrono::steady_clock::now();
    while (command_tport->tx_state != packet_transport::TRANSPORT_READY and
           command_tport->rx_state != packet_transport::TRANSPORT_READY) {
        std::this_thread::sleep_for(transport_ready_wait);
        if ((std::chrono::steady_clock::now() - start_time) > transport_ready_timeout) {
            LOG_ERROR("timeout waiting for command transport in vxsdr constructor");
            throw std::runtime_error("timeout waiting for command transport in vxsdr constructor");
        }
    }

    async_handler_thread        = vxsdr_thread([this] { vxsdr::imp::async_handler(); });

    auto res = vxsdr::imp::hello();
    if (not res) {
        LOG_ERROR("device did not respond to hello command");
        throw std::runtime_error("device did not respond to hello command in vxsdr constructor");
    }
    LOG_INFO("device info:");
    LOG_INFO("   device ID: {:d}", res->at(0));
    LOG_INFO("   device FPGA code version: {:s}", vxsdr::imp::version_number_to_string(res->at(1)));
    LOG_INFO("   device MCU code version: {:s}", vxsdr::imp::version_number_to_string(res->at(2)));
    LOG_INFO("   device serial number: {:d}", res->at(3));
    LOG_INFO("   device supported packet version: {:s}", vxsdr::imp::version_number_to_string(res->at(4)));
    // check that library and device support the same packet version
    if (get_library_packet_version() != res->at(4)) {
        LOG_WARN("library packet version is {:s}, device packet version is {:s}",
                vxsdr::imp::version_number_to_string(get_library_packet_version()), vxsdr::imp::version_number_to_string(res->at(4)));
    }
    LOG_INFO("   sample format: 0x{:x}", res->at(5));
    LOG_INFO("   number of subdevices: {:d}", res->at(6));
    LOG_INFO("   maximum data payload bytes: {:d}", res->at(7));

    // check that the library's wire sample type and the devices are the same
    if (std::is_same<vxsdr::wire_sample, std::complex<int16_t>>::value) {
        if ((res->at(5) & SAMPLE_DATATYPE_MASK) != SAMPLE_TYPE_COMPLEX_I16) {
            LOG_ERROR("library and device wire sample formats incompatible (0x{:x})", (res->at(5) & SAMPLE_DATATYPE_MASK));
            throw std::runtime_error("library and device wire sample formats incompatible");
        }
    } else {
        LOG_WARN("library wire sample type is not std::complex<int16_t>> -- untested");
    }

    // data transport constructor needs to know the sample granularity, number of subdevices, and  maximum samples_per_packet
    unsigned sample_granularity = (res->at(5) & SAMPLE_GRANULARITY_MASK) >> SAMPLE_GRANULARITY_SHIFT;
    unsigned num_subdevs = res->at(6);
    unsigned max_samps_per_packet = sample_granularity * (max_samples_per_packet<vxsdr::wire_sample>(res->at(7)) / sample_granularity);

    if (not vxsdr::imp::tx_stop() or not vxsdr::imp::rx_stop()) {
        LOG_ERROR("error stopping tx and rx");
        throw std::runtime_error("error stopping tx and rx in vxsdr constructor");
    }
    if (not vxsdr::imp::clear_status()) {
        LOG_ERROR("error clearing status");
        throw std::runtime_error("error clearing status in vxsdr constructor");
    }

    data_tport = std::make_unique<udp_data_transport>(config, sample_granularity, num_subdevs, max_samps_per_packet);

    start_time = std::chrono::steady_clock::now();
    while (data_tport->tx_state != packet_transport::TRANSPORT_READY and
           data_tport->rx_state != packet_transport::TRANSPORT_READY) {
        std::this_thread::sleep_for(transport_ready_wait);
        if ((std::chrono::steady_clock::now() - start_time) > transport_ready_timeout) {
            LOG_ERROR("timeout waiting for data transport in vxsdr constructor");
            throw std::runtime_error("timeout waiting for data transport in vxsdr constructor");
        }
    }

    // enable the TX
    if (not vxsdr::imp::set_tx_enabled(true)) {
        LOG_ERROR("error enabling tx");
        throw std::runtime_error("error enabling tx in vxsdr constructor");
    }
    start_time = std::chrono::steady_clock::now();
    while (not vxsdr::imp::get_tx_enabled()) {
        std::this_thread::sleep_for(rf_ready_wait);
        if ((std::chrono::steady_clock::now() - start_time) > rf_ready_timeout) {
            LOG_ERROR("timeout waiting for tx enabled in vxsdr constructor");
            throw std::runtime_error("timeout waiting for tx enabled in vxsdr constructor");
        }
    }

    // enable the RX
    if (not vxsdr::imp::set_rx_enabled(true)) {
        LOG_ERROR("error enabling rx");
        throw std::runtime_error("error enabling rx in vxsdr constructor");
    }
    start_time = std::chrono::steady_clock::now();
    while (not vxsdr::imp::get_rx_enabled()) {
        std::this_thread::sleep_for(rf_ready_wait);
        if ((std::chrono::steady_clock::now() - start_time) > rf_ready_timeout) {
            LOG_ERROR("timeout waiting for rx enabled in vxsdr constructor");
            throw std::runtime_error("timeout waiting for rx enabled in vxsdr constructor");
        }
    }
    LOG_DEBUG("vxsdr constructor complete");
}

vxsdr::imp::~imp() noexcept {
    LOG_DEBUG("vxsdr destructor entered");
    vxsdr::imp::tx_stop();
    vxsdr::imp::rx_stop();
    vxsdr::imp::set_tx_enabled(false);
    vxsdr::imp::set_rx_enabled(false);
    async_handler_stop_flag = true;
    LOG_DEBUG("joining async message handler thread");
    if (async_handler_thread.joinable()) {
        async_handler_thread.join();
    }
    // transports use log in destructors;
    // make sure that's done before log
    // shutdown
    LOG_DEBUG("resetting command transport");
    command_tport.reset();
    LOG_DEBUG("resetting data transport");
    data_tport.reset();
    LOG_DEBUG("vxsdr destructor complete");
    LOG_SHUTDOWN();
}

uint32_t vxsdr::imp::get_library_version() {
    return 10000 * (VERSION_MAJOR) + 100 * (VERSION_MINOR) + (VERSION_PATCH);
}

uint32_t vxsdr::imp::get_library_packet_version() {
    return 10000 * (PACKET_VERSION_MAJOR) + 100 * (PACKET_VERSION_MINOR) + (PACKET_VERSION_PATCH);
}

std::vector<std::string> vxsdr::imp::get_library_details() {
    std::vector<std::string> ret;
    ret.push_back("version: " + std::string(VERSION_STRING));
    ret.push_back("packet_version: " + std::string(PACKET_VERSION_STRING));
    ret.push_back("build_type: " + std::string(BUILD_TYPE));
    ret.push_back("compiler_info: " + std::string(COMPILER_INFO));
    ret.push_back("system_info: " + std::string(SYSTEM_INFO));
    return ret;
}

template <typename T> size_t vxsdr::imp::get_rx_data(std::vector<std::complex<T>>& data, size_t n_requested, const uint8_t subdev, const double timeout_s) {
    LOG_DEBUG("get_rx_data from subdevice {:d} entered", subdev);

    if(subdev >= data_tport->rx_data_queue.size()) {
        LOG_ERROR("incorrect subdevice {:d} in get_rx_data()", subdev);
        return 0;
    }

    if (timeout_s <= 0.0) {
        LOG_ERROR("timeout_s must be positive in get_rx_data()");
        return 0;
    }
    if (timeout_s > 3600.0) {
        LOG_ERROR("timeout_s must 3600 or less in get_rx_data()");
        return 0;
    }
    const unsigned timeout_duration_us = std::llround(timeout_s * 1e6);

    if (not data_tport->rx_usable()) {
        LOG_ERROR("data transport rx is not usable in get_rx_data()");
        return 0;
    }

    if (n_requested == 0) {
        if (data.size() == 0) {
            LOG_WARN("get_rx_data() called with n_requested and data.size() both zero");
            return 0;
        } else {
            n_requested = data.size();
        }
    } else {
        if (data.size() < n_requested) {
            LOG_WARN("data.size() = {:d} but n_requested = {:d}; resizing data in get_rx_data()", data.size(), n_requested);
            data.resize(n_requested);
        }
    }

    data.reserve(n_requested);

    LOG_DEBUG("receiving {:d} samples from subdevice {:d}", n_requested, subdev);

    size_t n_received = 0;

    // first get any samples in the queue from previous packets

    size_t saved_samples = data_tport->rx_sample_queue[subdev]->read_available();

    if (saved_samples > 0) {
        size_t n_to_copy = std::min(n_requested, saved_samples);
        std::vector<vxsdr::wire_sample> saved_data(n_to_copy);
        int64_t n_saved = data_tport->rx_sample_queue[subdev]->pop(saved_data.data(), n_to_copy);
        if constexpr(std::is_same<T, int16_t>()) {
            for(int64_t i = 0; i < n_saved; i++) {
                data[n_received + i] = saved_data[i];
            }
        } else if constexpr(std::is_floating_point<T>()) {
            constexpr T scale = 1.0 / 32'768.0;
            for(int64_t i = 0; i < n_saved; i++) {
                data[n_received + i] = std::complex<T>(scale * (T)saved_data[i].real(), scale * (T)saved_data[i].imag());
            }
        }
        n_received += n_saved;
    }

    // now get samples from new packets
    while (n_received < n_requested) {
        int64_t n_remaining = (int64_t)n_requested - (int64_t)n_received;
        // setting rx_data_queue_wait_us = 0 results in a busy wait
        data_queue_element q;
        if (data_tport->rx_data_queue[subdev]->pop_or_timeout(q, timeout_duration_us, rx_data_queue_wait_us)) {
            auto packet_data = vxsdr::imp::get_packet_data_span<vxsdr::wire_sample>(q);
            int64_t data_samples = packet_data.size();

            if (data_samples > 0) {
                int64_t n_to_copy = std::min(n_remaining, data_samples);
                if constexpr(std::is_same<T, int16_t>()) {
                    for(int64_t i = 0; i < n_to_copy; i++) {
                        data[n_received + i] = packet_data[i];
                    }
                } else if constexpr(std::is_floating_point<T>()) {
                    constexpr T scale = 1.0 / 32'768.0;
                    for(int64_t i = 0; i < n_to_copy; i++) {
                        data[n_received + i] = std::complex<T>(scale * (T)packet_data[i].real(), scale * (T)packet_data[i].imag());
                    }
                }
                n_received += n_to_copy;
            }
            // if there are leftover samples, push them to the sample queue
            if (data_samples > n_remaining) {
                int64_t n_pushed = data_tport->rx_sample_queue[subdev]->push(&packet_data[n_remaining], data_samples - n_remaining);
                if (n_pushed != data_samples - n_remaining) {
                    LOG_ERROR("error pushing data to rx sample queue for subdevice {:d} ({:d} of {:d} samples)", subdev, n_pushed, data_samples - n_remaining);
                    return n_received;
                }
            }
        } else {
            LOG_ERROR("timeout popping from rx data queue for subdevice {:d} ({:d} of {:d} samples)", subdev, n_received, n_requested);
            return n_received;
        }
    }
    LOG_DEBUG("get_rx_data complete from subdevice {:d} ({:d} samples)", subdev, n_received);
    return n_received;
}

// Need to explicitly instantiate template classes for all allowed types so compiler will include code in library
template size_t vxsdr::imp::get_rx_data(std::vector<std::complex<int16_t>>& data, size_t n_requested, const uint8_t subdev, const double timeout_s);
template size_t vxsdr::imp::get_rx_data(std::vector<std::complex<float>>& data, size_t n_requested, const uint8_t subdev, const double timeout_s);

template <typename T> size_t vxsdr::imp::put_tx_data(const std::vector<std::complex<T>>& data, size_t n_requested, const uint8_t subdev, const double timeout_s) {
    LOG_DEBUG("put_tx_data started");

    if (timeout_s <= 0.0) {
        LOG_ERROR("timeout_s must be positive in put_tx_data()");
        return 0;
    }
    if (timeout_s > 3600.0) {
        LOG_ERROR("timeout_s must 3600 or less in put_tx_data()");
        return 0;
    }
    const unsigned timeout_duration_us = std::llround(timeout_s * 1e6);

    if (not data_tport->tx_rx_usable()) {
        // need both available since acks must be received
        LOG_ERROR("data transport tx and rx are not both usable in put_tx_data()");
        return 0;
    }

    if (n_requested == 0) {
        if (data.size() == 0) {
            LOG_WARN("put_tx_data() called with n_requested and data.size() both zero");
            return 0;
        } else {
            n_requested = data.size();
        }
    } else {
        if (data.size() < n_requested) {
            LOG_WARN("data.size() = {:d} but n_requested = {:d}; reducing n_requested in put_tx_data()", data.size(), n_requested);
            n_requested = data.size();
        }
    }

    LOG_DEBUG("sending {:d} samples to subdevice {:d}", n_requested, subdev);

    // puts plain data_packets (no time, no stream)
    size_t n_put = 0;
    size_t n_packet_max = data_tport->get_max_samples_per_packet();
    for (size_t i = 0; i < n_requested; i += n_packet_max) {
        data_queue_element q;
        auto* p               = std::bit_cast<data_packet*>(&q);
        auto n_samples        = (unsigned)std::min(n_packet_max, n_requested - i);
        unsigned n_data_bytes = n_samples * sizeof(vxsdr::wire_sample);
        auto packet_size      = (uint16_t)(sizeof(packet_header) + n_data_bytes);
        p->hdr                = {PACKET_TYPE_TX_SIGNAL_DATA, 0, 0, subdev, 0, packet_size, 0};
        if constexpr(std::is_same<T, int16_t>()) {
            // data is in native format -- just copy
            std::copy_n(data.begin() + (int64_t)i, (int64_t)n_samples, std::begin(p->data));
        } else if constexpr(std::is_floating_point<T>()) {
            // must convert data from float and scale it
            constexpr T scale = 32'767.0;
            for(size_t j = 0; j < n_samples; j++) {
#ifndef VXSDR_LIB_TRUNCATE_FLOAT_CONVERSION
#if defined(VXSDR_COMPILER_CLANG) || defined(VXSDR_COMPILER_APPLECLANG) || defined(VXSDR_COMPILER_GCC)
                // for float32 data (the only floating-point data currently supported),
                // the implementation below is nearly as fast as truncating with clang++ or g++ with -fno-trapping-math,
                // (which should be set by CMake when gcc is used)
                // It rounds properly and quickly; however, it is untested with other compilers
                T re = scale * data[i + j].real();
                if (re > (T)0.0) {
                    re += (T)0.5;
                } else {
                    re -= (T)0.5;
                }
                T im = scale * data[i + j].imag();
                if (im > (T)0.0) {
                    im += (T)0.5;
                } else {
                    im -= (T)0.5;
                }
                p->data[j] = std::complex<int16_t>((int16_t)(re), (int16_t)(im));
#else // #if defined(VXSDR_COMPILER_CLANG) or defined(VXSDR_COMPILER_GCC)
                // the implementation below rounds properly using a library routine; it can be several times slower
                p->data[j] = std::complex<int16_t>((int16_t)std::lroundf(scale * data[i + j].real()),
                                                (int16_t)std::lroundf(scale * data[i + j].imag()));
#endif // #if defined(VXSDR_COMPILER_CLANG) or defined(VXSDR_COMPILER_GCC)
#else // #ifndef VXSDR_LIB_TRUNCATE_FLOAT_CONVERSION
                // truncate is fast but costs 6 dB in output noise floor
                p->data[j] = std::complex<int16_t>((int16_t)(scale * data[i + j].real()),
                                                (int16_t)(scale * data[i + j].imag()));
#endif // #ifndef VXSDR_LIB_TRUNCATE_FLOAT_CONVERSION
            }
        }
        // setting tx_data_queue_wait_us = 0 results in a busy wait
        if (data_tport->tx_data_queue->push_or_timeout(q, timeout_duration_us, tx_data_queue_wait_us)) {
            n_put += n_samples;
        } else {
            LOG_ERROR("timeout pushing to tx data queue");
            return n_put;
        }
    }
    LOG_DEBUG("put_tx_data complete ({:d} samples)", n_put);
    return n_put;
}

// Need to explicitly instantiate template classes for all allowed types so compiler will include code in library!
template size_t vxsdr::imp::put_tx_data(const std::vector<std::complex<int16_t>>& data, size_t n_requested, const uint8_t subdev, const double timeout_s);
template size_t vxsdr::imp::put_tx_data(const std::vector<std::complex<float>>& data, size_t n_requested, const uint8_t subdev, const double timeout_s);

bool vxsdr::imp::set_host_command_timeout(const double timeout_s) {
    if (timeout_s > 3600 or timeout_s < 1e-3) {
        return false;
    }
    vxsdr::imp::device_response_timeout_us = std::llround(timeout_s * 1e6);
    return true;
}

double vxsdr::imp::get_host_command_timeout() const {
    return 1e-6 * device_response_timeout_us;
}

// private functions

[[nodiscard]] bool vxsdr::imp::send_packet_and_check_response(packet& p, const std::string& cmd_name) {
    if (not command_tport->tx_rx_usable()) {
        LOG_ERROR("send_packet_and_check_response failed sending {:s}: command tx and/or rx not usable", cmd_name);
        return false;
    }
    if (not vxsdr::imp::cmd_queue_push_check(p)) {
        LOG_ERROR("send_packet_and_check_response failed sending {:s}: cmd queue push failed", cmd_name);
        return false;
    }
    command_queue_element q;
    if (command_tport->response_queue.pop_or_timeout(q, vxsdr::imp::device_response_timeout_us, device_response_wait_us)) {
        if (((p.hdr.packet_type == PACKET_TYPE_DEVICE_CMD and q.hdr.packet_type == PACKET_TYPE_DEVICE_CMD_RSP) or
             (p.hdr.packet_type == PACKET_TYPE_TX_RADIO_CMD and q.hdr.packet_type == PACKET_TYPE_TX_RADIO_CMD_RSP) or
             (p.hdr.packet_type == PACKET_TYPE_RX_RADIO_CMD and q.hdr.packet_type == PACKET_TYPE_RX_RADIO_CMD_RSP)) and
            q.hdr.command == p.hdr.command) {
            return true;
        }
        if (((p.hdr.packet_type == PACKET_TYPE_DEVICE_CMD and q.hdr.packet_type == PACKET_TYPE_DEVICE_CMD_ERR) or
             (p.hdr.packet_type == PACKET_TYPE_TX_RADIO_CMD and q.hdr.packet_type == PACKET_TYPE_TX_RADIO_CMD_ERR) or
             (p.hdr.packet_type == PACKET_TYPE_RX_RADIO_CMD and q.hdr.packet_type == PACKET_TYPE_RX_RADIO_CMD_ERR)) and
            q.hdr.command == p.hdr.command) {
            LOG_ERROR("command error in {:s}: {:s}", cmd_name, error_to_string(std::bit_cast<error_packet*>(&q)->value1));
            return false;
        }
        LOG_ERROR("invalid response received in {:s}", cmd_name);
        return false;
    }
    LOG_ERROR("timeout waiting for response in {:s}", cmd_name);
    return false;
}

[[nodiscard]] std::optional<command_queue_element> vxsdr::imp::send_packet_and_return_response(packet& p, const std::string& cmd_name) {
    if (not command_tport->tx_rx_usable()) {
        LOG_ERROR("send_packet_and_return_response failed sending {:s}: command tx and/or rx not usable", cmd_name);
        return std::nullopt;
    }
    if (not vxsdr::imp::cmd_queue_push_check(p)) {
        LOG_ERROR("send_packet_and_return_response failed sending {:s}: cmd queue push failed", cmd_name);
        return std::nullopt;
    }
    command_queue_element q;
    if (command_tport->response_queue.pop_or_timeout(q, vxsdr::imp::device_response_timeout_us, device_response_wait_us)) {
        if (((p.hdr.packet_type == PACKET_TYPE_DEVICE_CMD and q.hdr.packet_type == PACKET_TYPE_DEVICE_CMD_RSP) or
             (p.hdr.packet_type == PACKET_TYPE_TX_RADIO_CMD and q.hdr.packet_type == PACKET_TYPE_TX_RADIO_CMD_RSP) or
             (p.hdr.packet_type == PACKET_TYPE_RX_RADIO_CMD and q.hdr.packet_type == PACKET_TYPE_RX_RADIO_CMD_RSP)) and
            q.hdr.command == p.hdr.command) {
            return q;
        }
        if (((p.hdr.packet_type == PACKET_TYPE_DEVICE_CMD and q.hdr.packet_type == PACKET_TYPE_DEVICE_CMD_ERR) or
             (p.hdr.packet_type == PACKET_TYPE_TX_RADIO_CMD and q.hdr.packet_type == PACKET_TYPE_TX_RADIO_CMD_ERR) or
             (p.hdr.packet_type == PACKET_TYPE_RX_RADIO_CMD and q.hdr.packet_type == PACKET_TYPE_RX_RADIO_CMD_ERR)) and
            q.hdr.command == p.hdr.command) {
            LOG_ERROR("command error in {:s}: {:s}", cmd_name, error_to_string(std::bit_cast<error_packet*>(&q)->value1));
            return std::nullopt;
        }
        LOG_ERROR("invalid response received in {:s}", cmd_name);
        return std::nullopt;
    }
    LOG_ERROR("timeout waiting for response in {:s}", cmd_name);
    return std::nullopt;
}

[[nodiscard]] bool vxsdr::imp::cmd_queue_push_check(packet& p, const std::string& cmd_name) {
    auto* q = std::bit_cast<command_queue_element*>(&p);
    if (not command_tport->command_queue.push(*q)) {
        LOG_ERROR("error pushing to command queue in {:s}", cmd_name);
        return false;
    }
    return true;
}

void vxsdr::imp::async_handler() {
    LOG_DEBUG("async_handler started");
    while (not async_handler_stop_flag and command_tport->rx_state != packet_transport::TRANSPORT_SHUTDOWN) {
        command_queue_element a;
        while (command_tport->async_msg_queue.pop(a)) {
            vxsdr::imp::simple_async_message_handler(a);
        }
        std::this_thread::sleep_for(async_queue_wait);
    }
    LOG_DEBUG("async_handler finished");
}

void vxsdr::imp::time_point_to_time_spec_t(const vxsdr::time_point& t, time_spec_t& ts) {
    auto secs = std::chrono::time_point_cast<std::chrono::seconds>(t);
    auto nsecs = std::chrono::time_point_cast<std::chrono::nanoseconds>(t)
                    - std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);
    ts.seconds     = (uint32_t)secs.time_since_epoch().count();
    ts.nanoseconds = (uint32_t)nsecs.count();
}

void vxsdr::imp::duration_to_time_spec_t(const vxsdr::duration& d, time_spec_t& ts) {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(d);
    auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(d)
                    - std::chrono::duration_cast<std::chrono::nanoseconds>(secs);
    ts.seconds     = (uint32_t)secs.count();
    ts.nanoseconds = (uint32_t)nsecs.count();
}

void vxsdr::imp::simple_async_message_handler(const command_queue_element& a) {
    uint8_t type = a.hdr.command & ASYNC_ERROR_TYPE_MASK;
    if (type != ASYNC_NO_ERROR) {
        if (type == ASYNC_OUT_OF_SEQUENCE) {
            // out of sequence is special cased since it can be benign
            LOG_ASYNC_OOS("{:s} (subdev {:d})", "async_msg: " + async_msg_to_name(a.hdr.command), a.hdr.subdevice);
        } else {
            LOG_ASYNC("{:s} (subdev {:d})", "async_msg: " + async_msg_to_name(a.hdr.command), a.hdr.subdevice);
        }
    }
}

std::string vxsdr::imp::stream_state_to_string(const vxsdr::stream_state state) const {
    switch (state) {
        case STREAM_WAITING_FOR_START:
            return "WAITING_FOR_START";
        case STREAM_RUNNING:
            return "RUNNING";
        case STREAM_STOPPED:
            return "STOPPED";
        case STREAM_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

std::string vxsdr::imp::error_to_string(const uint32_t reason) const {
    switch (reason) {
        case ERR_NO_ERROR:
            return "NO_ERROR";
        case ERR_BAD_COMMAND:
            return "BAD_COMMAND";
        case ERR_BUSY:
            return "BUSY";
        case ERR_NO_SUCH_SUBDEVICE:
            return "NO_SUCH_SUBDEVICE";
        case ERR_NO_SUCH_CHANNEL:
            return "NO_SUCH_CHANNEL";
        case ERR_TIMEOUT:
            return "TIMEOUT";
        case ERR_BAD_HEADER_SIZE:
            return "BAD_HEADER_SIZE";
        case ERR_BAD_HEADER_FLAGS:
            return "BAD_HEADER_FLAGS";
        case ERR_BAD_PARAMETER:
            return "BAD_PARAMETER";
        case ERR_NOT_SUPPORTED:
            return "NOT_SUPPORTED";
        case ERR_BAD_PACKET_SIZE:
            return "BAD_PACKET_SIZE";
        case ERR_FAILED:
            return "FAILED";
        default:
            return "UNKNOWN ERROR";
    }
}

std::string vxsdr::imp::version_number_to_string(const uint32_t version) const {
    const unsigned major = version / 10000;
    const unsigned minor = (version / 100) % 100;
    const unsigned patch = version % 100;
    std::stringstream output;
    output << std::to_string(major) << ".";
    output << std::to_string(minor) << ".";
    output << std::to_string(patch);
    return output.str();
}

std::string vxsdr::imp::packet_type_to_name(const uint8_t number) const {
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

std::string vxsdr::imp::device_cmd_to_name(const uint8_t cmd) const {
    switch (cmd) {
        case DEVICE_CMD_HELLO:
            return "HELLO";
        case DEVICE_CMD_SET_TIME_NOW:
            return "SET_TIME_NOW";
        case DEVICE_CMD_SET_TIME_NEXT_PPS:
            return "SET_TIME_NEXT_PPS";
        case DEVICE_CMD_GET_TIME:
            return "GET_TIME";
        case DEVICE_CMD_GET_STATUS:
            return "GET_STATUS";
        case DEVICE_CMD_CLEAR_STATUS:
            return "CLEAR_STATUS";
        case DEVICE_CMD_GET_BUFFER_INFO:
            return "GET_BUFFER_INFO";
        case DEVICE_CMD_GET_BUFFER_USE:
            return "GET_BUFFER_USE";
        case DEVICE_CMD_GET_STREAM_STATE:
            return "GET_STREAM_STATE";
        case DEVICE_CMD_GET_TRANSPORT_INFO:
            return "GET_TRANSPORT_INFO";
        case DEVICE_CMD_GET_TRANSPORT_ADDR:
            return "GET_TRANSPORT_ADDR";
        case DEVICE_CMD_GET_MAX_PAYLOAD:
            return "GET_MAX_PAYLOAD";
        case DEVICE_CMD_CLEAR_DATA_BUFFER:
            return "CLEAR_DATA_BUFFER";
        case DEVICE_CMD_SET_TRANSPORT_ADDR:
            return "SET_TRANSPORT_ADDR";
        case DEVICE_CMD_SET_MAX_PAYLOAD:
            return "SET_MAX_PAYLOAD";
        case DEVICE_CMD_SAVE_TRANSPORT_ADDR:
            return "SAVE_TRANSPORT_ADDR";
        case DEVICE_CMD_GET_NUM_SUBDEVS:
            return "GET_NUM_SUDEVS";
        case DEVICE_CMD_GET_NUM_SENSORS:
            return "GET_NUM_SENSORS";
        case DEVICE_CMD_GET_TIMING_INFO:
            return "GET_TIMING_INFO";
        case DEVICE_CMD_GET_TIMING_STATUS:
            return "GET_TIMING_STATUS";
        case DEVICE_CMD_GET_TIMING_REF:
            return "GET_TIMING_REF";
        case DEVICE_CMD_SET_TIMING_REF:
            return "SET_TIMING_REF";
        case DEVICE_CMD_APP_UPDATE_MODE_SET:
            return "APP_UPDATE_MODE_SET";
        case DEVICE_CMD_APP_UPDATE_DATA:
            return "APP_UPDATE_DATA";
        case DEVICE_CMD_APP_UPDATE_DONE:
            return "APP_UPDATE_DONE";
        case DEVICE_CMD_STOP:
            return "STOP";
        case DEVICE_CMD_RESET:
            return "RESET";
        default:
            return "UNKNOWN_DEVICE_CMD";
    }
}

std::string vxsdr::imp::cmd_error_to_name(const uint32_t reason) const {
    switch (reason) {
        case ERR_NO_ERROR:
            return "NO_ERROR";
        case ERR_BAD_COMMAND:
            return "BAD_COMMAND";
        case ERR_BUSY:
            return "BUSY";
        case ERR_NO_SUCH_SUBDEVICE:
            return "NO_SUCH_SUBDEVICE";
        case ERR_NO_SUCH_CHANNEL:
            return "NO_SUCH_CHANNEL";
        case ERR_TIMEOUT:
            return "TIMEOUT";
        case ERR_BAD_HEADER_SIZE:
            return "BAD_HEADER_SIZE";
        case ERR_BAD_HEADER_FLAGS:
            return "BAD_HEADER_FLAGS";
        case ERR_BAD_PARAMETER:
            return "BAD_PARAMETER";
        case ERR_NOT_SUPPORTED:
            return "NOT_SUPPORTED";
        case ERR_BAD_PACKET_SIZE:
            return "BAD_PACKET_SIZE";
        case ERR_FAILED:
            return "FAILED";
        default:
            return "UNKNOWN_DEVICE_CMD_ERROR";
    }
}

std::string vxsdr::imp::radio_cmd_to_name(const uint8_t cmd) const {
    switch (cmd) {
        case RADIO_CMD_STOP:
            return "STOP";
        case RADIO_CMD_START:
            return "START";
        case RADIO_CMD_LOOP:
            return "LOOP";
        case RADIO_CMD_GET_RF_FREQ:
            return "GET_RF_FREQ";
        case RADIO_CMD_GET_IF_FREQ:
            return "GET_IF_FREQ";
        case RADIO_CMD_GET_RF_GAIN:
            return "GET_RF_GAIN";
        case RADIO_CMD_GET_SAMPLE_RATE:
            return "GET_SAMPLE_RATE";
        case RADIO_CMD_GET_RF_BW:
            return "GET_RF_BW";
        case RADIO_CMD_GET_RF_ENABLED:
            return "GET_RF_ENABLED";
        case RADIO_CMD_GET_RF_PORT:
            return "GET_RF_PORT";
        case RADIO_CMD_GET_NUM_RF_PORTS:
            return "GET_NUM_RF_PORTS";
        case RADIO_CMD_GET_RF_PORT_NAME:
            return "GET_RF_PORT_NAME";
        case RADIO_CMD_GET_LO_INPUT:
            return "GET_LO_INPUT";
        case RADIO_CMD_GET_MASTER_CLK:
            return "GET_MASTER_CLK";
        case RADIO_CMD_SET_RF_FREQ:
            return "SET_RF_FREQ";
        case RADIO_CMD_SET_RF_GAIN:
            return "SET_RF_GAIN";
        case RADIO_CMD_SET_SAMPLE_RATE:
            return "SET_SAMPLE_RATE";
        case RADIO_CMD_SET_RF_BW:
            return "SET_RF_BW";
        case RADIO_CMD_SET_RF_ENABLED:
            return "SET_RF_ENABLED";
        case RADIO_CMD_SET_RF_PORT:
            return "SET_RF_PORT";
        case RADIO_CMD_SET_LO_INPUT:
            return "SET_LO_INPUT";
        case RADIO_CMD_SET_MASTER_CLK:
            return "SET_MASTER_CLK";
        case RADIO_CMD_GET_RF_FREQ_RANGE:
            return "GET_RF_FREQ_RANGE";
        case RADIO_CMD_GET_RF_GAIN_RANGE:
            return "GET_RF_GAIN_RANGE";
        case RADIO_CMD_GET_SAMPLE_RATE_RANGE:
            return "GET_SAMPLE_RATE_RANGE";
        case RADIO_CMD_GET_NUM_CHANNELS:
            return "GET_NUM_CHANNELS";
        case RADIO_CMD_GET_IQ_BIAS:
            return "GET_IQ_BIAS";
        case RADIO_CMD_GET_IQ_CORR:
            return "GET_IQ_CORR";
        case RADIO_CMD_SET_IQ_BIAS:
            return "SET_IQ_BIAS";
        case RADIO_CMD_SET_IQ_CORR:
            return "SET_IQ_CORR";
        default:
            return "UNKNOWN_RADIO_CMD";
    }
}

std::string vxsdr::imp::async_msg_to_name(const uint8_t msg) const {
    uint8_t typ = msg & ASYNC_ERROR_TYPE_MASK;
    uint8_t sys = msg & ASYNC_AFFECTED_SYSTEM_MASK;
    std::string subsys;
    std::string typstr;
    switch(sys) {
            case ASYNC_UNSPECIFIED:
                subsys = "";
                break;
            case ASYNC_TX:
                subsys = "TX";
                break;
            case ASYNC_RX:
                subsys = "RX";
                break;
            case ASYNC_FPGA:
                subsys = "FPGA";
                break;
            default:
                subsys = "UNKNOWN";
                break;
        }
    switch (typ) {
        case ASYNC_NO_ERROR:
            typstr = "NO_ERROR";
            break;
        case ASYNC_DATA_UNDERFLOW:
            typstr = "DATA_UNDERFLOW";
            break;
        case ASYNC_DATA_OVERFLOW:
            typstr = "DATA_OVERFLOW";
            break;
        case ASYNC_OVER_TEMP:
            typstr = "OVER_TEMP";
            break;
        case ASYNC_FREQ_ERROR:
            typstr = "FREQ_ERROR";
            break;
        case ASYNC_OUT_OF_SEQUENCE:
            typstr = "OUT_OF_SEQUENCE";
            break;
        case ASYNC_PPS_TIMEOUT:
            typstr = "PPS_TIMEOUT";
            break;
        case ASYNC_VOLTAGE_ERROR:
            typstr =  "VOLTAGE_ERROR";
            break;
        case ASYNC_CURRENT_ERROR:
            typstr =  "CURRENT_ERROR";
            break;
        default:
            typstr = "UNKNOWN";
            break;
    }
    if (subsys.empty()) {
        return typstr;
    }
    return subsys + " " + typstr;
}

std::map<std::string, int64_t> vxsdr::imp::apply_config(const std::map<std::string, int64_t>& input_config) const {
    std::map<std::string, int64_t> config = vxsdr::imp::default_config;
    for (const auto& s : input_config) {
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
}