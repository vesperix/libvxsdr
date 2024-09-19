cmake_minimum_required(VERSION 3.16..3.28)

set(libvxsdr_version_major  0)
set(libvxsdr_version_minor 19)
set(libvxsdr_version_patch 19)

set(libvxsdr_version ${libvxsdr_version_major}.${libvxsdr_version_minor}.${libvxsdr_version_patch})

project(libvxsdr VERSION ${libvxsdr_version} DESCRIPTION "VXSDR host library" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# default options for all builds
option(VXSDR_ENABLE_LOGGING "Enable logging from within libvxsdr" ON)
option(VXSDR_PYTHON_BINDINGS "Make Python bindings for libvxsdr" ON)
option(BUILD_SHARED_LIBS "Build libvxsdr as a shared library" ON)
option(VXSDR_BUILD_UTILITIES "Build VXSDR utility programs" ON)

# developer options off by default
option(VXSDR_STATIC_ANALYSIS "Run clang-tidy during libvxsdr builds" OFF)

option(VXSDR_BUILD_DEPENDENCIES "Download and build external dependencies" OFF)
option(VXSDR_STANDALONE_ASIO "When building dependencies, use standalone ASIO instead of Boost::asio" ON)
option(VXSDR_BUILD_TESTS "Build VXSDR test programs and enable testing" OFF)
option(VXSDR_USE_BOOST_QUEUE "Use Boost::spsc_queue instead of folly::ProducerConsumerQueue" OFF)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type defined -- assuming Release")
    message(STATUS "Use cmake -DCMAKE_BUILD_TYPE=<type> to specify build type")
    set(CMAKE_BUILD_TYPE Release)
else()
    message(STATUS "Build type is ${CMAKE_BUILD_TYPE}")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(libvxsdr_source src/vxsdr.cpp
                    src/vxsdr_imp.cpp
                    src/device_commands.cpp
                    src/radio_commands.cpp
                    src/command_transport.cpp
                    src/data_transport.cpp
                    src/udp_command_transport.cpp
                    src/udp_data_transport.cpp
                    src/pcie_command_transport.cpp
                    src/pcie_data_transport.cpp
                    src/socket_utils.cpp
                    src/thread_utils.cpp
                    src/logging.cpp)
set(libvxsdr_header include/vxsdr.hpp)
set(vxsdr_python_src src/vxsdr_py.cpp)

set(vxsdr_utility_source utilities/vxsdr_find.cpp
                         utilities/vxsdr_set_addr.cpp
                         utilities/vxsdr_save_addr.cpp)

set(vxsdr_test_source testing/test_sleep_resolution.cpp
                      testing/test_float_convert.cpp
                      testing/test_data_queue.cpp
                      testing/test_net_settings.cpp
                      testing/test_localhost_xfer.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(VXSDR_BUILD_DEPENDENCIES)
    find_package(Git REQUIRED)
    include(FetchContent)
endif()
if(VXSDR_STANDALONE_ASIO AND VXSDR_BUILD_DEPENDENCIES)
    list(APPEND vxsdr_dependency_defs "-DVXSDR_STANDALONE_ASIO")
    message(STATUS "Using standalone Asio for networking")
else()
    message(STATUS "Using boost::asio for networking")
endif()

if(VXSDR_USE_BOOST_QUEUE)
    list(APPEND vxsdr_dependency_defs "-DVXSDR_USE_BOOST_QUEUE")
    message(STATUS "Using boost::spsc_queue for data and command queues")
else()
    message(STATUS "Using folly::ProducerConsumerQueue for data and command queues")
endif()

if(VXSDR_BUILD_DEPENDENCIES)
    message(STATUS "Downloading and building dependencies; this will take some time...")
    set(BOOST_ENABLE_CMAKE ON)
    if(VXSDR_STANDALONE_ASIO)
        FetchContent_Declare(
            asio
            GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
            GIT_TAG        asio-1-30-2
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(asio)
        if(VXSDR_USE_BOOST_QUEUE)
            set(BOOST_INCLUDE_LIBRARIES lockfree)
            FetchContent_Declare(
                Boost
                GIT_REPOSITORY https://github.com/boostorg/boost.git
                GIT_TAG        boost-1.85.0
                GIT_SHALLOW TRUE
            )
            FetchContent_MakeAvailable(Boost)
        endif()
    else()
        if(VXSDR_USE_BOOST_QUEUE)
            set(BOOST_INCLUDE_LIBRARIES lockfree;asio)
        else()
            set(BOOST_INCLUDE_LIBRARIES asio)
        endif()
        FetchContent_Declare(
            Boost
            GIT_REPOSITORY https://github.com/boostorg/boost.git
            GIT_TAG        boost-1.85.0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(Boost)
    endif()
else()
    find_package(Boost 1.67 REQUIRED)
endif()

if(NOT VXSDR_ENABLE_LOGGING)
    message(STATUS "Building library without logging")
    set(vxsdr_log_defs -DVXSDR_LIB_DISABLE_LOGGING)
else()
    message(STATUS "Building library with logging")
    if(VXSDR_BUILD_DEPENDENCIES)
        set(SPDLOG_HEADER_ONLY ON)
        set(FMT_HEADER_ONLY ON)
        FetchContent_Declare(
            spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG        v1.14.1
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(spdlog)
    else()
        find_package(spdlog 1.5.0 REQUIRED)
    endif()
endif()

set(vxsdr_build_defs -DVERSION_STRING="${libvxsdr_version}"
                     -DVERSION_MAJOR=${libvxsdr_version_major}
                     -DVERSION_MINOR=${libvxsdr_version_minor}
                     -DVERSION_PATCH=${libvxsdr_version_patch}
                     -DBUILD_TYPE="${CMAKE_BUILD_TYPE}"
                     -DCOMPILER_INFO="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
                     -DSYSTEM_INFO="${CMAKE_SYSTEM}")

if(UNIX)
    if(NOT APPLE)
        if(NOT CYGWIN)
            message(STATUS "Using Linux target (other Unix variants not supported)")
            set(vxsdr_system_defs -DVXSDR_TARGET_LINUX -DTARGET_OS=Linux)
            # PCIe only enabled on Linux
            set(vxsdr_transport_defs -DVXSDR_ENABLE_UDP -DVXSDR_ENABLE_PCIE)
        else() # CYGWIN
            message(WARNING "Cygwin target is not supported, good luck!")
            set(vxsdr_system_defs -DVXSDR_TARGET_LINUX -DTARGET_OS=Unsupported)
            set(vxsdr_transport_defs -DVXSDR_ENABLE_UDP)
        endif()
    elseif(APPLE)
        message(WARNING "macOS target is experimental")
        #message(STATUS "Using macOS target")
        set(vxsdr_system_defs -DVXSDR_TARGET_MACOS -DTARGET_OS=macOS)
        set(vxsdr_transport_defs -DVXSDR_ENABLE_UDP)
    endif()
elseif(WIN32)
    if(NOT CYGWIN)
        message(WARNING "Windows target is experimental")
        #message(STATUS "Using Windows target")
        set(vxsdr_system_defs -DVXSDR_TARGET_WINDOWS -DTARGET_OS=Windows)
        set(vxsdr_transport_defs -DVXSDR_ENABLE_UDP)
    else() # CYGWIN
        message(WARNING "Cygwin target is not supported, good luck!")
        set(vxsdr_system_defs -DVXSDR_TARGET_LINUX -DTARGET_OS=Unsupported)
        set(vxsdr_transport_defs -DVXSDR_ENABLE_UDP)
    endif()
else()
    message(FATAL_ERROR "Unable to recognize the target platform")
endif()

if(APPLE)
    # FIXME: are other Mac OS-specific settings needed?
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
else()
    set(CMAKE_INSTALL_RPATH $ORIGIN)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(vxsdr_compiler_defs -DVXSDR_COMPILER_CLANG -DCOMPILER="Clang")
    set(vxsdr_warning_options -Wall)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(vxsdr_optimization_options -O3)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(vxsdr_compiler_defs -DVXSDR_COMPILER_APPLECLANG -DCOMPILER="AppleClang")
    set(vxsdr_warning_options -Wall)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(vxsdr_optimization_options -O3)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(vxsdr_compiler_defs -DVXSDR_COMPILER_GCC -DCOMPILER="GCC")
    set(vxsdr_warning_options -Wall -Wno-packed-bitfield-compat)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        # -fno-trapping-math is needed for gcc to properly optimize float32-int16 conversions
        set(vxsdr_optimization_options -O3 -fno-trapping-math)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    set(vxsdr_compiler_defs -DVXSDR_COMPILER_INTEL -DCOMPILER="Intel")
    message(WARNING "Intel C++ is not supported")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(vxsdr_compiler_defs -D_WIN32_WINNT=0x0A00 -DVXSDR_COMPILER_MSVC -DCOMPILER="MSVC")
    set(vxsdr_warning_options /W3)
    set(vxsdr_misc_options /Zc:preprocessor /EHsc)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(vxsdr_optimization_options /O2)
    endif()
endif()

if(VXSDR_PYTHON_BINDINGS)
    message(STATUS "Python bindings requested")
    # When multiple Python versions are installed, you may need to
    # need to specify the version to use by specifying the version number
    # for example: -DVXSDR_PYTHON_VER=3.9:
    if(DEFINED VXSDR_PYTHON_VER)
        message(STATUS "Looking for specified version ${VXSDR_PYTHON_VER}")
        find_package(Python3 ${VXSDR_PYTHON_VER} EXACT COMPONENTS Interpreter Development)
    else()
        find_package(Python3 COMPONENTS Interpreter Development)
    endif()

    if(VXSDR_BUILD_DEPENDENCIES)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG        v2.13.5
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(pybind11)
    else()
        find_package(pybind11)
    endif()
    if(pybind11_FOUND AND Python3_FOUND)
        set(vxsdr_build_python -DBUILD_PYTHON_BINDINGS)
        set(Make_Python_Bindings YES)
    else()
        message(WARNING "Did not find Python development libraries and/or pybind11, skipping Python bindings")
        set(Make_Python_Bindings NO)
    endif()
else()
    message(STATUS "Python bindings not requested")
    set(Make_Python_Bindings NO)
endif()

# visibility settings and export header
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
include(GenerateExportHeader)
add_library(libvxsdr ${libvxsdr_source})
set(vxsdr_export_header_name ${CMAKE_CURRENT_BINARY_DIR}/vxsdr_export.h)
generate_export_header(libvxsdr EXPORT_FILE_NAME ${vxsdr_export_header_name})
set_target_properties(libvxsdr PROPERTIES PUBLIC_HEADER ${libvxsdr_header})

if(VXSDR_STATIC_ANALYSIS)
    find_program(clang_tidy NAMES clang-tidy REQUIRED)
    set_target_properties(libvxsdr PROPERTIES CMAKE_CXX_CLANG_TIDY ${clang_tidy})
    message(STATUS "Running clang-tidy during build")
endif()

target_compile_options(libvxsdr PRIVATE ${vxsdr_optimization_options}
                                        ${vxsdr_warning_options}
                                        ${vxsdr_misc_options})
target_compile_definitions(libvxsdr PRIVATE ${vxsdr_build_defs}
                                            ${vxsdr_log_defs}
                                            ${vxsdr_compiler_defs}
                                            ${vxsdr_dependency_defs}
                                            ${vxsdr_transport_defs}
                                            ${vxsdr_system_defs})
target_include_directories(libvxsdr PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
                                            ${Boost_INCLUDE_DIRS}
                                     PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
                                            $<INSTALL_INTERFACE:include>)

if(VXSDR_BUILD_DEPENDENCIES)
    if(VXSDR_STANDALONE_ASIO)
        target_include_directories(libvxsdr PRIVATE ${asio_SOURCE_DIR}/asio/include)
        # FIXME: does asio have a better definition?
        # target_link_libraries(libvxsdr INTERFACE asio::asio_header_only)
    else()
        target_link_libraries(libvxsdr PRIVATE Boost::asio)
    endif()
    if(VXSDR_USE_BOOST_QUEUE)
        target_link_libraries(libvxsdr PRIVATE Boost::lockfree)
    endif()
endif()

if(Make_Python_Bindings)
    target_compile_definitions(libvxsdr PRIVATE ${vxsdr_build_python})
    target_include_directories(libvxsdr PRIVATE ${pybind11_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS})
endif()

if(VXSDR_ENABLE_LOGGING)
    target_include_directories(libvxsdr PRIVATE ${spdlog_INCLUDE_DIRS})
    target_link_libraries(libvxsdr PRIVATE spdlog::spdlog_header_only)
endif()

set_target_properties(libvxsdr PROPERTIES SOVERSION ${libvxsdr_version_major})
set_target_properties(libvxsdr PROPERTIES VERSION ${libvxsdr_version})
set_target_properties(libvxsdr PROPERTIES OUTPUT_NAME vxsdr)

if(Make_Python_Bindings)
    pybind11_add_module(vxsdr_py ${vxsdr_python_src})
    target_compile_options(vxsdr_py PRIVATE ${vxsdr_warning_options}
                                            ${vxsdr_optimization_options}
                                            ${vxsdr_misc_options})
    target_compile_definitions(vxsdr_py PRIVATE ${vxsdr_build_python})
    target_include_directories(vxsdr_py PRIVATE include ${CMAKE_CURRENT_BINARY_DIR} ${pybind11_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS})
    target_link_libraries(vxsdr_py PRIVATE libvxsdr ${pybind11_LIBRARIES})
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        target_include_directories(vxsdr_py PRIVATE ${spdlog_INCLUDE_DIRS})
        target_link_libraries(vxsdr_py PRIVATE spdlog::spdlog_header_only)
    endif()
    set_target_properties(vxsdr_py PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(VXSDR_BUILD_UTILITIES)
    # build utilities for finding VXSDRs and changing IP adddresses
    foreach(source_file ${vxsdr_utility_source})
        get_filename_component(target_name ${source_file} NAME_WE)
        #cmake_path(GET source_file STEM target_name)
        add_executable(${target_name} ${source_file})
        target_compile_options(${target_name} PRIVATE ${vxsdr_optimization_options}
                                                      ${vxsdr_warning_options}
                                                      ${vxsdr_misc_options})
        target_compile_definitions(${target_name} PRIVATE ${vxsdr_build_defs}
                                                          ${vxsdr_dependency_defs}
                                                          ${vxsdr_compiler_defs}
                                                          ${vxsdr_system_defs})
        target_include_directories(${target_name} PRIVATE include)
        if(VXSDR_STANDALONE_ASIO)
            target_include_directories(${target_name} PRIVATE ${asio_SOURCE_DIR}/asio/include)
        endif()
        target_include_directories(${target_name} PRIVATE ${Boost_INCLUDE_DIRS})
        target_link_libraries(${target_name} PRIVATE Threads::Threads)
    endforeach()
endif()

if(VXSDR_BUILD_TESTS)
    include(CTest)
    foreach(source_file ${vxsdr_test_source})
        get_filename_component(target_name ${source_file} NAME_WE)
        #cmake_path(GET source_file STEM target_name)
        add_executable(${target_name} ${source_file} src/thread_utils.cpp)
        target_compile_options(${target_name} PRIVATE ${vxsdr_optimization_options}
                                                      ${vxsdr_warning_options}
                                                      ${vxsdr_misc_options})
        target_compile_definitions(${target_name} PRIVATE ${vxsdr_build_defs}
                                                          ${vxsdr_log_defs}
                                                          ${vxsdr_dependency_defs}
                                                          ${vxsdr_compiler_defs}
                                                          ${vxsdr_system_defs})
        target_include_directories(${target_name} PRIVATE include)
        # include spdlog only to allow use of its included fmtlib
        target_include_directories(${target_name} PRIVATE ${spdlog_INCLUDE_DIRS})
        target_link_libraries(${target_name} PRIVATE spdlog::spdlog_header_only)
        if(VXSDR_STANDALONE_ASIO)
            target_include_directories(${target_name} PRIVATE ${asio_SOURCE_DIR}/asio/include)
        endif()
        target_include_directories(${target_name} PRIVATE ${Boost_INCLUDE_DIRS})
        target_link_libraries(${target_name} PRIVATE Threads::Threads)
    endforeach()
    add_test(sleep_resolution test_sleep_resolution 2e-4 1000)
    add_test(queue_speed test_spsc_queue 10 160e6)
    add_test(float_convert_speed test_float_convert 0.2 160e6)
endif()

include(GNUInstallDirs)
install(TARGETS libvxsdr
    EXPORT vxsdrTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES ${vxsdr_export_header_name} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(Make_Python_Bindings)
    install(TARGETS vxsdr_py DESTINATION ${Python3_SITELIB})
endif()

if(VXSDR_BUILD_UTILITIES)
    install(TARGETS vxsdr_find RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(TARGETS vxsdr_set_addr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(TARGETS vxsdr_save_addr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

set(libvxsdr_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/vxsdr)

install(EXPORT vxsdrTargets
    FILE vxsdrTargets.cmake
    NAMESPACE vxsdr::
    DESTINATION ${libvxsdr_INSTALL_CMAKEDIR}
)

export(EXPORT vxsdrTargets
    FILE ${libvxsdr_INSTALL_CMAKEDIR}/vxsdrTargets.cmake
    NAMESPACE vxsdr::
)

set(VXSDR_INCLUDE_DIR "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
set(VXSDR_LIBRARY_DIR "${CMAKE_INSTALL_FULL_LIBDIR}")
set(VXSDR_BINARY_DIR  "${CMAKE_INSTALL_FULL_BINDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(vxsdrConfig.cmake.in
                              ${CMAKE_CURRENT_BINARY_DIR}/vxsdrConfig.cmake
                              INSTALL_DESTINATION ${libvxsdr_INSTALL_CMAKEDIR})
write_basic_package_version_file(vxsdrConfigVersion.cmake
                                 VERSION ${vxsdr_version}
                                 COMPATIBILITY SameMajorVersion)

set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS WORLD_READ WORLD_EXECUTE)
# otherwise files created below have default permissions for the installer (who can be root)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vxsdrConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/vxsdrConfigVersion.cmake
        DESTINATION ${libvxsdr_INSTALL_CMAKEDIR})