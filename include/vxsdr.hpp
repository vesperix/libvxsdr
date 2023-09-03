// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <complex>
#include <fstream>
#include <optional>
#include <vector>
#include <array>
#include <memory>
#include <string>
#include <ratio>
#include <chrono>
#include <map>
using namespace std::chrono_literals;

#include "vxsdr_lib_export.h"

/*! @class vxsdr vxsdr.hpp
    @brief The vxsdr class contains the host interface for the VXSDR
*/

class VXSDR_LIB_EXPORT vxsdr {
  public:
  /*!
    @brief The @p transport_type describes how data and commands are sent and received. (UDP is currently the only transport supported.)
  */
    enum transport_type { TRANSPORT_TYPE_UDP = 1, TRANSPORT_TYPE_PCIE };
  /*!
    @brief The @p stream_state type reports the status of TX or RX data streaming.
  */
    enum stream_state { STREAM_STOPPED = 0, STREAM_RUNNING, STREAM_WAITING_FOR_START, STREAM_ERROR };

  /*!
    @brief The @p duration type is used for acquisition and wait durations
  */
    using duration = std::chrono::duration<int64_t, std::ratio<1, 1000000000>>;

  /*!
    @brief The @p time_point type is used for start times.
  */
    using time_point = std::chrono::time_point<std::chrono::system_clock, duration>;

  /*!
      @brief Constructor for the @p vxsdr host interface class.
      @param local_address the identifier for the transport interface used on the host (for UDP, a string in xxx.xxx.xxx.xxx notation)
      @param device_address the identifier for the target device interface (for UDP, a string in xxx.xxx.xxx.xxx notation)
      @param settings a std::map<std::string, int64_t> containing changes to the default settings; if a setting is not included, it is
             left at the default value.
    */
    explicit vxsdr(const std::string& local_address,
          const std::string& device_address,
          const std::map<std::string, int64_t>& settings = {});

    //! Destructor for the vxsdr host interface class.
    ~vxsdr() noexcept;

    //! @brief Deleted copy constructor for the @p vxsdr host interface class.
    vxsdr(const vxsdr&) = delete;

    //! @brief Deleted copy assignment for the @p vxsdr host interface class.
    vxsdr& operator=(const vxsdr&) = delete;

    //! @brief Deleted move constructor for the @p vxsdr host interface class.
    vxsdr(vxsdr&&) = delete;

    //! @brief Deleted move assignment for the @p vxsdr host interface class.
    vxsdr& operator=(vxsdr&&) = delete;

    /*!
      @brief Gets the version number of this library
      @returns the library version number as 10,000 * major_version + 100 * minor_version + patch_level
    */
    uint32_t get_library_version();

    /*!
      @brief Gets the packet version number supported by this library
      @returns the packet version number as 10,000 * major_version + 100 * minor_version + patch_level
    */
    uint32_t get_library_packet_version();

    /*!
      @brief Gets more detailed information on this library
      @returns a std::vector of std::strings with version and build information
    */
    std::vector<std::string> get_library_details();

    /*!
      @brief Request basic information from the device.
      @returns a std::optional with a std::array containing the
          - device identifier
          - FPGA code version
          - MCU code version,
          - device serial number,
          - packet version supported
          - device status
    */
    std::optional<std::array<uint32_t, 6>> hello();

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
      @brief Get the maximum payload size in bytes for transport to and from the device. Note
      that the maximum packet size (which must not exceed the network MTU) is the maximum payload size
      plus the size of the packet header, plus the stream spec and time spec, if those are used,
      plus the size of the IPv4 header. The packet header, stream spec, and time spec are 8 bytes each,
      and the IPv4 header is typically 20 bytes.
      @returns a std::optional with the maximum payload in bytes
    */
    std::optional<unsigned> get_max_payload_bytes();

    /*!
      @brief Set the maximum payload size in bytes for transport to and from the device.
      The payload size must be a multiple of 8 bytes (2 samples) and 1024 <= max_payload_bytes <= 16384.
      We strongly recommend using the default of 8192, which is compatible with a typical jumbo packet 
      MTU of 9000, or a larger value if you are certain your network cards support it.
      @returns @p true if the size is set, @p false otherwise
      @param max_payload_bytes the maximum payload size in bytes
    */
    bool set_max_payload_bytes(const unsigned max_payload_bytes);


    /*!
      @brief Get the number of available sensors.
      @returns  a std::optional with the number of available sensors
      @param subdev the subdevice number
    */
    std::optional<unsigned> get_num_sensors(const uint8_t subdev = 0);

    /*!
      @brief Get the names of available sensors.
      @returns a std::vector of std:strings giving the names
      @param subdev the subdevice number
    */
    std::vector<std::string> get_sensor_names(const uint8_t subdev = 0);

