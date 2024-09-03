..
   Copyright (c) 2023 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

Host Settings
=============

When setting up a new host to use a VXSDR, it is necessary to provide the library
with information on how the host is configured, and to ensure that the host's
settings will provide the performance needed for data transfer to and from the VXSDR.

The library constructs a host interface based on information provided by the user, and
that interface provides the user with access to the VXSDR. The first step is telling the library
what transport is used to communicate with the VXSDR, and providing any necessary settings
(like IP addresses).

Specifying the Transport
-----------------------

The default transport is UDP; for this case, the local (host) and device (VXSDR) IPv4
addresses must be provided to the library at the time the interface is created (IPv6
addressing is not supported.) Examples are provided in C++ and Python below for this process.

.. highlight:: c++
.. code-block::

    // C++ setup for UDP transport
    std::string local_address = "192.168.200.101";
    std::string device_address = "192.168.200.4";
    std::map<std::string, int64_t> config;
    config["udp_transport:local_address"] = ntohl(inet_addr(local_address.c_str()));
    config["udp_transport:device_address"] = ntohl(inet_addr(device_address.c_str()));
    radio = std::make_unique<vxsdr>(config);


.. highlight:: python
.. code-block::

    # Python setup for UDP transport
    import ipaddress
    local_address = "192.168.200.101"
    device_address = "192.168.200.4"
    config = {}
    config["udp_transport:local_address"] = ipaddress.ip_address(local_address)
    config["udp_transport:device_address"] = ipaddress.ip_address(device_address)
    radio = vxsdr_py.vxsdr_py(config)

The PCIe transport is used when a processor is directly attached to the VXSDR; for
example, some versions of the VXSDR support an attached Nvidia Orin CPU/GPU. In this case,
the only necessary configuration is to tell the library to use PCIe for both command and
data transfer. Since the attached processors run Linux, this transport is only supported
under Linux.

.. highlight:: c++
.. code-block::

    // C++ setup for PCIe transport
    std::map<std::string, int64_t> config;
    config["data_transport"] = vxsdr::transport_type::TRANSPORT_TYPE_PCIE;
    config["command_transport"] = vxsdr::transport_type::TRANSPORT_TYPE_PCIE;
    radio = std::make_unique<vxsdr>(config);

.. highlight:: python
.. code-block::

    # Python setup for PCIe transport
    config = {}
    config["data_transport"] = int(vxsdr_py.transport_type.PCIE)
    config["command_transport"] = int(vxsdr_py.transport_type.PCIE)
    radio = vxsdr_py.vxsdr_py(config)

In addition to specifying the transport, there are several other settings which may
improve performance. These settings are summarized in the following sections.


Increase Network Adapter MTU (UDP)
----------------------------------

The maximum packet size allowed by an IP network adapter defaults to 1500 bytes
in most cases. This severely limits the data rate achievable. By default,
the host library assumes the network interface is able to send and receive "jumbo"
packets of up to 9000 bytes, and this is necessary to achieve the specified
sample rates.

Most operating systems provide both a graphical and a command-line method for
increasing the MTU. On Linux, for example, you can use either the
network settings menu or ``ifconfig``. Check your OS documentation, and set
the MTU for the interface used for the VXSDR to 9000 or more.

Processor Affinity
------------------

The VXSDR host software can set the processes which interact with the network
stack for data transport to run on specific processors. Because many current
CPU architectures have a mix of high-performance and high-efficiency processors,
it is important to identify which processors are the fastest, so that these can
be used for network processing for data transport.

Processor affinity for these threads is controlled by the following entries in
the configuration map passed to the constructor for the ``vxsdr`` class:

.. highlight:: c++
.. code-block::

    config["udp_data_transport:thread_affinity_offset"]   = 0;
    config["udp_data_transport:sender_thread_affinity"]   = 0;
    config["udp_data_transport:receiver_thread_affinity"] = 1;
    // or, for PCIe transport
    config["pcie_data_transport:thread_affinity_offset"]   = 0;
    config["pcie_data_transport:sender_thread_affinity"]   = 0;
    config["pcie_data_transport:receiver_thread_affinity"] = 1;

The ``thread_affinity_offset`` entry is added to the ``sender_thread_affinity``
and ``receiver_thread_affinity`` entries to determine the processor number for
these threads. In the example above, which shows the default settings, the
sender and receiver threads would be assigned to processors 0 and 1, respectively.

