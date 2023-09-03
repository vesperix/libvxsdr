..
   Copyright (c) 2023 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

Host Settings
=============

When setting up a new host to use a VXSDR, several changes from the
default configuration are needed. These are outlined below.

Increase Network Adapter MTU
----------------------------

The maximum packet size allowed by an IP network adapter defaults to 1500 bytes
in most cases. This severely limits the data rate achievable. By default,
a VXSDR assumes the network interface is able to send and recieve "jumbo"
packets of up to 9000 bytes, and this is necessary to achieve the specified
sample rates.

Most operating systems provide both a graphical and a command-line method for
increasing the MTU. On Linux, for example, you can use either the
network settings menu or ``ifconfig``. Check your OS documentation, and set
the MTU for the interface used for the VXSDR to 9000 or more.

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

Set Network Card Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
on the card. If the card offers settings for TX and RX Jumbo frames, set them
to the maximum, and set the buffers for regular frames to the maximum, for example:

.. highlight:: text
.. code-block::

   sudo ethtool -G <device> tx 8192
   sudo ethtool -G <device> tx 8192

Interrupt Colaescing
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

   sudo ethtool -C <device> adaptive-tx on
   sudo ethtool -C <device> adaptive-rx on


Increase network buffer size
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

Increase network buffer size
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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