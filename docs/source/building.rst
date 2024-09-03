..
   Copyright (c) 2023 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

Building the Library
====================

Prerequisites
-------------

The library currently supports Linux hosts only; macOS and Windows
support is still under development.
When the library builds correctly and passes testing with VXSDR devices on a
new host OS, we will update the build settings below and remove the "experimental" notation.

A C++ compiler and standard library supporting C++20, and the CMake cross-platform build
system version 3.16 or higher, are required to build the VXSDR host library. (The C++20
requirement can currently be satisfied by gcc 11 or later, or by clang 14 or later;
we do not now require all of the features introduced in C++20.)

The library itself depends on Boost 1.67 or higher; this is because it uses boost::lockfree queues
and the boost::asio networking interface.

When logging from within the library is enabled (the default) the library also depends on spdlog
version 1.5 or higher. Logging from within the library may be disabled by running the initial CMake
configure step (that is, ``cmake -B build``) with the option ``-D VXSDR_ENABLE_LOGGING=OFF``,
which removes the dependency on spdlog.

To build the Python interface, a Python 3 installation, including the Python include files, and
PyBind11 are required. If these are not present, the Python interface will not be built. The Python
interface is built by default; to disable it, run the initial CMake configure step
(``cmake -B build``) with the option ``-D VXSDR_PYTHON_BINDINGS=OFF``, which
removes the dependencies on Python and its development files.

Installing prerequisites on Ubuntu 22.04 or later
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. highlight:: text
.. code-block::

   sudo apt install g++ make git cmake libboost-all-dev libspdlog-dev
   sudo apt install python3-dev pybind11-dev

Installing prerequisites on Fedora 35 or later
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. highlight:: text
.. code-block::

   sudo dnf install gcc-c++ make git cmake boost-devel spdlog-devel
   sudo dnf install python3-devel pybind11-devel

Using CMake to build prerequisites
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
It is also possible to use CMake to download and build Boost, PyBind11, and
spdlog, removing the need to install these dependencies as packages. This is enabled by
running the initial CMake configure step (``cmake -B build``) with the option
``-D VXSDR_BUILD_DEPENDENCIES=ON``.

.. note::

   A C++ compiler, git, and CMake must still be installed, and the Python development package
   must be installed if you wish to build the Python interface.

Fetching dependencies allows the build to use newer versions than
are provided by the distribution, and ensures that known versions are used in the build.
However, the build takes longer and all the dependencies are downloaded as source, which
takes up more space than a binary package.

We recommend trying this approach for distributions other than Debian-based
(which should use the method shown for Ubuntu) and Red Hat-based (which should use the
method shown for Fedora).

Downloading, building, and installing the library
-------------------------------------------------

.. highlight:: text
.. code-block::

   git clone https://github.com/vesperix/libvxsdr.git
   cd libvxsdr
   cmake -B build <add any options here>
   cmake --build build
   sudo cmake --install build

Linking your program to the host library
----------------------------------------

The host library will be installed in the default location for your system by CMake.
It is named libvxsdr.(suffix), where (suffix) depends on the operating system and the file
type.

For example, on a Linux system, the dynamic library libvxsdr.so is installed by default.
You can use the normal command to add a link library
(for example, -lvxsdr for gcc and clang) to link with the VXSDR host library.

Unsupported Operating Systems
-----------------------------

The "experimental" sections below for macOS and Windows document our development
build settings, but they are **not supported, and neither macOS nor Windows hosts are currently usable**.

Building on macOS (experimental)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Install development tools

.. highlight:: text
.. code-block::

   xcode-select --install

Install Brew from https://brew.sh

.. highlight:: text
.. code-block::

   brew install cmake boost spdlog
   brew install pybind11

Build using Cmake from the command line.

Building on Windows (experimental)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Install Visual Studio from https://visualstudio.microsoft.com/downloads

Run the  Visual Studio installer, selecting "Desktop development with C++"
as the option (you do not need any .NET or Azure components to build the
VXSDR library).

Install Git for Windows from https://git-scm.com/download/win

Install CMake for Windows from https://cmake.org/download

Build using CMake from the command line, using the option
``-D VXSDR_BUILD_DEPENDENCIES=ON`` as described above.
