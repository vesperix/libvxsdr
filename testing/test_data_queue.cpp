// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <chrono>
#include <complex>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <vector>

#include "vxsdr_packets.hpp"
#include "vxsdr_queues.hpp"
#include "vxsdr_threads.hpp"

static constexpr size_t queue_length = 512;

static constexpr unsigned max_data_packet_bytes = 8192;
static constexpr unsigned max_data_payload_bytes = max_data_packet_bytes - sizeof(packet_header);
static constexpr unsigned max_data_length_samples = max_data_payload_bytes / 4;

static constexpr unsigned push_queue_wait_us = 100;
static constexpr unsigned pop_queue_wait_us  = 100;
static constexpr unsigned n_tries            = 10'000;  // ~1s timeout

static constexpr unsigned push_queue_interval_us = 0;
static constexpr unsigned pop_queue_interval_us  = 0;

// NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

auto queue = std::make_unique<vxsdr_queue<data_queue_element>>(queue_length);

std::mutex console_mutex;

void producer(const size_t n_items, double& push_rate) {
    auto t0 = std::chrono::steady_clock::now();

    for (size_t i = 0; i < n_items; i++) {
        data_queue_element p;
        p.hdr                  = {PACKET_TYPE_TX_SIGNAL_DATA, 0, 0, 0, 0, max_data_packet_bytes, 0};
        p.hdr.sequence_counter = i % (UINT16_MAX + 1);

        std::memset((void*)&p.data, 0xFF, max_data_payload_bytes);

        unsigned n_try = 0;

        while (not queue->push(p) and n_try < n_tries) {
            std::this_thread::sleep_for(std::chrono::microseconds(push_queue_wait_us));
            n_try++;
        }
        if (n_try >= n_tries) {
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cout << "producer: timeout waiting for push" << std::endl;
            std::cout << "failed" << std::endl;
            exit(-1);
        }

        if constexpr (push_queue_interval_us > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(push_queue_interval_us));
        }
    }

    auto t1                         = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = t1 - t0;
    push_rate                       = (max_data_length_samples * (double)n_items / d.count());
    std::lock_guard<std::mutex> guard(console_mutex);
    std::cout << "producer: " << n_items << " packets pushed in " << d.count() << " sec: " << push_rate << " samples/s"
              << std::endl;
}

void consumer(const size_t n_items, double& pop_rate) {
    constexpr size_t buffer_size = 512;
    auto t0                      = std::chrono::steady_clock::now();

    size_t i = 0;

    while (i < n_items) {
        static std::array<data_queue_element, buffer_size> p;

        unsigned n_try  = 0;
        size_t n_popped = 0;

        while (n_popped == 0 and n_try < n_tries) {
            n_popped = queue->pop(p.data(), buffer_size);
            if (n_popped == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(pop_queue_wait_us));
                n_try++;
            }
        }
        if (n_try >= n_tries) {
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cout << "consumer: timeout waiting for pop" << std::endl;
            std::cout << "failed" << std::endl;
            exit(-1);
        }

        for (size_t j = 0; j < n_popped; j++) {
            if (p[j].hdr.packet_size == 0) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cout << "consumer: zero size packet" << std::endl;
                std::cout << "failed" << std::endl;
                exit(-1);
            }
            if (p[j].hdr.sequence_counter != i++ % (UINT16_MAX + 1)) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cout << "consumer: sequence error" << std::endl;
                std::cout << "failed" << std::endl;
                exit(-1);
            }
        }
        if constexpr (pop_queue_interval_us > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(pop_queue_interval_us));
        }
    }

    auto t1                         = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = t1 - t0;
    std::lock_guard<std::mutex> guard(console_mutex);
    pop_rate = (max_data_length_samples * (double)n_items / d.count());
    std::cout << "consumer: " << n_items << " packets popped in " << d.count() << " sec: " << pop_rate << " samples/s" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: test_data_queue <number of seconds of data> <minimum sample rate>" << std::endl;
        return -1;
    }

    std::cout << "testing speed of queue used for data packets with " << max_data_length_samples << " samples/packet" << std::endl;

    double n_seconds    = std::strtod(argv[1], nullptr);
    double minimum_rate = std::strtod(argv[2], nullptr);

    size_t n_items = std::ceil(n_seconds * minimum_rate / max_data_length_samples);

    queue->reset();

    double pop_rate  = 0;
    double push_rate = 0;

    auto consumer_thread = vxsdr_thread(&consumer, n_items, std::ref(pop_rate));
    auto producer_thread = vxsdr_thread(&producer, n_items, std::ref(push_rate));

    producer_thread.join();
    consumer_thread.join();

    std::cout << "minimum rate = " << std::fixed << std::setprecision(2)
                << 1e-6 * std::min(push_rate, pop_rate) << std::endl;

    bool pass = (pop_rate > minimum_rate) and (push_rate > minimum_rate);
    std::cout << (pass ? "passed" : "failed") << std::endl;

    return (pass ? 0 : 1);
}
