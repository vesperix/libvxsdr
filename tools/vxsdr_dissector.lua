-- Copyright (c) 2023 Vesperix Corporation
-- SPDX-License-Identifier: GPL-3.0-or-later

local vxsdr = Proto("vxsdr", "VXSDR Protocol")

local vxsdr_cmd_port = 1030
local vxsdr_data_port = 1031
local vxsdr_min_len = 8

local packet_types = {
    [0x00] = "TX_SIGNAL_DATA",
    [0x01] = "RX_SIGNAL_DATA",
    [0x02] = "DEVICE_CMD",
    [0x03] = "TX_RADIO_CMD",
    [0x04] = "RX_RADIO_CMD",
    [0x05] = "ASYNC_MSG",

    [0x12] = "DEVICE_CMD_RSP",
    [0x13] = "TX_RADIO_CMD_RSP",
    [0x14] = "RX_RADIO_CMD_RSP",

    [0x22] = "DEVICE_CMD_ERR",
    [0x23] = "TX_RADIO_CMD_ERR",
    [0x24] = "RX_RADIO_CMD_ERR",

    [0x30] = "TX_SIGNAL_DATA_ACK",
    [0x31] = "RX_SIGNAL_DATA_ACK",
    [0x32] = "DEVICE_CMD_ACK",
    [0x33] = "TX_RADIO_CMD_ACK",
    [0x34] = "RX_RADIO_CMD_ACK",
}

local packet_types_index = {}
for k, v in pairs(packet_types) do
    packet_types_index[v] = k
end

local device_cmds = {
    [0x00] = "HELLO",
    [0x01] = "SET_TIME_NOW",
    [0x02] = "SET_TIME_NEXT_PPS",
    [0x03] = "GET_TIME",
    [0x04] = "GET_STATUS",
    [0x05] = "CLEAR_STATUS",
    [0x06] = "GET_BUFFER_INFO",
    [0x07] = "GET_BUFFER_USE",
    [0x08] = "GET_STREAM_STATE",
    [0x09] = "STOP",
    [0x0A] = "GET_TRANSPORT_INFO", -- unimplemented
    [0x0B] = "GET_TRANSPORT_ADDR",
    [0x0C] = "GET_MAX_PAYLOAD",
    [0x0D] = "CLEAR_DATA_BUFFER",
    [0x0E] = "DISCOVER",
    [0x0F] = "SET_TRANSPORT_ADDR",
    [0x10] = "SET_MAX_PAYLOAD",
    [0x11] = "SAVE_TRANSPORT_ADDR",
    [0x12] = "GET_TIMING_INFO",   -- unimplemented
    [0x13] = "GET_TIMING_STATUS",
    [0x14] = "GET_TIMING_REF",  -- unimplemented
    [0x15] = "SET_TIMING_REF",  -- unimplemented
    [0x16] = "GET_TIMING_RESOLUTION",
    [0x17] = "GET_NUM_SENSORS",
    [0x18] = "GET_SENSOR",
    [0x19] = "GET_SENSOR_NAME",
    [0x1A] = "GET_CAPABILITIES",
    -- 0x20-0x2F reserved
    [0x3C] = "APP_UPDATE_MODE_SET",
    [0x3D] = "APP_UPDATE_DATA",
    [0x3E] = "APP_UPDATE_DONE",
    [0x3F] = "RESET"  -- unimplemented
}

local device_cmd_rsps = device_cmds

local device_cmds_index = {}
for k, v in pairs(device_cmds) do
    device_cmds_index[v] = k
end

local device_cmd_rsps_index = {}
for k, v in pairs(device_cmd_rsps) do
    device_cmd_rsps_index[v] = k
end