    /*!
      @brief Get the value of a sensor.
      @returns a std::optional with a double giving the sensor reading
      @param sensor_name the desired sensor name
      @param subdev the subdevice number
    */
    std::optional<double> get_sensor_reading(const std::string& sensor_name, const uint8_t subdev = 0);

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
      device waits for the next PPS to respond
      @param t the time to set
    */
    bool set_time_next_pps(const vxsdr::time_point& t);

    /*!
      @brief Get the status of the device timing references.
      @returns a std::optional with a std::array containing
          - the external PPS lock status
          - external 10 MHz lock status
          - internal reference oscillator lock status

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
      @returns a std::optional with a  std::array containing the minimum and maximum frequencies in Hz
      @param subdev the subdevice number
    */
    std::optional<std::array<double, 2>> get_tx_freq_range(const uint8_t subdev = 0);

    /*!
      @brief Get the receive center frequency range.
      @returns a std::optional with a  std::array containing the minimum and maximum frequencies in Hz
      @param subdev the subdevice number
    */
    std::optional<std::array<double, 2>> get_rx_freq_range(const uint8_t subdev = 0);

    /*!
      @brief Get the current transmit center frequency.
      @returns a std::optional with the center frequency in Hz
      @param subdev the subdevice number
    */
    std::optional<double> get_tx_freq(const uint8_t subdev = 0);

    /*!
      @brief Get the current receive center frequency.
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
      @brief Get the gain range for a transmit channel.
      @returns a std::optional with a std::array containing the minimum and maximum gains in dB
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::array<double, 2>> get_tx_gain_range(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the gain range for a receive channel.
      @returns a std::optional with a std::array containing the minimum and maximum gains in dB
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::array<double, 2>> get_rx_gain_range(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the current gain for a transmit channel.
      @returns a std::optional with the gain in dB
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<double> get_tx_gain(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the current gain for a receive channel.
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
      @brief Get the current transmit sample rate.
      @returns a std::optional with the rate in samples/s
      @param subdev the subdevice number
    */
    std::optional<double> get_tx_rate(const uint8_t subdev = 0);

    /*!
      @brief Get the current receive sample rate.
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
      @brief Get the current transmit output port.
      @returns a std::optional with the port number
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<unsigned> get_tx_port(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the current receive input port.
      @returns a std::optional with the port number
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<unsigned> get_rx_port(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the name of a transmit output port.
      @returns a std::optional with the name of the specified output port
      @param port_num the port number
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::string> get_tx_port_name(const unsigned port_num, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the name of a receive input port.
      @returns a std::optional with the name of the specified input port
      @param port_num the port number
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::string> get_rx_port_name(const unsigned port_num, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the transmit output port by number.
      @returns @b true if the command succeeds, @b false otherwise
      @param port_num the desired output port number
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_tx_port(const unsigned port_num, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the transmit output port by name.
      @returns @b true if the command succeeds, @b false otherwise
      @param port_name the desired output port name
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_tx_port_by_name(const std::string& port_name, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the receive input port by number.
      @returns @b true if the command succeeds, @b false otherwise
      @param port_num the desired input port number
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_rx_port(const unsigned port_num, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the receive input port by name.
      @returns @b true if the command succeeds, @b false otherwise
      @param port_name the desired input port name
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_rx_port_by_name(const std::string& port_name, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the number of transmit subdevices.
      @returns a std::optional with the number of subdevices
    */
    std::optional<unsigned> get_tx_num_subdevs();

