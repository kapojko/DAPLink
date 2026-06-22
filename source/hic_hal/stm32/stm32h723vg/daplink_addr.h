/**
 * @file    daplink_addr.h
 * @brief   Memory map for STM32H723VG.
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DAPLINK_ADDR_H
#define DAPLINK_ADDR_H

/* Device sizes */

/* STM32H723VGT6: 1 MB internal flash, 320 KB D1 SRAM (AXI), 128 KB DTCM,
   64 KB ITCM. The DAPLink firmware itself runs from internal flash; the
   external W25Q32 (4 MB at 0x90000000) is currently unused by DAPLink
   (the driver and OCTOSPI1 init in sdk.c are kept for future use). */

#define DAPLINK_STM32H723_DTCM_START      0x20000000
#define DAPLINK_STM32H723_DTCM_SIZE       0x00020000

#define DAPLINK_STM32H723_AXISRAM_START   0x24000000
#define DAPLINK_STM32H723_AXISRAM_SIZE    0x00050000

#define DAPLINK_RAM_START                 (DAPLINK_STM32H723_DTCM_START)
#define DAPLINK_RAM_SIZE                  DAPLINK_STM32H723_DTCM_SIZE

/* Sector buffer for drag-n-drop IAP staging lives in AXI SRAM (DTCM
 * is too small for a 32 KB+ sector buffer). */
#define DAPLINK_RAM_SECTOR_BUFFER_START   (DAPLINK_STM32H723_AXISRAM_START)
#define DAPLINK_RAM_SECTOR_BUFFER_SIZE    0x00020000

/* Flash Programming Info - drag-n-drop IAP target is the internal flash
 * interface region (DAPLINK_ROM_IF_START). The internal flash driver in
 * flash.c operates on this memory.
 *   Sector size = 128 KB (STM32H723 internal flash sector).
 *   Min write size = 32 B (256-bit flash word). */
#define DAPLINK_SECTOR_SIZE               0x00020000
#define DAPLINK_MIN_WRITE_SIZE            0x00000020

/* ROM sizes - DAPLink firmware itself lives in internal flash */
#define DAPLINK_ROM_START                 0x08000000
#define DAPLINK_ROM_SIZE                  0x00080000 /* 512 KB used (BL+IF+CFG) */

#define DAPLINK_ROM_BL_START              0x08000000
#define DAPLINK_ROM_BL_SIZE               0x00020000 /* 128 KB bootloader - 1 sector */

#define DAPLINK_ROM_IF_START              0x08020000
#define DAPLINK_ROM_IF_SIZE               0x00040000 /* 256 KB interface  - 2 sectors */

#define DAPLINK_ROM_CONFIG_USER_START     0x08060000
#define DAPLINK_ROM_CONFIG_USER_SIZE      0x00020000 /* 128 KB user config - 1 sector */

/* External flash (W25Q32) - 4 MB XIP window for drag-n-drop IAP target */
#define DAPLINK_XIP_BASE                  0x90000000UL
#define DAPLINK_XIP_SIZE                  0x00400000UL

/* RAM sizes */

#define DAPLINK_RAM_APP_START             DAPLINK_STM32H723_DTCM_START
#define DAPLINK_RAM_APP_SIZE              0x0001FF00 /* DTCM 128 KB - 256 B shared */

#define DAPLINK_RAM_SHARED_START          (DAPLINK_RAM_APP_START + DAPLINK_RAM_APP_SIZE)
#define DAPLINK_RAM_SHARED_SIZE           0x00000100

/* Current build */

#if defined(DAPLINK_BL)

#define DAPLINK_ROM_APP_START             DAPLINK_ROM_BL_START
#define DAPLINK_ROM_APP_SIZE              DAPLINK_ROM_BL_SIZE
#define DAPLINK_ROM_UPDATE_START          DAPLINK_ROM_IF_START
#define DAPLINK_ROM_UPDATE_SIZE           DAPLINK_ROM_IF_SIZE

#elif defined(DAPLINK_IF)

#define DAPLINK_ROM_APP_START             DAPLINK_ROM_IF_START
#define DAPLINK_ROM_APP_SIZE              DAPLINK_ROM_IF_SIZE
#define DAPLINK_ROM_UPDATE_START          DAPLINK_ROM_BL_START
#define DAPLINK_ROM_UPDATE_SIZE           DAPLINK_ROM_BL_SIZE

#else

#error "Build must be either bootloader or interface"

#endif

#endif
