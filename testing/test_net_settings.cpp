// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <complex>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <vector>
#include <atomic>
#include <chrono>
using namespace std::chrono_literals;

// disable exceptions
#define FMT_EXCEPTIONS              (0)
// force header only
#define FMT_HEADER_ONLY

#include <spdlog/fmt/fmt.h>

#include "vxsdr_queues.hpp"
#include "vxsdr_net.hpp"
#include "vxsdr_packets.hpp"
#include "thread_utils.hpp"
#include "vxsdr_threads.hpp"

static constexpr size_t tx_queue_length =     512;
static constexpr size_t rx_queue_length =     512;

const unsigned network_send_buffer_size     =   262'144;
const unsigned network_receive_buffer_size  = 8'388'608;

static constexpr unsigned udp_host_receive_port =  1030;
static constexpr unsigned udp_host_send_port    = 55123;

static constexpr unsigned push_queue_wait_us = 100;
static constexpr unsigned pop_queue_wait_us  = 100;
static constexpr unsigned n_tries = 10'000; // ~1s timeout

static constexpr unsigned push_queue_interval_us = 0;
static constexpr unsigned pop_queue_interval_us  = 0;

static unsigned data_buffer_chunk = 32;
static unsigned tx_net_wait_us = 50;

// NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static vxsdr_queue<data_queue_element> tx_queue{tx_queue_length};
static vxsdr_queue<data_queue_element> rx_queue{rx_queue_length};

std::mutex console_mutex;

std::atomic_bool sender_thread_stop_flag = false;
std::atomic_bool receiver_thread_stop_flag = false;

void tx_producer(const size_t n_items, double& push_rate) {
    auto t0 = std::chrono::steady_clock::now();

    static data_queue_element p;
    size_t i = 0;

    for (i = 0; i < n_items; i++) {
        p.hdr.sequence_counter = i % (UINT16_MAX + 1UL);
        p.hdr.packet_size = sizeof(largest_data_packet);

        unsigned n_try = 0;

        while (not tx_queue.push(p) and n_try < n_tries) {
            std::this_thread::sleep_for(std::chrono::microseconds(push_queue_wait_us));
            n_try++;
        }
        if (n_try >= n_tries) {
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cout << "producer: timeout waiting for push" << std::endl;
            break;
        }

        if constexpr (push_queue_interval_us > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(push_queue_interval_us));
        }
    }

    auto t1                         = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = t1 - t0;
    push_rate                       = (MAX_DATA_LENGTH_SAMPLES * (double)i / d.count());
}

void tx_net_sender(net::ip::udp::socket& sender_socket)
{
    net_error_code::error_code err;
    sender_socket.set_option(net::socket_base::send_buffer_size((int)network_send_buffer_size), err);
    if (err) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cout << "cannot set network send buffer size: " << err.message() << std::endl;
        exit(1);
    }
    net::socket_base::send_buffer_size option;
    sender_socket.get_option(option, err);
    if (err) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cout << "cannot get network send buffer size: " << err.message() << std::endl;
        exit(2);
    }
    auto network_buffer_size = option.value();
    if (network_buffer_size != network_send_buffer_size) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cout << "cannot set network send buffer size: got " << network_buffer_size << std::endl;
        exit(3);
    }

    static constexpr unsigned data_buffer_size = 256;
    static std::array<data_queue_element, data_buffer_size> data_buffer;

    if (data_buffer_chunk > data_buffer_size) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cout << "packet pop chunk size must be less than " << data_buffer_size << std::endl;
        exit(4);
    }

    net::socket_base::message_flags flags = 0;
    while (not sender_thread_stop_flag) {
        unsigned n_popped = tx_queue.pop(&data_buffer.front(), data_buffer_chunk);
        for (unsigned i = 0; i < n_popped; i++) {
            size_t bytes = sender_socket.send(net::buffer(&data_buffer[i], data_buffer[i].hdr.packet_size), flags, err);
            if (err) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cout << "packet send error: " <<  err.message() << std::endl;
                return;
            }
            if (bytes != data_buffer[i].hdr.packet_size) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cout << "send packet size error" << std::endl;
                return;
            }
        }
        if (tx_net_wait_us > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(tx_net_wait_us));
        }
    }
}

