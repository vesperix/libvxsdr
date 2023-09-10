// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <filesystem>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>

#include "logging.hpp"
#include "vxsdr_imp.hpp"

#ifndef VXSDR_LIB_DISABLE_LOGGING

namespace vxsdr_lib_logging {

    spdlog::level::level_enum string_to_log_level(const std::string& s, spdlog::level::level_enum def) {
        spdlog::level::level_enum lev = def;

        char f = (char)std::toupper(s[0]);

        if(f == 'O' or f == 'N') { // "OFF" or "NONE"
            lev = spdlog::level::off;
        } else if(f == 'T') {      // "TRACE"
            lev = spdlog::level::trace;
        } else if(f == 'D') {      // "DEBUG"
            lev = spdlog::level::debug;
        } else if(f == 'I') {      // "INFO"
            lev = spdlog::level::info;
        } else if(f == 'W') {      // "WARN" or "WARNING"
            lev = spdlog::level::warn;
        } else if(f == 'E') {      // "ERR" or "ERROR"
            lev = spdlog::level::err;
        } else if(f == 'F' or f == 'C') { // "FATAL" or "CRITICAL"
            lev = spdlog::level::critical;
        }

        return lev;
    }

    void init() {
/*
    This sets up logging for the vxsdr library. This is the
    library's logging, not your program's, and it's used to
    record events in the library. You can disable library logging
    by defining VXSDR_LIB_DISABLE_LOGGING at build time.

    Logging levels and log entry patterns are set separately
    for the console and the logfile.

    Settings can be chosen at runtime using environment variables.

    Note that the environment variable names used to configure
    library logging are different from the names spdlog uses, so you can use
    spdlog in your program (if you wish) and have separate settings
    for logging from your program and the library

    Set the level using one of these letter codes:
        'O' or 'N'  = no logging at all
        'T'         = trace level (currently not used)
        'D'         = debug level
        'I'         = info level (reasonable for normal use)
        'W'         = warning level (something might be wrong)
        'E'         = error level (something is definitely wrong)
        'F' or 'C'  = fatal error (stopping)

    The log entry patterns use the spdlog syntax; see
        https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
    for all the choices.

    Console variables:
        VXSDR_LIB_LOG_CONSOLE_LEVEL          default is warn
        VXSDR_LIB_LOG_CONSOLE_PATTERN        default is "[%n:%L] %v"  =
                    [libvxsdr:<full log level>] <message>

    Logfile variables:
        VXSDR_LIB_LOG_FILE_LEVEL             default is none
        VXSDR_LIB_LOG_FILE_PATTERN           default is "[%Y-%m-%d %T.%f] [%t] [%L] %v" =
                    [<timestamp>] [<thread-id>] [<1-letter log level>] <message>

        VXSDR_LIB_LOG_FILE_NAME              default is "libvxsdr"
        VXSDR_LIB_LOG_FILE_PATH              default - "." (directory where the program is run)
        VXSDR_LIB_LOG_FILE_NAME_TIME_FORMAT  default is "%Y-%m-%d-%H.%M.%S"
                    allows a timestamp to be included in the logfile name
                    so that successive runs do not overwrite the log;
                    you are responsible for ensuring that all the characters
                    in the time which results are legal for filenames in your
                    operating system (for example, ':' is not allowed for most OS);
                    set to an empty string to keep the name unchanged between
                    runs and overwrite.

    If you wish to use more of spdlog's capabilities to customize library logging,
    those changes should be made in this file and the corresponding header file.
*/

        // defaults if no environment variables are set
        spdlog::level::level_enum console_level  = spdlog::level::warn;
        std::string console_pattern              = "[%n:%l] %v";

        spdlog::level::level_enum logfile_level = spdlog::level::off;
        std::string logfile_name                = VXSDR_LIB_LOGGER_NAME;
        std::string logfile_name_time_format    = "%Y-%m-%d-%H.%M.%S";
        std::string logfile_path                = ".";
        std::string logfile_pattern             = "[%Y-%m-%d %T.%f] [%t] [%L] %v";

        // see if environment variables are set and change settings if they are
        auto *env_console_level = std::getenv("VXSDR_LIB_LOG_CONSOLE_LEVEL");
        if (env_console_level != nullptr) {
            console_level = string_to_log_level(env_console_level, console_level);
        }
        auto *env_console_pattern = std::getenv("VXSDR_LIB_LOG_CONSOLE_PATTERN");
        if (env_console_pattern != nullptr) {
            console_pattern = std::string(env_console_pattern);
        }

        auto console_log = std::make_shared<spdlog::sinks::stderr_sink_mt>();
        console_log->set_pattern(console_pattern);
        console_log->set_level(console_level);

        auto *env_logfile_level = std::getenv("VXSDR_LIB_LOG_FILE_LEVEL");
        if (env_logfile_level != nullptr) {
            logfile_level = string_to_log_level(env_logfile_level, logfile_level);
        }
        auto *env_logfile_pattern = std::getenv("VXSDR_LIB_LOG_FILE_PATTERN");
        if (env_logfile_pattern != nullptr) {
            logfile_pattern = std::string(env_logfile_pattern);
        }
        auto *env_logfile_name = std::getenv("VXSDR_LIB_LOG_FILE_NAME");
        if (env_logfile_name != nullptr) {
            logfile_name = std::string(env_logfile_name);
        }
        auto *env_logfile_path = std::getenv("VXSDR_LIB_LOG_FILE_PATH");
        if (env_logfile_path != nullptr) {
            logfile_path = std::string(env_logfile_path);
        }
        auto *env_logfile_name_time_format = std::getenv("VXSDR_LIB_LOG_FILE_NAME_TIME_FORMAT");
        if (env_logfile_name_time_format != nullptr) {
            logfile_name_time_format = std::string(env_logfile_name_time_format);
        }

        // make the logger(s)
        std::shared_ptr<spdlog::logger> lib_logger = nullptr;
        spdlog::level::level_enum overall_level = spdlog::level::trace;

        if (logfile_level == spdlog::level::off) {
            // just set up the console log
            lib_logger = std::make_shared<spdlog::logger>(VXSDR_LIB_LOGGER_NAME, console_log);
            overall_level = console_level;

        } else {
            // set up console and file logs
            std::string logfile_full_name;
            std::string logfile_full_path;
            if (!logfile_name_time_format.empty()) {
                std::stringstream timestr;
                std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                struct tm tmp;
                timestr << std::put_time(localtime_r(&t, &tmp), logfile_name_time_format.c_str());
                logfile_full_name = logfile_name + "-" + timestr.str() + ".log";
            } else {
                logfile_full_name = logfile_name + ".log";
            }
            if (!logfile_path.empty()) {
                std::filesystem::path pth(logfile_path);
                pth.append(logfile_full_name);
                logfile_full_path = pth.string();
            }
            auto file_log = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile_full_path);
            file_log->set_pattern(logfile_pattern);
            file_log->set_level(logfile_level);

            lib_logger = std::make_shared<spdlog::logger>(VXSDR_LIB_LOGGER_NAME, spdlog::sinks_init_list({ console_log, file_log }));
            overall_level = std::min(logfile_level, console_level);
        }
        spdlog::register_logger(lib_logger);
        lib_logger->flush_on(spdlog::level::debug);
        lib_logger->set_level(overall_level);
        spdlog::set_default_logger(lib_logger);
    }

    void shutdown() {
        spdlog::drop(VXSDR_LIB_LOGGER_NAME);
    }
}
#endif // #ifndef VXSDR_LIB_DISABLE_LOGGING