local cmd_errs = {
    [0x00] = "NO_ERROR",
    [0x01] = "BAD_CMD",
    [0x02] = "BUSY",
    [0x03] = "NO_SUCH_SUBDEVICE",
    [0x04] = "NO_SUCH_CHANNEL",
    [0x05] = "TIMEOUT",
    [0x06] = "BAD_HEADER_SIZE",
    [0x07] = "BAD_HEADER_FLAGS",
    [0x08] = "BAD_PARAMETER",
    [0x09] = "NOT_SUPPORTED",
    [0x0A] = "BAD_PACKET_SIZE",
    [0x0B] = "INTERNAL_ERROR",
    [0x0C] = "FAILED"
}

local async_msgs = {
    [0x00] = "NO_ERROR",
    [0x01] = "DATA_UNDERFLOW",
    [0x02] = "DATA_OVERFLOW",
    [0x03] = "OVER_TEMP",
    [0x04] = "POWER_ERROR",
    [0x05] = "FREQ_ERROR",
    [0x06] = "OUT_OF_SEQUENCE",
    [0x07] = "CTRL_OVERFLOW",
    [0x08] = "PPS_TIMEOUT",
    [0x09] = "VOLTAGE_ERROR",
    [0x0A] = "CURRENT_ERROR"
}

local async_sys_id = {
    [0x00] = "UNSPECIFIED",
    [0x10] = "TX",
    [0x20] = "RX",
    [0x30] = "FPGA"
 }

local radio_cmds = {
    [0x01] = "STOP",
    [0x02] = "START",
    [0x03] = "LOOP",
    [0x04] = "GET_RF_FREQ",
    [0x05] = "GET_RF_GAIN",
    [0x06] = "GET_SAMPLE_RATE",
    [0x07] = "GET_RF_BW",  -- unimplemented
    [0x08] = "GET_RF_ENABLED",
    [0x09] = "GET_RF_PORT",
    [0x0A] = "GET_NUM_RF_PORTS",
    [0x0B] = "GET_RF_PORT_NAME",
    [0x0C] = "GET_LO_INPUT", --unimplemented
    [0x0D] = "GET_LOCK_STATUS",
    [0x0E] = "GET_MASTER_CLK",
    [0x0F] = "GET_FILTER_COEFFS",
    [0x10] = "SET_RF_FREQ",
    [0x11] = "SET_RF_GAIN",
    [0x12] = "SET_SAMPLE_RATE",
    [0x13] = "SET_RF_BW",  -- unimplemented
    [0x14] = "SET_RF_ENABLED",
    [0x15] = "SET_RF_PORT",
    [0x16] = "SET_LO_INPUT", -- unimplemented
    [0x17] = "SET_MASTER_CLK", --unimplemented
    [0x18] = "GET_NUM_SUBDEVS",
    [0x19] = "GET_RF_FREQ_RANGE",
    [0x1A] = "GET_RF_GAIN_RANGE",
    [0x1B] = "GET_SAMPLE_RATE_RANGE",
    [0x1C] = "GET_NUM_CHANNELS",
    [0x1D] = "SET_ADC_TEST_PATTERN",
    [0x1E] = "GET_FILTER_LENGTH",
    [0x1F] = "GET_IQ_BIAS",
    [0x20] = "GET_IQ_CORR",
    [0x21] = "SET_IQ_BIAS",
    [0x22] = "SET_IQ_CORR",
    [0x23] = "SET_FILTER_ENABLED",
    [0x24] = "SET_FILTER_COEFFS",
    [0x25] = "GET_RF_NUM_GAIN_STAGES",
    [0x26] = "GET_RF_GAIN_STAGE_NAME",
    [0x27] = "GET_RF_GAIN_RANGE_STAGE",
    [0x28] = "GET_RF_GAIN_STAGE",
    [0x29] = "SET_RF_GAIN_STAGE",
    [0x2A] = "GET_RF_NUM_FREQ_STAGES",
    [0x2B] = "GET_RF_FREQ_STAGE_NAME",
    [0x2C] = "GET_RF_FREQ_RANGE_STAGE",
    [0x2D] = "GET_RF_FREQ_STAGE",
    [0x2E] = "SET_RF_FREQ_STAGE",
    [0x2F] = "GET_RF_BW_RANGE",
    [0x30] = "GET_CAPABILITIES"
}

