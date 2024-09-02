#include "vxsdr_transport.hpp"

bool data_transport::send_packet(packet& packet) {
    if (not packet_transport::send_packet(packet)) {
        return false;
    }

    // additional stats for data packets
    auto header_size = get_packet_preamble_size(packet.hdr);
    if (packet.hdr.packet_type == PACKET_TYPE_TX_SIGNAL_DATA and packet.hdr.packet_size > header_size) {
        samples_sent += (packet.hdr.packet_size - header_size) / sizeof(vxsdr::wire_sample);
        samples_sent_current_stream += (packet.hdr.packet_size - header_size) / sizeof(vxsdr::wire_sample);
    }

    return true;
}

void data_transport::data_send() {
    LOG_DEBUG("{:s} data tx started", transport_type);
    enum throttling_state { NO_THROTTLING = 0, NORMAL_THROTTLING = 1, HARD_THROTTLING = 2 };
    static constexpr unsigned data_buffer_size = 256;
    static std::array<data_queue_element, data_buffer_size> data_buffer;

    uint64_t data_packets_processed = 0;
    uint64_t last_check             = 0;
    // Note: all of these must be less than or equal to data_buffer_size
    //       since they are used for unchecked indexing into data_buffer!
    unsigned buffer_low_packets_to_send      = data_buffer_size;
    unsigned buffer_normal_packets_to_send   = data_buffer_size;
    unsigned buffer_check_default_packets    = data_buffer_size;
    unsigned buffer_check_throttling_packets = data_buffer_size / 2;

    throttling_state throttling_state = NO_THROTTLING;
    unsigned buffer_check_interval = buffer_check_default_packets;
    unsigned max_packets_to_send   = buffer_normal_packets_to_send;

    if (tx_data_queue == nullptr) {
        tx_state = TRANSPORT_SHUTDOWN;
        LOG_FATAL("queue not initialized in {:s} data tx", transport_type);
        throw(std::runtime_error("queue not initialized in " + transport_type + " data tx"));
        return;
    }

    tx_state = TRANSPORT_READY;
    LOG_DEBUG("{:s} data tx in READY state", transport_type);

    while (not sender_thread_stop_flag) {
        if constexpr (use_tx_throttling) {
            // There are 3 throttling states: no throttling, normal throttling, and hard throttling;
            // transitions are shown in the state machine below.
            // Note the hysteresis in entering and exiting normal throttling (throttle_off_percent < throttle_on_percent),
            // which reduces bouncing between states.
            if (throttling_state == NO_THROTTLING) {
                if (tx_buffer_fill_percent >= throttle_hard_percent) {
                    throttling_state = HARD_THROTTLING;
                    LOG_TRACE("{:s} data tx entering throttling state HARD from NONE ({:2d}% full)",
                                transport_type, (int)tx_buffer_fill_percent);
                } else if (tx_buffer_fill_percent >= throttle_on_percent) {
                    throttling_state = NORMAL_THROTTLING;
                    LOG_TRACE("{:s} data tx entering throttling state NRML from NONE ({:2d}% full)",
                                transport_type, (int)tx_buffer_fill_percent);
                }
            } else if (throttling_state == NORMAL_THROTTLING) {
                if (tx_buffer_fill_percent >= throttle_hard_percent) {
                    throttling_state = HARD_THROTTLING;
                    LOG_TRACE("{:s} data tx entering throttling state HARD from NRML ({:2d}% full)",
                                transport_type, (int)tx_buffer_fill_percent);
                } else if (tx_buffer_fill_percent < throttle_off_percent) {
                    throttling_state = NO_THROTTLING;
                    LOG_TRACE("{:s} data tx entering throttling state NONE from NRML ({:2d}% full)",
                                transport_type, (int)tx_buffer_fill_percent);
                }
            } else {  // current_state == HARD_THROTTLING
                if (tx_buffer_fill_percent < throttle_off_percent) {
                    throttling_state = NO_THROTTLING;
                    LOG_TRACE("{:s} data tx entering throttling state NONE from HARD ({:2d}% full)",
                                transport_type, (int)tx_buffer_fill_percent);
                } else if (tx_buffer_fill_percent < throttle_hard_percent) {
                    throttling_state = NORMAL_THROTTLING;
                    LOG_TRACE("{:s} data tx entering throttling state NRML from HARD ({:2d}% full)",
                                transport_type, (int)tx_buffer_fill_percent);
                }
            }
            // In no throttling and normal throttling, two control variables are set:
            //    buffer_check_interval_packets = the number of packets to send between requesting an update on device buffer fill
            //    max_packets_to_send           = the maximum number of packets to send in a burst
            if (throttling_state == NORMAL_THROTTLING) {
                buffer_check_interval = buffer_check_throttling_packets;
                max_packets_to_send   = buffer_normal_packets_to_send;

            } else if (throttling_state == NO_THROTTLING) {
                buffer_check_interval = buffer_check_default_packets;
                max_packets_to_send   = buffer_low_packets_to_send;
            }
        } else {
            throttling_state = NO_THROTTLING;
        }
        if (use_tx_throttling and throttling_state == HARD_THROTTLING) {
            // when hard throttling, send one empty data packet and request ack to update buffer use
            data_buffer[0].hdr = {PACKET_TYPE_TX_SIGNAL_DATA, 0, FLAGS_REQUEST_ACK, 0, 0, sizeof(header_only_packet), 0};
            send_packet(data_buffer[0]);
            last_check = data_packets_processed;
            std::this_thread::sleep_for(std::chrono::microseconds(send_thread_sleep_us));
        } else {
            // when not hard throttling, send at most max_packets_to_send packets and update buffer fills
            // every buffer_check_interval packets
            unsigned n_popped = tx_data_queue->pop_or_timeout(&data_buffer.front(), max_packets_to_send, send_thread_wait_us, send_thread_sleep_us);
            for (unsigned i = 0; i < n_popped; i++) {
                if (use_tx_throttling and (data_packets_processed == 0 or data_packets_processed - last_check >= buffer_check_interval)) {
                    // request ack to update buffer use
                    data_buffer[i].hdr.flags |= FLAGS_REQUEST_ACK;
                    last_check = data_packets_processed;
                }
                if (send_packet(data_buffer[i])) {
                    data_packets_processed++;
                }
                if (use_tx_throttling and throttling_state != NO_THROTTLING) {
                    // if we are throttling, pause between each packet
                    std::this_thread::sleep_for(std::chrono::microseconds(throttle_amount_us));
                }
            }

        }
    }

    if (rx_state == TRANSPORT_READY or rx_state == TRANSPORT_ERROR) {
        // send a last empty packet with an ack request so that the stats are updated
        data_buffer[0].hdr = {PACKET_TYPE_TX_SIGNAL_DATA, 0, FLAGS_REQUEST_ACK, 0, 0, sizeof(header_only_packet), 0};
        send_packet(data_buffer[0]);
        // wait for the response to be received by the data rx
        std::this_thread::sleep_for(20ms);
    } else {
        LOG_WARN("{:s} data rx unavailable at tx shutdown: stats will not be updated", transport_type);
    }

    tx_state = TRANSPORT_SHUTDOWN;

    LOG_DEBUG("{:s} data tx exiting", transport_type);
}

