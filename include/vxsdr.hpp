// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <complex>
#include <optional>
#include <vector>
#include <array>
#include <memory>
#include <string>
#include <ratio>
#include <map>
#include <chrono>
using namespace std::chrono_literals;

#include "vxsdr_lib_export.h"

/*! @file vxsdr.hpp
    @class vxsdr
    @brief The vxsdr class contains the host interface for the VXSDR
*/

class VXSDR_LIB_EXPORT vxsdr {
  public:
  /*!
    @enum transport_type
    @brief The @p transport_type describes the transports used to send and receive data and commands.
    (UDP is currently the only transport supported.)
  */
    enum transport_type { TRANSPORT_TYPE_UDP = 1, TRANSPORT_TYPE_PCIE };
  /*!
    @enum stream_state
    @brief The @p stream_state type reports the status of TX or RX data streaming.
  */
    enum stream_state { STREAM_STOPPED = 0, STREAM_RUNNING, STREAM_WAITING_FOR_START, STREAM_ERROR };

  /*!
    @enum async_message_handler
    @brief The @p async_message_handler type controls how asynchronous messages (including overflow, underflow, and sequence errors) are reported.
  */
    enum async_message_handler { ASYNC_NULL = 0, ASYNC_BRIEF_STDERR, ASYNC_FULL_STDERR, ASYNC_FULL_LOG, ASYNC_THROW };

  /*!
    @struct async_message_exception
    @brief The @p async_message_exception type is used to report asynchronous messages when the message handler is asked to throw exceptions by
    specifying @p ASYNC_THROW as the reporting method. The used must define a handler for this type which encloses all uses of the @p vxsdr class;
    otherwise, any async message will result in an unhandled exception, which terminates the program.
  */
  struct async_message_exception : public std::runtime_error {
    public:
      async_message_exception(const std::string& msg) : std::runtime_error{msg} {}
  };

  /*!
    @brief The @p wire_sample type is used for data transfer between the host and device; wire samples may be translated
    to and from other types by the host library.
  */
    using wire_sample = std::complex<int16_t>;

  /*!
    @brief The @p filter_coefficient type is used for representing filter coefficients.
  */
    using filter_coefficient = std::complex<int16_t>;

  /*!
    @brief The @p duration type is used for acquisition and wait durations; it has a 1 nanosecond resolution, although the
    granularity of the device clock may be larger.
  */
    using duration = std::chrono::duration<int64_t, std::ratio<1, 1000000000>>;

  /*!
    @brief The @p time_point type is used for start times.
  */
    using time_point = std::chrono::time_point<std::chrono::system_clock, duration>;

  /*!
      @brief Constructor for the @p vxsdr host interface class.
      @param config a std::map<std::string, int64_t> containing configuration settings for the host interface;
      if a setting is not included in this map, it is left at the default value.

      @details The @p config map must include information on how to
      communicate with the VXSDR device; for example, with UDP transport,
      the settings map must include the following key-value pairs, which are not
      set by default:
        - "udp_transport:local_address" = ipv4 local address in host order
        - "udp_transport:device_address" = ipv4 device address in host order
    */
    explicit vxsdr(const std::map<std::string, int64_t>& config = {});

    //! Destructor for the vxsdr host interface class.
    ~vxsdr() noexcept;

    vxsdr(const vxsdr&) = delete;
    vxsdr& operator=(const vxsdr&) = delete;
    vxsdr(vxsdr&&) = delete;
    vxsdr& operator=(vxsdr&&) = delete;

    /*!
      @brief Gets the version number of this library.
      @returns the library version number as 10,000 * major_version + 100 * minor_version + patch_level
    */
    uint32_t get_library_version();

    /*!
      @brief Gets the packet version number supported by this library.
      @returns the packet version number as 10,000 * major_version + 100 * minor_version + patch_level
    */
    uint32_t get_library_packet_version();

    /*!
      @brief Gets more detailed information on this library.
      @returns a std::vector of std::strings with version and build information
    */
    std::vector<std::string> get_library_details();

    /*!
      @brief Request basic information from the device.
      @returns a std::optional with a std::array containing:
          - device identifier
          - FPGA code version
          - MCU code version,
          - device serial number,
          - packet version supported,
          - wire sample data format,
          - number of subdevices,
          - maximum sample payload size in bytes
    */
    std::optional<std::array<uint32_t, 8>> hello();

    /*!
      @brief Reset the device.
      @returns @b true if the command succeeds, @b false otherwise
    */
    bool reset();

    /*!
      @brief Clear the device status.
      @returns @b true if the command succeeds, @b false otherwise
      @param subdev the subdevice number
    */
    bool clear_status(const uint8_t subdev = 0);

    // FIXME: document status outputs
    /*!
      @brief Get the device status.
      @returns a std::optional with a std::array containing device-dependent status information
      @param subdev the subdevice number
    */
    std::optional<std::array<uint32_t, 8>> get_status(const uint8_t subdev = 0);

