// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <thread>
#include <chrono>

#ifdef VXSDR_USE_BOOST_QUEUES

// Confines the boost specifics to this file

// use std::atomic so queues are header-only
#ifndef BOOST_LOCKFREE_FORCE_STD_ATOMIC
#define BOOST_LOCKFREE_FORCE_STD_ATOMIC  (1)
#endif

#include <boost/lockfree/spsc_queue.hpp>

template<typename Element> class vxsdr_queue : public boost::lockfree::spsc_queue<Element, boost::lockfree::fixed_sized<true>> {
    public:
        explicit vxsdr_queue<Element>(const size_t size) : boost::lockfree::spsc_queue<Element, boost::lockfree::fixed_sized<true>>{size} {};
};

#else

#include "third_party/ProducerConsumerQueue.h"

template<typename Element> class vxsdr_queue : public folly::ProducerConsumerQueue<Element> {
    public:
        explicit vxsdr_queue<Element>(const uint32_t size) : folly::ProducerConsumerQueue<Element>(size) {};

        bool push(Element& e) { return folly::ProducerConsumerQueue<Element>::write(e); };
        size_t push(Element* p, size_t max = 1) {
            size_t n_pushed = 0;
            while(folly::ProducerConsumerQueue<Element>::write(*(p + n_pushed)) and n_pushed < max) {
                n_pushed++;
            }
            return n_pushed;
        };
        bool pop(Element& e) { return folly::ProducerConsumerQueue<Element>::read(e); };
        size_t pop(Element* p, size_t max = 1) {
            size_t n_popped = 0;
            while(folly::ProducerConsumerQueue<Element>::read(*(p + n_popped)) and n_popped < max) {
                n_popped++;
            }
            return n_popped;
        };
        size_t read_available() { return folly::ProducerConsumerQueue<Element>::sizeGuess(); };
        void reset() { Element e; while(folly::ProducerConsumerQueue<Element>::read(e)); }
};

#endif
