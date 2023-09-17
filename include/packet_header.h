// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PACKET_HEADER_H
#define PACKET_HEADER_H
/*
  VXSDR packet definitions
  Version 1.0.6 18 Sep 2023
*/

#define PACKET_VERSION_STRING          "1.0.6"
#define PACKET_VERSION_MAJOR               (1)
#define PACKET_VERSION_MINOR               (0)
#define PACKET_VERSION_PATCH               (6)

/*
   The packet header and the elements used to fill it are defined below.
   Each packet contains a 64-bit header, followed by any necessary payload.
   The widths of each field in the header, in bits, are shown below:

   |TYPE | COMMAND | FLAGS | SUBDEVICE | CHANNEL | SIZE | SEQUENCE
      6       6        4         8          8       16      16

   NOTE: the size and sequence elements are little-endian uint16_ts.
*/

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define VXSDR_PACKET_TYPE_BITS        (6)
#define VXSDR_COMMAND_BITS            (6)
#define VXSDR_FLAGS_BITS              (4)

#pragma pack(push, 1)

typedef struct {
    uint16_t packet_type       : VXSDR_PACKET_TYPE_BITS;
    uint16_t command           : VXSDR_COMMAND_BITS;
    uint16_t flags             : VXSDR_FLAGS_BITS;
    uint8_t  subdevice;
    uint8_t  channel;
    uint16_t packet_size;            // length of packet including header in bytes
    uint16_t sequence_counter;       // single sequence for all packets
} packet_header;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif // __cplusplus

// Packet Types: 6 bits
//   TX data is sent only from a host to a radio
#define PACKET_TYPE_TX_SIGNAL_DATA             (0x00U)
//   RX data is sent only from a radio to a host
#define PACKET_TYPE_RX_SIGNAL_DATA             (0x01U)
//   Commands are sent only from a host to a radio, and may
//   have responses or acks, depending on the command
#define PACKET_TYPE_DEVICE_CMD                 (0x02U)
#define PACKET_TYPE_TX_RADIO_CMD               (0x03U)
#define PACKET_TYPE_RX_RADIO_CMD               (0x04U)
//   Async messages are sent by a radio only, and are not
//   responses to a host message
#define PACKET_TYPE_ASYNC_MSG                  (0x05U)

// indicators are applied to each packet type to indicate responses, errors, and acks
// they occupy the top 2 bits of the packet type, which are zero for a command
#define PACKET_RSP_INDICATOR                   (0x10U)
#define PACKET_ERR_INDICATOR                   (0x20U)
#define PACKET_ACK_INDICATOR                   (0x30U)
#define PACKET_INDICATOR_MASK                  (0x30U)
#define PACKET_TYPE_MASK                       (0x0FU)

// convenience macros to turn a command into an err, rsp, or ack
#define PACKET_TYPE_MAKE_ERR(x)                (((x) & PACKET_TYPE_MASK) | PACKET_ERR_INDICATOR)
#define PACKET_TYPE_MAKE_RSP(x)                (((x) & PACKET_TYPE_MASK) | PACKET_RSP_INDICATOR)
#define PACKET_TYPE_MAKE_ACK(x)                (((x) & PACKET_TYPE_MASK) | PACKET_ACK_INDICATOR)

//   Errors, responses, and acks are sent by a radio only,
//   and only in response to a host packet
#define PACKET_TYPE_DEVICE_CMD_ERR             PACKET_TYPE_MAKE_ERR(PACKET_TYPE_DEVICE_CMD)
#define PACKET_TYPE_TX_RADIO_CMD_ERR           PACKET_TYPE_MAKE_ERR(PACKET_TYPE_TX_RADIO_CMD)
#define PACKET_TYPE_RX_RADIO_CMD_ERR           PACKET_TYPE_MAKE_ERR(PACKET_TYPE_RX_RADIO_CMD)

#define PACKET_TYPE_DEVICE_CMD_RSP             PACKET_TYPE_MAKE_RSP(PACKET_TYPE_DEVICE_CMD)
#define PACKET_TYPE_TX_RADIO_CMD_RSP           PACKET_TYPE_MAKE_RSP(PACKET_TYPE_TX_RADIO_CMD)
#define PACKET_TYPE_RX_RADIO_CMD_RSP           PACKET_TYPE_MAKE_RSP(PACKET_TYPE_RX_RADIO_CMD)