    /*!
      @brief Get the number of receive subdevices.
      @returns a std::optional with the number of subdevices
    */
    std::optional<unsigned> get_rx_num_subdevs();

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
      @returns a std::optional with a std::vector<std::complex<int16t>> containing the filter coefficients
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::vector<std::complex<int16_t>>> get_tx_filter_coeffs(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the receive frontend FIR filter coefficients.
      @returns a std::optional with a std::vector<std::complex<int16t>> containing the filter coefficients
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::vector<std::complex<int16_t>>> get_rx_filter_coeffs(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the transmit frontend FIR filter coefficients.
      @returns @b true if the command succeeds, @b false otherwise
      @param coeffs a std::vector<std::complex<int16t>> containing the filter coefficients
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_tx_filter_coeffs(const std::vector<std::complex<int16_t>>& coeffs, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the receive frontend FIR filter coefficients.
      @returns @b true if the command succeeds, @b false otherwise
      @param coeffs a std::vector<std::complex<int16t>> containing the filter coefficients
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_rx_filter_coeffs(const std::vector<std::complex<int16_t>>& coeffs, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the IQ DC bias for a transmit channel.
      @returns a std::optional with a std::array containing (i_bias, q_bias) with
          (-1 >= @p i_bias >= 1) and (-1 >= @p q_bias >= 1)
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::array<double, 2>> get_tx_iq_bias(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the IQ DC bias for a receive channel.
      @returns a std::optional with a std::array containing (i_bias, q_bias) with
          (-1 >= @p i_bias >= 1) and (-1 >= @p q_bias >= 1)
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::array<double, 2>> get_rx_iq_bias(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the IQ DC bias for the transmitter to control LO feedthrough.
      @returns @b true if the command succeeds, @b false otherwise
      @param bias   a std::array containing i_bias, q_bias in that order
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_tx_iq_bias(const std::array<double, 2> bias, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the IQ DC bias for the receiver to control LO feedthrough.
      @returns @b true if the command succeeds, @b false otherwise
      @param bias   a std::array containing i_bias, q_bias in that order
      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_rx_iq_bias(const std::array<double, 2> bias, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the IQ correction for a transmit channel to control image rejection.
      @returns @b true if the command succeeds, @b false otherwise
      @param corr    a std::array containing a_ii, a_iq, a_qi, a_qq (in that order),
          which are the coefficients of the matrix which transforms the IQ data on transmission.

      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_tx_iq_corr(const std::array<double, 4> corr, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Set the IQ correction for a receive channel to control image rejection.
      @returns @b true if the command succeeds, @b false otherwise
      @param corr    a std::array containing a_ii, a_iq, a_qi, a_qq (in that order),
          which are the coefficients of the matrix which transforms the IQ data on reception.

      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    bool set_rx_iq_corr(const std::array<double, 4> corr, const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the IQ correction for a transmit channel.
      @returns a std::optional with a std::array containing a_ii, a_iq, a_qi, a_qq (in that order),
          which are the coefficients of the matrix which transforms the IQ data on transmission.

      @param subdev the subdevice number
      @param channel the channel number within the subdevice
    */
    std::optional<std::array<double, 4>> get_tx_iq_corr(const uint8_t subdev = 0, const uint8_t channel = 0);

    /*!
      @brief Get the IQ correction for a receive channel.
      @returns a std::optional with a std::array containing a_ii, a_iq, a_qi, a_qq (in that order),
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
      If @p t is less than the current time, start immediately;
      if @p n_repeat is 0, continue until a stop command is sent.
      Note that if you wish to send the samples to be looped only once, @p n_samples must be
      small enough that the entire looped waveform fits in the device's transmit buffer.
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
      for @p n_repeat iterations.
      If @p t is less than the current time, start immediately;
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
      @brief Stop transmitting immediately.
      @returns @b true if the command succeeds, @b false otherwise
      @param subdev the subdevice number
    */
    bool tx_stop_now(const uint8_t subdev = 0);

    /*!
      @brief Stop receiving immediately.
      @returns @b true if the command succeeds, @b false otherwise
      @param subdev the subdevice number
    */
    bool rx_stop_now(const uint8_t subdev = 0);

    /*!
      @brief Send transmit data to the device.
      @returns the number of samples placed in the queue for transmission
      @param data the complex @p itn16_t samples to be sent
      @param subdev the subdevice number
      @param timeout_s timeout in seconds
    */
    size_t put_tx_data(const std::vector<std::complex<int16_t>>& data,
                       const uint8_t subdev   = 0,
                       const double timeout_s = 10);

    /*!
      @brief Send transmit data to the device.
      @returns the number of samples placed in the queue for transmission
      @param data the complex @p itn16_t samples to be sent
      @param subdev the subdevice number
      @param timeout_s timeout in seconds
    */
    size_t put_tx_data(const std::vector<std::complex<float>>& data,
                       const uint8_t subdev   = 0,
                       const double timeout_s = 10);

    /*!
      @brief Receive data from the device and return it in a vector.
      @returns the number of samples received before a sequence error, or @p n_desired if no sequence errors occur
      @param data the vector for the received data
      @param n_desired the number of samples to be received (0 means use data.size(); if data.size() < n_desired, only data.size() will be acquired)
      @param subdev the subdevice number
      @param timeout_s timeout in seconds
    */
    size_t get_rx_data(std::vector<std::complex<int16_t>>& data,
                       const size_t n_desired = 0,
                       const uint8_t subdev = 0,
                       const double timeout_s = 10);

    /*!
      @brief Receive data from the device and return it in a vector.
      @returns the number of samples received before a sequence error, or @p n_desired if no sequence errors occur
      @param data the vector for the received data
      @param n_desired the number of samples to be received (0 means use data.size(); if data.size() < n_desired, only data.size() will be acquired)
      @param subdev the subdevice number
      @param timeout_s timeout in seconds
    */
    size_t get_rx_data(std::vector<std::complex<float>>& data,
                       const size_t n_desired = 0,
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
      @brief Get the current timeout used by the host for commands sent to the device.
      @returns the timeout in seconds
    */
    [[nodiscard]] double get_host_command_timeout() const;

  private:
    class imp;
    std::unique_ptr<imp> p_imp;
};
