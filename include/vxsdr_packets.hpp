// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <complex>
#include <cstdint>

#include "packet_header.h"

using stream_state_t = enum { STREAM_STOPPED = 0, STREAM_RUNNING, STREAM_WAITING_FOR_START, STREAM_ERROR };

// The remainder is entirely C++ class definitions

#pragma pack(push, 1)

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
    uint32_t reserved = 0;
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

class time_packet : public packet {
  public:
    time_spec_t time = {0, 0};
};

class time_stream_packet : public packet {
  public:
    time_spec_t time = {0, 0};
    stream_spec_t stream_id = 0;
};

class samples_packet : public packet {
  public:
    uint64_t n_samples = 0;
};

class time_samples_packet : public packet {
  public:
    time_spec_t time = {0, 0};
    uint64_t n_samples = 0;
};

class loop_packet : public time_samples_packet {
  public:
    time_spec_t t_delay = {0, 0};
    uint32_t n_repeat = 0;
    uint32_t reserved = 0;
};

class largest_cmd_or_rsp_packet : public time_stream_packet {
  public:
    uint8_t payload[MAX_CMD_RSP_PAYLOAD_BYTES] = {0};
};

using command_queue_element = largest_cmd_or_rsp_packet;

// data packets
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


// alignment of the buffers used to queue data packets
constexpr unsigned VXSDR_DATA_BUFFER_ALIGNMENT = 64;

class alignas(VXSDR_DATA_BUFFER_ALIGNMENT) data_queue_element : public largest_data_packet {
};

#pragma pack(pop)

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
VXSDR_CHECK_SIZE_EQUALS(filter_coeff_packet, (4 * MAX_FRONTEND_FILTER_LENGTH + 16));
VXSDR_CHECK_SIZE_LESS_EQUAL(filter_coeff_packet, sizeof(largest_cmd_or_rsp_packet));
VXSDR_CHECK_SIZE_EQUALS(largest_cmd_or_rsp_packet, MAX_CMD_RSP_PACKET_BYTES);
VXSDR_CHECK_SIZE_EQUALS(async_msg_packet, 8);
VXSDR_CHECK_SIZE_EQUALS(largest_data_packet, MAX_DATA_PACKET_BYTES);
VXSDR_CHECK_SIZE_LESS_EQUAL(largest_data_packet, sizeof(data_queue_element));
