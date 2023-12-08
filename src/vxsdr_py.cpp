// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <vxsdr.hpp>

#include <cstddef>
#include <cstdint>
#include <array>
#include <complex>
#include <map>
#include <string>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/complex.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/iostream.h>

namespace py = pybind11;

template <typename T> std::vector<T> numpy_to_vector(const py::array_t<T> &np) {
    //FIXME: this may be possible without copying
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return std::vector<T>(np.data(), np.data() + np.size());
}

class vxsdr_py : public vxsdr {
    public:
        vxsdr_py(const std::string& local_ip_address,
                 const std::string& device_ip_address,
                 const std::map<std::string, int64_t>& settings) :
                      vxsdr(local_ip_address,
                            device_ip_address,
                            settings) {}

        size_t put_tx_data(const py::array_t<std::complex<float>, py::array::c_style | py::array::forcecast>& data_np,
                                const size_t n_requested = 0, const uint8_t subdev = 0, const double timeout_s = 10) {
            if (data_np.ndim() != 1) {
                throw py::type_error("Numpy array for VXSDR data must be 1-D");
                return 0;
            }
            return vxsdr::put_tx_data(numpy_to_vector<std::complex<float>>(data_np), subdev, timeout_s);
        }
        size_t get_rx_data(py::array_t<std::complex<float>, py::array::c_style> data_np,
                                const size_t n_desired = 0, const uint8_t subdev = 0, const double timeout_s = 10) {
            if (data_np.ndim() != 1) {
                throw py::type_error("Numpy array for VXSDR data must be 1-D");
                return 0;
            }
            std::vector<std::complex<float>> data(data_np.size());
            auto n_ret = vxsdr::get_rx_data(data, n_desired, subdev, timeout_s);
            // FIXME: should be possible to do this faster
            auto d = data_np.mutable_unchecked<1>();
            for (unsigned i = 0; i < n_ret; i++) {
                d[i] = data[i];
            }
            return n_ret;
        }
};

//FIXME: verify argument names, order, and defaults against C++ docs

