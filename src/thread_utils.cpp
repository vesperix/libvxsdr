// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <thread>

#ifdef VXSDR_TARGET_LINUX
#include <bits/types/struct_sched_param.h>
#include <pthread.h>
#include <sched.h>

int set_thread_affinity(std::thread& thread, const unsigned cpunum) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpunum, &set);
    // returns 0 on success
    return pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &set);
}

int set_thread_priority_realtime(std::thread& thread, int priority) {
    const int min_priority = sched_get_priority_min(SCHED_RR);
    const int max_priority = sched_get_priority_max(SCHED_RR);
    if (priority > max_priority) {
        priority = max_priority;
    } else if (priority < min_priority) {
        priority = min_priority;
    }
    int policy = SCHED_RR;
    struct sched_param param {};

    param.sched_priority = priority;
    // returns 0 on success
    return pthread_setschedparam(thread.native_handle(), policy, &param);
}
#endif  //  VXSDR_TARGET_LINUX

#ifdef VXSDR_TARGET_WINDOWS
#include <windows.h>

int set_thread_affinity(std::thread& thread, const unsigned cpunum) {
    if (SetThreadAffinityMask(thread.native_handle(), 1ULL << cpunum) != 0) {
        return 0;
    }
    return -1;
}

int set_thread_priority_realtime(std::thread& thread, int priority) {
    // Windows has an ugly set of allowed priorities; try to do something reasonable
    constexpr int min_priority = -7;
    constexpr int max_priority = 6;
    if (priority > max_priority) {
        priority = THREAD_PRIORITY_TIME_CRITICAL;
    } else if (priority < min_priority) {
        priority = THREAD_PRIORITY_IDLE;
    }
    if (SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)) {
        if (SetThreadPriority(thread.native_handle(), priority)) {
            return 0;
        }
    }
    return -1;
}
#endif  // VXSDR_TARGET_WINDOWS

#ifdef VXSDR_TARGET_MACOS
#include <pthread.h>
#include <sched.h>

// macOS does not support thread affinity, so this is a no-op
int set_thread_affinity(std::thread& thread, const unsigned cpunum) {
    return 0;
}

int set_thread_priority_realtime(std::thread& thread, int priority) {
    const int min_priority = sched_get_priority_min(SCHED_RR);
    const int max_priority = sched_get_priority_max(SCHED_RR);
    if (priority > max_priority) {
        priority = max_priority;
    } else if (priority < min_priority) {
        priority = min_priority;
    }
    int policy = SCHED_RR;
    struct sched_param param;
    param.sched_priority = priority;
    // returns 0 on success
    return pthread_setschedparam(thread.native_handle(), policy, &param);
}
#endif  // VXSDR_TARGET_MACOS
