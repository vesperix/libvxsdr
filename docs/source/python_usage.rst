..
   Copyright (c) 2023 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

Python Usage
============

Python bindings are provided for all API functions through pybind11. At the moment, the Python
implementation and documentation are very simple, and use the default pybind11 mappings between
C++ STL types and Python, with one exception: transmit and receive data are passed in
1-dimensional Numpy arrays with ``dtype=complex64`` (that is, two 32-bit floats).

All the other library functions that return arrays or vectors of some type in C++ return lists
of the corresponding type in Python. The ``vxsdr::time_point`` and ``vxsdr::duration`` types
are translated into ``datetime.datetime`` and ``datetime.timedelta``, respectively (this is the
default pybind11 behavior).

The std::optional return types used by the C++ API are translated by pybind11 into ``None`` or the
expected data type, depending on the success of the call, so the Python API is very similar to the
C++ API (and a bit more natural in Python).

.. highlight:: c++
.. code-block::

    // returns a std::optional<double>
    auto result = radio->get_rx_freq();
    // all std::optionals evaluate to true if the function succeeds
    if (result) {
        // use the result
        do_something(result.value());
    } else {
        // error occurred
    }

.. highlight:: python
.. code-block::

    # returns an Optional[float]
    result = radio.get_rx_freq()
    # None means a failure occurred
    if result is not None:
        # use the result
        do_something(result)
    else:
        # error occurred

We have not optimized the Python interface for high throughput; although it is capable of sending and
receiving data at 160 MSPS, we have not verified that long periods of sustained data transfer are
possible, and this will depend on the speed of the host system and the amount of processing
performed in Python. We recommend C++ for use cases requiring long periods of continuous data transfer.
