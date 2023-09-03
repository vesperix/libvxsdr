..
   Copyright (c) 2023 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

Logging
=======

The VXSDR library has an internal logging capability, used to
record events in the library and notify the user of errors.
Logging is based on spdlog (https://github.com/gabime/spdlog),
and is designed not to interfere with any logging your program might
do, even if your program also uses spdlog.

You can disable library logging
by setting the option VXSDR_ENABLE_LOGGING=OFF on the cmake command line.

There are two types of log possible, a log sent to the console (standard error)
and a log sent to a file. Each of these can have their logging level and their
log entry pattern independently selected at runtime using
environment variables.

Note that the environment variable names used to configure
library logging are different from the names spdlog uses, so you can use
spdlog in your program (if you wish) and have separate settings
for logging from your program and the library

The logging levels for the console and the log file can be set independently
using one of these letter codes:

==========  ==============================================
Code        Meaning
==========  ==============================================
'O' or 'N'  no logging at all
'T'         trace level (currently not used)
'D'         debug level
'I'         info level (reasonable for normal use)
'W'         warning level (something might be wrong)
'E'         error level (something is definitely wrong)
'F' or 'C'  fatal error (stopping)
==========  ==============================================

The environment variables that can be used for customization, and their defaults, are shown below.

- VXSDR_LIB_LOG_CONSOLE_LEVEL
   default: "W" = warn
- VXSDR_LIB_LOG_CONSOLE_PATTERN
   default: "[%n:%l] %v" = [libvxsdr:<log level>] <message>
- VXSDR_LIB_LOG_FILE_LEVEL
   default: "N" = none
- VXSDR_LIB_LOG_FILE_PATTERN
   default: "[%Y-%m-%d %T.%f] [%t] [%L] %v" = [<timestamp>] [<thread-id>] [<1-letter level>] <message>
- VXSDR_LIB_LOG_FILE_NAME
   default: "libvxsdr"
- VXSDR_LIB_LOG_FILE_PATH
   default: "." (directory where the program is run)
- VXSDR_LIB_LOG_FILE_NAME_TIME_FORMAT
   default: "%Y-%m-%d-%H.%M.%S"



The log entry patterns use the spdlog syntax; see
https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
for all the choices.

The log file name time format allows a timestamp to be included in the logfile name
so that successive runs do not overwrite the log. You are responsible for ensuring
that all the characters in the time which results are legal for filenames in your
operating system (for example, ':' is not allowed for most OS).

If you wish to use more of spdlog's capabilities to customize library logging,
those changes should be made in logging.hpp and logging.cpp.