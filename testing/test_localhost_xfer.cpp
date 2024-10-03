// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <atomic>
#include <chrono>
#include <complex>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <vector>
using namespace std::chrono_literals;

#include "thread_utils.hpp"
#include "vxsdr_net.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_queues.hpp"
#include "vxsdr_threads.hpp"

static constexpr unsigned data_packet_samples = 2400;
static constexpr unsigned data_packet_bytes   = 4 * data_packet_samples + 8;

static constexpr size_t tx_queue_length =   512;
static constexpr size_t rx_queue_length = 1'024;

static constexpr size_t sender_buffer_length   = 16;
static constexpr size_t consumer_buffer_length = 512;

const unsigned network_send_buffer_size    = 1'048'576;
const unsigned network_receive_buffer_size = 8'388'608;

static constexpr unsigned push_queue_wait_us = 100;
static constexpr unsigned pop_queue_wait_us  = 100;
static constexpr unsigned n_tries            = 10'000;  // ~1s timeout

static constexpr unsigned push_queue_interval_us = 0;
static constexpr unsigned pop_queue_interval_us  = 0;

static constexpr unsigned tx_net_wait_us = 10;

static constexpr unsigned udp_host_receive_port = 1030;
static constexpr unsigned udp_host_send_port    = 55123;

// NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static vxsdr_queue<data_queue_element> tx_queue{tx_queue_length};
static vxsdr_queue<data_queue_element> rx_queue{rx_queue_length};

std::mutex console_mutex;

std::atomic_bool sender_thread_stop_flag   = false;
std::atomic_bool receiver_thread_stop_flag = false;

void tx_producer(const size_t n_items, double& push_rate) {
    auto t0 = std::chrono::steady_clock::now();

    size_t i = 0;

    for (i = 0; i < n_items; i++) {
        data_queue_element p;
        p.hdr                  = {PACKET_TYPE_TX_SIGNAL_DATA, 0, 0, 0, 0, data_packet_bytes, 0};
        p.hdr.sequence_counter = i % (UINT16_MAX + 1);

        unsigned n_try = 0;

        while (not tx_queue.push(p) and n_try < n_tries) {
            std::this_thread::sleep_for(std::chrono::microseconds(push_queue_wait_us));
            n_try++;
        }
        if (n_try >= n_tries) {
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cerr << "producer: timeout waiting for push" << std::endl;
            break;
        }

        if constexpr (push_queue_interval_us > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(push_queue_interval_us));
        }
    }

    auto t1                         = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = t1 - t0;
    push_rate                       = (data_packet_samples * (double)i / d.count());
    std::lock_guard<std::mutex> guard(console_mutex);
    std::cout << "producer: " << i << " packets pushed in " << d.count() << " sec: " << push_rate << " samples/s" << std::endl;
}

void tx_net_sender(net::ip::udp::socket& sender_socket) {
    net_error_code::error_code err;
    sender_socket.set_option(net::socket_base::send_buffer_size((int)network_send_buffer_size), err);
    if (err) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cerr << "cannot set network send buffer size: " << err.message() << std::endl;
        return;
    }
    net::socket_base::send_buffer_size option;
    sender_socket.get_option(option, err);
    if (err) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cerr << "cannot get network send buffer size: " << err.message() << std::endl;
        return;
    }
    auto network_buffer_size = option.value();
    if (network_buffer_size != network_send_buffer_size) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cerr << "cannot set network send buffer size: got " << network_buffer_size << std::endl;
        return;
    }

    unsigned buffer_read_size = sender_buffer_length;
    static std::array<data_queue_element, sender_buffer_length> data_buffer;
    net::socket_base::message_flags flags = 0;
    while (not sender_thread_stop_flag) {
        unsigned n_popped = tx_queue.pop(data_buffer.data(), buffer_read_size);
        for (unsigned i = 0; i < n_popped; i++) {
            if (data_buffer[i].hdr.packet_size == 0) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cerr << "tx queue error: zero size packet popped" << std::endl;
                return;
            }
            size_t bytes = sender_socket.send(net::buffer(&data_buffer[i], data_buffer[i].hdr.packet_size), flags, err);
            if (err) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cerr << "packet send error: " << err.message() << std::endl;
                return;
            }
            if (bytes != data_buffer[i].hdr.packet_size) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cerr << "send packet size error" << std::endl;
                return;
            }
        }
        if constexpr (tx_net_wait_us > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(tx_net_wait_us));
        }
    }
}

void rx_net_receiver(net::ip::udp::socket& receiver_socket) {
    uint16_t expected_seq = 0;

    net_error_code::error_code err;
    receiver_socket.set_option(net::ip::udp::socket::reuse_address(true), err);
    if (err) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cerr << "cannot set reuse address option on receive socket: " << err.message() << std::endl;
        return;
    }
    receiver_socket.set_option(net::socket_base::receive_buffer_size((int)network_receive_buffer_size), err);
    if (err) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cerr << "cannot set network receive buffer size: " << err.message() << std::endl;
        return;
    }
    net::socket_base::receive_buffer_size option;
    receiver_socket.get_option(option, err);
    if (err) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cerr << "cannot get network receive buffer size: " << err.message() << std::endl;
        return;
    }
    auto network_buffer_size = option.value();
    if (network_buffer_size != network_receive_buffer_size) {
        std::lock_guard<std::mutex> guard(console_mutex);
        std::cerr << "cannot set network receive buffer size: got " << network_buffer_size << std::endl;
        return;
    }

    static data_queue_element recv_buffer;

    while (not receiver_thread_stop_flag) {
        net::socket_base::message_flags flags = 0;
        auto bytes_in_packet                  = receiver_socket.receive(net::buffer(&recv_buffer, sizeof(recv_buffer)), flags, err);
        if (err) {
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cerr << "packet receive error: " << err.message() << std::endl;
            return;
        }
        if (bytes_in_packet > 0 and not receiver_thread_stop_flag) {
            if (bytes_in_packet != recv_buffer.hdr.packet_size) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cerr << "packet receive size error" << std::endl;
                return;
            }
            if (recv_buffer.hdr.sequence_counter != expected_seq) {
                std::cerr << "receiver: sequence error: "
                        << std::setw(6) << recv_buffer.hdr.sequence_counter << " "
                        << std::setw(6) << expected_seq
                        << std::setw(6) << recv_buffer.hdr.sequence_counter - expected_seq << std::endl;
                expected_seq = recv_buffer.hdr.sequence_counter;
            }
            expected_seq++;
            if (not rx_queue.push(recv_buffer)) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cerr << "receive packet push error" << std::endl;
                return;
            }
        }
    }
}

