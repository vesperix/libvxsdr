// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "vxsdr_threads.hpp"

int set_thread_affinity(vxsdr_thread& thread, const unsigned cpunum);
int set_thread_priority_realtime(vxsdr_thread& thread, int priority);