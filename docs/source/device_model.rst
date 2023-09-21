..
   Copyright (c) 2023 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

Device Model
============

The VXSDR host API implements a simple model of a software defined radio: a device which
contains subdevices, each of which contains channels. The capabilities of each of these
parts of the radio are described below.

Device
------

The top level of the device model is the VXSDR device, which has the following components:

    * Data and Command Transport
    * Time Reference
    * Frequency Reference

A device is linked to an instance of the VXSDR class on the host by a transport layer;
currently Ethernet UDP is the only supported transport. Each device also has a
time and frequency reference for use by its subdevices. Current devices all have the ability
to generate internal signals or accept external pulse-per-second and 10 MHz to align with other
devices.

Subdevices
----------

Each device contains up to 254 subdevices, each of which has the following settings:

    * Tuning
        - TX frequency
        - RX frequency
    * Sample Rate
        - TX sample rate
        - RX sample rate
    * LO Selection
        - internal
        - external (optional)
    * Sensors
        - device-dependent sensors (optional)

A subdevice uses the transport, time, and frequency references provided by the device, and
has frequency, sample rate, and gain settings which are shared with all its channels. This
means that all TX channels in a subdevice must have the same frequency, rate, and gain, as
must all RX channels. The settings for TX and RX, however, are independent on all current
devices, so the frequencies, sample rates, and gains can be different between TX and RX
on current devices.

Channels
--------
Each subdevice has 0-254 TX channels, and 0-254 RX channels, so transmit-only or receive-only
systems are possible. Each TX or RX channel has the following settings:

    * Gain
    * Bias Correction (TX channels only)
    * IQ Correction
    * FIR Filtering (optional, depending on FPGA size)

Bias and IQ correction reduce spurious signals outside of the desired band, and FIR filtering
can be used for multiple purposes, including fractional delay and equalization.

Data to and from all channels on a subdevice is sent interleaved over the transport; interleaving and
deinterleaving is the responsibility of the host.
