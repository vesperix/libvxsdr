// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <array>
#include <future>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>

#include "logging.hpp"
#include "vxsdr.hpp"
#include "vxsdr_imp.hpp"
#include "vxsdr_threads.hpp"

/*! @file device_commands.cpp
    @brief Device command functions for the @p vxsdr class.
*/

std::optional<std::array<uint32_t, 6>> vxsdr::imp::hello() {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_HELLO, 0, 0, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "hello()");
    if (res) {
        auto q                      = res.value();

        auto* r                     = std::bit_cast<six_uint32_packet*>(&q);
        std::array<uint32_t, 6> res = {r->value1, r->value2, r->value3, r->value4, r->value5, r->value6};
        return res;
    }
    return std::nullopt;
}

bool vxsdr::imp::reset() {
    header_only_packet p;
    p.hdr = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_RESET, 0, 0, 0, sizeof(p), 0};
    return vxsdr::imp::send_packet_and_check_response(p, "reset()");
}

bool vxsdr::imp::clear_status(const uint8_t subdev) {
    header_only_packet p;
    p.hdr = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_CLEAR_STATUS, 0, subdev, 0, sizeof(p), 0};
    return vxsdr::imp::send_packet_and_check_response(p, "clear_status()");
}

std::optional<std::array<uint32_t, 8>> vxsdr::imp::get_status(const uint8_t subdev) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_GET_STATUS, 0, subdev, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_status()");
    if (res) {
        auto q                      = res.value();

        auto* r                     = std::bit_cast<eight_uint32_packet*>(&q);
        std::array<uint32_t, 8> res = {r->value1, r->value2, r->value3, r->value4, r->value5, r->value6, r->value7, r->value8};
        return res;
    }
    return std::nullopt;
}

bool vxsdr::imp::set_time_now(const vxsdr::time_point& t) {
    cmd_or_rsp_packet_time p = {};
    p.hdr = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_SET_TIME_NOW, FLAGS_TIME_PRESENT, 0, 0, sizeof(p), 0};
    vxsdr::imp::time_point_to_time_spec_t(t, p.time);
    return vxsdr::imp::send_packet_and_check_response(p, "set_time_now()");
}

bool vxsdr::imp::set_time_next_pps(const vxsdr::time_point& t) {
    cmd_or_rsp_packet_time p = {};
    p.hdr = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_SET_TIME_NEXT_PPS, FLAGS_TIME_PRESENT, 0, 0, sizeof(p), 0};
    vxsdr::imp::time_point_to_time_spec_t(t, p.time);
    return vxsdr::imp::send_packet_and_check_response(p, "set_time_next_pps()");
}

std::optional<vxsdr::time_point> vxsdr::imp::get_time_now() {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_GET_TIME, 0, 0, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_time_now()");
    if (res) {
        auto q = res.value();
        if ((q.hdr.flags & FLAGS_TIME_PRESENT) != 0) {

            auto* r = std::bit_cast<cmd_or_rsp_packet_time*>(&q);
            return vxsdr::time_point(
                std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds(r->time.seconds) +
                                                                      std::chrono::nanoseconds(r->time.nanoseconds)));
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::array<uint32_t, 2>> vxsdr::imp::get_buffer_info(const uint8_t subdev) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_GET_BUFFER_INFO, 0, subdev, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_buffer_info()");
    if (res) {
        auto q                      = res.value();

        auto* r                     = std::bit_cast<two_uint32_packet*>(&q);
        // order for return is TX, RX, opposite of packet order
        std::array<uint32_t, 2> ret = {r->value2, r->value1};
        return ret;
    }
    return std::nullopt;
}

std::optional<std::array<uint32_t, 2>> vxsdr::imp::get_buffer_use(const uint8_t subdev) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_GET_BUFFER_USE, 0, subdev, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_buffer_use()");
    if (res) {
        auto q                      = res.value();

        auto* r                     = std::bit_cast<two_uint32_packet*>(&q);
        // order for return is TX, RX, opposite of packet order
        std::array<uint32_t, 2> ret = {r->value2, r->value1};
        return ret;
    }
    return std::nullopt;
}

std::optional<vxsdr::stream_state> vxsdr::imp::get_tx_stream_state(const uint8_t subdev) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_GET_STREAM_STATE, 0, subdev, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_tx_stream_state()");
    if (res) {
        auto q                      = res.value();

        auto* r                     = std::bit_cast<one_uint64_packet*>(&q);
        bool running_flag = (r->value1 & STREAM_STATE_TX_RUNNING_FLAG) != 0;
        bool waiting_flag = (r->value1 & STREAM_STATE_TX_WAITING_FLAG) != 0;
        vxsdr::stream_state tx_stream_state;
        if (not running_flag and not waiting_flag) {
            tx_stream_state = STREAM_STOPPED;
        } else if (not running_flag and waiting_flag) {
            tx_stream_state = STREAM_WAITING_FOR_START;
        }
        else if (running_flag and not waiting_flag) {
            tx_stream_state = STREAM_RUNNING;
        } else {
            tx_stream_state = STREAM_ERROR;
        }
        return tx_stream_state;
    }
    return std::nullopt;
}