//FIXME: determine whether library logging to stdout should be mapped to Python stdout
//       see https://pybind11.readthedocs.io/en/stable/advanced/pycpp/utilities.html#capturing-standard-output-from-ostream

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PYBIND_DEF_SIMPLE(n, docstr)     .def(#n, &vxsdr_py::n, docstr)
// functions with only a subdev argument
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PYBIND_DEF_SUBDEV(n, docstr)     .def(#n, &vxsdr_py::n, docstr, py::arg("subdev") = 0)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PYBIND_DEF_SUBDEV_CHAN(n, docstr)     .def(#n, &vxsdr_py::n, docstr, py::arg("subdev") = 0,  py::arg("channel") = 0)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PYBIND_DEF_ARGS(n, docstr, ...)  .def(#n, &vxsdr_py::n, docstr, __VA_ARGS__)
// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-vararg, hicpp-vararg)
PYBIND11_MODULE(vxsdr_py, m) {
    // optional module docstring
    m.doc() = "VXSDR Python interface";

    py::enum_<vxsdr_py::stream_state>(m, "stream_state", py::arithmetic())
        .value("Stopped", vxsdr_py::stream_state::STREAM_STOPPED)
        .value("Running", vxsdr_py::stream_state::STREAM_RUNNING)
        .value("Waiting", vxsdr_py::stream_state::STREAM_WAITING_FOR_START)
        .value("Error",   vxsdr_py::stream_state::STREAM_ERROR)
    .export_values();

    py::enum_<vxsdr_py::transport_type>(m, "transport_type", py::arithmetic())
        .value("UDP", vxsdr_py::transport_type::TRANSPORT_TYPE_UDP)
        .value("PCIE", vxsdr_py::transport_type::TRANSPORT_TYPE_PCIE)
    .export_values();

    // bindings to vxsdr class
    py::class_<vxsdr_py>(m, "vxsdr_py")
         // constructor
        .def(py::init<const std::string, const std::string, const std::map<std::string, int64_t>&>(),
                "Constructor for the vxsdr_py class",
                py::arg("local_ip_address"),
                py::arg("device_ip_address"),
                py::arg("settings"))
        // library information
        PYBIND_DEF_SIMPLE(get_library_version, "Get the version number of the library.")
        PYBIND_DEF_SIMPLE(get_library_packet_version, "Gets the packet version number supported by the library.")
        PYBIND_DEF_SIMPLE(get_library_details, "Get version and build information on the library.")
        // device information
        PYBIND_DEF_SIMPLE(hello, "Request basic information from the device.")
        PYBIND_DEF_SIMPLE(reset, "Reset the device.")
        PYBIND_DEF_SUBDEV(clear_status, "Clear the device status.")
        PYBIND_DEF_SUBDEV(get_status, "Get the device status.")
        PYBIND_DEF_SUBDEV(get_buffer_info, "Get the size of the device transmit and receive buffers.")
        PYBIND_DEF_SUBDEV(get_buffer_use, "Get the current number of bytes used in the device transmit and receive buffers.")
        PYBIND_DEF_SIMPLE(get_max_payload_bytes, "Get the maximum payload size in bytes for transport to and from the device.")
        PYBIND_DEF_ARGS(set_max_payload_bytes, "Set the maximum payload size in bytes for transport to and from the device.",
                py::arg("max_payload_bytes"))
        // sensors
        PYBIND_DEF_SUBDEV(get_num_sensors, "Get the number of available sensors.")
        PYBIND_DEF_SUBDEV(get_sensor_names, "Get the names of available sensors.")
        PYBIND_DEF_ARGS(get_sensor_reading, "Get the sensor reading.",
                py::arg("sensor_name"),
                py::arg("subdev") = 0)
        // timing
        PYBIND_DEF_SIMPLE(get_time_now, "Get the device time immediately.")
        PYBIND_DEF_ARGS(set_time_now, "Set the device time immediately.",
                py::arg("t"))
        PYBIND_DEF_ARGS(set_time_next_pps,
                "Set the device time at the next PPS received by the device.",
                py::arg("t"))
        PYBIND_DEF_SIMPLE(get_timing_status, "Get the status of the device timing references.")
        PYBIND_DEF_SIMPLE(get_timing_resolution, "Get the resolution of the device's clock.")

        // ip addressing
        PYBIND_DEF_SUBDEV(set_ipv4_address, "Set the IPv4 address of the device (will end communication with this instance; read the docs first!).")
        PYBIND_DEF_SUBDEV(save_ipv4_address, "Save the IPv4 address of the device to nonvolatile memory in the device (read the docs first!).")
        PYBIND_DEF_ARGS(discover_ipv4_addresses, "Broadcast a device discovery packet to the given IPv4 broadcast address; return the addresses of devices which respond.",
                py::arg("local_addr"),
                py::arg("broadcast_addr"),
                py::arg("timeout_s") = 10)
        // enable and disable
        PYBIND_DEF_SUBDEV(get_tx_enabled, "Determine if the transmit RF section is enabled.")
        PYBIND_DEF_SUBDEV(get_rx_enabled, "Determine if the receive RF section is enabled.")
        PYBIND_DEF_ARGS(set_tx_enabled, "Enable or disable the transmit RF section.",
                py::arg("enabled"),
                py::arg("subdev") = 0)
        PYBIND_DEF_ARGS(set_rx_enabled, "Enable or disable the receive RF section.",
                py::arg("enabled"),
                py::arg("subdev") = 0)
        // tuning
        PYBIND_DEF_SUBDEV(get_tx_freq_range, "Get the transmit center frequency range.")
        PYBIND_DEF_SUBDEV(get_rx_freq_range, "Get the receive center frequency range.")
        PYBIND_DEF_SUBDEV(get_tx_freq, "Get the current transmit center frequency.")
        PYBIND_DEF_SUBDEV(get_rx_freq, "Get the current receive center frequency.")
        PYBIND_DEF_ARGS(set_tx_freq, "Set the transmit center frequency.",
                py::arg("freq"),
                py::arg("subdev") = 0)
        PYBIND_DEF_ARGS(set_rx_freq, "Set the receive center frequency.",
                py::arg("freq"),
                py::arg("subdev") = 0)
        // gain control
        PYBIND_DEF_SUBDEV_CHAN(get_tx_gain_range, "Get the gain range for a transmit channel.")
        PYBIND_DEF_SUBDEV_CHAN(get_rx_gain_range, "Get the gain range for a receive channel.")
        PYBIND_DEF_SUBDEV_CHAN(get_tx_gain, "Get the current gain for a transmit channel.")
        PYBIND_DEF_SUBDEV_CHAN(get_rx_gain, "Get the current gain for a receive channel.")
        PYBIND_DEF_ARGS(set_tx_gain, "Set the gain for a transmit channel.",
                py::arg("gain"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        PYBIND_DEF_ARGS(set_rx_gain, "Set the gain for a receive channel.",
                py::arg("gain"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        // sampling rate
        PYBIND_DEF_SUBDEV(get_tx_rate_range, "Get the transmit sample rate range.")
        PYBIND_DEF_SUBDEV(get_rx_rate_range, "Get the receive sample rate range.")
        PYBIND_DEF_SUBDEV(get_tx_rate, "Get the current transmit sample rate.")
        PYBIND_DEF_SUBDEV(get_rx_rate, "Get the current receive sample rate.")
        PYBIND_DEF_ARGS(set_tx_rate, "Set the transmit sample rate.",
                py::arg("rate"),
                py::arg("subdev") = 0)
        PYBIND_DEF_ARGS(set_rx_rate, "Set the receive sample rate.",
                py::arg("rate"),
                py::arg("subdev") = 0)
        // inputs and outputs
        PYBIND_DEF_SUBDEV_CHAN(get_tx_num_ports, "Get the number of transmit output ports.")
        PYBIND_DEF_SUBDEV_CHAN(get_rx_num_ports, "Get the number of receive input ports.")
        PYBIND_DEF_ARGS(get_tx_port_name, "Get the name of a transmit output port.",
                py::arg("port_num"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        PYBIND_DEF_ARGS(get_rx_port_name, "Get the name of a receive input port.",
                py::arg("port_num"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        PYBIND_DEF_SUBDEV_CHAN(get_tx_port, "Get the current transmit output port.")
        PYBIND_DEF_SUBDEV_CHAN(get_rx_port, "Get the current receive input port.")
        PYBIND_DEF_ARGS(set_tx_port, "Set the transmit output port by number.",
                py::arg("port_num"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        PYBIND_DEF_ARGS(set_rx_port, "Set the receive input port by number.",
                py::arg("port_num"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        PYBIND_DEF_ARGS(set_tx_port_by_name, "Set the transmit output port by name.",
                py::arg("port_name"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        PYBIND_DEF_ARGS(set_rx_port_by_name, "Set the receive input port by name.",
                py::arg("port_name"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        // radio information
        PYBIND_DEF_SIMPLE(get_tx_num_subdevs, "Get the number of transmit subdevices.")
        PYBIND_DEF_SIMPLE(get_rx_num_subdevs, "Get the number of receive subdevices.")
        PYBIND_DEF_SUBDEV(get_tx_num_channels, "Get the number of transmit channels.")
        PYBIND_DEF_SUBDEV(get_rx_num_channels, "Get the number of receive channels.")
        PYBIND_DEF_SUBDEV(get_rx_stream_state, "Get the receive stream state")
        PYBIND_DEF_SUBDEV(get_tx_stream_state, "Get the transmit stream state")
        PYBIND_DEF_SUBDEV(get_tx_lo_locked, "Determine if the transmit LO is locked.")
        PYBIND_DEF_SUBDEV(get_rx_lo_locked, "Determine if the receive LO is locked.")
        // external lo
        PYBIND_DEF_SUBDEV(get_tx_external_lo_enabled, " Determine if the transmit external LO input is enabled.")
        PYBIND_DEF_SUBDEV(get_rx_external_lo_enabled, " Determine if the receive external LO input is enabled.")
        PYBIND_DEF_ARGS(set_tx_external_lo_enabled,
                "Enable or disable the transmit external LO input.",
                py::arg("enabled"),
                py::arg("subdev") = 0)
        PYBIND_DEF_ARGS(set_rx_external_lo_enabled,
                "Enable or disable the receive external LO input.",
                py::arg("enabled"),
                py::arg("subdev") = 0)
        // digital filters
        PYBIND_DEF_SUBDEV(get_tx_filter_length, "Get the transmit frontend filter maximum length.")
        PYBIND_DEF_SUBDEV(get_rx_filter_length, "Get the receive frontend filter maximum length.")
        PYBIND_DEF_SUBDEV_CHAN(get_tx_filter_coeffs, "Get the transmit frontend FIR filter coefficients.")
        PYBIND_DEF_SUBDEV_CHAN(get_rx_filter_coeffs, "Get the receive frontend FIR filter coefficients.")
        PYBIND_DEF_ARGS(set_tx_filter_coeffs, "Set the transmit frontend FIR filter coefficients.",
                py::arg("coeffs"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        PYBIND_DEF_ARGS(set_rx_filter_coeffs, "Set the receive frontend FIR filter coefficients.",
                py::arg("coeffs"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        // corrections
        PYBIND_DEF_SUBDEV_CHAN(get_tx_iq_bias, "Get the IQ DC bias for the transmitter.")
        PYBIND_DEF_SUBDEV_CHAN(get_rx_iq_bias, "Get the IQ DC bias for the receiver.")
        PYBIND_DEF_ARGS(set_tx_iq_bias,
                "Set the IQ DC bias for a transmit channel to control LO feedthrough.",
                py::arg("bias"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        PYBIND_DEF_ARGS(set_rx_iq_bias,
                "Set the IQ DC bias for a receive channel to control LO feedthrough.",
                py::arg("bias"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)

        PYBIND_DEF_SUBDEV_CHAN(get_tx_iq_corr, "Get the IQ correction for a transmit channel.")
        PYBIND_DEF_SUBDEV_CHAN(get_rx_iq_corr, "Get the IQ correction for a receive channel.")
        PYBIND_DEF_ARGS(set_tx_iq_corr,
                "Set the IQ correction for a transmit channel to control image rejection.",
                py::arg("corr"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        PYBIND_DEF_ARGS(set_rx_iq_corr,
                "Set the IQ correction for a receive channel to control image rejection.",
                py::arg("corr"),
                py::arg("subdev") = 0,
                py::arg("channel") = 0)
        // setup transmit and received
        PYBIND_DEF_ARGS(tx_start,
                "Start transmitting at specified time until the specified number of samples are sent.",
                py::arg("t"),
                py::arg("n"),
                py::arg("subdev") = 0)
        PYBIND_DEF_ARGS(rx_start,
                "Start receiving at specified time until the specified number of samples are sent.",
                py::arg("t"),
                py::arg("n"),
                py::arg("subdev") = 0)
        // repeating transmit and receive
        PYBIND_DEF_ARGS(tx_loop,
                "Start transmitting at specified time until the specified number of samples are sent, repeating n_repeat times at intervals of t_repeat",
                py::arg("t"),
                py::arg("n"),
                py::arg("t_repeat") = 0,
                py::arg("n_repeat") = 0,
                py::arg("subdev") = 0)
        PYBIND_DEF_ARGS(rx_loop,
                "Start receiving at specified time until the specified number of samples are sent, repeating n_repeat times at intervals of t_repeat",
                py::arg("t"),
                py::arg("n"),
                py::arg("t_repeat") = 0,
                py::arg("n_repeat") = 0,
                py::arg("subdev") = 0)
        // interrupting transmit and receive
        PYBIND_DEF_SUBDEV(tx_stop_now, "Stop transmitting immediately.")
        PYBIND_DEF_SUBDEV(rx_stop_now, "Stop receiving immediately.")
        // sending and receiving samples
        PYBIND_DEF_ARGS(put_tx_data,
                "Send transmit data to the device.",
                py::arg("data"),
                py::arg("n_requested") = 0,
                py::arg("subdev") = 0,
                py::arg("timeout") = 10)
        PYBIND_DEF_ARGS(get_rx_data,
                "Receive data from the device.",
                py::arg("data"),
                py::arg("n_desired"),
                py::arg("subdev") = 0,
                py::arg("timeout") = 10)
        // host control functions
        PYBIND_DEF_ARGS(set_host_command_timeout,
                "Set the timeout used by the host for commands sent to the device.",
                py::arg("timeout"))
        PYBIND_DEF_SIMPLE(get_host_command_timeout,
                "Get the current timeout used by the host for commands sent to the device.")
        ;

}
