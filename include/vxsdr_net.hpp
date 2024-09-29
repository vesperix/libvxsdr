// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifndef VXSDR_STANDALONE_ASIO
// this means use boost::asio (the default)
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/io_context.hpp>
#include <boost/asio/ts/net.hpp>
#include <boost/asio/ts/socket.hpp>
namespace net                  = boost::asio;
namespace net_error_code       = boost::system;
namespace net_error_code_types = boost::system::errc;
#else
// use standalone asio
#include <asio/ts/buffer.hpp>
#include <asio/ts/io_context.hpp>
#include <asio/ts/net.hpp>
#include <asio/ts/socket.hpp>
namespace net                  = asio;
namespace net_error_code       = asio;
namespace net_error_code_types = asio::error;
#endif