The Portable Hardware Locality package, developed by the Open MPI project, can be
helpful in identifying and mapping processor types and cache hierarchies. On
Ubuntu 22.04 or later, this package can be installed from the standard
repositories:

.. highlight:: shell
.. code-block::

    sudo apt install hwloc

Information on obtaining ``hwloc`` for many operating systems and architectures is
available at https://www.open-mpi.org/projects/hwloc.

Once the package is installed, the ``lstopo`` command will run tests to determine the
processor and cache hierarchy and show the results is graphical form.

Linux Host Settings
-------------------

Allow real-time priority
~~~~~~~~~~~~~~~~~~~~~~~~

The VXSDR host software typically runs with real-time priority.
To enable this on Linux systems, we suggest creating a group, and
making each radio user a member. For example:

.. highlight:: shell
.. code-block::

    sudo groupadd vxsdr
    sudo usermod -a -G vxsdr <username>

for the username of each user who needs to access the radio. Then,
in ``/etc/security/limits.conf``, add the following line:

.. code-block::

    @vxsdr           -    rtprio     99

which will allow all members of the ``vxsdr`` group to
set realtime priority. You will need to log out and log back in
for these changes to take effect.

If you are not able to set realtime priority, and would like to try running
without it, you can set the VXSDR constructor to not use realtime priority;
see the API reference for details.

Network Card Parameters (UDP)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Most high speed (10 Gb or over) network cards have a large number of settings
which can be tuned for higher performance. This section lists af few common
settings.

Network Card Ring Buffers
^^^^^^^^^^^^^^^^^^^^^^^^^

Query the card to see the current settings and the maximums, for example:

.. highlight:: text
.. code-block::

   ethtool -g <device>
   Ring parameters for <device>:
   Pre-set maximums:
   RX:		8192
   RX Mini:	n/a
   RX Jumbo:	n/a
   TX:		8192
   Current hardware settings:
   RX:		1024
   RX Mini:	n/a
   RX Jumbo:	n/a
   TX:		1024

Your current and maximum settings may differ from those shown above, depending
on the card. Set the buffers for normal and jumbo frames to the maximum; for example,
if you have the card shown above, set TX and RX to 8192:

.. highlight:: text
.. code-block::

   sudo ethtool -G <device> tx 8192 rx 8192

Interrupt Coalescing
^^^^^^^^^^^^^^^^^^^^

Network cards can batch packets so that the rate of kernel interrupts to handle them
is reduced. This is often on by default, but can be checked:

.. highlight:: text
.. code-block::

   ethtool -c <device>
   Coalesce parameters for <device>:
   Adaptive RX: on  TX: on
   (many other outputs . . .)

If adaptive TX and RX are off, they can be turned on with:

.. highlight:: text
.. code-block::

   sudo ethtool -C <device> adaptive-tx on adaptive-rx on


Increase network buffer size (UDP)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It is also necessary to increase the maximum network buffer size. The actual buffer sizes used
can be chosen at runtime, but the maximum size set by the OS must be large enough to accommodate
the runtime choice.

On Linux systems, you can set the maximum sizes temporarily by running these commands:

.. highlight:: text
.. code-block::

   sudo sysctl -w net.core.wmem_max=16777216
   sudo sysctl -w net.core.rmem_max=16777216

These changes will not persist after a restart. To make them persistent, add
the following lines to the file ``/etc/sysctl.conf``:

.. highlight:: text
.. code-block::

   net.core.wmem_max=16777216
   net.core.rmem_max=16777216

macOS Host Settings
-------------------

Increase network buffer size (UDP)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On macOS systems, the maximum buffer size is smaller, but the process is similar to Linux.
You can increase the limit temporarily by running the command:

.. highlight:: text
.. code-block::

   sudo sysctl -w kern.ipc.maxsockbuf=16777216

The 16 MB size shown above is the maximum allowed on macOS 13, and this value determines
the maximum combined size of the transmit and receive network buffers.

Modern macOS systems do not use ``/etc/sysctl.conf``; on older versions, it used to be possible
to make the buffer size permanent in the same way as for Linux systems. Since macOS 11,
``/etc/sysctl.conf`` does not exist by default, and if created, settings there are ignored.

Windows Host Settings
---------------------

(This section will be updated when Windows is officially supported.)