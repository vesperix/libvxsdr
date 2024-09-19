..
   Copyright (c) 2023 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

.. |linux_build_status| image:: https://github.com/vesperix/libvxsdr/actions/workflows/github_linux_build.yaml/badge.svg
   :alt: Linux build status badge

.. |macos_build_status| image:: https://github.com/vesperix/libvxsdr/actions/workflows/github_macos_build.yaml/badge.svg
   :alt: macOS build status badge

.. |windows_build_status| image:: https://github.com/vesperix/libvxsdr/actions/workflows/github_windows_build.yaml/badge.svg
   :alt: Windows build status badge

Introduction
============

This is the host library for the VXSDR software defined radio API;
it lets you control the radio and send and receive data.

Licensing
---------

The library source code is licensed under the GNU GPL version 3 or (at your option) any later version.
See the LICENSE file for the complete terms of the source code license.

Documentation
=============

Documentation for the host library is available at https://libvxsdr.readthedocs.io.

The documentation is licensed under the Creative Commons Attribution-ShareAlike 4.0
International Public License; see
https://creativecommons.org/licenses/by-sa/4.0/legalcode
for the complete terms of the documentation license.

Building the Library
====================

Build Status
------------

Results of automated builds of the library and Python bindings for Linux (Ubuntu 24.04)
on GitHub's CI system:

|linux_build_status|

This is updated each time the main branch is changed. Ports for macOS and Windows
are under development, and their build status will be added when they are released.

Build Requirements
------------------

The library currently supports Linux hosts only; macOS and Windows
support is still under development.
When the library builds correctly and passes testing with VXSDR devices on a
new host OS, we will update the build settings below and remove the "experimental" notation.

A C++ compiler and standard library supporting C++20, and the CMake cross-platform build
system version 3.16 or higher, are required to build the VXSDR host library. (The C++20
requirement can currently be satisfied by gcc 11 or later, or by clang 14 or later;
we do not yet require most of the features introduced in C++20.)

The library has several dependencies, which may be satisfied either by installing them separately,
or by downloading and building the dependencies as part of the build process. The quickest is to
install the dependencies separately; see the documentation for how to download and build.

The library requires an implementation of the Asio networking interface; the most common way to
satisfy this is by using the boost::asio networking interface. Use of the standalone Asio distribution
is also possible, and is the default when downloading and building dependencies.

A single-producer-single-consumer queue is also required; the default is Meta's folly::ProducerConsumerQueue,
which is included with the library distribution. It is also possible to use boost::spsc_queue, using
the option ``-D VXSDR_USE_BOOST_QUEUE=ON``.

When logging from within the library is enabled (the default) the library also depends on spdlog
version 1.5 or higher. Logging from within the library may be disabled by running the initial CMake
configure step (that is, ``cmake -B build``) with the option ``-D VXSDR_ENABLE_LOGGING=OFF``,
which removes the dependency on spdlog.

To build the Python interface, a Python 3 installation, including the Python include files, and
PyBind11 are required. If these are not present, the Python interface will not be built. The Python
interface is built by default; to disable it, run the initial CMake configure step
(``cmake -B build``) with the option ``-D VXSDR_PYTHON_BINDINGS=OFF``, which
removes the dependencies on Python and its development files.

Installing dependencies on Ubuntu 22.04 or later
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. highlight:: text
.. code-block::

   sudo apt install g++ make git cmake libboost-all-dev libspdlog-dev
   sudo apt install python3-dev pybind11-dev

Installing dependencies on Fedora 35 or later
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. highlight:: text
.. code-block::

   sudo dnf install gcc-c++ make git cmake boost-devel spdlog-devel
   sudo dnf install python3-devel pybind11-devel

Downloading, building, and installing the library
-------------------------------------------------

.. code-block:: shell

   git clone https://github.com/vesperix/libvxsdr.git
   cd libvxsdr
   cmake -B build
   cmake --build build
   sudo cmake --install build

Linking your program to the host llbrary
----------------------------------------

The host library will be installed in the default location for your system by CMake.
It is named libvxsdr.(suffix), where (suffix) depends on the operating system and the file
type.

For example, on a Linux system, the dynamic library libvxsdr.so is installed by default.
You can use the normal command to add a link library
(for example, -lvxsdr for gcc and clang) to link with the VXSDR host library.
