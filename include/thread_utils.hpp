// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <thread>

int set_thread_affinity(std::thread& thread, const unsigned cpunum);
int set_thread_priority_realtime(std::thread& thread, int priority);