local bool_flags = {[0] = "False", [1] = "True"}

local radio_cmds_index = {}
for k, v in pairs(radio_cmds) do
    radio_cmds_index[v] = k
end

local radio_cmd_rsps = radio_cmds

local radio_cmd_rsps_index = {}
for k, v in pairs(radio_cmd_rsps) do
    radio_cmd_rsps_index[v] = k
end

local packet_type_mask      = 0x003F
local command_mask          = 0x0FC0
local flags_mask            = 0xF000
-- FIXME: these need testing
local async_type_mask       = 0x03C0
local async_sys_mask        = 0x0C00

local pf_packet_type        = ProtoField.uint16("vxsdr.packet_type", "Packet Type", base.HEX, packet_types, packet_type_mask)
local pf_command            = ProtoField.uint16("vxsdr.command", "Command", base.HEX, nil, command_mask)
local pf_flags              = ProtoField.uint16("vxsdr.flags", "Flags", base.HEX, nil, flags_mask)
local pf_target_subdevice   = ProtoField.uint8("vxsdr.target_subdevice", "Target Subdevice", base.DEC)
local pf_channel            = ProtoField.uint8("vxsdr.channel", "Channel", base.DEC)
local pf_packet_size        = ProtoField.uint16("vxsdr.packet_size", "Packet Size", base.DEC)
local pf_sequence_counter   = ProtoField.uint16("vxsdr.sequence_counter", "Sequence Counter", base.DEC)

local pf_device_command     = ProtoField.uint16("vxsdr.device_command", "Device Command", base.HEX, device_cmds, command_mask)
local pf_device_command_err = ProtoField.uint16("vxsdr.device_command_err", "Device Command Error", base.HEX, device_cmds, command_mask)
local pf_device_command_rsp = ProtoField.uint16("vxsdr.device_command_rsp", "Device Command Response", base.HEX, device_cmds, command_mask)
local pf_radio_command      = ProtoField.uint16("vxsdr.radio_command", "Radio Command", base.HEX, radio_cmds, command_mask)
local pf_radio_command_err  = ProtoField.uint16("vxsdr.radio_command_err", "Radio Command Error", base.HEX, radio_cmds,command_mask)
local pf_radio_command_rsp  = ProtoField.uint16("vxsdr.radio_command_rsp", "Radio Command Response", base.HEX, radio_cmds,command_mask)
local pf_device_error       = ProtoField.uint32("vxsdr.device_error", "Device Error Reason", base.HEX, cmd_errs)
local pf_radio_error        = ProtoField.uint32("vxsdr.radio_error", "Radio Error Reason", base.HEX, cmd_errs)
local pf_async_msg_type     = ProtoField.uint16("vxsdr.async_msg_type", "Async Message Type", base.HEX, async_msgs, async_type_mask)
local pf_async_msg_sys      = ProtoField.uint16("vxsdr.async_msg_sys", "Async Message System", base.HEX, async_sys_id, async_sys_mask)

-- FIXME: still doesn't display flags properly
local pf_flag_ack           = ProtoField.uint16("vxsddr.flag_request_ack", "Request Ack", base.DEC, bool_flags, 0x1000)
local pf_flag_time          = ProtoField.uint16("vxsddr.flag_time_present", "Time Present", base.DEC, bool_flags, 0x2000)
local pf_flag_stream        = ProtoField.uint16("vxsddr.flag_stream_id_present", "Stream ID Present", base.DEC, bool_flags, 0x4000)

