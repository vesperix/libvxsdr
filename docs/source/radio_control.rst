..
   Copyright (c) 2023 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

Radio Control Functions
-----------------------

Enable and Disable
~~~~~~~~~~~~~~~~~~
Each subdevice may allow the TX and RX sections be enabled or disabled by the functions below.

.. doxygenfunction:: get_tx_enabled
.. doxygenfunction:: get_rx_enabled
.. doxygenfunction:: set_tx_enabled
.. doxygenfunction:: set_rx_enabled

Tuning
~~~~~~
Each subdevice may have an adjustable RF frequency, with information and
control provided by the functions below.

.. doxygenfunction:: get_tx_freq_range
.. doxygenfunction:: get_rx_freq_range
.. doxygenfunction:: get_tx_freq
.. doxygenfunction:: get_rx_freq
.. doxygenfunction:: set_tx_freq
.. doxygenfunction:: set_rx_freq

Gain Control
~~~~~~~~~~~~
Each subdevice may have an adjustable gain, with information and
control provided by the functions below.

.. doxygenfunction:: get_tx_gain_range
.. doxygenfunction:: get_rx_gain_range
.. doxygenfunction:: get_tx_gain
.. doxygenfunction:: get_rx_gain
.. doxygenfunction:: set_tx_gain
.. doxygenfunction:: set_rx_gain


Sampling Rate
~~~~~~~~~~~~~
Each subdevice may have an adjustable sampling rate, with information and
control provided by the functions below.

.. doxygenfunction:: get_tx_rate_range
.. doxygenfunction:: get_rx_rate_range
.. doxygenfunction:: get_tx_rate
.. doxygenfunction:: get_rx_rate
.. doxygenfunction:: set_tx_rate
.. doxygenfunction:: set_rx_rate

Inputs and Outputs
~~~~~~~~~~~~~~~~~~
Each channel may have selectable input and output ports, with information and
control provided by the functions below.

.. doxygenfunction:: get_tx_num_ports
.. doxygenfunction:: get_rx_num_ports
.. doxygenfunction:: get_tx_port_name
.. doxygenfunction:: get_rx_port_name
.. doxygenfunction:: get_tx_port
.. doxygenfunction:: get_rx_port
.. doxygenfunction:: set_tx_port
.. doxygenfunction:: set_rx_port
.. doxygenfunction:: set_tx_port_by_name
.. doxygenfunction:: set_rx_port_by_name


Radio Information
~~~~~~~~~~~~~~~~~
Each device or subdevice provides several functions to determine its properties and
its state.

Device level functions:
^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenfunction:: get_tx_num_subdevs
.. doxygenfunction:: get_rx_num_subdevs

Subdevice level functions:
^^^^^^^^^^^^^^^^^^^^^^^^^^
.. doxygenfunction:: get_tx_num_channels
.. doxygenfunction:: get_rx_num_channels
.. doxygenfunction:: get_tx_stream_state
.. doxygenfunction:: get_rx_stream_state
.. doxygenfunction:: get_tx_lo_locked
.. doxygenfunction:: get_rx_lo_locked

External LO
~~~~~~~~~~~
Each subdevice may allow selection of an external LO,
allowing phase synchronization across many devices.

.. doxygenfunction:: get_tx_external_lo_enabled
.. doxygenfunction:: get_rx_external_lo_enabled
.. doxygenfunction:: set_tx_external_lo_enabled
.. doxygenfunction:: set_rx_external_lo_enabled

Digital Filters
~~~~~~~~~~~~~~~
Each channel may have a complex FIR filter which operates at
the master clock rate for equalization, fractional delay, or other
purposes.

The filter lengths are the same across all channels, and the filters on
all channels are enabled or disabled together.

.. doxygenfunction:: get_tx_filter_length
.. doxygenfunction:: get_rx_filter_length
.. doxygenfunction:: set_tx_filter_enabled
.. doxygenfunction:: set_rx_filter_enabled

The coefficients of the filters may differ across channels.

.. doxygenfunction:: get_tx_filter_coeffs
.. doxygenfunction:: get_rx_filter_coeffs
.. doxygenfunction:: set_tx_filter_coeffs
.. doxygenfunction:: set_rx_filter_coeffs

Corrections
~~~~~~~~~~~
Each channel may allow manual adjustment of IQ bias, which provides control
of LO feedthrough, and manual correction of IQ amplitude and phase imbalance,
which provides control of image rejection.

.. doxygenfunction:: get_tx_iq_bias
.. doxygenfunction:: get_rx_iq_bias
.. doxygenfunction:: set_tx_iq_bias
.. doxygenfunction:: set_rx_iq_bias
.. doxygenfunction:: get_tx_iq_corr
.. doxygenfunction:: get_rx_iq_corr
.. doxygenfunction:: set_tx_iq_corr
.. doxygenfunction:: set_rx_iq_corr