..
   Copyright (c) 2023 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

Transmit and Receive Functions
------------------------------

Setting up transmit and receive
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: tx_start
.. doxygenfunction:: rx_start

Setting up repeating transmit and receive
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: tx_loop
.. doxygenfunction:: rx_loop

Interrupting transmit and receive
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: tx_stop
.. doxygenfunction:: rx_stop

Sending and receiving samples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: put_tx_data(const std::vector<std::complex<int16_t>> &data, size_t n_requested = 0, const uint8_t subdev = 0, const double timeout_s = 10)
.. doxygenfunction:: put_tx_data(const std::vector<std::complex<float>> &data, size_t n_requested = 0, const uint8_t subdev = 0, const double timeout_s = 10)
.. doxygenfunction:: get_rx_data(std::vector<std::complex<int16_t>> &data, const size_t n_requested = 0, const uint8_t subdev = 0, const double timeout_s = 10)
.. doxygenfunction:: get_rx_data(std::vector<std::complex<float>> &data, const size_t n_requested = 0, const uint8_t subdev = 0, const double timeout_s = 10)