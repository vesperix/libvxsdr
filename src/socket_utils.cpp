// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vxsdr_net.hpp"

#ifdef VXSDR_TARGET_LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int get_socket_mtu(net::ip::udp::socket& sock) {
    int mtu = 0;
    // FIXME: test this
    socklen_t size = sizeof(mtu);
    int retval = getsockopt(sock.native_handle(), IPPROTO_IP, IP_MTU, (void *)&mtu, &size);
    if (retval == 0) {
        return mtu;
    }
    return -1;
}

int set_socket_dontfrag(net::ip::udp::socket& sock) {
    // for UDP sockets, this just forces the DNF flag to be set; does not do discovery
    // FIXME: test this
    int val = IP_PMTUDISC_PROBE;
    return setsockopt(sock.native_handle(), IPPROTO_IP, IP_MTU_DISCOVER, (void *)&val, sizeof(val));
}

#endif  //  VXSDR_TARGET_LINUX

#ifdef VXSDR_TARGET_WINDOWS
#include <winsock.h>

int get_socket_mtu(net::ip::udp::socket& sock) {
    int mtu = 0;
    // FIXME: test this
    int size = sizeof(mtu);
    int retval = getsockopt(sock.native_handle(), SOL_SOCKET, SO_MAX_MSG_SIZE, (char *)&mtu, &size);
    if (retval == 0) {
        return mtu;
    }
    return -1;
}

int set_socket_dontfrag(net::ip::udp::socket& sock) {
    int val = 1;
    // FIXME: test this
    return setsockopt(sock.native_handle(), IPPROTO_IP, IP_DONTFRAG, &val, sizeof(val));
}

#endif  // VXSDR_TARGET_WINDOWS

#ifdef VXSDR_TARGET_MACOS
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

int get_socket_mtu(net::ip::udp::socket& sock) {
    // zero return means not available on Mac OS
    return 0;
}

int set_socket_dontfrag(net::ip::udp::socket& sock) {
    // FIXME: test this
    int val = 1;
    return setsockopt(sock.native_handle(), IPPROTO_IP, IP_DONTFRAG, &val, sizeof(val));
}

#endif  // VXSDR_TARGET_MACOS
