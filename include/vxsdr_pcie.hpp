// Copyright (c) 2024 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <algorithm>
#include <bit>
#include <cerrno>

#include <sys/mman.h>
#include <sys/ioctl.h>

#ifdef VXSDR_TARGET_LINUX
#include <linux/ioctl.h>
#endif

#include "vxsdr_packets.hpp"
#include "logging.hpp"
#include "vxsdr_dma_cmd.h"


class pcie_dma_interface {
    private:
        std::string dma_path;
        int dma_filedes = 0;
        size_t pcie_buffer_size = 0;
        int rx_ddr_size = 0;
        int tx_ddr_size = 0;
        std::vector<void *> tx_buffer_ptrs;
        std::vector<void *> rx_buffer_ptrs;
    public:
        pcie_dma_interface(const std::string dev_path = "/dev/vxsdr_dma",
                           const int tx_cmd_timeout_ms = 1500, const int rx_cmd_timeout_ms = 1500,
                           const int tx_data_timeout_ms = 100, const int rx_data_timeout_ms = 500) {
            dma_path = dev_path;
            LOG_DEBUG("pcie_dma_interface constructor entered");
            LOG_DEBUG("pcie_dma_interface attempting to open {:s}", dma_path);

            dma_filedes = open(dma_path.c_str(), O_RDWR);
            if (dma_filedes == -1) {
                LOG_ERROR("pcie_dma_interface unable to open dma at path {:s}", dma_path);
                throw std::runtime_error("pcie_dma_interface unable to open dma");
            }

            // clear the rx data buffer
            int result = ioctl(dma_filedes, IOCTL_RX_CLEAR_DATA, 0);
            if (result < 0) {
                LOG_ERROR("pcie_dma_interface error clearing rx data buffer");
                throw std::runtime_error("pcie_dma_interface error clearing rx data buffer");
            }

            result = ioctl(dma_filedes, IOCTL_GET_DATA_MSG_BUFFER_SIZE, 0);
            if (result <= 0) {
                LOG_ERROR("pcie_dma_interface incorrect dma buffer size returned");
                throw std::runtime_error("pcie_dma_interface incorrect dma buffer size returned");
            }
            pcie_buffer_size = result;
            LOG_DEBUG("pcie_dma_interface IOCTL_BUF_SIZE: {:d}", pcie_buffer_size);

            result = ioctl(dma_filedes, IOCTL_TX_BUFFER_CNT, 0);
            if (result < 0) {
                LOG_ERROR("pcie_dma_interface incorrect tx buffer count returned");
                throw std::runtime_error("pcie_dma_interface incorrect tx buffer count returned");
            }
            LOG_DEBUG("pcie_dma_interface IOCTL_TX_BUF_CNT: {:d}", result);
            tx_buffer_ptrs.resize(result);

            result = ioctl(dma_filedes, IOCTL_RX_BUFFER_CNT, 0);
            if (result < 0) {
                LOG_ERROR("pcie_dma_interface incorrect rx buffer count returned");
                throw std::runtime_error("pcie_dma_interface incorrect rx buffer count returned");
            }
            LOG_DEBUG("pcie_dma_interface IOCTL_RX_BUF_CNT: {:d}", result);
            rx_buffer_ptrs.resize(result);

            rx_ddr_size = ioctl(dma_filedes, IOCTL_GET_RX_DEV_DDR_SIZE, 0);
            LOG_DEBUG("pcie_dma_interface rx ddr buffer size: {:d}", rx_ddr_size);

            tx_ddr_size = ioctl(dma_filedes, IOCTL_GET_TX_DEV_DDR_SIZE, 0);
            LOG_DEBUG("pcie_dma_interface tx ddr buffer size: {:d}", tx_ddr_size);

            result = ioctl(dma_filedes, IOCTL_TX_BLOCK_TIMEOUT, tx_cmd_timeout_ms);
            if (result < 0) {
                LOG_ERROR("pcie_dma_interface error setting tx cmd pcie timeout");
                throw std::runtime_error("pcie_dma_interface error setting tx cmd pcie timeout");
            }

            result = ioctl(dma_filedes, IOCTL_RX_BLOCK_TIMEOUT, rx_cmd_timeout_ms);
            if (result < 0) {
                LOG_ERROR("pcie_dma_interface error setting rx cmd pcie timeout");
                throw std::runtime_error("pcie_dma_interface error setting rx pcie cmd timeout");
            }

            result = ioctl(dma_filedes, IOCTL_TX_IOCTL_BLOCK_TIMEOUT, tx_data_timeout_ms);
            if (result < 0) {
                LOG_ERROR("pcie_dma_interface error setting tx data pcie timeout");
                throw std::runtime_error("pcie_dma_interface error setting tx data pcie timeout");
            }

            result = ioctl(dma_filedes, IOCTL_RX_IOCTL_BLOCK_TIMEOUT, rx_data_timeout_ms);
            if (result < 0) {
                LOG_ERROR("pcie_dma_interface error setting rx data pcie timeout");
                throw std::runtime_error("pcie_dma_interface error setting rx data pcie timeout");
            }

            // clear the rx cmd buffer
            result = ioctl(dma_filedes, IOCTL_RX_CLEAR_CTRL, 0);
            if (result < 0) {
                LOG_ERROR("pcie_dma_interface error clearing rx cmd buffer");
                throw std::runtime_error("pcie_dma_interface error clearing rx cmd buffer");
            }

            // reset the tx data cmd and data buffers
            result = ioctl(dma_filedes, IOCTL_TX_RESET, 0);
            if (result < 0) {
                LOG_ERROR("pcie_dma_interface error resetting cmd and data tx");
                throw std::runtime_error("pcie_dma_interface error resetting cmd and data tx");
            }

            //select tx dma mmap linking
            result = ioctl(dma_filedes, IOCTL_MMAP_TX_SEL, 1);
            if (result < 0) {
                LOG_ERROR("pcie_dma_interface IOCTL_MMAP_TX_SEL(1) ioctl failed with return: {:d}", result);
                throw std::runtime_error("pcie_dma_interface unable to select tx dma mmap linking");
            }

            //create mmap associations for tx dma
            for (unsigned i = 0; i < tx_buffer_ptrs.size(); i++) {
                tx_buffer_ptrs[i] = mmap(NULL, pcie_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, dma_filedes, 0);
                if (tx_buffer_ptrs[i] == MAP_FAILED) {
                    LOG_ERROR("pcie_dma_interface mmap tx failed");
                    throw std::runtime_error("pcie_dma_interface unable to mmap tx buffer");
                }
            }

            result = ioctl(dma_filedes, IOCTL_MMAP_TX_SEL, 0);
            if (result < 0) {
                LOG_ERROR("IOCTL_MMAP_TX_SEL(0) ioctl failed with return {:d}", result);
                throw std::runtime_error("pcie_dma_interface unable to select rx dma mmap linking");
            }

            //create mmap associations for rx dma
            for (unsigned i = 0; i < rx_buffer_ptrs.size(); i++) {
                rx_buffer_ptrs[i] = mmap(NULL, pcie_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, dma_filedes, 0);
                if (rx_buffer_ptrs[i] == MAP_FAILED) {
                    LOG_ERROR("pcie_dma_interface mmap rx failed");
                    throw std::runtime_error("pcie_dma_interface unable to mmap rx buffer");
                }
            }

            LOG_DEBUG("pcie_dma_interface constructor complete");
        }
        ~pcie_dma_interface() {
            LOG_DEBUG("pcie_dma_interface destructor entered");

            LOG_DEBUG("pcie_dma_interface releasing tx mmap regions");
            for (unsigned i = 0; i < tx_buffer_ptrs.size(); i++) {
                munmap(tx_buffer_ptrs[i], pcie_buffer_size);
            }
            LOG_DEBUG("pcie_dma_interface releasing rx mmap regions");
            for (unsigned i = 0; i < rx_buffer_ptrs.size(); i++) {
                munmap(rx_buffer_ptrs[i], pcie_buffer_size);
            }

            LOG_DEBUG("pcie_dma_interface closing file descriptor");
            close(dma_filedes);
            LOG_DEBUG("pcie_dma_interface destructor complete");
        }
        size_t pcie_dma_command_send(const void *buf_ptr, const size_t buf_size, int &error_code) {
	        auto result = write(dma_filedes, buf_ptr, buf_size);
            if (result < 0) {
                error_code = errno;
                return 0;
            }
            error_code = 0;
            return result;
        }
        size_t pcie_dma_command_receive(void *buf_ptr, const size_t buf_size, int &error_code) {
	        auto result = read(dma_filedes, buf_ptr, buf_size);
            if (result < 0) {
                error_code = errno;
                return 0;
            }
            auto h_ptr = reinterpret_cast<packet_header *>(buf_ptr);
            if (h_ptr->packet_size < result) {
                result = h_ptr->packet_size;
            }
            error_code = 0;
            return result;
        }
        size_t pcie_dma_data_send(const void *buf_ptr, const size_t buf_size, int &error_code) {
            int idx = ioctl(dma_filedes, IOCTL_CHECKOUT_TX_BUFFER, 0);
            if (idx < 0) {
                 error_code = errno;
                return 0;
            }
            memcpy(tx_buffer_ptrs[idx], buf_ptr, std::min(buf_size, pcie_buffer_size));
            if (ioctl(dma_filedes, IOCTL_UPLOAD_TX_BUFFER_BLOCKING, buf_size) < 0) {
                error_code = errno;
                ioctl(dma_filedes, IOCTL_RELEASE_TX_BUFFER, 0);
                return 0;
            }
            if (ioctl(dma_filedes, IOCTL_RELEASE_TX_BUFFER, 0) < 0) {
                error_code = errno;
                return 0;
            }
            error_code = 0;
            return buf_size;
        }
        size_t pcie_dma_data_receive(void *buf_ptr, const size_t buf_size, int &error_code) {
		    int idx = ioctl(dma_filedes, IOCTL_CHECKOUT_RX_BUFFER_BLOCKING, 0);
            if (idx < 0) {
                error_code = errno;
                return 0;
            }
            auto h_ptr = reinterpret_cast<packet_header *>(rx_buffer_ptrs[idx]);
            auto bytes_to_copy = std::min((size_t)h_ptr->packet_size, std::min(buf_size, pcie_buffer_size));
            memcpy(buf_ptr, rx_buffer_ptrs[idx], bytes_to_copy);
            if (ioctl(dma_filedes, IOCTL_RELEASE_RX_BUFFER, 0) < 0) {
                error_code = errno;
                return 0;
            }
            error_code = 0;
            return bytes_to_copy;
        }
};
