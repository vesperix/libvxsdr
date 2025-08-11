// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <future>
#include <iostream>
#include <string>

#include "option_utils.hpp"

#include "vxsdr_net.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_threads.hpp"

void add_setup_options(option_utils::program_options& desc) {
    desc.add_option("local_address", "IPv4 address of local interface", option_utils::supported_types::STRING, true);
    desc.add_option("new_device_address", "new IPv4 address of device to be saved", option_utils::supported_types::STRING, true);
    desc.add_flag("help", "print usage");
}

bool send_packet(net::ip::udp::socket& sender_socket, net::ip::udp::endpoint& device_endpoint, packet& packet) {
    static size_t packets_sent  = 0;
    packet.hdr.sequence_counter = (uint16_t)(packets_sent % (UINT16_MAX + 1));
    packets_sent++;
    return packet.hdr.packet_size == sender_socket.send_to(net::buffer(&packet, packet.hdr.packet_size), device_endpoint);
}

bool receive_packet(net::ip::udp::socket& receiver_socket,
                    largest_data_packet& recv_buffer,
                    net::ip::udp::endpoint& remote_endpoint,
                    const unsigned receive_timeout_ms) {
    auto result =
        receiver_socket.async_receive_from(net::buffer(&recv_buffer, sizeof(recv_buffer)), remote_endpoint, net::use_future);
    if (result.wait_for(std::chrono::milliseconds(receive_timeout_ms)) == std::future_status::ready) {
        if (result.get() > 0) {
            return true;
        }
    }
    return false;
}

bool receive_device_cmd_response_packet(net::ip::udp::socket& receiver_socket,
                                        net::ip::udp::endpoint& device_endpoint,
                                        largest_cmd_or_rsp_packet& results,
                                        const double timeout_s) {
    net::ip::udp::endpoint remote_endpoint;
    largest_data_packet packet;
    unsigned timeout_ms = std::lround(1000 * timeout_s);
    auto timeout        = std::chrono::milliseconds(timeout_ms);
    auto start_time     = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time <= timeout) {
        if (receive_packet(receiver_socket, packet, remote_endpoint, timeout_ms)) {
            if (remote_endpoint == device_endpoint and packet.hdr.packet_type == PACKET_TYPE_DEVICE_CMD_RSP) {
                std::memcpy((void*)&results, &packet, std::min((size_t)packet.hdr.packet_size, sizeof(results)));
                return true;
            }
        }
    }
    std::cerr << "receive_device_cmd_response_packet: error return" << std::endl;
    return false;
}

bool send_device_cmd_and_check_response(net::ip::udp::socket& sender_socket,
                                        net::ip::udp::endpoint& device_endpoint,
                                        packet& packet,
                                        net::ip::udp::socket& receiver_socket,
                                        largest_cmd_or_rsp_packet& r,
                                        const double timeout_s) {
    static size_t packets_sent = 0;
    if (packet.hdr.packet_type != PACKET_TYPE_DEVICE_CMD) {
        return false;
    }
    packet.hdr.sequence_counter = (uint16_t)(packets_sent % (UINT16_MAX + 1));
    packets_sent++;
    size_t bytes = sender_socket.send_to(net::buffer(&packet, packet.hdr.packet_size), device_endpoint);
    if (bytes != packet.hdr.packet_size) {
        return false;
    }
    if (not receive_device_cmd_response_packet(receiver_socket, device_endpoint, r, timeout_s) or
        r.hdr.command != packet.hdr.command) {
        return false;
    }
    return true;
}

bool send_save_transport_addr_packet(net::ip::udp::socket& sender_socket,
                                     net::ip::udp::endpoint& device_endpoint,
                                     net::ip::address_v4& new_address,
                                     net::ip::udp::socket& receiver_socket,
                                     const double timeout_s) {
    one_uint32_packet p         = {};
    p.hdr                       = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_SAVE_TRANSPORT_ADDR, 0, 0, 0, sizeof(p), 0};
    p.value1                    = new_address.to_uint();
    largest_cmd_or_rsp_packet r = {};
    return send_device_cmd_and_check_response(sender_socket, device_endpoint, p, receiver_socket, r, timeout_s);
}

#define BANNER_STRING VERSION_STRING " (" SYSTEM_INFO " " COMPILER_INFO ")"

int main(int argc, char* argv[]) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::cout << "vxsdr_save_addr" << BANNER_STRING << std::endl;
    const unsigned udp_host_receive_port   = 1030;
    const unsigned udp_host_send_port      = 55123;
    const unsigned udp_device_receive_port = 1030;
    const double timeout_s                 = 10;

    try {
        option_utils::program_options desc(
            "vxsdr_save_addr",
            "Permanently saves a new IPv4 address for a VXSDR device; use vxsdr_set_addr to change the address first");

        add_setup_options(desc);

        auto vm = desc.parse(argc, argv);

        auto local_addr           = net::ip::make_address_v4(vm["local_address"].as<std::string>());
        auto new_destination_addr = net::ip::make_address_v4(vm["new_device_address"].as<std::string>());

        net::ip::udp::endpoint local_send_endpoint(local_addr, udp_host_send_port);
        net::ip::udp::endpoint local_receive_endpoint(local_addr, udp_host_receive_port);
        net::ip::udp::endpoint new_device_endpoint(new_destination_addr, udp_device_receive_port);

        net::io_context ctx;
        auto work = net::make_work_guard(ctx);
        vxsdr_thread context_thread([&ctx]() { ctx.run(); });

        net::ip::udp::socket sender_socket(ctx, local_send_endpoint);
        net::ip::udp::socket receiver_socket(ctx, local_receive_endpoint);

        std::cout << "Saving IPv4 address " << new_destination_addr.to_string() << " to nonvolatile memory . . ." << std::endl;

        if (send_save_transport_addr_packet(sender_socket, new_device_endpoint, new_destination_addr, receiver_socket, timeout_s)) {
            std::cout << "Save command sent successfully." << std::endl;
            std::cout << "Power cycle the device, then run vxsdr_find to confirm that the new address" << std::endl;
            std::cout << "has been properly saved to nonvolatile memory." << std::endl;
        } else {
            std::cerr << "Error saving address. Power cycle the device to return to the original address." << std::endl;
        }

        ctx.stop();
        context_thread.join();
    } catch (std::exception& e) {
        std::cerr << "Error: exception caught - " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Done." << std::endl;
    return EXIT_SUCCESS;
}