    /*!
      @brief Get the size of the device transmit and receive buffers.
      @returns a std::optional with a std::array containing the transmit and receive buffer sizes in bytes, in that order
      @param subdev the subdevice number
    */
    std::optional<std::array<uint32_t, 2>> get_buffer_info(const uint8_t subdev = 0);

    /*!
      @brief Get the current number of bytes used in the device transmit and receive buffers.
      @returns a std::optional with a std::array containing the transmit and receive buffer usage in bytes, in that order
      @param subdev the subdevice number

    */
    std::optional<std::array<uint32_t, 2>> get_buffer_use(const uint8_t subdev = 0);

    /*!
      @brief Get the device's limit on maximum sample payload size in bytes, which does not account for any
      transport limits. Note that for UDP transport, the maximum packet size, which must not exceed the network MTU,
      is the maximum sample payload size plus the size of the VXSDR packet header, plus the stream spec and time spec,
      plus the size of the UDP and IPv4 headers. The packet header, stream spec, and time spec are 8 bytes each,
      the UDP header is 8 bytes, and IPv4 header is typically 20 bytes, for a total of 52 bytes.
      @returns a std::optional with the maximum sample payload in bytes
    */
    std::optional<unsigned> get_max_payload_bytes();

    /*!
      @brief Set the maximum sample payload size in bytes for transport to and from the device.
      The sample payload size must be between 1024 and 16384 bytes, and will be adjusted by the device to match
      the device's sample granularity. For UDP transport, we strongly recommend using the UDP default of 8192,
      which is compatible with a typical jumbo packet MTU of 9000; only use a larger value if you are certain your
      network card supports it. A larger MTU can be specified using the udp_data_transport:mtu_bytes setting.
      @returns @p true if the requested size is set, @p false otherwise
      @param max_payload_bytes the maximum sample payload size in bytes
    */
    bool set_max_payload_bytes(const unsigned max_payload_bytes);

    /*!
      @brief Get the number of subdevices.
      @returns a std::optional with the number of subdevices
    */
    std::optional<unsigned> get_num_subdevices();

    /*!
      @brief Get the number of available sensors.
      @returns  a std::optional with the number of available sensors
      @param subdev the subdevice number
    */
    std::optional<unsigned> get_num_sensors(const uint8_t subdev = 0);

    /*!
      @brief Get the name of a sensor.
      @returns a std::optional with a std:string giving the name
      @param sensor_number the sensor number
      @param subdev the subdevice number
    */
    std::optional<std::string> get_sensor_name(const unsigned sensor_number, const uint8_t subdev = 0);

    /*!
      @brief Get the value of a sensor.
      @returns a std::optional with a double giving the sensor reading
      @param sensor_number the desired sensor number
      @param subdev the subdevice number
    */
    std::optional<double> get_sensor_reading(const unsigned sensor_number, const uint8_t subdev = 0);

    /*!
      @brief Get the device time immediately.
      @returns a std::optional with a vxsdr::time_point containing the device time
    */
    std::optional<vxsdr::time_point> get_time_now();

    /*!
      @brief Set the device time immediately.
      @returns @b true if the command succeeds, @b false otherwise
      @param t the time to set
    */
    bool set_time_now(const vxsdr::time_point& t);

    /*!
      @brief Set the device time at the next PPS received by the device.
      @returns @b true if the command succeeds, @b false otherwise; note that the
      device waits for the next PPS to respond, so this command may not return for nearly
      1 second
      @param t the time to set
    */
    bool set_time_next_pps(const vxsdr::time_point& t);

    /*!
      @brief Get the status of the device timing references.
      @returns a std::optional with a std::array containing
          - the external PPS lock status
          - external 10 MHz lock status
          - internal reference oscillator lock status
        Where @b true means the device is locked to that reference, and @b false means not locked.
        Note that the first two will be false if no external PPS and 10 MHz are connected.
    */
    std::optional<std::array<bool, 3>> get_timing_status();

    /*!
      @brief Get the resolution of the device's clock.
      @returns a std::optional with a double containing the resolution in seconds
    */
    std::optional<double> get_timing_resolution();

    /*!
      @brief Set the IPv4 address of the device.
      This will disconnect the device if the given address is different from its current address.
      The IP address is not saved to nonvolatile memory in the device by this command.
      @returns @p true if the address is set, @p false otherwise
      @param device_address the device address to set
    */
    bool set_ipv4_address(const std::string& device_address);

    /*!
      @brief Save the IPv4 address of the device to nonvolatile memory in the device.
      The IP address provided must be the same as the device's current IP address. A special-purpose program
      is required to change and save a device's IP address.
      @returns @p true if the address is saved, @p false otherwise
      @param device_address the device address to save
    */
    bool save_ipv4_address(const std::string& device_address);

