// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "vxsdr_net.hpp"

int get_socket_mtu(net::ip::udp::socket& sock);
int set_socket_dontfrag(net::ip::udp::socket& sock);