void rx_net_receiver(net::ip::udp::socket& receiver_socket)
{
    net_error_code::error_code err;
    receiver_socket.set_option(net::ip::udp::socket::reuse_address(true), err);
    if (err) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cout << "cannot set reuse address option on receive socket: " << err.message() << std::endl;
        exit(5);
    }
    receiver_socket.set_option(net::socket_base::receive_buffer_size((int)network_receive_buffer_size), err);
    if (err) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cout << "cannot set network receive buffer size: " << err.message() << std::endl;
        exit(6);
    }
    net::socket_base::receive_buffer_size option;
    receiver_socket.get_option(option, err);
    if (err) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cout << "cannot get network receive buffer size: " << err.message() << std::endl;
        exit(7);
    }
    auto network_buffer_size = option.value();
    if (network_buffer_size != network_receive_buffer_size) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cout << "cannot set network receive buffer size: got " << network_buffer_size << std::endl;
        exit(8);
    }

    static data_queue_element recv_buffer;

    while (not receiver_thread_stop_flag) {
        net::socket_base::message_flags flags = 0;
        auto bytes_in_packet = receiver_socket.receive(net::buffer(&recv_buffer, sizeof(recv_buffer)), flags, err);
        if (err) {
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cout << "packet receive error: " <<  err.message() << std::endl;
            return;
        }
        if (bytes_in_packet > 0 and not receiver_thread_stop_flag) {
            if (bytes_in_packet != recv_buffer.hdr.packet_size) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cout << "packet receive size error" << std::endl;
                return;
            }
            if (not rx_queue.push(recv_buffer)) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cout << "receive packet push error" << std::endl;
                return;
            }
        }
    }
}

void rx_consumer(const size_t n_items, double& pop_rate, unsigned& seq_errors) {
    constexpr size_t buffer_size = 512;
    auto t0 = std::chrono::steady_clock::now();

    size_t i = 0;
    uint16_t expected_seq = 0;

    while (i < n_items) {
        static std::array<data_queue_element, buffer_size> p;

        unsigned n_try = 0;
        size_t n_popped = 0;

        while (n_popped == 0 and n_try < n_tries) {
            n_popped = rx_queue.pop(&p.front(), buffer_size);
            if (n_popped == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(pop_queue_wait_us));
                n_try++;
            }
        }
        if (n_try >= n_tries) {
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cout << "consumer: timeout waiting for pop" << std::endl;
            break;
        }

        for (size_t j = 0; j < n_popped; j++) {
            if (p[j].hdr.sequence_counter != expected_seq) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cout << "consumer: sequence error: " << p[j].hdr.sequence_counter << " " << expected_seq
                        << " " << (p[j].hdr.sequence_counter - expected_seq) << std::endl;
            }
            if (p[j].hdr.sequence_counter != expected_seq) {
                seq_errors++;
                expected_seq = p[j].hdr.sequence_counter;
            }
            i++;
            expected_seq++;
        }

        if constexpr (pop_queue_interval_us > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(pop_queue_interval_us));
        }
    }

    auto t1                         = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = t1 - t0;
    pop_rate = (MAX_DATA_LENGTH_SAMPLES * (double)i / d.count());
}

