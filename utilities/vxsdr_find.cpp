// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <string>
#include <future>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <vector>

#include "option_utils.hpp"

#include "vxsdr_net.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_threads.hpp"

void add_setup_options(option_utils::program_options& desc) {
    desc.add_option("local_address", "IPv4 address of local interface", option_utils::supported_types::STRING, true);
    desc.add_option("netmask", "IPv4 netmask of local interface", option_utils::supported_types::STRING, false, "255.255.255.0");
    desc.add_flag("help", "print usage");
}

bool send_packet(net::ip::udp::socket& sender_socket, net::ip::udp::endpoint& device_endpoint, packet& packet) {
    static size_t packets_sent  = 0;
    packet.hdr.sequence_counter = (uint16_t)(packets_sent % (UINT16_MAX + 1));
    packets_sent++;
    return packet.hdr.packet_size == sender_socket.send_to(net::buffer(&packet, packet.hdr.packet_size), device_endpoint);
}

bool receive_packet(net::ip::udp::socket& receiver_socket,
                    largest_cmd_or_rsp_packet& recv_buffer,
                    net::ip::udp::endpoint& remote_endpoint,
                    const unsigned receive_timeout_ms) {

    auto result = receiver_socket.async_receive_from(net::buffer(&recv_buffer, sizeof(recv_buffer)), remote_endpoint, net::use_future);
    if (result.wait_for(std::chrono::milliseconds(receive_timeout_ms)) == std::future_status::ready) {
    //    std::cerr << "type = 0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (unsigned)recv_buffer.hdr.packet_type << " "
    //              << "cmd  = 0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (unsigned)recv_buffer.hdr.command  << " "
    //              << "size =   " << std::setw(6) << std::setfill(' ') << (unsigned)recv_buffer.hdr.packet_size  << std::endl;
        return true;
    }
    return false;
}

bool send_discover_packet(net::ip::udp::socket& sender_socket, net::ip::udp::endpoint& device_endpoint) {
    header_only_packet p = {};
    p.hdr = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_DISCOVER, 0, 0, 0, sizeof(p), 0};
    return send_packet(sender_socket, device_endpoint, p);
}

bool send_hello_packet(net::ip::udp::socket& sender_socket, net::ip::udp::endpoint& device_endpoint) {
    header_only_packet p;
    p.hdr = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_HELLO, 0, 0, 0, sizeof(p), 0};
    return send_packet(sender_socket, device_endpoint, p);
}

bool receive_discover_response_packets(net::ip::udp::socket& receiver_socket,
                                       std::vector<one_uint32_packet>& results,
                                       const double timeout_s) {
    net::ip::udp::endpoint remote_endpoint;
    largest_cmd_or_rsp_packet packet;
    packet.hdr = {0, 0, 0, 0, 0, 0, 0};
    unsigned timeout_ms = std::lround(1000 * timeout_s);
    while (receive_packet(receiver_socket, packet, remote_endpoint, timeout_ms)) {
        if (packet.hdr.packet_type == PACKET_TYPE_DEVICE_CMD_RSP and packet.hdr.command == DEVICE_CMD_DISCOVER) {
            one_uint32_packet res;
            std::memcpy((void *)&res, &packet, std::min((size_t)packet.hdr.packet_size, sizeof(res)));
            results.push_back(res);
        }
        packet.hdr = {0, 0, 0, 0, 0, 0, 0};
    }
    return not results.empty();
}

bool receive_hello_response_packet(net::ip::udp::socket& receiver_socket,
                                       eight_uint32_packet& result,
                                       const double timeout_s) {
    net::ip::udp::endpoint remote_endpoint;
    largest_cmd_or_rsp_packet packet;
    packet.hdr = {0, 0, 0, 0, 0, 0, 0};
    unsigned timeout_ms = std::lround(1000 * timeout_s);
    receive_packet(receiver_socket, packet, remote_endpoint, timeout_ms);
    if (packet.hdr.packet_type == PACKET_TYPE_DEVICE_CMD_RSP and packet.hdr.command == DEVICE_CMD_HELLO) {
        std::memcpy((void *)&result, &packet, std::min((size_t)packet.hdr.packet_size, sizeof(result)));
        return true;
    }
    return false;
}

void output_hello_response(eight_uint32_packet& response) {
    std::cout << "      device id                 = " << response.value1 << std::endl;
    std::cout << "      fpga firmware version     = " << response.value2 << std::endl;
    std::cout << "      mcu software version      = " << response.value3 << std::endl;
    std::cout << "      serial number             = " << response.value4 << std::endl;
}

#define BANNER_STRING VERSION_STRING " (" SYSTEM_INFO " " COMPILER_INFO ")"

int main(int argc, char* argv[]) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::cout << "vxsdr_find " << BANNER_STRING << std::endl;
    const unsigned udp_host_receive_port   = 1030;
    const unsigned udp_host_send_port      = 55123;
    const unsigned udp_device_receive_port = 1030;
    const double timeout_s                 = 2; // maximum delay in discover response is 1024 ms
    try {
        option_utils::program_options desc("vxsdr_find", "Finds VXSDR devices on the local network");

        add_setup_options(desc);

        auto vm = desc.parse(argc, argv);

        auto local_addr = net::ip::address_v4::from_string(vm["local_address"].as<std::string>());
        auto netmask    = net::ip::address_v4::from_string(vm["netmask"].as<std::string>());
        net::ip::network_v4 local_net(local_addr, netmask);

        net::ip::udp::endpoint local_send_endpoint(local_addr, udp_host_send_port);
        net::ip::udp::endpoint local_receive_endpoint(local_addr, udp_host_receive_port);
        net::ip::udp::endpoint discover_endpoint(local_net.broadcast(), udp_device_receive_port);

        std::cout << "Searching for VXSDR devices using broadcast address "
                  << local_net.broadcast().to_string() << " . . ." << std::endl;

        net::io_context ctx;
        auto work = net::make_work_guard(ctx);
        vxsdr_thread context_thread([&ctx](){ ctx.run(); });

        net::ip::udp::socket sender_socket(ctx, local_send_endpoint);
        // sender_socket.set_option(net::ip::udp::socket::reuse_address(true));
        sender_socket.set_option(net::socket_base::broadcast(true));
        net::ip::udp::socket receiver_socket(ctx, local_receive_endpoint);

        std::vector<one_uint32_packet> results;

        if (not send_discover_packet(sender_socket, discover_endpoint)) {
            std::cerr << "Error: failed to send discover packet" << std::endl;
        } else {
            if (receive_discover_response_packets(receiver_socket, results, timeout_s)) {
                if (results.size() == 1) {
                    std::cout << "Found 1 VXSDR device:" << std::endl;
                } else {
                    std::cout << "Found " << results.size() << " VXSDR devices:" << std::endl;
                }
                for (auto& result : results) {
                    auto device_addr = net::ip::make_address_v4(result.value1);
                    net::ip::udp::endpoint hello_endpoint(device_addr, udp_device_receive_port);
                    std::cout << "   Device at address " << device_addr.to_string() << ":" << std::endl;
                    if (send_hello_packet(sender_socket, hello_endpoint)) {
                        eight_uint32_packet r {};
                        if (receive_hello_response_packet(receiver_socket, r, timeout_s)) {
                            output_hello_response(r);
                        } else {
                            std::cerr << "Error: no response to hello packet" << std::endl;
                        }
                    } else {
                        std::cerr << "Error: failed to send hello packet" << std::endl;
                    }
                }
            } else {
                std::cout << "No VXSDR devices found." << std::endl;
            }
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