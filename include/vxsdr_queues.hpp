// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <thread>

#ifdef VXSDR_USE_BOOST_QUEUE

// Confines the boost specifics to this file

// use std::atomic so queues are header-only
#ifndef BOOST_LOCKFREE_FORCE_STD_ATOMIC
#define BOOST_LOCKFREE_FORCE_STD_ATOMIC (1)
#endif

#include <boost/lockfree/spsc_queue.hpp>

template <typename Element>
class vxsdr_queue : public boost::lockfree::spsc_queue<Element, boost::lockfree::fixed_sized<true>> {
  public:
    explicit vxsdr_queue<Element>(const size_t size)
        : boost::lockfree::spsc_queue<Element, boost::lockfree::fixed_sized<true>>{size - 1} {};
};

//#endif // #ifdef VXSDR_USE_BOOST_QUEUE

//#ifdef VXSDR_USE_FOLLY_QUEUE

#else

#include "third_party/ProducerConsumerQueue.h"

template <typename Element>
class vxsdr_queue : public folly::ProducerConsumerQueue<Element> {
  public:
    explicit vxsdr_queue<Element>(const uint32_t size) : folly::ProducerConsumerQueue<Element>(size){};

    bool push(Element& e) { return folly::ProducerConsumerQueue<Element>::write(e); };
    size_t push(Element* p, size_t n_max) {
        size_t n_pushed = 0;
        while (n_pushed < n_max) {
            if (folly::ProducerConsumerQueue<Element>::write(*(p + n_pushed))) {
                n_pushed++;
            } else {
                break;
            }
        }
        return n_pushed;
    };
    bool pop(Element& e) { return folly::ProducerConsumerQueue<Element>::read(e); };
    size_t pop(Element* p, size_t n_max) {
        size_t n_popped = 0;
        while (n_popped < n_max) {
            if (folly::ProducerConsumerQueue<Element>::read(*(p + n_popped))) {
                n_popped++;
            } else {
                break;
            }
        }
        return n_popped;
    };
    size_t read_available() { return folly::ProducerConsumerQueue<Element>::sizeGuess(); };
    void reset() {
        Element e;
        while (folly::ProducerConsumerQueue<Element>::read(e))
            ;
    }
};

#endif // #ifdef VXSDR_USE_FOLLY_QUEUE

#ifdef VXSDR_USE_RIGTORP_QUEUE

#include "third_party/SPSCQueue.h"

template <typename Element>
class vxsdr_queue : public rigtorp::SPSCQueue<Element> {
  public:
    explicit vxsdr_queue<Element>(const size_t size) : rigtorp::SPSCQueue<Element>(size) {};

    bool push(Element& e) { return rigtorp::SPSCQueue<Element>::try_push(e); };
    bool push(Element&& e) { return rigtorp::SPSCQueue<Element>::try_push(e); };
    size_t push(Element* p, size_t n_max) {
        size_t n_pushed = 0;
        while (n_pushed < n_max) {
            if (push(*(p + n_pushed))) {
                n_pushed++;
            } else {
                break;
            }
        }
        return n_pushed;
    };
    bool pop(Element& e) {
        auto* p = rigtorp::SPSCQueue<Element>::front();
        if (p != nullptr) {
            e = *p;
            return true;
        }
        return false;
     };
    size_t pop(Element* p, size_t n_max) {
        size_t n_popped = 0;
        while (n_popped < n_max) {
            if (pop(*(p + n_popped))) {
                n_popped++;
            } else {
                break;
            }
        }
        return n_popped;
    };
    size_t read_available() { return rigtorp::SPSCQueue<Element>::size(); };
    void reset() {
        while (rigtorp::SPSCQueue<Element>::front() != nullptr) {
            rigtorp::SPSCQueue<Element>::pop();
        }
    }
};

#endif // #ifdef VXSDR_USE_RIGTORP_QUEUE