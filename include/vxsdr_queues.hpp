// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <thread>
#include <chrono>

// Confines the boost specifics to this file and adds utility functions for timeouts

// use std::atomic so queues are header-only
#ifndef BOOST_LOCKFREE_FORCE_STD_ATOMIC
#define BOOST_LOCKFREE_FORCE_STD_ATOMIC  (1)
#endif

#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>

template<typename Element> class spsc_queue : public boost::lockfree::spsc_queue<Element, boost::lockfree::fixed_sized<true>> {
    public:
        explicit spsc_queue<Element>(const size_t size)
                : boost::lockfree::spsc_queue<Element, boost::lockfree::fixed_sized<true>>(size) {};
        bool pop_or_timeout(Element& e, const unsigned timeout_us, const unsigned check_interval_us) {
            unsigned wait_time_us = 0;
            do {
                if (boost::lockfree::spsc_queue<Element, boost::lockfree::fixed_sized<true>>::pop(e)) {
                    return true;
                }
                if (check_interval_us > 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(check_interval_us));
                    wait_time_us += check_interval_us;
                }
            } while(wait_time_us < timeout_us);
            return false;
        }
        bool push_or_timeout(Element& e, const unsigned timeout_us, const unsigned check_interval_us) {
            unsigned wait_time_us = 0;
            do {
                if (boost::lockfree::spsc_queue<Element, boost::lockfree::fixed_sized<true>>::push(e)) {
                    return true;
                }
                if (check_interval_us > 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(check_interval_us));
                    wait_time_us += check_interval_us;
                }
            } while(wait_time_us < timeout_us);
            return false;
        }
};

template<typename Element> class mpmc_queue : public boost::lockfree::queue<Element> {
    public:
        explicit mpmc_queue<Element>(const size_t size) : boost::lockfree::queue<Element>(size) {};
        bool pop_or_timeout(Element& e, const unsigned timeout_us, const unsigned check_interval_us) {
            unsigned wait_time_us = 0;
            do {
                if ( boost::lockfree::queue<Element>::pop(e)) {
                    return true;
                }
                if (check_interval_us > 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(check_interval_us));
                    wait_time_us += check_interval_us;
                }
            } while(wait_time_us < timeout_us);
            return false;
        }
        bool push_or_timeout(Element& e, const unsigned timeout_us, const unsigned check_interval_us) {
            unsigned wait_time_us = 0;
            do {
                if ( boost::lockfree::queue<Element>::push(e)) {
                    return true;
                }
                if (check_interval_us > 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(check_interval_us));
                    wait_time_us += check_interval_us;
                }
            } while(wait_time_us < timeout_us);
            return false;
        }
        void reset() {
            Element e;
            while(boost::lockfree::queue<Element>::pop(e)) {};
        }
};