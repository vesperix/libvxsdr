..
   Copyright (c) 2024 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

Host Library Configuration
==========================

There are many configuration options that affect the operation of the host library. These are provided
to the library when it is created using a ``std::map<std::string, int64_t>`` containing the option settings.

Most of the options used by the library are set by default, and do not need to be changed in normal operation.
However, the IPv4 addresses to use for communication with the VXSDR must be set by the user.

There are four IPv4 addresses to be set: the local and device addresses for the command and data transports.
Normally, the local addresses and the device addresses for both transports are the same, so only two distinct IPv4
addresses are used; one is the address of the network interface on the host, the other is the address of the
radio.

.. note::

    The IPv4 addresses must be provided in host order, which is usually (on little-endian systems) different
    from the network byte order that some utility functions return. Be certain to check that the
    addresses you provide have been translated into the correct order.

The necessary options can be set in C++ like this:

.. highlight:: c++
.. code-block::

    #include <vxsdr.hpp>
    #include <arpa/inet.h> // need routines to handle ipv4 addresses
     . . .
    // provide the IPv4 addresses as uint32_t in HOST order
    uint32_t local_addr = ntohl(inet_addr("192.168.200.201"));
    uint32_t device_addr = ntohl(inet_addr("192.168.200.1"));
    std::map<std::string, int64_t> my_config;
    my_config["udp_transport:local_address"]  = local_addr;
    my_config["udp_transport:device_address"] = device_addr;
    radio = std::make_unique<vxsdr>(my_config);

In Python, a similar approach is:

.. highlight:: python
.. code-block::

    import vxsdr_py
    import ipaddress
     . . .
    local_addr = ipaddress.ip_address("192.168.200.201")
    device_addr = ipaddress.ip_address("192.168.200.1")
    my_config = {}
    my_config["udp_transport:local_address"]  = local_addr
    my_config["udp_transport:device_address"] = device_addr
    radio = vxsdr_py.vxsdr_py(my_config)