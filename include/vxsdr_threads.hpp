// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <thread>

// use jthread if available

#ifdef __cpp_lib_jthread
using vxsdr_thread = std::jthread;
#else
using vxsdr_thread = std::thread;
#endif