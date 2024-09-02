// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef VXSDR_ENABLE_PCIE

#include "vxsdr_transport.hpp"

/*! @file pcie_command_transport.cpp
    @brief Constructor, destructor, and utility functions for the @p vxsdr_pcie command transport classes.
*/

pcie_command_transport::pcie_command_transport(const std::map<std::string, int64_t>& settings, std::shared_ptr<pcie_dma_interface> pcie_iface) {
    LOG_DEBUG("pcie command transport constructor entered");

    auto config = packet_transport::apply_transport_settings(settings, default_settings);

    pcie_if = pcie_iface;

    rx_state        = TRANSPORT_STARTING;
    receiver_thread = vxsdr_thread([this] { command_receive(); });

    tx_state      = TRANSPORT_STARTING;
    sender_thread = vxsdr_thread([this] { command_send(); });

    auto start_time = std::chrono::steady_clock::now();
    while (tx_state != TRANSPORT_READY or rx_state != TRANSPORT_READY) {
        std::this_thread::sleep_for(pcie_ready_wait);
        if ((std::chrono::steady_clock::now() - start_time) > pcie_ready_timeout) {
            LOG_ERROR("timeout waiting for transport ready in pcie command transport constructor");
            throw std::runtime_error("timeout waiting for transport ready in pcie command transport constructor");
        }
    }
}

pcie_command_transport::~pcie_command_transport() noexcept {
    LOG_DEBUG("pcie command transport destructor entered");

    rx_state = TRANSPORT_SHUTDOWN;

    receiver_thread_stop_flag = true;

    LOG_DEBUG("joining pcie command receiver thread");
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }
    LOG_DEBUG("joining pcie command sender thread");
    tx_state = TRANSPORT_SHUTDOWN;
    sender_thread_stop_flag = true;
    if (sender_thread.joinable()) {
        sender_thread.join();
    }

    if (log_stats_on_exit) {
        log_stats();
    }
    LOG_DEBUG("pcie command transport destructor complete");
}

size_t pcie_command_transport::packet_send(const packet& packet, int& error_code) {
    return pcie_if->command_send(&packet, packet.hdr.packet_size, error_code);
}

size_t pcie_command_transport::packet_receive(command_queue_element& packet, int& error_code) {
    packet.hdr = { 0, 0, 0, 0, 0, 0, 0 };
    return pcie_if->command_receive(&packet, sizeof(packet), error_code);
}

#endif // #ifdef VXSDR_ENABLE_PCIE