local pf_uint16_data        = ProtoField.uint16("vxsdr.u16_data", "UInt16 Data", base.DEC)
local pf_uint32_data        = ProtoField.uint32("vxsdr.u32_data", "UInt32 Data", base.DEC)
local pf_uint64_data        = ProtoField.uint64("vxsdr.u64_data", "UInt64 Data", base.DEC)
local pf_double_data        = ProtoField.double("vxsdr.f64_data", "Double Data")
local pf_boolean_data       = ProtoField.new("Boolean Data", "vxsddr.u32_bool_data", ftypes.UINT32,
                              {[0] = "False", [1] = "True"}, base.DEC)

local pf_uint32_stage       = ProtoField.uint32("vxsdr.u32_stage", "Stage", base.DEC)

local pf_double_fmin        = ProtoField.double("vxsdr.f64_fmin", "Minumum (Hz)")
local pf_double_fmax        = ProtoField.double("vxsdr.f64_fmax", "Maximum (Hz)")

local pf_double_gmin        = ProtoField.double("vxsdr.f64_gmin", "Minumum (dB)")
local pf_double_gmax        = ProtoField.double("vxsdr.f64_gmax", "Maximum (dB)")

local pf_uint32_sec         = ProtoField.uint32("vxsdr.u32_sec", "Seconds", base.DEC)
local pf_uint32_nsec        = ProtoField.uint32("vxsdr.u32_nsec", "Nanoseconds", base.DEC)

local pf_double_res         = ProtoField.double("vxsdr.f64_time_resolution", "Time Resolution")

local pf_stream_state       = ProtoField.new("Stream State", "vxsdr.stream_state", ftypes.UINT32,
                              {[0] = "Stopped", [1] = "Waiting", [2] = "Running", [3] = "Error"}, base.DEC, 0x00000003)

local pf_ext_pps_lock       = ProtoField.new("External PPS Lock", "vxsddr.ext_pps_lock", ftypes.UINT32,
                              {[0] = "False", [1] = "True"}, base.DEC, 0x00000001)
local pf_ext_10MHz_lock     = ProtoField.new("External 10Mhz Lock", "vxsdr.ext_10Mhz_lock", ftypes.UINT32,
                              {[0] = "False", [1] = "True"}, base.DEC, 0x00000002)
local pf_ref_osc_lock       = ProtoField.new("Reference Oscillator Lock", "vxsdr.ref_lock", ftypes.UINT32,
                              {[0] = "False", [1] = "True"}, base.DEC, 0x00000004)

local pf_double_ibias       = ProtoField.double("vxsdr.f64_ibias", "I Bias")
local pf_double_qbias       = ProtoField.double("vxsdr.f64_qbias", "Q Bias")

local pf_double_a11         = ProtoField.double("vxsdr.f64_iqcorr_a11", "a(1,1)")
local pf_double_a12         = ProtoField.double("vxsdr.f64_iqcorr_a12", "a(1,2)")
local pf_double_a21         = ProtoField.double("vxsdr.f64_iqcorr_a21", "a(2,1)")
local pf_double_a22         = ProtoField.double("vxsdr.f64_iqcorr_a22", "a(2,2)")

local pf_double_freq        = ProtoField.double("vxsdr.f64_freq", "Frequency (Hz)")
local pf_double_rtol        = ProtoField.double("vxsdr.f64_rtol", "Relative Tolerance")

local pf_double_gain        = ProtoField.double("vxsdr.f64gain", "Gain (dB)")
local pf_double_rate        = ProtoField.double("vxsdr.f64rate", "Rate (Hz)")