    /*!
      @brief
      Broadcast a device discovery packet to the given IPv4 broadcast address, and return the
      IPv4 addresses of the devices which respond.
      @returns a std::vector containing the IPv4 addresses of discovered devices (which may have zero length if no devices are
      found)
      @param local_addr the local address to send from
      @param broadcast_addr the broadcast address
      @param timeout_s the time in seconds to wait for responses
    */
    std::vector<std::string> discover_ipv4_addresses(const std::string& local_addr,
                                                            const std::string& broadcast_addr,
                                                            const double timeout_s = 10);

    /*!
      @brief Determine if the transmit RF section is enabled.
      @returns @b true if the command succeeds and the section is enabled, @b false otherwise
      @param subdev the subdevice number
    */
    bool get_tx_enabled(const uint8_t subdev = 0);

    /*!
      @brief Determine if the receive RF section is enabled.
      @returns @b true if the command succeeds and the section is enabled, @b false otherwise
      @param subdev the subdevice number
    */
    bool get_rx_enabled(const uint8_t subdev = 0);

    /*!
      @brief Enable or disable the transmit RF section.
      @returns @b true if the command succeeds, @b false otherwise
      @param enabled the desired state
      @param subdev the subdevice number
    */
    bool set_tx_enabled(const bool enabled, const uint8_t subdev = 0);

    /*!
      @brief Enable or disable the receive RF section.
      @returns @b true if the command succeeds, @b false otherwise
      @param enabled the desired state
      @param subdev the subdevice number
    */
    bool set_rx_enabled(const bool enabled, const uint8_t subdev = 0);


    /*!
      @brief Get the transmit center frequency range.
      @returns a std::optional with a std::array containing the minimum and maximum frequencies in Hz
      @param subdev the subdevice number
    */
    std::optional<std::array<double, 2>> get_tx_freq_range(const uint8_t subdev = 0);

    /*!
      @brief Get the receive center frequency range.
      @returns a std::optional with a std::array containing the minimum and maximum frequencies in Hz
      @param subdev the subdevice number
    */
    std::optional<std::array<double, 2>> get_rx_freq_range(const uint8_t subdev = 0);

    /*!
      @brief Get the transmit center frequency.
      @returns a std::optional with the center frequency in Hz
      @param subdev the subdevice number
    */
    std::optional<double> get_tx_freq(const uint8_t subdev = 0);

    /*!
      @brief Get the receive center frequency.
      @returns a std::optional with the center frequency in Hz
      @param subdev the subdevice number
    */
    std::optional<double> get_rx_freq(const uint8_t subdev = 0);

    /*!
      @brief Set the transmit center frequency.
      @returns @b true if the command succeeds, @b false otherwise
      @param freq_hz the desired frequency in Hz
      @param subdev the subdevice number
    */
    bool set_tx_freq(const double freq_hz, const uint8_t subdev = 0);

    /*!
      @brief Set the receive center frequency.
      @returns @b true if the command succeeds, @b false otherwise
      @param freq_hz the desired frequency in Hz
      @param subdev the subdevice number
    */
    bool set_rx_freq(const double freq_hz, const uint8_t subdev = 0);

    /*!
      @brief Get the transmit intermediate frequency.
      @returns a std::optional with the intermediate frequency in Hz
      @param subdev the subdevice number
    */
    std::optional<double> get_tx_if_freq(const uint8_t subdev = 0);

    /*!
      @brief Get the receive intermediate frequency.
      @returns a std::optional with the intermediate frequency in Hz
      @param subdev the subdevice number
    */
    std::optional<double> get_rx_if_freq(const uint8_t subdev = 0);

    /*!
      @brief Get the number of transmit tuning stages.
      @returns  a std::optional with the number of stages
      @param subdev the subdevice number
    */
    std::optional<unsigned> get_tx_num_freq_stages(const uint8_t subdev = 0);

    /*!
      @brief Get the number of receive tuning stages.
      @returns  a std::optional with the number of stages
      @param subdev the subdevice number
    */
    std::optional<unsigned> get_rx_num_freq_stages(const uint8_t subdev = 0);

    /*!
      @brief Get the name of a transmit tuning stage.
      @returns a std::optional with a std::string containing the name
      @param stage_num the number of the stage
      @param subdev the subdevice number
    */
    std::optional<std::string> get_tx_freq_stage_name(const unsigned stage_num, const uint8_t subdev = 0);

    /*!
      @brief Get the name of a receive tuning stage.
      @returns a std::optional with a a std::string containing the name
      @param stage_num the number of the stage
      @param subdev the subdevice number
    */
    std::optional<std::string> get_rx_freq_stage_name(const unsigned stage_num, const uint8_t subdev = 0);

    /*!
      @brief Get the center frequency range for a transmit tuning stage.
      @returns a std::optional with a std::array containing the minimum and maximum frequencies in Hz
      @param stage_num the number of the stage
      @param subdev the subdevice number
    */
    std::optional<std::array<double, 2>> get_tx_freq_range_stage(const unsigned stage_num, const uint8_t subdev = 0);

