..
   Copyright (c) 2023 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

C++ Usage
=========

The C++ API is the native API for VXSDR radios; it is documented in detail in the API Reference section below.
The API is intended to use modern C++ features (C++11 and later) where the are useful, and currently
requires a compiler supporting the C++20 standard. However, only a few C++20 features are currently used,
and the partial support offered by gcc since at least version 9.4 is sufficient.

The ``std::optional`` template class from C++17 is heavily used in the API to carry results from functions that usually
return a value, but may rarely fail. For example, the ``get_rx_freq`` function returns a ``std::optional<double>``.

For those who are unfamiliar with ``std::optional``, it provides a uniform alternative to the use of a "flag"
value to indicate an error return. For example, an old-fashioned C or C++ interface might use a negative result from
``get_rx_freq`` to indicate an error:

.. highlight:: c++
.. code-block::

    double freq = radio->get_rx_freq_old();
    if (freq >= 0.0) {
        // use the result
        do_something(freq);
    } else {
        // error occurred
    }


One difficulty with this approach is that the appropriate "flag" value can be different for different functions,
so each function needs to choose and document its error "flag" value.

The VXSDR API uses ``std::optional``, which removes the need for a "flag" value, and works in the same
way for all return types:

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


For those nostalgic for earlier idioms, it's possible to use the ``value_or()`` member of
``std::optional`` to emulate the old style:

.. highlight:: c++
.. code-block::

    double freq = radio->get_rx_freq().value_or(-1.0);
    if (freq >= 0.0) {
        // use the result
        do_something(freq);
    } else {
        // error occurred
    }

We recommend using the modern approach; ``std::optional`` is not perfect, but it does
provide a uniform way of handling errors without worrying about keeping track of
which "flag" value applies to each function.