vxsdr.fields = { pf_packet_type, pf_command, pf_flags, pf_target_subdevice, pf_channel, pf_packet_size, pf_sequence_counter,
                 pf_device_command, pf_radio_command, pf_device_command_err, pf_radio_command_err,
                 pf_device_command_rsp, pf_radio_command_rsp, pf_device_error, pf_radio_error, pf_async_msg_type, pf_async_msg_sys,
--                 pf_flag_ack, pf_flag_time, pf_flag_stream,
                 pf_uint16_data, pf_uint32_data, pf_uint64_data, pf_double_data, pf_boolean_data,
                 pf_uint32_stage,
                 pf_double_fmin, pf_double_fmax, pf_double_gmin, pf_double_gmax,
                 pf_uint32_sec, pf_uint32_nsec, pf_double_res, pf_stream_state,
                 pf_ext_pps_lock, pf_ext_10MHz_lock, pf_ref_osc_lock, pf_double_ibias, pf_double_qbias,
                 pf_double_a11, pf_double_a12, pf_double_a21, pf_double_a22, pf_double_freq, pf_double_rtol,
                 pf_double_gain, pf_double_rate }

function vxsdr.dissector(tvbuf, pktinfo, root)

    pktinfo.cols.protocol:set("VXSDR")
    local pkt_len = tvbuf:reported_length_remaining()
    local pkt_type = bit32.band(tvbuf(0, 1):le_uint(), packet_type_mask)
    local pkt_cmd  = bit32.rshift(bit32.band(tvbuf(0, 2):le_uint(), 0x0FC0), 6)
    local pkt_size = tvbuf(4, 2):le_uint()

    local tree = root:add(vxsdr, tvbuf(0, pktlen))

    if pkt_len < vxsdr_min_len then
        return
    end

    tree:add_le(   pf_packet_type,      tvbuf(0, 2))
    if pkt_type == packet_types_index["DEVICE_CMD"] then
        tree:add_le(   pf_device_command,         tvbuf(0, 2))
    elseif pkt_type == packet_types_index["DEVICE_CMD_RSP"] then
        tree:add_le(   pf_device_command_rsp,     tvbuf(0, 2))
    elseif pkt_type == packet_types_index["DEVICE_CMD_ERR"] then
        tree:add_le(   pf_device_command_err,     tvbuf(0, 2))
    elseif pkt_type == packet_types_index["TX_RADIO_CMD"] or pkt_type ==packet_types_index["RX_RADIO_CMD"] then
        tree:add_le(   pf_radio_command,          tvbuf(0, 2))
    elseif pkt_type == packet_types_index["TX_RADIO_CMD_RSP"] or pkt_type ==packet_types_index["RX_RADIO_CMD_RSP"] then
        tree:add_le(   pf_radio_command_rsp,      tvbuf(0, 2))
    elseif ppkt_type == packet_types_index["TX_RADIO_CMD_ERR"] or pkt_type ==packet_types_index["RX_RADIO_CMD_ERR"] then
        tree:add_le(   pf_radio_command_err,      tvbuf(0, 2))
    elseif pkt_type == packet_types_index["ASYNC_MSG"] then
        tree:add_le(   pf_async_msg_type,         tvbuf(0, 2))
        tree:add_le(   pf_async_msg_sys,          tvbuf(0, 2))
    else
        tree:add_le(   pf_command,                tvbuf(0, 2))
    end
    tree:add_le(pf_flags,            tvbuf(0, 2))
