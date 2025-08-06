// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

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
    desc.add_option("device_address", "current IPv4 address of device", option_utils::supported_types::STRING, true);
    desc.add_option("new_device_address", "new IPv4 address of device", option_utils::supported_types::STRING, true);
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

bool send_device_cmd(net::ip::udp::socket& sender_socket, net::ip::udp::endpoint& device_endpoint, packet& packet) {
    static size_t packets_sent = 0;
    if (packet.hdr.packet_type != PACKET_TYPE_DEVICE_CMD) {
        return false;
    }
    packet.hdr.sequence_counter = (uint16_t)(packets_sent % (UINT16_MAX + 1));
    packets_sent++;
    return packet.hdr.packet_size == sender_socket.send_to(net::buffer(&packet, packet.hdr.packet_size), device_endpoint);
}

bool send_set_transport_addr_packet(net::ip::udp::socket& sender_socket,
                                    net::ip::udp::endpoint& device_endpoint,
                                    net::ip::address_v4& new_address) {
    one_uint32_packet p = {};
    p.hdr               = {PACKET_TYPE_DEVICE_CMD, DEVICE_CMD_SET_TRANSPORT_ADDR, 0, 0, 0, sizeof(p), 0};
    p.value1            = new_address.to_uint();
    return send_device_cmd(sender_socket, device_endpoint, p);
}

#define BANNER_STRING VERSION_STRING " (" SYSTEM_INFO " " COMPILER_INFO ")"

int main(int argc, char* argv[]) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::cout << "Starting " << argv[0] << " " << BANNER_STRING << std::endl;
    const unsigned udp_host_receive_port   = 1030;
    const unsigned udp_host_send_port      = 55123;
    const unsigned udp_device_receive_port = 1030;

    try {
        option_utils::program_options desc(
            "vxsdr_set_addr", "Sets a new IPv4 address for a VXSDR device; use vxsdr_save_addr to make the change permanent");

        add_setup_options(desc);

        auto vm = desc.parse(argc, argv);

        auto local_addr               = net::ip::make_address_v4(vm["local_address"].as<std::string>());
        auto current_destination_addr = net::ip::make_address_v4(vm["current_device_address"].as<std::string>());
        auto new_destination_addr     = net::ip::make_address_v4(vm["new_device_address"].as<std::string>());

        net::ip::udp::endpoint local_send_endpoint(local_addr, udp_host_send_port);
        net::ip::udp::endpoint local_receive_endpoint(local_addr, udp_host_receive_port);
        net::ip::udp::endpoint device_endpoint(current_destination_addr, udp_device_receive_port);

        net::io_context ctx;
        auto work = net::make_work_guard(ctx);
        vxsdr_thread context_thread([&ctx]() { ctx.run(); });

        net::ip::udp::socket sender_socket(ctx, local_send_endpoint);
        net::ip::udp::socket receiver_socket(ctx, local_receive_endpoint);

        std::cout << "Changing IPv4 address " << current_destination_addr.to_string() << " to " << new_destination_addr.to_string()
                  << " . . ." << std::endl;

        if (send_set_transport_addr_packet(sender_socket, device_endpoint, new_destination_addr)) {
            std::cout << "Change command sent successfully." << std::endl;
            std::cout << "Change the host interface to network settings that can reach the new address, then" << std::endl;
            std::cout << "run vxsdr_find to confirm that the address has been changed, and vxsdr_save_addr" << std::endl;
            std::cout << "to save the change to nonvolatile memory." << std::endl;
        } else {
            std::cerr << "Error changing address. Power cycle the device to retrun to the original address." << std::endl;
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