    /*!
      @brief Get the center frequency range for a receive tuning stage.
      @returns a std::optional with a std::array containing the minimum and maximum frequencies in Hz
      @param stage_num the number of the stage
      @param subdev the subdevice number
    */
    std::optional<std::array<double, 2>> get_rx_freq_range_stage(const unsigned stage_num, const uint8_t subdev = 0);

    /*!
      @brief Get the center frequency of a transmit tuning stage.
      @param stage_num the number of the stage
      @returns a std::optional with the center frequency in Hz
      @param subdev the subdevice number
    */
    std::optional<double> get_tx_freq_stage(const unsigned stage_num, const uint8_t subdev = 0);

    /*!
      @brief Get the center frequency of a receive tuning stage.
      @returns a std::optional with the center frequency in Hz
      @param stage_num the number of the stage
      @param subdev the subdevice number
    */
    std::optional<double> get_rx_freq_stage(const unsigned stage_num, const uint8_t subdev = 0);

    /*!
      @brief Set the center frequency of a transmit tuning stage.
      @returns @b true if the command succeeds, @b false otherwise
      @param freq_hz the desired frequency in Hz
      @param stage_num the number of the stage
      @param subdev the subdevice number
    */
    bool set_tx_freq_stage(const double freq_hz, const unsigned stage_num, const uint8_t subdev = 0);

    /*!
      @brief Set the center frequency of a receive tuning stage.
      @returns @b true if the command succeeds, @b false otherwise
      @param freq_hz the desired frequency in Hz
      @param stage_num the number of the stage
      @param subdev the subdevice number
    */
    bool set_rx_freq_stage(const double freq_hz, const unsigned stage_num, const uint8_t subdev = 0);

    /*!
      @brief Get the gain range for transmit channels.
      @returns a std::optional with a std::array containing the minimum and maximum gains in dB
      @param subdev the subdevice number
    */
    std::optional<std::array<double, 2>> get_tx_gain_range(const uint8_t subdev = 0);

    /*!
      @brief Get the gain range for receive channels.
      @returns a std::optional with a std::array containing the minimum and maximum gains in dB
      @param subdev the subdevice number
    */
    std::optional<std::array<double, 2>> get_rx_gain_range(const uint8_t subdev = 0);

    /*!
      @brief Get the gain for a transmit channel.
      @returns a std::optional with the gain in dB
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<double> get_tx_gain(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the gain for a receive channel.
      @returns a std::optional with the gain in dB
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<double> get_rx_gain(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the gain for a transmit channel.
      @returns @b true if the command succeeds, @b false otherwise
      @param gain_db the desired gain in dB
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_tx_gain(const double gain_db, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the gain for a receive channel.
      @returns @b true if the command succeeds, @b false otherwise
      @param gain_db the desired gain in dB
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_rx_gain(const double gain_db, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the number of transmit gain stages.
      @returns  a std::optional with the number of stages
      @param subdev the subdevice number
    */
    std::optional<unsigned> get_tx_num_gain_stages(const uint8_t subdev = 0);

    /*!
      @brief Get the number of receive gain stages.
      @returns  a std::optional with the number of stages
      @param subdev the subdevice number
    */
    std::optional<unsigned> get_rx_num_gain_stages(const uint8_t subdev = 0);

    /*!
      @brief Get the name of a transmit gain stage.
      @returns a std::optional with a std::string containing the name
      @param stage_num the number of the stage
      @param subdev the subdevice number
    */
    std::optional<std::string> get_tx_gain_stage_name(const unsigned stage_num, const uint8_t subdev = 0);

    /*!
      @brief Get the name of a receive gain stage.
      @returns a std::optional with a std::string containing the name
      @param stage_num the number of the stage
      @param subdev the subdevice number
    */
    std::optional<std::string> get_rx_gain_stage_name(const unsigned stage_num, const uint8_t subdev = 0);

    /*!
      @brief Get the gain range for a transmit gain control stage.
      @returns a std::optional with a std::array containing the minimum and maximum gains in dB
      @param stage_num the number of the stage
      @param subdev the subdevice number
    */
    std::optional<std::array<double, 2>> get_tx_gain_range_stage(const unsigned stage_num, const uint8_t subdev = 0);

    /*!
      @brief Get the gain range for a receive gain control stage.
      @returns a std::optional with a std::array containing the minimum and maximum gains in dB
      @param stage_num the number of the stage
      @param subdev the subdevice number
    */
    std::optional<std::array<double, 2>> get_rx_gain_range_stage(const unsigned stage_num, const uint8_t subdev = 0);