#define PACKET_TYPE_TX_SIGNAL_DATA_ACK         PACKET_TYPE_MAKE_ACK(PACKET_TYPE_TX_SIGNAL_DATA)
#define PACKET_TYPE_RX_SIGNAL_DATA_ACK         PACKET_TYPE_MAKE_ACK(PACKET_TYPE_RX_SIGNAL_DATA)
#define PACKET_TYPE_DEVICE_CMD_ACK             PACKET_TYPE_MAKE_ACK(PACKET_TYPE_DEVICE_CMD)
#define PACKET_TYPE_TX_RADIO_CMD_ACK           PACKET_TYPE_MAKE_ACK(PACKET_TYPE_TX_RADIO_CMD)
#define PACKET_TYPE_RX_RADIO_CMD_ACK           PACKET_TYPE_MAKE_ACK(PACKET_TYPE_RX_RADIO_CMD)

// Device Commands : 6 bits
#define DEVICE_CMD_HELLO                       (0x00)

#define DEVICE_CMD_SET_TIME_NOW                (0x01)  // error if TIME_PRESENT is 0
#define DEVICE_CMD_SET_TIME_NEXT_PPS           (0x02)  // error if TIME_PRESENT is 0
#define DEVICE_CMD_GET_TIME                    (0x03)

#define DEVICE_CMD_GET_STATUS                  (0x04)
#define DEVICE_CMD_CLEAR_STATUS                (0x05)  // resets error flags and counters

#define DEVICE_CMD_GET_BUFFER_INFO             (0x06)  // info on how many buffers and their purpose
#define DEVICE_CMD_GET_BUFFER_USE              (0x07)  // asks for buffer info

#define DEVICE_CMD_GET_STREAM_STATE            (0x08)

#define DEVICE_CMD_STOP                        (0x09)  // stop all streaming and return to wait state within session

#define DEVICE_CMD_GET_TRANSPORT_INFO          (0x0A)  // UNIMPLEMENTED // how many interfaces, what kind
#define DEVICE_CMD_GET_TRANSPORT_ADDR          (0x0B)
#define DEVICE_CMD_GET_MAX_PAYLOAD             (0x0C)

#define DEVICE_CMD_CLEAR_DATA_BUFFER           (0x0D)

#define DEVICE_CMD_DISCOVER                    (0x0E)
#define DEVICE_CMD_SET_TRANSPORT_ADDR          (0x0F)
#define DEVICE_CMD_SET_MAX_PAYLOAD             (0x10)
#define DEVICE_CMD_SAVE_TRANSPORT_ADDR         (0x11)
// (unimplemented DEVICE_CMD_GET_CHANNEL_INFO removed)
#define DEVICE_CMD_GET_TIMING_INFO             (0x13)  // UNIMPLEMENTED // what timing inputs are available
#define DEVICE_CMD_GET_TIMING_STATUS           (0x14)  // PPS present/absent; 10 MHz locked/unlocked
#define DEVICE_CMD_GET_TIMING_REF              (0x15)  // UNIMPLEMENTED // current sources of 10 MHz and PPS
#define DEVICE_CMD_SET_TIMING_REF              (0x16)  // UNIMPLEMENTED // set sources of 10 MHz and PPS
#define DEVICE_CMD_GET_TIMING_RESOLUTION       (0x17)

#define DEVICE_CMD_GET_NUM_SENSORS             (0x18)
#define DEVICE_CMD_GET_SENSOR                  (0x19) // UNIMPLEMENTED
#define DEVICE_CMD_GET_SENSOR_NAME             (0x1A) // UNIMPLEMENTED

// these may be removed before release
#define DEVICE_CMD_READ_DAC_REG                (0x30)
#define DEVICE_CMD_WRITE_DAC_REG               (0x31)
#define DEVICE_CMD_READ_ADC_REG                (0x32)
#define DEVICE_CMD_WRITE_ADC_REG               (0x33)

#define DEVICE_CMD_APP_UPDATE_MODE_SET         (0x3C)
#define DEVICE_CMD_APP_UPDATE_DATA             (0x3D)
#define DEVICE_CMD_APP_UPDATE_DONE             (0x3E)
#define DEVICE_CMD_RESET                       (0x3F)  // do a power on reset (closing session and returning to idle)

// Generic Errors used for Device Commands and Radio Commands
#define ERR_NO_ERROR                           (0x00)  // no error
#define ERR_BAD_COMMAND                        (0x01)  // generic bad stuff happened; can substitute for more detailed ones
#define ERR_BUSY                               (0x02)  // can't accept that command because we're running
#define ERR_NO_SUCH_SUBDEVICE                  (0x03)  // the specified subdevice doesn't exist
#define ERR_NO_SUCH_CHANNEL                    (0x04)  // the specified channel doesn't exist
#define ERR_TIMEOUT                            (0x05)  // tried to do command but something didn't answer
#define ERR_BAD_HEADER_SIZE                    (0x06)  // size field in header does not match allowed values
#define ERR_BAD_HEADER_FLAGS                   (0x07)  // flags field in header does not match allowed values
#define ERR_BAD_PARAMETER                      (0x08)  // command parameter is out of allowed range
#define ERR_NOT_SUPPORTED                      (0x09)  // this device can't do that command
#define ERR_BAD_PACKET_SIZE                    (0x0A)  // packet size does not match header
#define ERR_FAILED                             (0x0B)  // should have worked, but didn't

