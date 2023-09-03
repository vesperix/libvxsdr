// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <chrono>
#include <complex>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <array>

#include "vxsdr_queues.hpp"
#include "vxsdr_packets.hpp"

static constexpr size_t queue_length = 262'144;

static constexpr unsigned push_queue_timeout_us = 1000000;
static constexpr unsigned pop_queue_timeout_us  = 1000000;

static constexpr unsigned push_queue_sleep_us = 100;
static constexpr unsigned pop_queue_sleep_us  = 100;

static constexpr unsigned push_queue_interval_us = 0;
static constexpr unsigned pop_queue_interval_us  = 0;

// NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static spsc_queue<data_queue_element> queue{queue_length};

std::mutex console_mutex;

void producer(const size_t n_items, double& push_rate) {
    auto t0 = std::chrono::steady_clock::now();

    for (size_t i = 0; i < n_items; i++) {
        data_queue_element p;
        p.hdr.sequence_counter = i % (UINT16_MAX + 1);

        if (not queue.push_or_timeout(p, push_queue_timeout_us, push_queue_sleep_us)) {
            std::cout << "producer: timeout waiting for push" << std::endl;
            exit(-1);
        }
        if constexpr (push_queue_interval_us > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(push_queue_interval_us));
        }
    }

    auto t1                         = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = t1 - t0;
    push_rate                       = (MAX_DATA_LENGTH_SAMPLES * (double)n_items / d.count());
    std::lock_guard<std::mutex> guard(console_mutex);
    std::cout << "producer: " << n_items << " packets pushed in " << d.count() << " sec: " << push_rate << " samples/s"
              << std::endl;
}

void consumer(const size_t n_items, double& pop_rate) {
    constexpr size_t buffer_size = 512;
    auto t0 = std::chrono::steady_clock::now();

    size_t i = 0;

    while (i < n_items) {
        static std::array<data_queue_element, buffer_size> p;

        size_t n_popped = queue.pop_or_timeout(&p.front(), buffer_size, pop_queue_timeout_us, pop_queue_sleep_us);

        if (n_popped == 0) {
            std::cout << "consumer: timeout waiting for pop" << std::endl;
            exit(-1);
        }

        for (size_t j = 0; j < n_popped; j++) {
            if (p[j].hdr.sequence_counter != i++ % (UINT16_MAX + 1)) {
                std::cout << "consumer: data error" << std::endl;
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
    pop_rate = (MAX_DATA_LENGTH_SAMPLES * (double)n_items / d.count());
    std::cout << "consumer: " << n_items << " packets popped in " << d.count() << " sec: " << pop_rate << " samples/s" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: test_spsc_queue <number of seconds of data> <minimum sample rate>" << std::endl;
        return -1;
    }

    std::cout << "testing speed of queue used for data packets" << std::endl;

    double n_seconds    = std::strtod(argv[1], nullptr);
    double minimum_rate = std::strtod(argv[2], nullptr);

    size_t n_items = std::ceil(n_seconds * minimum_rate / MAX_DATA_LENGTH_SAMPLES);

    queue.reset();

    double pop_rate = 0;
    double push_rate = 0;

    auto consumer_thread = std::thread(&consumer, n_items, std::ref(pop_rate));
    auto producer_thread = std::thread(&producer, n_items, std::ref(push_rate));

    producer_thread.join();
    consumer_thread.join();

    bool pass = (pop_rate > minimum_rate) and (push_rate > minimum_rate);

    std::cout << (pass ? "passed" : "failed") << std::endl;

    return (pass ? 0 : 1);
}
