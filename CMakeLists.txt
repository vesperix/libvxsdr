cmake_minimum_required(VERSION 3.16..3.27)

set(vxsdr_lib_version_major  0)
set(vxsdr_lib_version_minor 14)
set(vxsdr_lib_version_patch  9)

set(vxsdr_lib_version ${vxsdr_lib_version_major}.${vxsdr_lib_version_minor}.${vxsdr_lib_version_patch})

project(vxsdr_lib VERSION ${vxsdr_lib_version} DESCRIPTION "VXSDR host library" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# default options for all builds
option(VXSDR_ENABLE_LOGGING "Enable logging from within libvxsdr" ON)
option(VXSDR_PYTHON_BINDINGS "Make Python bindings for libvxsdr" ON)
option(BUILD_SHARED_LIBS "Build libvxsdr as a shared library" ON)

# only make utilities and test programs when asked
option(VXSDR_BUILD_UTILITIES "Build VXSDR utility programs" OFF)
option(VXSDR_BUILD_TESTS "Build VXSDR test programs and enable testing" OFF)
# developer options off by default
option(VXSDR_STATIC_ANALYSIS "Run clang-tidy during libvxsdr builds" OFF)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type defined -- assuming Release")
    message(STATUS "Use cmake -DCMAKE_BUILD_TYPE=<type> to specify build type")
    set(CMAKE_BUILD_TYPE Release)
else()
    message(STATUS "Build type is ${CMAKE_BUILD_TYPE}")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(vxsdr_lib_source src/vxsdr.cpp
                     src/vxsdr_imp.cpp
                     src/device_commands.cpp
                     src/radio_commands.cpp
                     src/udp_command_transport.cpp
                     src/udp_data_transport.cpp
                     src/socket_utils.cpp
                     src/thread_utils.cpp
                     src/logging.cpp)
set(vxsdr_lib_header include/vxsdr.hpp)
set(vxsdr_python_src src/vxsdr_py.cpp)

set(vxsdr_utility_source utilities/vxsdr_find.cpp
                         utilities/vxsdr_set_addr.cpp
                         utilities/vxsdr_save_addr.cpp)

set(vxsdr_test_source testing/test_sleep_resolution.cpp
                      testing/test_float_convert.cpp
                      testing/test_spsc_queue.cpp
                      testing/test_net_settings.cpp
                      testing/test_localhost_xfer.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(VXSDR_FETCH_DEPENDENCIES)
    find_package(Git REQUIRED)
    include(FetchContent)
endif()

if(VXSDR_FETCH_DEPENDENCIES)
    message(STATUS "Downloading and building Boost components; this will take some time...")
    set(BOOST_ENABLE_CMAKE ON)
    if(VXSDR_STANDALONE_ASIO)
        set(vxsdr_dependency_defs "-DVXSDR_STANDALONE_ASIO")
        FetchContent_Declare(
            asio
            GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
            GIT_TAG        asio-1-30-1
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(asio)
        set(BOOST_INCLUDE_LIBRARIES lockfree)
        FetchContent_Declare(
            Boost
            GIT_REPOSITORY https://github.com/boostorg/boost.git
            GIT_TAG        boost-1.84.0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(Boost)
    else()
        set(BOOST_INCLUDE_LIBRARIES lockfree asio)
        FetchContent_Declare(
            Boost
            GIT_REPOSITORY https://github.com/boostorg/boost.git
            GIT_TAG        boost-1.84.0
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
    if(VXSDR_FETCH_DEPENDENCIES)
        set(SPDLOG_HEADER_ONLY ON)
        set(FMT_HEADER_ONLY ON)
        FetchContent_Declare(
            spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG        v1.13.0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(spdlog)
    else()
        find_package(spdlog 1.5.0 REQUIRED)
    endif()
endif()

set(vxsdr_build_defs -DVERSION_STRING="${vxsdr_lib_version}"
                     -DVERSION_MAJOR=${vxsdr_lib_version_major}
                     -DVERSION_MINOR=${vxsdr_lib_version_minor}
                     -DVERSION_PATCH=${vxsdr_lib_version_patch}
                     -DCOMPILER_INFO="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
                     -DSYSTEM_INFO="${CMAKE_SYSTEM}")

if(UNIX)
    if(NOT APPLE)
        if(NOT CYGWIN)
            message(STATUS "Using Linux target (other Unix variants not supported)")
            set(vxsdr_system_defs -DVXSDR_TARGET_LINUX -DTARGET_OS=Linux)
        else() # CYGWIN
            message(WARNING "Cygwin target is not supported, good luck!")
            set(vxsdr_system_defs -DVXSDR_TARGET_LINUX -DTARGET_OS=Unsupported)
        endif()
    elseif(APPLE)
        message(WARNING "macOS target is experimental")
        #message(STATUS "Using macOS target")
        set(vxsdr_system_defs -DVXSDR_TARGET_MACOS -DTARGET_OS=macOS)
    endif()
elseif(WIN32)
    if(NOT CYGWIN)
        message(WARNING "Windows target is experimental")
        #message(STATUS "Using Windows target")
        set(vxsdr_system_defs -DVXSDR_TARGET_WINDOWS -DTARGET_OS=Windows)
    else() # CYGWIN
        message(WARNING "Cygwin target is not supported, good luck!")
        set(vxsdr_system_defs -DVXSDR_TARGET_LINUX -DTARGET_OS=Unsupported)
    endif()
else()
    message(FATAL_ERROR "Unable to recognize the target platform")
endif()

if(NOT APPLE)
    set(CMAKE_INSTALL_RPATH $ORIGIN) # per Craig Scott cppCon 2019
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

set(vxsdr_lib_defs -DBUILD_TYPE="${CMAKE_BUILD_TYPE}")

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

    if(VXSDR_FETCH_DEPENDENCIES)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG        v2.12
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
add_library(vxsdr_lib ${vxsdr_lib_source})
generate_export_header(vxsdr_lib)
# the generated export header is in ${CMAKE_CURRENT_BINARY_DIR}
target_sources(vxsdr_lib INTERFACE ${vxsdr_lib_header} ${CMAKE_CURRENT_BINARY_DIR}/vxsdr_lib_export.h)
get_target_property(vxsdr_lib_public_headers vxsdr_lib INTERFACE_SOURCES)
set_target_properties(vxsdr_lib PROPERTIES PUBLIC_HEADER "${vxsdr_lib_public_headers}")
target_include_directories(vxsdr_lib INTERFACE ${CMAKE_CURRENT_BUILD_DIR})

if(VXSDR_STATIC_ANALYSIS)
    set_target_properties(vxsdr_lib PROPERTIES CMAKE_CXX_CLANG_TIDY "clang-tidy")
endif()
target_compile_options(vxsdr_lib PRIVATE ${vxsdr_optimization_options}
                                         ${vxsdr_warning_options}
                                         ${vxsdr_misc_options})
target_compile_definitions(vxsdr_lib PRIVATE ${vxsdr_lib_defs}
                                             ${vxsdr_build_defs}
                                             ${vxsdr_log_defs}
                                             ${vxsdr_compiler_defs}
                                             ${vxsdr_dependency_defs}
                                             ${vxsdr_system_defs})
target_include_directories(vxsdr_lib PRIVATE include ${CMAKE_CURRENT_BINARY_DIR} ${Boost_INCLUDE_DIRS})

if(VXSDR_FETCH_DEPENDENCIES)
    if(VXSDR_STANDALONE_ASIO)
        target_include_directories(vxsdr_lib PRIVATE ${asio_SOURCE_DIR}/asio/include)
        # FIXME: does asio have a better definition?
        # target_link_libraries(vxsdr_lib INTERFACE asio::asio_header_only)
    else()
        target_link_libraries(vxsdr_lib PRIVATE Boost::asio)
    endif()
    target_link_libraries(vxsdr_lib PRIVATE Boost::lockfree)
endif()

if(Make_Python_Bindings)
    target_compile_definitions(vxsdr_lib PRIVATE ${vxsdr_build_python})
    target_include_directories(vxsdr_lib PRIVATE ${pybind11_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS})
endif()

if(VXSDR_ENABLE_LOGGING)
    target_include_directories(vxsdr_lib PRIVATE ${spdlog_INCLUDE_DIRS})
    target_link_libraries(vxsdr_lib PRIVATE spdlog::spdlog_header_only)
endif()

set_target_properties(vxsdr_lib PROPERTIES SOVERSION ${vxsdr_lib_version_major})
set_target_properties(vxsdr_lib PROPERTIES VERSION ${vxsdr_lib_version})
set_target_properties(vxsdr_lib PROPERTIES OUTPUT_NAME vxsdr)

if(Make_Python_Bindings)
    pybind11_add_module(vxsdr_py ${vxsdr_python_src})
    target_compile_options(vxsdr_py PRIVATE ${vxsdr_warning_options}
                                            ${vxsdr_optimization_options}
                                            ${vxsdr_misc_options})
    target_compile_definitions(vxsdr_py PRIVATE ${vxsdr_build_python})
    target_include_directories(vxsdr_py PRIVATE include ${CMAKE_CURRENT_BINARY_DIR} ${pybind11_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS})
    target_link_libraries(vxsdr_py PRIVATE vxsdr_lib ${pybind11_LIBRARIES})
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        target_include_directories(vxsdr_py PRIVATE ${spdlog_INCLUDE_DIRS})
        target_link_libraries(vxsdr_py PRIVATE spdlog::spdlog_header_only)
    endif()
    set_target_properties(vxsdr_py PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(VXSDR_BUILD_UTILITIES)
    find_package(Boost 1.67 REQUIRED COMPONENTS program_options)
    # build utilities for finding VXSDRs and changing IP adddresses
    foreach(source_file ${vxsdr_utility_source})
        get_filename_component(target_name ${source_file} NAME_WE)
        #cmake_path(GET source_file STEM target_name)
        add_executable(${target_name} ${source_file})
        target_compile_options(${target_name} PRIVATE ${vxsdr_optimization_options}
                                                      ${vxsdr_warning_options}
                                                      ${vxsdr_misc_options})
        target_compile_definitions(${target_name} PRIVATE ${vxsdr_build_defs}
                                                          ${vxsdr_compiler_defs}
                                                          ${vxsdr_system_defs})
        target_include_directories(${target_name} PRIVATE include)
        target_include_directories(${target_name} PRIVATE ${Boost_INCLUDE_DIRS})
        target_link_libraries(${target_name} PRIVATE Boost::program_options Threads::Threads)
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
        target_compile_definitions(${target_name} PRIVATE ${vxsdr_lib_defs}
                                                          ${vxsdr_build_defs}
                                                          ${vxsdr_log_defs}
                                                          ${vxsdr_compiler_defs}
                                                          ${vxsdr_system_defs})
        target_include_directories(${target_name} PRIVATE include)
        target_include_directories(${target_name} PRIVATE ${Boost_INCLUDE_DIRS})
        target_link_libraries(${target_name} PRIVATE Threads::Threads)
    endforeach()
    add_test(sleep_resolution test_sleep_resolution 2e-4 1000)
    add_test(queue_speed test_spsc_queue 10 160e6)
    add_test(float_convert_speed test_float_convert 0.2 160e6)
endif()

include(GNUInstallDirs)
install(TARGETS vxsdr_lib)

if(Make_Python_Bindings)
    install(TARGETS vxsdr_py DESTINATION ${Python3_SITELIB})
endif()

if(VXSDR_BUILD_UTILITIES)
    install(TARGETS vxsdr_find RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(TARGETS vxsdr_set_addr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(TARGETS vxsdr_save_addr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

include(CMakePackageConfigHelpers)
configure_package_config_file(vxsdrConfig.cmake.in
                              ${CMAKE_CURRENT_BINARY_DIR}/vxsdrConfig.cmake
                              INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vxsdr)
write_basic_package_version_file(vxsdrConfigVersion.cmake
                                 VERSION ${vxsdr_version}
                                 COMPATIBILITY SameMajorVersion)
set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS WORLD_READ WORLD_EXECUTE)
# otherwise ${CMAKE_INSTALL_LIBDIR}/cmake/vxsdr created below has
# default permissions for the installer (who can be root)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vxsdrConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/vxsdrConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vxsdr)