    /*!
      @brief Get the gain for a transmit gain control stage.
      @returns a std::optional with the gain in dB
      @param stage_num the number of the stage
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<double> get_tx_gain_stage(const unsigned stage_num, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the gain for a receive gain control stage.
      @returns a std::optional with the gain in dB
      @param stage_num the number of the stage
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<double> get_rx_gain_stage(const unsigned stage_num, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the gain for a transmit gain control stage.
      @returns @b true if the command succeeds, @b false otherwise
      @param gain_db the desired gain in dB
      @param stage_num the number of the stage
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_tx_gain_stage(const double gain_db, const unsigned stage_num, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the gain for a receive gain control stage.
      @returns @b true if the command succeeds, @b false otherwise
      @param gain_db the desired gain in dB
      @param stage_num the number of the stage
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_rx_gain_stage(const double gain_db, const unsigned stage_num, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the transmit sample rate range.
      @returns a std::optional with a std::array containing the minimum and maximum sample rates in samples/s
      @param subdev the subdevice number
    */
    std::optional<std::array<double, 2>> get_tx_rate_range(const uint8_t subdev = 0);

    /*!
      @brief Get the receive sample rate range.
      @returns a std::optional with a std::array containing the minimum and maximum sample rates in samples/s
      @param subdev the subdevice number
    */
    std::optional<std::array<double, 2>> get_rx_rate_range(const uint8_t subdev = 0);

    /*!
      @brief Get the transmit sample rate.
      @returns a std::optional with the rate in samples/s
      @param subdev the subdevice number
    */
    std::optional<double> get_tx_rate(const uint8_t subdev = 0);

    /*!
      @brief Get the receive sample rate.
      @returns a std::optional with the rate in samples/s
      @param subdev the subdevice number
    */
    std::optional<double> get_rx_rate(const uint8_t subdev = 0);

    /*!
      @brief Set the transmit sample rate.
      @returns @b true if the command succeeds, @b false otherwise
      @param rate_samples_sec the desired rate in samples/s
      @param subdev the subdevice number
    */
    bool set_tx_rate(const double rate_samples_sec, const uint8_t subdev = 0);

    /*!
      @brief Set the receive sample rate.
      @returns @b true if the command succeeds, @b false otherwise
      @param rate_samples_sec the desired rate in samples/s
      @param subdev the subdevice number
    */
    bool set_rx_rate(const double rate_samples_sec, const uint8_t subdev = 0);