void data_transport::data_receive() {
    LOG_DEBUG("{:s} data rx started", transport_type);
    uint16_t last_seq = 0;
    bytes_received    = 0;
    samples_received  = 0;
    packets_received  = 0;
    sequence_errors   = 0;

    if (rx_data_queue.empty()) {
        rx_state = TRANSPORT_SHUTDOWN;
        LOG_FATAL("queues not initialized in {:s} data rx", transport_type);
        throw(std::runtime_error("queues not initialized in " + transport_type + " data rx"));
        return;
    }

    rx_state = TRANSPORT_READY;
    LOG_DEBUG("{:s} data rx in READY state", transport_type);

    while ((rx_state == TRANSPORT_READY or rx_state == TRANSPORT_ERROR) and not receiver_thread_stop_flag) {
        static data_queue_element recv_buffer;
        int err = 0;
        size_t bytes_in_packet = 0;

        // sync receive
        bytes_in_packet = packet_receive(recv_buffer, err);

        if (not receiver_thread_stop_flag) {
            if (err != 0 and err != ETIMEDOUT) {
                rx_state = TRANSPORT_ERROR;
                LOG_ERROR("{:s} data receive error: {:s}", transport_type, std::strerror(err));
                if (throw_on_rx_error) {
                    throw(std::runtime_error(transport_type + " data receive error"));
                }
            } else if (bytes_in_packet > 0) {
                // check size and discard unless packet size agrees with header
                if (recv_buffer.hdr.packet_size != bytes_in_packet) {
                    rx_state = TRANSPORT_ERROR;
                    LOG_ERROR("packet size error in {:s} data rx (header {:d}, packet {:d})",
                            transport_type, (uint16_t)recv_buffer.hdr.packet_size, bytes_in_packet);
                    if (throw_on_rx_error) {
                        throw(std::runtime_error("packet size error in " + transport_type + " data rx"));
                    }
                } else {
                    // update stats
                    packets_received++;
                    packet_types_received.at(recv_buffer.hdr.packet_type)++;
                    bytes_received += bytes_in_packet;

                    // check sequence and update sequence counter
                    if (packets_received > 1 and recv_buffer.hdr.sequence_counter != (uint16_t)(last_seq + 1)) {
                        rx_state = TRANSPORT_ERROR;
                        uint16_t received = recv_buffer.hdr.sequence_counter;
                        LOG_ERROR("sequence error in {:s} data rx (expected {:d}, received {:d})",
                                transport_type, (uint16_t)(last_seq + 1), received);
                        sequence_errors++;
                        sequence_errors_current_stream++;
                        if (throw_on_rx_error) {
                            throw(std::runtime_error("sequence error in " + transport_type + " data rx"));
                        }
                    }
                    last_seq = recv_buffer.hdr.sequence_counter;

                    if (recv_buffer.hdr.packet_type == PACKET_TYPE_RX_SIGNAL_DATA) {
                        // check subdevice
                        if (recv_buffer.hdr.subdevice < num_rx_subdevs) {
                            uint16_t preamble_size = get_packet_preamble_size(recv_buffer.hdr);
                            // update sample stats
                            size_t n_samps = (recv_buffer.hdr.packet_size - preamble_size) / sizeof(vxsdr::wire_sample);
                            samples_received += n_samps;
                            samples_received_current_stream += n_samps;
                            if (not rx_data_queue[recv_buffer.hdr.subdevice]->push(recv_buffer)) {
                                rx_state = TRANSPORT_ERROR;
                                LOG_ERROR("error pushing to data queue in {:s} data rx (subdevice {:d} sample {:d})",
                                        transport_type, recv_buffer.hdr.subdevice, samples_received);
                                if (throw_on_rx_error) {
                                    throw(std::runtime_error("error pushing to data queue in " + transport_type + " data rx"));
                                }
                            }
                        } else {
                            LOG_WARN("{:s} data rx discarded rx data packet from unknown subdevice {:d}",
                                    transport_type, recv_buffer.hdr.subdevice);
                        }
                    } else if (recv_buffer.hdr.packet_type == PACKET_TYPE_TX_SIGNAL_DATA_ACK) {
                        auto* r = std::bit_cast<six_uint32_packet*>(&recv_buffer);
                        tx_buffer_used_bytes = r->value3;
                        tx_buffer_size_bytes = r->value4;
                        tx_packet_oos_count  = r->value5;
                        if (tx_buffer_size_bytes > 0) {
                            tx_buffer_fill_percent = (unsigned)std::min(100ULL, (100ULL * tx_buffer_used_bytes) / tx_buffer_size_bytes);
                        } else {
                            tx_buffer_fill_percent = 0;
                        }
                    } else {
                        LOG_WARN("{:s} data rx discarded incorrect packet (type {:d})", transport_type, (int)recv_buffer.hdr.packet_type);
                    }
                }
            }
        }
    }

    rx_state = TRANSPORT_SHUTDOWN;

    LOG_DEBUG("{:s} data rx exiting", transport_type);
}