--  flags are still not displayed correctly
--    tree:add(   pf_flag_ack,         tvbuf(0, 2))
--    tree:add(   pf_flag_time,        tvbuf(0, 2))
--    tree:add(   pf_flag_stream,      tvbuf(0, 2))
    tree:add(   pf_target_subdevice, tvbuf(2, 1))
    tree:add(   pf_channel,          tvbuf(3, 1))
    tree:add_le(pf_packet_size,      tvbuf(4, 2))
    tree:add_le(pf_sequence_counter, tvbuf(6, 2))
    if pkt_size > 8 then
        -- device commands
        if pkt_type == packet_types_index["DEVICE_CMD"] then
            -- set time now or next pps
            if pkt_cmd == device_cmds_index["SET_TIME_NOW"] or pkt_cmd == device_cmds_index["SET_TIME_NEXT_PPS"] then
                tree:add_le(pf_uint32_sec, tvbuf( 8, 4))
                tree:add_le(pf_uint32_nsec, tvbuf(12, 4))
            elseif pkt_cmd == device_cmds_index["GET_NUM_SENSORS"] or
                   pkt_cmd == device_cmds_index["GET_SENSOR_NAME"] then
                tree:add_le(pf_uint32_sec, tvbuf( 8, 4))
            end
        -- device command responses
        elseif pkt_type == packet_types_index["DEVICE_CMD_RSP"] then
            if pkt_cmd == device_cmd_rsps_index["HELLO"] then
                tree:add_le(pf_uint32_data, tvbuf( 8, 4))
                tree:add_le(pf_uint32_data, tvbuf(12, 4))
                tree:add_le(pf_uint32_data, tvbuf(16, 4))
                tree:add_le(pf_uint32_data, tvbuf(20, 4))
            elseif pkt_cmd == device_cmd_rsps_index["GET_TIME"] then
                tree:add_le(pf_uint32_sec, tvbuf( 8, 4))
                tree:add_le(pf_uint32_nsec, tvbuf(12, 4))
            elseif pkt_cmd == device_cmd_rsps_index["GET_TIMING_RESOLUTION"] then
                tree:add_le(pf_double_res, tvbuf( 8, 8))
            elseif pkt_cmd == device_cmd_rsps_index["GET_TIMING_STATUS"] then
                tree:add_le(pf_ext_pps_lock, tvbuf( 8, 4))
                tree:add_le(pf_ext_10MHz_lock, tvbuf( 8, 4))
                tree:add_le(pf_ref_osc_lock, tvbuf( 8, 4))
            elseif pkt_cmd == device_cmd_rsps_index["GET_BUFFER_INFO"] or pkt_cmd == device_cmd_rsps_index["GET_BUFFER_USE"] then
                -- get_time, buffer info, buffer use response
                tree:add_le(pf_uint32_data, tvbuf( 8, 4))
                tree:add_le(pf_uint32_data, tvbuf(12, 4))
            elseif pkt_cmd == device_cmd_rsps_index["GET_STREAM_STATE"] then
                tree:add_le(pf_stream_state, tvbuf( 8, 4))
            elseif pkt_cmd == device_cmd_rsps_index["GET_STATUS"] then
                -- status response
                tree:add_le(pf_uint32_data, tvbuf( 8, 4))
                tree:add_le(pf_uint32_data, tvbuf(12, 4))
                tree:add_le(pf_uint32_data, tvbuf(16, 4))
                tree:add_le(pf_uint32_data, tvbuf(20, 4))
                tree:add_le(pf_uint32_data, tvbuf(24, 4))
                tree:add_le(pf_uint32_data, tvbuf(28, 4))
            end
        -- device command errors
        elseif pkt_type == packet_types_index["DEVICE_CMD_ERR"] then
            tree:add_le(pf_device_error, tvbuf( 8, 4))
            tree:add_le(pf_uint32_data,  tvbuf(12, 4))
        -- radio commands
        elseif pkt_type == packet_types_index["TX_RADIO_CMD"] or pkt_type == packet_types_index["RX_RADIO_CMD"] then
            -- set rf_enabled or filter_enables
            if pkt_cmd == radio_cmds_index["SET_RF_ENABLED"] or
               pkt_cmd == radio_cmds_index["SET_FILTER_ENABLED"]  then
                tree:add_le(pf_boolean_data, tvbuf(8, 4))
            -- set freq, gain, or rate
            elseif pkt_cmd == radio_cmds_index["SET_RF_FREQ"] then
                tree:add_le(pf_double_freq, tvbuf(8, 8))
                tree:add_le(pf_double_rtol, tvbuf(16, 8))
            elseif pkt_cmd == radio_cmds_index["SET_RF_FREQ_STAGE"] then
                tree:add_le(pf_uint32_stage, tvbuf(8, 4))
                tree:add_le(pf_double_freq, tvbuf(16, 8))
            elseif pkt_cmd == radio_cmds_index["SET_RF_GAIN"] then
                tree:add_le(pf_double_gain, tvbuf(8, 8))
            elseif pkt_cmd == radio_cmds_index["SET_RF_GAIN_STAGE"] then
                tree:add_le(pf_uint32_stage, tvbuf(8, 4))
                tree:add_le(pf_double_gain, tvbuf(16, 8))
            elseif pkt_cmd == radio_cmds_index["GET_RF_GAIN_STAGE"] or
                   pkt_cmd == radio_cmds_index["GET_RF_FREQ_STAGE"] or
                   pkt_cmd == radio_cmds_index["GET_RF_GAIN_STAGE_NAME"] or
                   pkt_cmd == radio_cmds_index["GET_RF_FREQ_STAGE_NAME"] or
                   pkt_cmd == radio_cmds_index["GET_RF_GAIN_RANGE_STAGE"] or
                   pkt_cmd == radio_cmds_index["GET_RF_FREQ_RANGE_STAGE"] then
                tree:add_le(pf_uint32_stage, tvbuf(8, 4))
            elseif pkt_cmd == radio_cmds_index["SET_SAMPLE_RATE"]  then
                tree:add_le(pf_double_rate, tvbuf(8, 8))
            elseif pkt_cmd == radio_cmds_index["SET_IQ_BIAS"] then
                tree:add_le(pf_double_ibias, tvbuf(8, 8))
                tree:add_le(pf_double_qbias, tvbuf(16, 8))
            elseif pkt_cmd == radio_cmds_index["SET_IQ_CORR"] then
                tree:add_le(pf_double_a11, tvbuf(8, 8))
                tree:add_le(pf_double_a12, tvbuf(16, 8))
                tree:add_le(pf_double_a21, tvbuf(24, 8))
                tree:add_le(pf_double_a22, tvbuf(32, 8))
            -- start
            elseif pkt_cmd == radio_cmds_index["START"]  then
                tree:add_le(pf_uint32_sec, tvbuf( 8, 4))
                tree:add_le(pf_uint32_nsec, tvbuf(12, 4))
                tree:add_le(pf_uint64_data, tvbuf(16, 8))
            -- loop
            elseif pkt_cmd == radio_cmds_index["LOOP"]  then
                tree:add_le(pf_uint32_sec, tvbuf( 8, 4))
                tree:add_le(pf_uint32_nsec, tvbuf(12, 4))
                tree:add_le(pf_uint64_data, tvbuf(16, 8))
                tree:add_le(pf_uint32_sec, tvbuf(24, 4))
                tree:add_le(pf_uint32_nsec, tvbuf(28, 4))
                tree:add_le(pf_uint32_data, tvbuf(32, 4))
            elseif pkt_cmd == radio_cmds_index["STOP"]  then
                tree:add_le(pf_uint32_sec, tvbuf( 8, 4))
                tree:add_le(pf_uint32_nsec, tvbuf(12, 4))
            -- set rf port
            elseif pkt_cmd == radio_cmds_index["SET_RF_PORT"] or
                   pkt_cmd == radio_cmds_index["SET_LO_INPUT"] then
                tree:add_le(pf_uint32_data, tvbuf( 8, 4))
            --elseif pkt_cmd == radio_cmds_index["SET_FILTER_COEFFICIENTS"]  then
            end
        -- radio command responses
        elseif pkt_type == packet_types_index["TX_RADIO_CMD_RSP"] or pkt_type == packet_types_index["RX_RADIO_CMD_RSP"] then
            -- get rf_enabled
            if pkt_cmd == radio_cmd_rsps_index["GET_RF_ENABLED"] then
                tree:add_le(pf_boolean_data, tvbuf(8, 4))
            -- get freq, gain, or rate
            elseif pkt_cmd == radio_cmd_rsps_index["GET_RF_FREQ"] or
                   pkt_cmd == radio_cmd_rsps_index["GET_RF_FREQ_STAGE"] then
                tree:add_le(pf_double_freq, tvbuf(8, 8))
            elseif pkt_cmd == radio_cmd_rsps_index["GET_RF_GAIN"] or
                   pkt_cmd == radio_cmd_rsps_index["GET_RF_GAIN_STAGE"]then
                tree:add_le(pf_double_gain, tvbuf(8, 8))
            elseif pkt_cmd == radio_cmd_rsps_index["GET_SAMPLE_RATE"]  then
                tree:add_le(pf_double_freq, tvbuf(8, 8))
            elseif pkt_cmd == radio_cmd_rsps_index["GET_RF_FREQ_RANGE"] or
                   pkt_cmd == radio_cmd_rsps_index["GET_RF_FREQ_RANGE_STAGE"] or
                   pkt_cmd == radio_cmd_rsps_index["GET_SAMPLE_RATE_RANGE"] then
                tree:add_le(pf_double_fmin, tvbuf(8, 8))
                tree:add_le(pf_double_fmax, tvbuf(16, 8))
            elseif pkt_cmd == radio_cmd_rsps_index["GET_RF_GAIN_RANGE"] or
                   pkt_cmd == radio_cmd_rsps_index["GET_RF_GAIN_RANGE_STAGE"] then
                tree:add_le(pf_double_gmin, tvbuf(8, 8))
                tree:add_le(pf_double_gmax, tvbuf(16, 8))
            elseif pkt_cmd == radio_cmd_rsps_index["GET_IQ_BIAS"] then
                tree:add_le(pf_double_ibias, tvbuf(8, 8))
                tree:add_le(pf_double_qbias, tvbuf(16, 8))
            elseif pkt_cmd == radio_cmd_rsps_index["GET_IQ_CORR"] then
                tree:add_le(pf_double_a11, tvbuf(8, 8))
                tree:add_le(pf_double_a12, tvbuf(16, 8))
                tree:add_le(pf_double_a21, tvbuf(24, 8))
                tree:add_le(pf_double_a22, tvbuf(32, 8))
            elseif pkt_cmd == radio_cmd_rsps_index["GET_NUM_SUBDEVS"] or
                   pkt_cmd == radio_cmd_rsps_index["GET_NUM_CHANNELS"] or
                   pkt_cmd == radio_cmd_rsps_index["GET_NUM_RF_PORTS"] or
                   pkt_cmd == radio_cmd_rsps_index["GET_FILTER_LENGTH"] or
                   pkt_cmd == radio_cmd_rsps_index["GET_RF_NUM_GAIN_STAGES"] or
                   pkt_cmd == radio_cmd_rsps_index["GET_RF_NUM_FREQ_STAGES"] or
                   pkt_cmd == radio_cmd_rsps_index["GET_RF_PORT"] then
                tree:add_le(pf_uint32_data, tvbuf( 8, 4))
            end
        -- radio command errors
        elseif pkt_type == packet_types_index["TX_RADIO_CMD_ERR"] or pkt_type == packet_types_index["RX_RADIO_CMD_ERR"] then
            tree:add_le(pf_radio_error, tvbuf( 8, 4))
            tree:add_le(pf_uint32_data, tvbuf(12, 4))
        -- tx data acks
        elseif pkt_type == packet_types_index["TX_SIGNAL_DATA_ACK"] then
            tree:add_le(pf_uint64_data, tvbuf( 8, 8))
            tree:add_le(pf_uint32_data, tvbuf(16, 4))
            tree:add_le(pf_uint32_data, tvbuf(20, 4))
        end
    end
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(vxsdr_cmd_port, vxsdr)
udp_port:add(vxsdr_data_port, vxsdr)
