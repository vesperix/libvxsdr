..
   Copyright (c) 2023 Vesperix Corporation
   SPDX-License-Identifier: CC-BY-SA-4.0

Building the Library
====================

Prerequisites
-------------

The library currently supports Linux hosts only; macOS and Windows
support is still under development. The "experimental" sections below
for macOS and Windows document our development build settings, but they are
**not yet supported, and neither macOS nor Windows hosts are currently usable**.
When the library builds correctly and passes testing with VXSDR devices on a
new host OS, we will update the build settings below and remove the "experimental" notation.

A C++ compiler and standard library supporting C++20, and the CMake cross-platform build
system version 3.16 or higher are required to build the VXSDR host library. (The C++20
requirement can currently be satisfied by gcc 11 or later, or by clang 14 or later;
we do not now require all of the features introduced in C++20.)

The library itself depends on Boost 1.67 or higher; this is because it uses boost::lockfree queues
and the boost::asio networking interface.

When logging from within the library is enabled (the default) the library also depends on spdlog
version 1.5 or higher. Logging from within the library may be disabled by running CMake
with ``-DVXSDR_ENABLE_LOGGING=OFF``, which removes the dependency on spdlog.

To build the Python interface, a Python 3 installation, including the Python include files, and
PyBind11 are required. If these are not present, the Python interface will not be built. The Python
interface is built by default; to disable it, run CMake with ``-DVXSDR_PYTHON_BINDINGS=OFF``, which
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

Installing prerequisites on macOS (experimental)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Install development tools

.. highlight:: text
.. code-block::

   xcode-select --install

Install Brew from https://brew.sh

.. highlight:: text
.. code-block::

   brew install cmake boost spdlog
   brew install pybind11

Installing prerequisites on Windows (experimental)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Install Visual Studio from https://visualstudio.microsoft.com/downloads

Install Git for Windows from https://git-scm.com/download/win

Run the  Visual Studio installer, selecting "Desktop development with C++"
as the option (you do not need any .NET or Azure components to build the
VXSDR library).

The latest versions of Visual Studio include git and vcpkg, but they are
configured for use from inside the Visual Studio GUI. For a simple build,
we recommend using the command-line procedure shown below.

.. highlight:: text
.. code-block::

   git clone https://github.com/microsoft/vcpkg
   vcpkg\bootstrap-vcpkg.bat
   vcpkg\vcpkg install vcpkg-cmake:x64-windows
   vcpkg\vcpkg install boost-program-options:x64-windows boost-asio:x64-windows boost-lockfree:x64-windows
   vcpkg\vcpkg install spdlog:x64-windows pybind11:x64-windows


Downloading, building, and installing the library
-------------------------------------------------

.. highlight:: text
.. code-block::

   git clone https://github.com/vesperix/libvxsdr.git
   cd libvxsdr
   cmake -B build
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
