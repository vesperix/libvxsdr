// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/*
  Logging can be customized by editing the init() function in logging.cpp
*/

#include <string>

#ifndef VXSDR_LIB_DISABLE_LOGGING

// disable exceptions in logging
#define SPDLOG_NO_EXCEPTIONS
#define FMT_EXCEPTIONS (0)

// force header only
#define SPDLOG_HEADER_ONLY
#define FMT_HEADER_ONLY

#include <spdlog/spdlog.h>

inline const std::string VXSDR_LIB_LOGGER_NAME = "libvxsdr";

namespace vxsdr_lib_logging {
void init();
void shutdown();
}  // namespace vxsdr_lib_logging

#define LOG_INIT()         vxsdr_lib_logging::init()
#define LOG_SHUTDOWN()     vxsdr_lib_logging::shutdown()

#define LOG_TRACE(...)     spdlog::get(VXSDR_LIB_LOGGER_NAME)->trace(__VA_ARGS__)
#define LOG_DEBUG(...)     spdlog::get(VXSDR_LIB_LOGGER_NAME)->debug(__VA_ARGS__)
#define LOG_INFO(...)      spdlog::get(VXSDR_LIB_LOGGER_NAME)->info(__VA_ARGS__)
#define LOG_WARN(...)      spdlog::get(VXSDR_LIB_LOGGER_NAME)->warn(__VA_ARGS__)
#define LOG_ERROR(...)     spdlog::get(VXSDR_LIB_LOGGER_NAME)->error(__VA_ARGS__)
#define LOG_FATAL(...)     spdlog::get(VXSDR_LIB_LOGGER_NAME)->critical(__VA_ARGS__)

// what level to use for async messages
#define LOG_ASYNC(...)     spdlog::get(VXSDR_LIB_LOGGER_NAME)->error(__VA_ARGS__)
#define LOG_ASYNC_OOS(...) spdlog::get(VXSDR_LIB_LOGGER_NAME)->warn(__VA_ARGS__)

#else  // VXSDR_LIB_DISABLE_LOGGING defined

#define LOG_INIT()         {}
#define LOG_SHUTDOWN()     {}
#define LOG_TRACE(...)     {}
#define LOG_DEBUG(...)     {}
#define LOG_INFO(...)      {}
#define LOG_WARN(...)      {}
#define LOG_ERROR(...)     {}
#define LOG_FATAL(...)     {}
#define LOG_ASYNC(...)     {}
#define LOG_ASYNC_OOS(...) {}

#undef VXSDR_SEQ_ERR_DIAG
#undef VXSDR_RESPONSE_TIME_DIAG

#endif  // VXSDR_LIB_DISABLE_LOGGING