// Radio Commands
#define RADIO_CMD_STOP_NOW                     (0x01)  // stops anything going on
#define RADIO_CMD_START                        (0x02)  // error if TIME_PRESENT is 0
#define RADIO_CMD_LOOP                         (0x03)  // error if TIME_PRESENT is 0

#define RADIO_CMD_GET_RF_FREQ                  (0x04)
#define RADIO_CMD_GET_RF_GAIN                  (0x05)
#define RADIO_CMD_GET_SAMPLE_RATE              (0x06)
#define RADIO_CMD_GET_RF_BW                    (0x07)
#define RADIO_CMD_GET_RF_ENABLED               (0x08)
#define RADIO_CMD_GET_RF_PORT                  (0x09)
#define RADIO_CMD_GET_NUM_RF_PORTS             (0x0A)
#define RADIO_CMD_GET_RF_PORT_NAME             (0x0B)
#define RADIO_CMD_GET_LO_INPUT                 (0x0C)
#define RADIO_CMD_GET_LOCK_STATUS              (0x0D)
#define RADIO_CMD_GET_MASTER_CLK               (0x0E)
#define RADIO_CMD_GET_FILTER_COEFFS            (0x10)

#define RADIO_CMD_SET_RF_FREQ                  (0x11)
#define RADIO_CMD_SET_RF_GAIN                  (0x12)
#define RADIO_CMD_SET_SAMPLE_RATE              (0x13)
#define RADIO_CMD_SET_RF_BW                    (0x14)  // UNIMPLEMENTED
#define RADIO_CMD_SET_RF_ENABLED               (0x15)
#define RADIO_CMD_SET_RF_PORT                  (0x16)
#define RADIO_CMD_SET_RF_PORT_BY_NAME          (0x17)
#define RADIO_CMD_SET_LO_INPUT                 (0x18)
#define RADIO_CMD_SET_MASTER_CLK               (0x19)  // UNIMPLEMENTED

#define RADIO_CMD_GET_NUM_SUBDEVS              (0x20)
#define RADIO_CMD_GET_RF_FREQ_RANGE            (0x21)
#define RADIO_CMD_GET_RF_GAIN_RANGE            (0x22)
#define RADIO_CMD_GET_SAMPLE_RATE_RANGE        (0x23)
#define RADIO_CMD_GET_NUM_CHANNELS             (0x24)
#define RADIO_CMD_SET_ADC_TEST_PATTERN         (0x25)
#define RADIO_CMD_GET_FILTER_LENGTH            (0x26)

#define RADIO_CMD_GET_IQ_BIAS                  (0x27)
#define RADIO_CMD_GET_IQ_CORR                  (0x28)
#define RADIO_CMD_SET_IQ_BIAS                  (0x29)
#define RADIO_CMD_SET_IQ_CORR                  (0x2A)
#define RADIO_CMD_SET_FILTER_ENABLED           (0x2B)
#define RADIO_CMD_SET_FILTER_COEFFS            (0x2C)

#define RADIO_CMD_GET_RF_GAIN_STAGE_NAME       (0x1A)
#define RADIO_CMD_GET_RF_GAIN_RANGE_STAGE      (0x1B)
#define RADIO_CMD_GET_RF_GAIN_STAGE            (0x1C)
#define RADIO_CMD_SET_RF_GAIN_STAGE            (0x1D)

#define RADIO_CMD_GET_RF_FREQ_STAGE_NAME       (0x1E)
#define RADIO_CMD_GET_RF_FREQ_RANGE_STAGE      (0x1F)
#define RADIO_CMD_GET_RF_FREQ_STAGE            (0x0F)
#define RADIO_CMD_SET_RF_FREQ_STAGE            (0x2E)

// Flags : 4 bits
#define FLAGS_REQUEST_ACK                      (0x01U)
#define FLAGS_TIME_PRESENT                     (0x02U)
#define FLAGS_STREAM_ID_PRESENT                (0x04U)

// Async messages : 6 bits in command field