std::optional<vxsdr::stream_state> vxsdr::imp::get_rx_stream_state(const uint8_t subdev) {
    header_only_packet p;
    p.hdr    = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_GET_STREAM_STATE, 0, subdev, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_rx_stream_state()");
    if (res) {
        auto q                      = res.value();

        auto* r                     = std::bit_cast<one_uint64_packet*>(&q);
        bool running_flag = (r->value1 & STREAM_STATE_RX_RUNNING_FLAG) != 0;
        bool waiting_flag = (r->value1 & STREAM_STATE_RX_WAITING_FLAG) != 0;
        vxsdr::stream_state rx_stream_state;
        if (not running_flag and not waiting_flag) {
            rx_stream_state = STREAM_STOPPED;
        } else if (not running_flag and waiting_flag) {
            rx_stream_state = STREAM_WAITING_FOR_START;
        }
        else if (running_flag and not waiting_flag) {
            rx_stream_state = STREAM_RUNNING;
        } else {
            rx_stream_state = STREAM_ERROR;
        }
        return rx_stream_state;
    }
    return std::nullopt;
}

std::optional<std::array<bool, 3>> vxsdr::imp::get_timing_status() {
    // FIXME: these need to be defined as part of a generic API when stable
    constexpr uint32_t TIMING_STATUS_EXT_PPS_LOCK   = 0x00000001;
    constexpr uint32_t TIMING_STATUS_EXT_10MHZ_LOCK = 0x00000002;
    constexpr uint32_t TIMING_STATUS_REF_OSC_LOCK   = 0x00000004;
    header_only_packet p                            = {};
    p.hdr    = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_GET_TIMING_STATUS, 0, 0, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_timing_status()");
    if (res) {
        auto q                  = res.value();

        auto* r                 = std::bit_cast<one_uint32_packet*>(&q);
        std::array<bool, 3> ret = {(bool)(r->value1 & TIMING_STATUS_EXT_PPS_LOCK),
                                   (bool)(r->value1 & TIMING_STATUS_EXT_10MHZ_LOCK),
                                   (bool)(r->value1 & TIMING_STATUS_REF_OSC_LOCK)};
        return ret;
    }
    return std::nullopt;
}

std::optional<double> vxsdr::imp::get_timing_resolution() {
    header_only_packet p = {};
    p.hdr    = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_GET_TIMING_RESOLUTION, 0, 0, 0, sizeof(p), 0};
    auto res = vxsdr::imp::send_packet_and_return_response(p, "get_timing_resolution()");
    if (res) {
        auto q  = res.value();
        auto* r = std::bit_cast<one_double_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}

std::vector<std::string> vxsdr::imp::discover_ipv4_addresses(const std::string& local_addr_str,
                                                                    const std::string& broadcast_addr_str,
                                                                    const double timeout_s) {
    const unsigned destination_port = 1030;
    net::ip::address_v4 local_addr = net::ip::address_v4::from_string(local_addr_str);
    net::ip::address_v4 broadcast_addr = net::ip::address_v4::from_string(broadcast_addr_str);
    std::vector<std::string> ret;

    // wait 1 / 1000 of the timeout specified each rx
    unsigned discover_wait_ms = lround(timeout_s);

   net_error_code error;
    net::io_context discover_context;

    auto work           = net::make_work_guard(discover_context);
    auto context_thread = vxsdr_thread([&discover_context] { discover_context.run(); });

    net::ip::udp::endpoint local_endpoint(local_addr, destination_port);
    net::ip::udp::endpoint device_endpoint(broadcast_addr, destination_port);

    net::ip::udp::socket discover_socket(discover_context, local_endpoint);

    discover_socket.open(net::ip::udp::v4(), error);
    if (error) {
        LOG_ERROR("unable to open socket in discover_ipv4_addresses()");
        if (context_thread.joinable()) {
            context_thread.join();
        }
        return ret;
    }

    discover_socket.set_option(net::ip::udp::socket::reuse_address(true), error);
    if (error) {
        LOG_ERROR("unable to set reuse_address option in discover_ipv4_addresses()");
        if (context_thread.joinable()) {
            context_thread.join();
        }
        return ret;
    }
    discover_socket.set_option(net::socket_base::broadcast(true), error);
    if (error) {
        LOG_ERROR("unable to set broadcast option in discover_ipv4_addresses()");
        if (context_thread.joinable()) {
            context_thread.join();
        }
        return ret;
    }

    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_DISCOVER, 0, 0, 0, sizeof(p), 0};

    auto bytes_sent      = discover_socket.send_to(net::buffer(&p, sizeof(p)), device_endpoint);
    if (bytes_sent != sizeof(p)) {
        LOG_ERROR("error sending discover packet in discover_ipv4_addresses()");
        if (context_thread.joinable()) {
            context_thread.join();
        }
        return ret;
    }

    auto t_start                    = std::chrono::steady_clock::now();
    std::chrono::duration<double> t = {};

    do {
        one_uint32_packet q = {};
        net::ip::udp::endpoint remote_endpoint;

        std::future<size_t> result =
            discover_socket.async_receive_from(net::buffer(&q, sizeof(q)), remote_endpoint, net::use_future);

        if (result.wait_for(std::chrono::milliseconds(discover_wait_ms)) == std::future_status::ready) {
            if (result.get() == sizeof(one_uint32_packet) and q.hdr.packet_type == PACKET_TYPE_DEVICE_CMD_RSP and
                q.hdr.command == p.hdr.command) {

                auto* r = std::bit_cast<one_uint32_packet*>(&q);
                ret.push_back(net::ip::address_v4(r->value1).to_string());
            } else {
                LOG_WARN("extraneous response received in discover_ipv4_addresses()");
            }
        }
        t = t_start - std::chrono::steady_clock::now();
    } while (t.count() <= timeout_s);

    discover_socket.close(error);
    if (error) {
        LOG_ERROR("error closing socket in discover_ipv4_addresses()");
    }
    if (context_thread.joinable()) {
        context_thread.join();
    }

    return ret;
}

