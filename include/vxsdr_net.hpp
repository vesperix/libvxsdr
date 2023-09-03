// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifndef VXSDR_STANDALONE_ASIO
// this means use boost::asio (the default)
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/io_context.hpp>
#include <boost/asio/ts/socket.hpp>
#include <boost/asio/ts/net.hpp>
namespace net = boost::asio;
using net_error_code = boost::system::error_code;
#else
// use standalone asio
#include <asio/ts/buffer.hpp>
#include <asio/ts/io_context.hpp>
#include <asio/ts/socket.hpp>
#include <asio/ts/net.hpp>
namespace net = asio;
using net_error_code = asio::error_code;
#endif