// lower 4 bits give error type
#define ASYNC_NO_ERROR                         (0x00)
#define ASYNC_DATA_UNDERFLOW                   (0x01)
#define ASYNC_DATA_OVERFLOW                    (0x02)
#define ASYNC_OVER_TEMP                        (0x03)  // too hot
#define ASYNC_POWER_ERROR                      (0x04)
#define ASYNC_FREQ_ERROR                       (0x05)  // loss of lock, etc
#define ASYNC_OUT_OF_SEQUENCE                  (0x06)
#define ASYNC_CTRL_OVERFLOW                    (0x07)
#define ASYNC_PPS_TIMEOUT                      (0x08)
#define ASYNC_VOLTAGE_ERROR                    (0x09)
#define ASYNC_CURRENT_ERROR                    (0x0A)
// 0x0B - 0x0F unused
#define ASYNC_ERROR_TYPE_MASK                  (0x0FU)

// upper 2 bits give affected system
#define ASYNC_UNSPECIFIED                      (0x00)
#define ASYNC_TX                               (0x10)
#define ASYNC_RX                               (0x20)
#define ASYNC_FPGA                             (0x30)
#define ASYNC_AFFECTED_SYSTEM_MASK             (0x30U)

// Stream State
#define STREAM_STATE_RX_RUNNING_FLAG           (0x1ULL)
#define STREAM_STATE_RX_WAITING_FLAG           (0x2ULL)
#define STREAM_STATE_TX_RUNNING_FLAG           (0x1ULL << 32U)
#define STREAM_STATE_TX_WAITING_FLAG           (0x2ULL << 32U)

// Capabilities
#define CAPABILITY_HAS_TUNING                  (0x01U)
#define CAPABILITY_HAS_MANUAL_GAIN             (0x02U)
#define CAPABILITY_HAS_AUTO_GAIN               (0x03U)
#define CAPABILITY_HAS_ADJUSTABLE_RF_BW        (0x04U)
#define CAPABILITY_HAS_ADJUSTABLE_IF_BW        (0x05U)
#define CAPABILITY_HAS_REF_LOCK_DETECT         (0x06U)
#define CAPABILITY_HAS_SYNTH_LOCK_DETECT       (0x07U)
#define CAPABILITY_HAS_EXTERNAL_REF            (0x08U)
#define CAPABILITY_HAS_EXTERNAL_PPS            (0x09U)
#define CAPABILITY_HAS_EXTERNAL_LO_INPUT       (0x0AU)
#define CAPABILITY_HAS_LIMITER                 (0x0BU)
#define CAPABILITY_HAS_LIMIT_DETECT            (0x0CU)
#define CAPABILITY_HAS_MANUAL_IQ_BIAS_CORR     (0x0DU)
#define CAPABILITY_HAS_MANUAL_IQ_BALANCE_CORR  (0x0EU)
#define CAPABILITY_HAS_AUTO_IQ_BIAS_CORR       (0x0FU)
#define CAPABILITY_HAS_AUTO_IQ_BALANCE_CORR    (0x10U)
#define CAPABILITY_HAS_TEMP_MEASURE            (0x11U)
#define CAPABILITY_HAS_DC_POWER_MEASURE        (0x12U)
#define CAPABILITY_HAS_RF_POWER_MEASURE        (0x13U)

#define MAX_DATA_LENGTH_SAMPLES                (2048UL)
#define MAX_FRONTEND_FILTER_LENGTH             (16U)
#define MAX_PAYLOAD_LENGTH_BYTES               (4 * MAX_FRONTEND_FILTER_LENGTH + 8)  // maximum length of a CMD or RSP packet, excluding header
#define MAX_NAME_LENGTH_BYTES                  (8) // maximum length of a name (sensors, gains stages) including terminating null

#define VXSDR_ALL_SUBDEVICES                   (0xFF)
#define VXSDR_ALL_CHANNELS                     (0xFF)

#ifdef __cplusplus
#define VXSDR_CHECK_SIZE_EQUALS(x, n)     static_assert(sizeof(x) == (n), "sizeof(" #x ") != " #n)
#define VXSDR_CHECK_SIZE_LESS_EQUAL(x, n) static_assert(sizeof(x) <= (n), "sizeof(" #x ") > " #n)
#else // need to use the old form until c23
#define VXSDR_CHECK_SIZE_EQUALS(x, n)     _Static_assert(sizeof(x) == (n), "sizeof(" #x ") != " #n)
#define VXSDR_CHECK_SIZE_LESS_EQUAL(x, n) _Static_assert(sizeof(x) <= (n), "sizeof(" #x ") > " #n)
#endif

// Check that size is as expected
VXSDR_CHECK_SIZE_EQUALS(packet_header, 8);

#endif // PACKET_HEADER_H
