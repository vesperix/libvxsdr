// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cerrno>

#include "logging.hpp"
#include "thread_utils.hpp"
#include "vxsdr_imp.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_transport.hpp"
#include "vxsdr_threads.hpp"

/*! @file pcie_data_transport.cpp
    @brief Constructor, destructor, and utility functions for the @p vxsdr_pcie data transport classes.
*/

pcie_data_transport::pcie_data_transport(const std::map<std::string, int64_t>& settings,
                                         std::shared_ptr<pcie_dma_interface> pcie_iface,
                                         const unsigned granularity,
                                         const unsigned n_rx_subdevs,
                                         const unsigned max_samps_per_packet)
            : data_transport(granularity, n_rx_subdevs, max_samps_per_packet) {
    LOG_DEBUG("pcie data transport constructor entered");
    num_rx_subdevs = n_rx_subdevs;

    auto config = packet_transport::apply_transport_settings(settings, default_settings);

    pcie_if = pcie_iface;

    LOG_DEBUG("using transmit data buffer of {:d} packets", config["pcie_data_transport:tx_data_queue_packets"]);
    tx_data_queue = std::make_unique<spsc_queue<data_queue_element>>(config["pcie_data_transport:tx_data_queue_packets"]);

    for (unsigned i = 0; i < num_rx_subdevs; i++) {
        rx_data_queue.push_back(
            std::make_unique<spsc_queue<data_queue_element>>(config["pcie_data_transport:rx_data_queue_packets"]));
        rx_sample_queue.push_back(
            std::make_unique<spsc_queue<vxsdr::wire_sample>>(MAX_DATA_LENGTH_SAMPLES));
    }

    LOG_DEBUG("using {:d} receive data buffers of {:d} packets", num_rx_subdevs, config["pcie_data_transport:rx_data_queue_packets"]);
    LOG_DEBUG("using {:d} receive sample buffers of {:d} samples", num_rx_subdevs, MAX_DATA_LENGTH_SAMPLES);

    rx_state        = TRANSPORT_STARTING;
    receiver_thread = vxsdr_thread([this] { data_receive(); });

    if (config["pcie_data_transport:thread_affinity_offset"] >= 0 and config["pcie_data_transport:receiver_thread_affinity"] >= 0) {
        auto desired_affinity =
            config["pcie_data_transport:thread_affinity_offset"] + config["pcie_data_transport:receiver_thread_affinity"];
        if (set_thread_affinity(receiver_thread, desired_affinity) != 0) {
            LOG_ERROR("unable to set pcie data receiver thread affinity in pcie data transport constructor");
            throw std::runtime_error("unable to set pcie data receiver thread affinity in pcie data transport constructor");
        }
        LOG_DEBUG("pcie data receiver thread affinity set to cpu {:d}", desired_affinity);
    }
    if (config["pcie_data_transport:thread_priority"] >= 0) {
        if (set_thread_priority_realtime(receiver_thread, (int)config["pcie_data_transport:thread_priority"]) != 0) {
            LOG_ERROR("unable to set pcie data receiver thread realtime priority in pcie data transport constructor");
            throw std::runtime_error("unable to set pcie data receiver thread realtime priority in pcie data transport constructor");
        }
        LOG_DEBUG("pcie data receiver thread priority set to {:d}", config["pcie_data_transport:thread_priority"]);
    }

    tx_state      = TRANSPORT_STARTING;
    sender_thread = vxsdr_thread([this] { data_send(); });

    if (config["pcie_data_transport:thread_affinity_offset"] >= 0 and config["pcie_data_transport:sender_thread_affinity"] >= 0) {
        auto desired_affinity =
            config["pcie_data_transport:thread_affinity_offset"] + config["pcie_data_transport:sender_thread_affinity"];
        if (set_thread_affinity(sender_thread, desired_affinity) != 0) {
            LOG_ERROR("unable to set pcie data sender thread affinity in pcie data transport constructor");
            throw std::runtime_error("unable to set pcie data sender thread affinity in pcie data transport constructor");
        }
        LOG_DEBUG("pcie data sender thread affinity set to cpu {:d}", desired_affinity);
    }
    if (config["pcie_data_transport:thread_priority"] >= 0) {
        if (set_thread_priority_realtime(sender_thread, (int)config["pcie_data_transport:thread_priority"]) != 0) {
            LOG_ERROR("unable to set pcie data sender thread realtime priority in pcie data transport constructor");
            throw std::runtime_error("unable to set pcie data sender thread realtime priority in pcie data transport constructor");
        }
        LOG_DEBUG("pcie data sender thread priority set to {:d}", config["pcie_data_transport:thread_priority"]);
    }

    auto start_time = std::chrono::steady_clock::now();
    while (tx_state != TRANSPORT_READY or rx_state != TRANSPORT_READY) {
        std::this_thread::sleep_for(pcie_ready_wait);
        if ((std::chrono::steady_clock::now() - start_time) > pcie_ready_timeout) {
            LOG_ERROR("timeout waiting for transport ready in pcie data transport constructor");
            throw std::runtime_error("timeout waiting for transport ready in pcie data transport constructor");
        }
    }

    LOG_DEBUG("pcie data transport constructor complete");
}

pcie_data_transport::~pcie_data_transport() noexcept {
    LOG_DEBUG("pcie data transport destructor entered");

    tx_state = TRANSPORT_SHUTDOWN;
    sender_thread_stop_flag = true;

    // tx must shut down before rx since tx sends a final ack request to update stats
    LOG_DEBUG("joining pcie data sender thread");
    if (sender_thread.joinable()) {
        sender_thread.join();
    }

    rx_state = TRANSPORT_SHUTDOWN;
    receiver_thread_stop_flag = true;

    LOG_DEBUG("joining pcie data receiver thread");
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }

    if (log_stats_on_exit) {
        log_stats();
    }
    LOG_DEBUG("pcie data transport destructor complete");
}

size_t pcie_data_transport::packet_send(const packet& packet, int& error_code) {
    return pcie_if->data_send(&packet, packet.hdr.packet_size, error_code);
}

size_t pcie_data_transport::packet_receive(data_queue_element& packet, int& error_code) {
    packet.hdr = { 0, 0, 0, 0, 0, 0, 0 };
    return pcie_if->data_receive(&packet, sizeof(packet), error_code);
}