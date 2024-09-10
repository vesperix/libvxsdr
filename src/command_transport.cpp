#include <array>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <string>
#include <stdexcept>

#include <errno.h>

#include "logging.hpp"
#include "vxsdr_packets.hpp"
#include "vxsdr_queues.hpp"
#include "vxsdr_transport.hpp"

void command_transport::command_send() {
    LOG_DEBUG("{:s} command tx started", get_transport_type());
    static command_queue_element packet_buffer;

    tx_state = TRANSPORT_READY;
    LOG_DEBUG("{:s} command tx in READY state", get_transport_type());

    while (not sender_thread_stop_flag) {
        if (command_queue.pop_or_timeout(packet_buffer, send_thread_wait_us)) {
            send_packet(packet_buffer);
        }
    }

    tx_state = TRANSPORT_SHUTDOWN;

    LOG_DEBUG("{:s} command tx exiting", get_transport_type());
}

void command_transport::command_receive() {
    LOG_DEBUG("{:s} command rx started", get_transport_type());
    uint16_t last_seq = 0;
    bytes_received    = 0;
    packets_received  = 0;
    sequence_errors   = 0;

    rx_state = TRANSPORT_READY;
    LOG_DEBUG("{:s} command rx in READY state", get_transport_type());

    while ((rx_state == TRANSPORT_READY or rx_state == TRANSPORT_ERROR) and not receiver_thread_stop_flag) {
        static command_queue_element recv_buffer;
        int err = 0;
        size_t bytes_in_packet = 0;

        // sync receive
        bytes_in_packet = packet_receive(recv_buffer, err);

        if (not receiver_thread_stop_flag) {
            if (err != 0 and err != ETIMEDOUT) { // timeouts are ignored
                rx_state = TRANSPORT_ERROR;
                LOG_ERROR("{:s} command rx error: {:s}", get_transport_type(), std::strerror(err));
                if (throw_on_rx_error) {
                    throw(std::runtime_error(get_transport_type() + " command rx error"));
                }
            } else if (bytes_in_packet > 0) {
                // check size and discard unless packet size agrees with header
                if (recv_buffer.hdr.packet_size != bytes_in_packet) {
                    rx_state = TRANSPORT_ERROR;
                    LOG_ERROR("packet size error in {:s} command rx (header {:d}, packet {:d})",
                            get_transport_type(), (uint16_t)recv_buffer.hdr.packet_size, bytes_in_packet);
                    if (throw_on_rx_error) {
                        throw(std::runtime_error("packet size error in " + get_transport_type() + " command rx"));
                    }
                } else {
                    // update stats
                    packets_received++;
                    packet_types_received.at(recv_buffer.hdr.packet_type)++;
                    bytes_received += bytes_in_packet;

                    // check sequence and update sequence counter
                    if (packets_received > 1 and recv_buffer.hdr.sequence_counter != (uint16_t)(last_seq + 1)) {
                        uint16_t received = recv_buffer.hdr.sequence_counter;
                        rx_state = TRANSPORT_ERROR;
                        LOG_ERROR("sequence error in {:s} command rx (expected {:d}, received {:d})",
                            get_transport_type(), (uint16_t)(last_seq + 1), received);
                        sequence_errors++;
                        if (throw_on_rx_error) {
                            throw(std::runtime_error("sequence error in " + get_transport_type() + " command rx"));
                        }
                    }
                    last_seq = recv_buffer.hdr.sequence_counter;

                    switch (recv_buffer.hdr.packet_type) {
                        case PACKET_TYPE_ASYNC_MSG:
                            if (not async_msg_queue.push_or_timeout(recv_buffer, queue_push_wait_us)) {
                                rx_state = TRANSPORT_ERROR;
                                LOG_ERROR("timeout pushing to async message queue in {:s} command rx", get_transport_type());
                                if (throw_on_rx_error) {
                                    throw(std::runtime_error("timeout pushing to async message queue in " + get_transport_type() + " command rx"));
                                }
                            }
                            break;

                        case PACKET_TYPE_DEVICE_CMD_RSP:
                        case PACKET_TYPE_TX_RADIO_CMD_RSP:
                        case PACKET_TYPE_RX_RADIO_CMD_RSP:
                        case PACKET_TYPE_DEVICE_CMD_ERR:
                        case PACKET_TYPE_TX_RADIO_CMD_ERR:
                        case PACKET_TYPE_RX_RADIO_CMD_ERR:
                            if (not response_queue.push_or_timeout(recv_buffer, queue_push_wait_us)) {
                                rx_state = TRANSPORT_ERROR;
                                LOG_ERROR("timeout pushing to command response queue in {:s} command rx", get_transport_type());
                                if (throw_on_rx_error) {
                                    throw(std::runtime_error("timeout pushing to command response queue in " + get_transport_type() + " command rx"));
                                }
                            }
                            break;

                        default:
                            LOG_WARN("{:s} command rx discarded incorrect packet (type {:d})", get_transport_type(), (int)recv_buffer.hdr.packet_type);
                            break;
                    }
                }
            }
        }
    }

    rx_state = TRANSPORT_SHUTDOWN;

    LOG_DEBUG("{:s} command rx exiting", get_transport_type());
}