void run_test(const double n_seconds, const double minimum_rate, const unsigned wait_us, const unsigned chunk_size,
             double& push_rate, double& pop_rate, unsigned& seq_errors) {

    data_buffer_chunk = chunk_size;
    tx_net_wait_us = wait_us;

    std::vector<int> thread_priority = { 1, 1};
    std::vector<int> thread_affinity = { 0, 1};

    size_t n_items = std::ceil(n_seconds * minimum_rate / MAX_DATA_LENGTH_SAMPLES);

    auto localhost_addr = net::ip::address_v4::from_string("127.0.0.1");

    net::ip::udp::endpoint local_send_endpoint(localhost_addr, udp_host_send_port);
    net::ip::udp::endpoint local_receive_endpoint(localhost_addr, udp_host_receive_port);

    net::io_context ctx;
    net::ip::udp::socket sender_socket(ctx, local_send_endpoint);
    net::ip::udp::socket receiver_socket(ctx, local_receive_endpoint);

    sender_socket.connect(local_receive_endpoint);
    receiver_socket.connect(local_send_endpoint);

    auto tx_thread = vxsdr_thread(&tx_net_sender, std::ref(sender_socket));
    auto rx_thread = vxsdr_thread(&rx_net_receiver, std::ref(receiver_socket));

    if (thread_affinity.at(0) >= 0) {
        if (set_thread_affinity(tx_thread, thread_affinity.at(0)) != 0) {
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cout << "error setting tx thread affinity" << std::endl;
            exit(-1);
        }
    }
    if (thread_priority.at(0) >= 0) {
        if (set_thread_priority_realtime(tx_thread, thread_priority.at(0)) != 0) {
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cout << "error setting tx thread priority" << std::endl;
            exit(-1);
        }
    }

    if (thread_affinity.at(1) >= 0) {
        if (set_thread_affinity(rx_thread, thread_affinity.at(1)) != 0) {
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cout << "error setting rx thread affinity" << std::endl;
            exit(-1);
        }
    }
    if (thread_affinity.at(1) >= 0) {
        if (set_thread_affinity(rx_thread, thread_affinity.at(1)) != 0) {
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cout << "error setting ex thread affinity" << std::endl;
            exit(-1);
        }
    }

    pop_rate = 0;
    push_rate = 0;
    seq_errors = 0;

    auto consumer_thread = vxsdr_thread(&rx_consumer, n_items, std::ref(pop_rate), std::ref(seq_errors));
    // brief wait to ensure consumer thread is ready before starting producer
    std::this_thread::sleep_for(10ms);
    auto producer_thread = vxsdr_thread(&tx_producer, n_items, std::ref(push_rate));

    producer_thread.join();
    consumer_thread.join();

    sender_thread_stop_flag = true;
    receiver_thread_stop_flag = true;

    net_error_code::error_code err;
    receiver_socket.shutdown(net::ip::udp::socket::shutdown_receive, err);
    if (err and err != net_error_code_types::not_connected) {
        // the not connected error is expected since it's a UDP socket
        std::cout << "receiver socket shutdown: " << err.message() << std::endl;
    }

    if(tx_thread.joinable()) {
        tx_thread.join();
    }
    if(rx_thread.joinable()) {
        rx_thread.join();
    }
    ctx.stop();
}


int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "usage: test_net_settings <number of seconds of data> <minimum sample rate> <chunk_delay_us> <packet_pop_chunk_size>" << std::endl;
        return -1;
    }
    double n_seconds    = atof(argv[1]);
    double min_rate     = atof(argv[2]);
    unsigned delay_us   = atoi(argv[3]);
    unsigned chunk_size = atoi(argv[4]);

    double push_rate = 0, pop_rate = 0;
    unsigned seq_errors = 0;
    bool pass = true;
    try {
        run_test(n_seconds, min_rate, delay_us, chunk_size, push_rate, pop_rate, seq_errors);
        std::cout << fmt::format(" {:10.1f} {:10d} {:10d} {:12.2e} {:12.2e} {:12.2e} {:10d}",
                                n_seconds, delay_us, chunk_size, min_rate, push_rate, pop_rate, seq_errors) << std::endl;

        pass = (seq_errors > 0) || (push_rate < min_rate) || (pop_rate < min_rate);
    } catch (std::exception& e) {
        std::cerr << "exception caught: " << e.what() << std::endl;
        return 2;
    }
    return (pass ? 0 : 1);
}