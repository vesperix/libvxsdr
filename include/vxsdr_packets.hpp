// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <complex>
#include <cstdint>

#include "packet_header.h"

/* Times are specified using the following type.
   NOTE that unlike common Unix/Linux practice, the
   elements are unsigned.
*/

struct time_spec_t {
  uint32_t              seconds;
  uint32_t              nanoseconds;
};

using stream_spec_t = uint64_t;

using stream_state_t = enum { STREAM_STOPPED = 0, STREAM_RUNNING, STREAM_WAITING_FOR_START, STREAM_ERROR };

// FIXME: these are for timing status; need to be defined as part of a generic API when stable
constexpr uint32_t TIMING_STATUS_EXT_PPS_LOCK   = 0x00000001;
constexpr uint32_t TIMING_STATUS_EXT_10MHZ_LOCK = 0x00000002;
constexpr uint32_t TIMING_STATUS_REF_OSC_LOCK   = 0x00000004;

// The remainder is entirely C++ class definitions

class packet {
  public:
    packet_header hdr = {0, 0, 0, 0, 0, 0, 0};
};

class header_only_packet : public packet {};

class async_msg_packet : public packet {};

class one_uint32_packet : public packet {
  public:
    uint32_t value1   = 0;
    uint32_t reserved = 0;
};

using error_packet = one_uint32_packet;

class two_uint32_packet : public packet {
  public:
    uint32_t value1 = 0;
    uint32_t value2 = 0;
};

class four_uint32_packet : public packet {
  public:
    uint32_t value1 = 0;
    uint32_t value2 = 0;
    uint32_t value3 = 0;
    uint32_t value4 = 0;
};

class six_uint32_packet : public packet {
  public:
    uint32_t value1 = 0;
    uint32_t value2 = 0;
    uint32_t value3 = 0;
    uint32_t value4 = 0;
    uint32_t value5 = 0;
    uint32_t value6 = 0;
};

class eight_uint32_packet : public packet {
  public:
    uint32_t value1 = 0;
    uint32_t value2 = 0;
    uint32_t value3 = 0;
    uint32_t value4 = 0;
    uint32_t value5 = 0;
    uint32_t value6 = 0;
    uint32_t value7 = 0;
    uint32_t value8 = 0;
};

class one_double_packet : public packet {
  public:
    double value1 = 0.0;
};

class two_double_packet : public packet {
  public:
    double value1 = 0.0;
    double value2 = 0.0;
};

class four_double_packet : public packet {
  public:
    double value1 = 0.0;
    double value2 = 0.0;
    double value3 = 0.0;
    double value4 = 0.0;
};

class one_uint64_packet : public packet {
  public:
    uint64_t value1 = 0;
};

class filter_coeff_packet : public packet {
  public:
    uint32_t length = 0;
    std::complex<int16_t> coeffs[MAX_FRONTEND_FILTER_LENGTH] = {0};
};

class name_packet : public packet {
  public:
    char name1[MAX_NAME_LENGTH_BYTES] = {0};
};

class uint32_double_packet : public packet {
  public:
    uint32_t value1   = 0;
    uint32_t reserved = 0;
    double value2     = 0.0;

};

class uint32_two_double_packet : public packet {
  public:
    uint32_t value1   = 0;
    uint32_t reserved = 0;
    double value2     = 0.0;
    double value3     = 0.0;
};

class data_packet : public packet {
  public:
    std::complex<int16_t> data[MAX_DATA_LENGTH_SAMPLES] = {0};
};

class data_packet_time : public packet {
  public:
    time_spec_t time = {0, 0};
    std::complex<int16_t> data[MAX_DATA_LENGTH_SAMPLES] = {0};
};

class data_packet_stream : public packet {
  public:
    stream_spec_t stream_id = 0;
    std::complex<int16_t> data[MAX_DATA_LENGTH_SAMPLES] = {0};
};

class data_packet_time_stream : public packet {
  public:
    time_spec_t time = {0, 0};
    stream_spec_t stream_id = 0;
    std::complex<int16_t> data[MAX_DATA_LENGTH_SAMPLES] = {0};
};

class largest_data_packet : public data_packet_time_stream {};

class cmd_or_rsp_packet_time : public packet {
  public:
    time_spec_t time = {0, 0};
};

class cmd_or_rsp_packet_time_stream : public packet {
  public:
    time_spec_t time = {0, 0};
    stream_spec_t stream_id = 0;
};

class cmd_or_rsp_packet_samples : public packet {
  public:
    uint64_t n_samples = 0;
};

class cmd_or_rsp_packet_time_samples : public packet {
  public:
    time_spec_t time = {0, 0};
    uint64_t n_samples = 0;
};

class loop_packet : public cmd_or_rsp_packet_time_samples {
  public:
    time_spec_t t_delay = {0, 0};
    uint32_t n_repeat = 0;
};

class largest_cmd_or_rsp_packet : public cmd_or_rsp_packet_time_samples {
  public:
    uint8_t payload[MAX_PAYLOAD_LENGTH_BYTES] = {0};
};

using command_queue_element = largest_cmd_or_rsp_packet;

// alignment of the buffers used to queue data packets
constexpr unsigned VXSDR_DATA_BUFFER_ALIGNMENT = 64;

class alignas(VXSDR_DATA_BUFFER_ALIGNMENT) data_queue_element : public largest_data_packet {
};

// checks to ensure that sizes of these objects are as expected
VXSDR_CHECK_SIZE_EQUALS(time_spec_t, 8);
VXSDR_CHECK_SIZE_EQUALS(stream_spec_t, 8);
VXSDR_CHECK_SIZE_EQUALS(header_only_packet, 8);
VXSDR_CHECK_SIZE_EQUALS(one_uint32_packet, 16);
VXSDR_CHECK_SIZE_EQUALS(error_packet, 16);
VXSDR_CHECK_SIZE_EQUALS(two_uint32_packet, 16);
VXSDR_CHECK_SIZE_EQUALS(four_uint32_packet, 24);
VXSDR_CHECK_SIZE_EQUALS(six_uint32_packet, 32);
VXSDR_CHECK_SIZE_EQUALS(one_double_packet, 16);
VXSDR_CHECK_SIZE_EQUALS(two_double_packet, 24);
VXSDR_CHECK_SIZE_EQUALS(four_double_packet, 40);
VXSDR_CHECK_SIZE_EQUALS(uint32_double_packet, 24);
VXSDR_CHECK_SIZE_EQUALS(uint32_two_double_packet, 32);
VXSDR_CHECK_SIZE_EQUALS(one_uint64_packet, 16);
VXSDR_CHECK_SIZE_EQUALS(name_packet, (MAX_NAME_LENGTH_BYTES + 8));
VXSDR_CHECK_SIZE_LESS_EQUAL(name_packet, sizeof(largest_cmd_or_rsp_packet));
VXSDR_CHECK_SIZE_EQUALS(filter_coeff_packet, (4 * MAX_FRONTEND_FILTER_LENGTH + 12));
VXSDR_CHECK_SIZE_LESS_EQUAL(filter_coeff_packet, sizeof(largest_cmd_or_rsp_packet));
VXSDR_CHECK_SIZE_EQUALS(async_msg_packet, 8);
VXSDR_CHECK_SIZE_EQUALS(largest_data_packet, (4 * MAX_DATA_LENGTH_SAMPLES + sizeof(packet_header) + sizeof(time_spec_t) + sizeof(stream_spec_t)));
VXSDR_CHECK_SIZE_LESS_EQUAL(largest_data_packet, sizeof(data_queue_element));
