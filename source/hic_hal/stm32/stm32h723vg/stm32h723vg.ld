/**
 * @file    stm32h723vg.ld
 * @brief   Linker script for STM32H723VG - extends daplink.ld with
 *          sector buffer in AXI SRAM (DTCM is too small for 32 KB).
 *
 * DAPLink Interface Firmware
 * SPDX-License-Identifier: Apache-2.0
 */

#include "daplink_addr.h"

/* Sector buffer for drag-n-drop IAP staging - placed in AXI SRAM
 * (D1 SRAM) because the DTCM (128 KB) is too small. */
MEMORY
{
    m_sector_buf (RW) : ORIGIN = DAPLINK_RAM_SECTOR_BUFFER_START, LENGTH = DAPLINK_RAM_SECTOR_BUFFER_SIZE
}

SECTIONS
{
    .sector_buf_section (NOLOAD) :
    {
        KEEP(*(sector_buf_section))
    } > m_sector_buf
}

#include "daplink.ld"