void rx_consumer(const size_t n_items, double& pop_rate, unsigned& seq_errors) {
    std::vector<data_queue_element> p{consumer_buffer_length};

    auto t0 = std::chrono::steady_clock::now();

    size_t i              = 0;
    uint16_t expected_seq = 0;

    while (i < n_items) {
        unsigned n_try  = 0;
        size_t n_popped = 0;

        while (n_popped == 0 and n_try < n_tries) {
            n_popped = rx_queue.pop(&p.front(), consumer_buffer_length);
            if (n_popped == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(pop_queue_wait_us));
                n_try++;
            }
        }
        if (n_try >= n_tries) {
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cerr << "consumer: timeout waiting for pop" << std::endl;
            break;
        }

        for (size_t j = 0; j < n_popped; j++) {
            if (p[j].hdr.sequence_counter != expected_seq) {
                std::cerr << "consumer: sequence error: "
                        << std::setw(6) << p[j].hdr.sequence_counter << " "
                        << std::setw(6) << expected_seq
                        << std::setw(6) << p[j].hdr.sequence_counter - expected_seq << std::endl;
                expected_seq = p[j].hdr.sequence_counter;
                seq_errors++;
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
    pop_rate                        = (data_packet_samples * (double)i / d.count());
    std::lock_guard<std::mutex> guard(console_mutex);
    std::cout << "consumer: " << i << " packets popped in " << d.count() << " sec: " << pop_rate << " samples/s with " << seq_errors
              << " sequence errors" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: test_localhost_xfer <number of seconds of data> <minimum sample rate>" << std::endl;
        return -1;
    }

    std::vector<int> thread_priority = {1, 1};
    std::vector<int> thread_affinity = {0, 1};

    std::cout << "testing speed of data transfer through localhost" << std::endl;

    double n_seconds    = std::strtod(argv[1], nullptr);
    double minimum_rate = std::strtod(argv[2], nullptr);

    size_t n_items = std::ceil(n_seconds * minimum_rate / MAX_DATA_LENGTH_SAMPLES);

    bool pass = true;
    try {
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
                std::cout << "failed" << std::endl;
                exit(-1);
            }
        }
        if (thread_priority.at(0) >= 0) {
            if (set_thread_priority_realtime(tx_thread, thread_priority.at(0)) != 0) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cout << "error setting tx thread priority" << std::endl;
                std::cout << "failed" << std::endl;
                exit(-1);
            }
        }

        if (thread_affinity.at(1) >= 0) {
            if (set_thread_affinity(rx_thread, thread_affinity.at(1)) != 0) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cout << "error setting rx thread affinity" << std::endl;
                std::cout << "failed" << std::endl;
                exit(-1);
            }
        }
        if (thread_affinity.at(1) >= 0) {
            if (set_thread_affinity(rx_thread, thread_affinity.at(1)) != 0) {
                std::lock_guard<std::mutex> guard(console_mutex);
                std::cout << "error setting ex thread affinity" << std::endl;
                std::cout << "failed" << std::endl;
                exit(-1);
            }
        }

        double pop_rate     = 0;
        double push_rate    = 0;
        unsigned seq_errors = 0;

        auto consumer_thread = vxsdr_thread(&rx_consumer, n_items, std::ref(pop_rate), std::ref(seq_errors));
        // brief wait to ensure consumer thread is ready before starting producer
        std::this_thread::sleep_for(10ms);
        auto producer_thread = vxsdr_thread(&tx_producer, n_items, std::ref(push_rate));

        producer_thread.join();
        consumer_thread.join();

        sender_thread_stop_flag   = true;
        receiver_thread_stop_flag = true;

        net_error_code::error_code err;
        receiver_socket.shutdown(net::ip::udp::socket::shutdown_receive, err);
        if (err and err != net_error_code_types::not_connected) {
            // the not connected error is expected since it's a UDP socket
            std::lock_guard<std::mutex> guard(console_mutex);
            std::cerr << "receiver socket shutdown: " << err.message() << std::endl;
        }

        if (tx_thread.joinable()) {
            tx_thread.join();
        }
        if (rx_thread.joinable()) {
            rx_thread.join();
        }
        ctx.stop();

        pass = (pop_rate > minimum_rate) and (push_rate > minimum_rate) and seq_errors == 0;

        std::lock_guard<std::mutex> guard(console_mutex);
        std::cout << "minimum rate = " << std::fixed << std::setprecision(2)
                  << 1e-6 * std::min(push_rate, pop_rate) << std::endl;
        std::cout << (pass ? "passed" : "failed") << std::endl;
    } catch (std::exception& e) {
        std::cerr << "exception caught: " << e.what() << std::endl;
        return 2;
    }
    return (pass ? 0 : 1);
}