bool vxsdr::imp::set_ipv4_address(const std::string& device_address_str) {
    net::ip::address_v4 device_address = net::ip::address_v4::from_string(device_address_str);
    one_uint32_packet p = {};
    p.hdr               = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_SET_TRANSPORT_ADDR, 0, 0, 0, sizeof(p), 0};
    p.value1            = device_address.to_uint();
    return vxsdr::imp::send_packet_and_check_response(p, "set_ipv4_address()");
}

bool vxsdr::imp::save_ipv4_address(const std::string& device_address_str) {
    net::ip::address_v4 device_address = net::ip::address_v4::from_string(device_address_str);
    one_uint32_packet p = {};
    p.hdr               = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_SAVE_TRANSPORT_ADDR, 0, 0, 0, sizeof(p), 0};
    p.value1            = device_address.to_uint();
    return vxsdr::imp::send_packet_and_check_response(p, "save_ipv4_address()");
}

std::optional<unsigned> vxsdr::imp::get_max_payload_bytes() {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_GET_MAX_PAYLOAD, 0, 0, 0, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_max_payload_bytes()");
    if (res) {
        auto q  = res.value();
        auto* r = std::bit_cast<one_uint32_packet*>(&q);
        return (unsigned)r->value1;
    }
    return std::nullopt;
}

bool vxsdr::imp::set_max_payload_bytes(const unsigned max_payload_bytes) {
    one_uint32_packet p = {};
    p.hdr               = {PACKET_TYPE_DEVICE_CMD,  DEVICE_CMD_SET_MAX_PAYLOAD, 0, 0, 0, sizeof(p), 0};
    p.value1            = max_payload_bytes;
    return vxsdr::imp::send_packet_and_check_response(p, "set_max_payload_bytes()");
}

std::optional<unsigned> vxsdr::imp::get_num_sensors(const uint8_t subdev) {
    header_only_packet p = {};
    p.hdr                = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_GET_NUM_SENSORS, 0, subdev, 0, sizeof(p), 0};
    auto res             = vxsdr::imp::send_packet_and_return_response(p, "get_num_sensors()");
    if (res) {
        auto q  = res.value();
        auto* r = std::bit_cast<one_uint32_packet*>(&q);
        return (unsigned)r->value1;
    }
    return std::nullopt;
}

std::vector<std::string> vxsdr::imp::get_sensor_names(const uint8_t subdev) {
    constexpr int MAX_NAMES = 256;
    std::vector<std::string> names;
    for (int i = 0; i < MAX_NAMES; i++) {
        one_uint32_packet p = {};
        p.hdr         = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_GET_SENSOR_NAME, 0, subdev, 0, sizeof(p), 0};
        p.value1      = i;
        auto res      = vxsdr::imp::send_packet_and_return_response(p, "get_sensor_names()");
        if (res) {
            auto q  = res.value();
            auto* r = std::bit_cast<name_packet*>(&q);
            if (strlen(&r->name[0]) > 0) {
                names.emplace_back(&r->name[0]);
            }
        } else {
            break;
        }
    }
    return names;
}

std::optional<double> vxsdr::imp::get_sensor_reading(const std::string& sensor_name, const uint8_t subdev) {
    name_packet p = {};
    p.hdr         = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_GET_SENSOR, 0, subdev, 0, sizeof(p), 0};
    strncpy((char*)p.name, sensor_name.c_str(), MAX_PAYLOAD_LENGTH_BYTES - 1);
    p.name[MAX_PAYLOAD_LENGTH_BYTES - 1] = 0;
    auto res      = vxsdr::imp::send_packet_and_return_response(p, "get_sensor()");
    if (res) {
        auto q  = res.value();
        auto* r = std::bit_cast<one_double_packet*>(&q);
        return r->value1;
    }
    return std::nullopt;
}