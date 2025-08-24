..
   Copyright (c) 2023 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

Device Control Functions
------------------------

Device Information
~~~~~~~~~~~~~~~~~~
The hello function provides device information, including the packet version
used by the device (which may be checked against the version supported by the library)

.. doxygenfunction:: hello
.. doxygenfunction:: compute_sample_granularity

The reset function performs a hard reset of the device.

.. doxygenfunction:: reset
.. doxygenfunction:: get_status
.. doxygenfunction:: clear_status
.. doxygenfunction:: get_num_subdevices
.. doxygenfunction:: get_buffer_info
.. doxygenfunction:: get_buffer_use
.. doxygenfunction:: get_max_payload_bytes
.. doxygenfunction:: set_max_payload_bytes

Sensors
~~~~~~~
A subdevice may have sensors to report measurements like temperature, voltage, current, or
RF power levels. These functions provide information on any available sensors, and read
the sensors.

.. doxygenfunction:: get_num_sensors
.. doxygenfunction:: get_sensor_name
.. doxygenfunction:: get_sensor_reading

Timing
~~~~~~
A device maintains time and frequency references for all its subdevices. These are
queried and controlled by the functions below.

.. doxygenfunction:: get_time_now
.. doxygenfunction:: set_time_now
.. doxygenfunction:: set_time_next_pps
.. doxygenfunction:: get_timing_status
.. doxygenfunction:: get_timing_resolution

IP Addressing
~~~~~~~~~~~~~
The device API provides functions to discover devices on a network, and to change a device's
IP address. These functions are not used in normal operation; standalone programs are provided
to perform these operations.

.. doxygenfunction:: set_ipv4_address
.. doxygenfunction:: save_ipv4_address
.. doxygenfunction:: discover_ipv4_addresses