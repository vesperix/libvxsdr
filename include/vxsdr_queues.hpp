// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <thread>
#include <chrono>

// Confines the boost specifics to this file

// use std::atomic so queues are header-only
#ifndef BOOST_LOCKFREE_FORCE_STD_ATOMIC
#define BOOST_LOCKFREE_FORCE_STD_ATOMIC  (1)
#endif

#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>

template<typename Element> class data_queue : public boost::lockfree::spsc_queue<Element, boost::lockfree::fixed_sized<true>> {
    public:
        explicit data_queue<Element>(const size_t size) : boost::lockfree::spsc_queue<Element, boost::lockfree::fixed_sized<true>>{size} {};
};

template<typename Element> class cmd_queue : public boost::lockfree::queue<Element> {
    public:
        explicit cmd_queue<Element>(const size_t size) : boost::lockfree::queue<Element>{size} {};
        void reset() {
            Element e;
            while(boost::lockfree::queue<Element>::pop(e)) {};
        }
};