    /*!
      @brief Get the number of transmit output ports for a channel.
      @returns a std::optional with the number of output ports
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<unsigned> get_tx_num_ports(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
     @brief Get the number of receive input ports for a channel.
     @returns a std::optional with the number of input ports
     @param subdev the subdevice number
     @param channel the channel number within the subdevice
   */
    std::optional<unsigned> get_rx_num_ports(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the transmit output port.
      @returns a std::optional with the port number
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<unsigned> get_tx_port(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the receive input port.
      @returns a std::optional with the port number
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<unsigned> get_rx_port(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the name of a transmit output port.
      @returns a std::optional with a std::string containing the name of the specified output port
      @param port_num the port number
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::string> get_tx_port_name(const unsigned port_num, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the name of a receive input port.
      @returns a std::optional with a std::string containing the name of the specified input port
      @param port_num the port number
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::string> get_rx_port_name(const unsigned port_num, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the transmit output port.
      @returns @b true if the command succeeds, @b false otherwise
      @param port_num the desired output port number
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_tx_port(const unsigned port_num, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the receive input port.
      @returns @b true if the command succeeds, @b false otherwise
      @param port_num the desired input port number
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_rx_port(const unsigned port_num, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the number of transmit channels.
      @returns a std::optional with the number of channels
      @param subdev the subdevice number
    */
    std::optional<unsigned> get_tx_num_channels(const uint8_t subdev = 0);

    /*!
      @brief Get the number of receive channels.
      @returns a std::optional with the number of channels
      @param subdev the subdevice number
    */
    std::optional<unsigned> get_rx_num_channels(const uint8_t subdev = 0);

    /*!
      @brief Get the current radio transmit stream state.
      @returns  a std::optional with a stream_state containing the transmit stream state
      @param subdev the subdevice number
    */
    std::optional<vxsdr::stream_state> get_tx_stream_state(const uint8_t subdev = 0);

    /*!
      @brief Get the current radio receive stream state.
      @returns  a std::optional with a stream_state containing the receive stream state
      @param subdev the subdevice number
    */
    std::optional<vxsdr::stream_state> get_rx_stream_state(const uint8_t subdev = 0);

    /*!
      @brief Determine whether the TX LO is locked.
      @returns @b true if the command succeeds and the TX LO is locked, @b false otherwise
      @param subdev the subdevice number
    */
    bool get_tx_lo_locked(const uint8_t subdev = 0);

    /*!
      @brief Determine whether the RX LO is locked.
      @returns @b true if the command succeeds and the RX LO is locked, @b false otherwise
      @param subdev the subdevice number
    */
    bool get_rx_lo_locked(const uint8_t subdev = 0);

    /*!
      @brief Determine if the transmit external LO input is enabled.
      @returns @b true if the command succeeds and the external LO is enabled, @b false otherwise
      @param subdev the subdevice number
    */
    bool get_tx_external_lo_enabled(const uint8_t subdev = 0);

    /*!
      @brief Determine if the receive external LO input is enabled.
      @returns @b true if the command succeeds and the external LO is enabled, @b false otherwise
      @param subdev the subdevice number
    */
    bool get_rx_external_lo_enabled(const uint8_t subdev = 0);

    /*!
      @brief Enable or disable the transmit external LO input.
      @returns @b true if the command succeeds, @b false otherwise
      @param enabled the desired state
      @param subdev the subdevice number
    */
    bool set_tx_external_lo_enabled(const bool enabled, const uint8_t subdev = 0);

    /*!
      @brief Enable or disable the receive external LO input.
      @returns @b true if the command succeeds, @b false otherwise
      @param enabled the desired state
      @param subdev the subdevice number
    */
    bool set_rx_external_lo_enabled(const bool enabled, const uint8_t subdev = 0);

    /*!
      @brief Get the transmit frontend FIR filter's length (maximum number of complex coefficients).
      @returns a std::optional with the length (zero if no filter is present)
      @param subdev the subdevice number
    */
    std::optional<unsigned> get_tx_filter_length(const uint8_t subdev = 0);

    /*!
      @brief Get the receive frontend FIR filter's length (maximum number of complex coefficients).
      @returns a std::optional with the with the length (zero if no filter is present)
      @param subdev the subdevice number
    */
    std::optional<unsigned> get_rx_filter_length(const uint8_t subdev = 0);

    /*!
      @brief Enable or disable the transmit frontend FIR filter.
      @returns @b true if the command succeeds, @b false otherwise
      @param enabled the desired state
      @param subdev the subdevice number
    */
    bool set_tx_filter_enabled(const bool enabled, const uint8_t subdev = 0);

    /*!
      @brief Enable or disable the receive frontend FIR filter.
      @returns @b true if the command succeeds, @b false otherwise
      @param enabled the desired state
      @param subdev the subdevice number
    */
    bool set_rx_filter_enabled(const bool enabled, const uint8_t subdev = 0);

    /*!
      @brief Get the transmit frontend FIR filter coefficients.
      @returns a std::optional with a std::vector<filter_coefficient> containing the filter coefficients
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::vector<filter_coefficient>> get_tx_filter_coeffs(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the receive frontend FIR filter coefficients.
      @returns a std::optional with a std::vector<filter_coefficient> containing the filter coefficients
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::vector<filter_coefficient>> get_rx_filter_coeffs(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the transmit frontend FIR filter coefficients.
      @returns @b true if the command succeeds, @b false otherwise
      @param coeffs a std::vector<filter_coefficient> containing the filter coefficients
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_tx_filter_coeffs(const std::vector<filter_coefficient>& coeffs, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the receive frontend FIR filter coefficients.
      @returns @b true if the command succeeds, @b false otherwise
      @param coeffs a std::vector<filter_coefficient> containing the filter coefficients
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_rx_filter_coeffs(const std::vector<filter_coefficient>& coeffs, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the IQ DC bias for a transmit channel.
      @returns a std::optional with a std::array containing @f$i_{bias}, q_{bias}@f$
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::array<double, 2>> get_tx_iq_bias(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the IQ DC bias for a receive channel.
      @returns a std::optional with a std::array containing @f$i_{bias}, q_{bias}@f$
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::array<double, 2>> get_rx_iq_bias(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the IQ DC bias for the transmitter to control LO feedthrough.
      @details Bias correction adds a constant offset to the data stream:
           \f[
            \begin{bmatrix} i_{out} \\ q_{out} \end{bmatrix}
            =
              \begin{bmatrix} i_{in}   \\ q_{in} \end{bmatrix} +
              \begin{bmatrix} i_{bias} \\ q_{bias} \end{bmatrix}
           \f]
      On transmit, this adjusts the mixer inputs so that local oscillator feedthrough can be minimized.

      The values are normalized so that @f$1 \ge i_{bias} \ge -1@f$ and @f$1 \ge q_{bias} \ge -1@f$, where
      the extremes are the maximum values allowed by the device.

      Most devices have a fixed precision (e.g. 12 bits) for the coefficients.
      The actual coefficients used can be determined by reading the coefficients back after they are set
      with the @p get_tx_iq_bias() command.

      @returns @b true if the command succeeds, @b false otherwise
      @param bias   a std::array containing @f$i_{bias}, q_{bias}@f$ in that order
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_tx_iq_bias(const std::array<double, 2> bias, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the IQ DC bias for the receiver to control LO feedthrough.
      @details  Most devices have a fixed precision (e.g. 12 bits) for the coefficients.
      The actual coefficients used can be determined by reading the coefficients back after they are set
      with the @p get_rx_iq_bias() command.

      @returns @b true if the command succeeds, @b false otherwise
      @param bias   a std::array containing @f$i_{bias}, q_{bias}@f$ in that order
      (see the definition in the @p set_tx_iq_bias() section)
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_rx_iq_bias(const std::array<double, 2> bias, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the IQ correction for a transmit channel to control image rejection.
      @details IQ correction transforms the sample stream using the following equation:
           \f[
            \begin{bmatrix} i_{out} \\ q_{out} \end{bmatrix}
            =
              \begin{bmatrix}
              a_{ii} & a_{iq} \\
              a_{qi} & a_{qq}
              \end{bmatrix}
            \begin{bmatrix} i_{in} \\ q_{in} \end{bmatrix}
           \f]
      If no transformation is needed, the matrix can be the identity matrix (which is the default).
      Normally, it is sufficient to use only two coefficients to transform the data:
           \f[
            \begin{bmatrix} i_{out} \\ q_{out} \end{bmatrix}
            =
              \begin{bmatrix}
              1 & 0 \\
              a_{qi} & a_{qq}
              \end{bmatrix}
            \begin{bmatrix} i_{in} \\ q_{in} \end{bmatrix}
           \f]
      where @f$a_{qi} \approx 0@f$ and @f$a_{qq} \approx 1@f$.

      Most devices have a fixed precision (e.g. 12 bits) for the coefficients. Some devices also restrict
      one of the four coefficients to be zero for efficiency reasons; for these devices,
      a Givens rotation is used to zero that coefficient if needed.

      The actual coefficients used can be determined by reading the coefficients back after they are set
      with the @p get_tx_iq_corr() command.
      @returns @b true if the command succeeds, @b false otherwise
      @param corr    a std::array containing @f$a_{ii}, a_{iq}, a_{qi}, a_{qq}@f$ (in that order),
          which are the coefficients of the matrix which transforms the IQ data on transmission.
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_tx_iq_corr(const std::array<double, 4> corr, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the IQ correction for a receive channel to control image rejection.
      @details  Most devices have a fixed precision and restrictions on which coefficients can be nonzero (as discussed in the
      @p set_tx_iq_corr() section). The actual coefficients used can be determined by reading the coefficients back after they are set
      with the @p get_rx_iq_corr() command.
      @returns @b true if the command succeeds, @b false otherwise
      @param corr    a std::array containing @f$a_{ii}, a_{iq}, a_{qi}, a_{qq}@f$ (in that order),
          which are the coefficients of the matrix which transforms the IQ data on reception
          (see the definition in the @p set_tx_iq_corr() section)
      @param subdev the subdevice number
      @param channel the channel number within the subdevice

    */
    bool set_rx_iq_corr(const std::array<double, 4> corr, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the IQ correction for a transmit channel.
      @returns a std::optional with a std::array containing @f$a_{ii}, a_{iq}, a_{qi}, a_{qq}@f$ (in that order),
          which are the coefficients of the matrix which transforms the IQ data on transmission.

      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::array<double, 4>> get_tx_iq_corr(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the IQ correction for a receive channel.
      @returns a std::optional with a std::array containing @f$a_{ii}, a_{iq}, a_{qi}, a_{qq}@f$ (in that order),
          which are the coefficients of the matrix which transforms the IQ data on reception.

      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::array<double, 4>> get_rx_iq_corr(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Start transmitting at time @p t until @p n samples are sent.
      If @p t is less than the current time, start immediately;
      if @p n is 0, continue until a stop command is sent.
      @returns @b true if the command succeeds, @b false otherwise
      @param t the start time
      @param n the number of samples to send
      @param subdev the subdevice number
    */
    bool tx_start(const vxsdr::time_point& t,
                                        const uint64_t n = 0,
                                        const uint8_t subdev = 0);

    /*!
      @brief Start receiving at time @p t until @p n samples are received.
      If @p t is less than the current time, start immediately;
      if @p n is 0, continue until a stop command is sent.
      @returns @b true if the command succeeds, @b false otherwise
      @param t the start time
      @param n the number of samples to receive
      @param subdev the subdevice number
    */
    bool rx_start(const vxsdr::time_point& t,
                                        const uint64_t n = 0,
                                        const uint8_t subdev = 0);

    /*!
      @brief Start transmitting at time @p t until @p n samples are sent,
      repeating with a delay after each transmission of @p t_delay, for @p n_repeat iterations.
      If @p t is less than or equal to the current time, start immediately;
      if @p n_repeat is 0, continue until a stop command is sent.
      If @p n_samples is small enough that the entire looped waveform fits in the device's transmit buffer
      (whose size can be checked with the @p get_buffer_info() command),
      the samples need only be sent once; otherwise, they must be resent for each repetition.
      @returns @b true if the command succeeds, @b false otherwise
      @param t the start time
      @param n the number of samples to send
      @param t_delay the delay between repeat transmissions
      @param n_repeat the total number of transmissions
      @param subdev the subdevice number
    */
    bool tx_loop(const vxsdr::time_point& t,
                                        const uint64_t n,
                                        const vxsdr::duration& t_delay = vxsdr::duration::zero(),
                                        const uint32_t n_repeat = 0,
                                        const uint8_t subdev = 0);

    /*!
      @brief Start receiving at time @p t until @p n samples are received,
      repeating with a delay after each reception of @p t_delay, for @p n_repeat iterations.
      If @p t is less than or equal to the current time, start immediately;
      if @p n_repeat is 0, continue until a stop command is sent.
      @returns @b true if the command succeeds, @b false otherwise
      @param t the start time
      @param n the number of samples to receive
      @param t_delay the delay between repeat receptions
      @param n_repeat the total number of receptions
      @param subdev the subdevice number
    */
    bool rx_loop(const vxsdr::time_point& t,
                                        const uint64_t n,
                                        const vxsdr::duration& t_delay = vxsdr::duration::zero(),
                                        const uint32_t n_repeat = 0,
                                        const uint8_t subdev = 0);

    /*!
      @brief Stop transmitting at time @p t.
      If @p t is less than or equal to the current time, stop immediately;
      note that the default constructor sets @p t to zero, which will cause an immediate stop.
      @returns @b true if the command succeeds, @b false otherwise
      @param t the stop time
      @param subdev the subdevice number
    */
    bool tx_stop(const vxsdr::time_point& t = {}, const uint8_t subdev = 0);

    /*!
      @brief Stop receiving at time @p t.
      If @p t is less than the current time, stop immediately;
      note that the default constructor sets @p t to zero, which will cause an immediate stop.
      @returns @b true if the command succeeds, @b false otherwise
      @param t the stop time
      @param subdev the subdevice number
    */
    bool rx_stop(const vxsdr::time_point& t = {}, const uint8_t subdev = 0);

    /*!
      @brief Send transmit data to the device.
      @returns the number of samples placed in the queue for transmission
      @param data the @p complex<int16_t> vector of data to be sent
      @param n_requested the number of samples to be sent (0 means use data.size();
          if data.size() \< n_requested, only data.size() will be sent)
      @param subdev the subdevice number
      @param timeout_s timeout in seconds
    */
    size_t put_tx_data(const std::vector<std::complex<int16_t>>& data,
                       size_t n_requested = 0,
                       const uint8_t subdev   = 0,
                       const double timeout_s = 10);

    /*!
      @brief Send transmit data to the device.
      @returns the number of samples placed in the queue for transmission
      @param data the @p complex<float> vector of data to be sent
      @param n_requested the number of samples to be sent (0 means use data.size();
          if data.size() \< n_requested, only data.size() will be sent)
      @param subdev the subdevice number
      @param timeout_s timeout in seconds
    */
    size_t put_tx_data(const std::vector<std::complex<float>>& data,
                       size_t n_requested = 0,
                       const uint8_t subdev   = 0,
                       const double timeout_s = 10);

    /*!
      @brief Receive data from the device and return it in a vector.
      @returns the number of samples received before a sequence error, or @p n_desired if no sequence errors occur
      @param data the @p complex<int16_t> vector for the received data
      @param n_requested the number of samples to be received (0 means use data.size();
          if data.size() \< n_requested, only data.size() will be received)
      @param subdev the subdevice number
      @param timeout_s timeout in seconds
    */
    size_t get_rx_data(std::vector<std::complex<int16_t>>& data,
                       const size_t n_requested = 0,
                       const uint8_t subdev = 0,
                       const double timeout_s = 10);

    /*!
      @brief Receive data from the device and return it in a vector.
      @returns the number of samples received before a sequence error, or @p n_desired if no sequence errors occur
      @param data the @p complex<float> vector for the received data
      @param n_requested the number of samples to be received (0 means use data.size();
          if data.size() \< n_requested, only data.size() will be acquired)
      @param subdev the subdevice number
      @param timeout_s timeout in seconds
    */
    size_t get_rx_data(std::vector<std::complex<float>>& data,
                       const size_t n_requested = 0,
                       const uint8_t subdev = 0,
                       const double timeout_s = 10);

   /*!
      @brief Set the timeout used by the host for commands sent to the device.
      We do not recommend values less than 0.5 seconds
      @returns @p true if the timeout is set, @p false if it is out of range
      @param timeout_s the timeout in seconds (must be greater than 0 and less than or equal to 3600)
    */
    bool set_host_command_timeout(const double timeout_s);

    /*!
      @brief Get the timeout used by the host for commands sent to the device.
      @returns the timeout in seconds
    */
    [[nodiscard]] double get_host_command_timeout() const;

    /*!
      @brief Helper function to compute the sample granularity from the wire format returned by hello().
      @returns the sample granularity
    */
    unsigned compute_sample_granularity(const uint32_t wire_format) const;

  private:
    class imp;
    std::unique_ptr<imp> p_imp;
};
