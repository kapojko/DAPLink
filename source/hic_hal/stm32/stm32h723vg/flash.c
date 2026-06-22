/**
 * @file    flash.c
 * @brief   Flash driver for STM32H723VG.
 *
 * Two backends are kept here under #if/#else switches inside each
 * function:
 *   - The default (USE_INTERNAL_FLASH = 1) drives the internal flash
 *     interface region at DAPLINK_ROM_IF_START via the HAL.
 *   - The alternative (USE_INTERNAL_FLASH = 0) drives the external
 *     W25Q32 over OCTOSPI1 (XIP window at 0x90000000); it is currently
 *     unused by DAPLink firmware but kept here for future use, along
 *     with the W25Q32 driver, the OCTOSPI1 pin init in sdk.c and the
 *     MPU XIP region.
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2019, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "flash_hal.h"        // FlashOS Structures
#include "target_config.h"    // target_device
#include "stm32h7xx.h"
#include "string.h"
#include "target_board.h"
#include "daplink_addr.h"
#include "util.h"

/* Select the active backend. */
#define USE_INTERNAL_FLASH   1

#if !USE_INTERNAL_FLASH
#include "W25Q32.h"
#endif

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/

#if !USE_INTERNAL_FLASH
/* External-flash helpers (W25Q32 over OCTOSPI1). */

static uint32_t xip_to_w25q_addr(uint32_t adr)
{
    util_assert((adr >= DAPLINK_XIP_BASE) &&
                (adr < (DAPLINK_XIP_BASE + DAPLINK_XIP_SIZE)));
    return adr - DAPLINK_XIP_BASE;
}

OSPI_HandleTypeDef hospi1;

static HAL_StatusTypeDef octospi1_init(void)
{
    OSPIM_CfgTypeDef sOspiManagerCfg = {0};

    hospi1.Instance = OCTOSPI1;
    hospi1.Init.FifoThreshold         = 4;
    hospi1.Init.DualQuad              = HAL_OSPI_DUALQUAD_DISABLE;
    hospi1.Init.MemoryType            = HAL_OSPI_MEMTYPE_MICRON;
    hospi1.Init.DeviceSize            = 22;          // 4 MB / 8
    hospi1.Init.ChipSelectHighTime    = 2;
    hospi1.Init.FreeRunningClock      = HAL_OSPI_FREERUNCLK_DISABLE;
    hospi1.Init.ClockMode             = HAL_OSPI_CLOCK_MODE_0;
    hospi1.Init.WrapSize              = HAL_OSPI_WRAP_NOT_SUPPORTED;
    hospi1.Init.ClockPrescaler        = 1;            // kernel / 2
    hospi1.Init.SampleShifting        = HAL_OSPI_SAMPLE_SHIFTING_NONE;
    hospi1.Init.DelayHoldQuarterCycle = HAL_OSPI_DHQC_DISABLE;
    hospi1.Init.ChipSelectBoundary    = 0;
    hospi1.Init.DelayBlockBypass      = HAL_OSPI_DELAY_BLOCK_BYPASSED;
    hospi1.Init.MaxTran               = 0;
    hospi1.Init.Refresh               = 0;

    if (HAL_OSPI_Init(&hospi1) != HAL_OK) {
        return HAL_ERROR;
    }

    sOspiManagerCfg.ClkPort   = 1;
    sOspiManagerCfg.NCSPort   = 1;
    sOspiManagerCfg.IOLowPort = HAL_OSPIM_IOPORT_1_LOW;
    if (HAL_OSPIM_Config(&hospi1, &sOspiManagerCfg,
                         HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return HAL_ERROR;
    }
    return HAL_OK;
}
#endif /* !USE_INTERNAL_FLASH */

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/

uint32_t Init(uint32_t adr, uint32_t clk, uint32_t fnc)
{
#if USE_INTERNAL_FLASH
    //
    // No special init required
    //
    return (0);
#else
    if (octospi1_init() != HAL_OK) {
        return 1;
    }
    if (W25Q_Init() != W25Q_Result_Ok) {
        return 1;
    }
    if (W25Q_EnterMemoryMapped() != W25Q_Result_Ok) {
        return 1;
    }
    return 0;
#endif
}

uint32_t UnInit(uint32_t fnc)
{
#if USE_INTERNAL_FLASH
    //
    // No special uninit required
    //
    return (0);
#else
    return 0;
#endif
}

uint32_t EraseChip(void)
{
#if USE_INTERNAL_FLASH
   FLASH_EraseInitTypeDef erase_init;
   uint32_t error;
   uint32_t ret = 0;
   if (g_board_info.target_cfg) {
        // Called from the bootloader. Interface flashing only concerns 1 flash region.
        // The start in the board info excludes the bootloader. The HAL erase takes a
        // sector index, so need to calculate which sector the start address refers to.
        util_assert(((g_board_info.target_cfg->flash_regions[0].end - g_board_info.target_cfg->flash_regions[0].start) % FLASH_SECTOR_SIZE == 0) &&
                    ((g_board_info.target_cfg->flash_regions[0].start - FLASH_BASE) % FLASH_SECTOR_SIZE == 0));

        memset(&erase_init, 0, sizeof(erase_init));

        uint8_t start_sector_index = (g_board_info.target_cfg->flash_regions[0].start - FLASH_BASE) / FLASH_SECTOR_SIZE;
        uint8_t number_of_sectors = (g_board_info.target_cfg->flash_regions[0].end - g_board_info.target_cfg->flash_regions[0].start) / FLASH_SECTOR_SIZE;
        erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase_init.Sector = start_sector_index;
        erase_init.Banks = FLASH_BANK_1;
        erase_init.NbSectors = number_of_sectors;

        HAL_FLASH_Unlock();
        if (HAL_FLASHEx_Erase(&erase_init, &error) != HAL_OK) {
            ret = 1;
        }
        HAL_FLASH_Lock();

   } else {
       ret = 1;
   }
   return ret;
#else
    uint32_t ret = 0;

    if (g_board_info.target_cfg) {
        // Called from the bootloader when re-flashing the IF image.
        // Erase all sectors within the configured flash region (the XIP window).
        const region_info_t *region = &g_board_info.target_cfg->flash_regions[0];
        const uint32_t size = region->end - region->start;
        const uint32_t num_sectors = size / W25Q_SECTOR_SIZE;

        for (uint32_t i = 0; i < num_sectors; i++) {
            const uint32_t sector_addr = region->start + i * W25Q_SECTOR_SIZE;
            if (W25Q_SectorErase(xip_to_w25q_addr(sector_addr)) != W25Q_Result_Ok) {
                ret = 1;
                break;
            }
        }
    } else {
        ret = 1;
    }
    return ret;
#endif
}

uint32_t EraseSector(uint32_t adr)
{
#if USE_INTERNAL_FLASH
    util_assert((adr - FLASH_BASE) % FLASH_SECTOR_SIZE == 0);

    FLASH_EraseInitTypeDef erase_init;
    uint32_t error;
    uint32_t ret = 0;

    memset(&erase_init, 0, sizeof(erase_init));

    uint8_t sector_index = (adr - FLASH_BASE) / FLASH_SECTOR_SIZE;
    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = sector_index;
    erase_init.Banks = FLASH_BANK_1;
    erase_init.NbSectors = 1;

    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&erase_init, &error) != HAL_OK) {
        ret = 1;
    }
    HAL_FLASH_Lock();

    return ret;
#else
    util_assert((adr - DAPLINK_XIP_BASE) % W25Q_SECTOR_SIZE == 0);

    if (W25Q_SectorErase(xip_to_w25q_addr(adr)) != W25Q_Result_Ok) {
        return 1;
    }
    return 0;
#endif
}

uint32_t ProgramPage(uint32_t adr, uint32_t sz, uint32_t *buf)
{
#if USE_INTERNAL_FLASH
    util_assert(sz % 32 == 0); // this chip does 256bit writes

    uint32_t i;
    uint32_t ret = 0;

    HAL_FLASH_Unlock();
    for (i = 0; i < sz / 32; i++) {
        ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, adr + i * 32, (uint32_t)buf + i * 32);
        if (ret != HAL_OK) {
            ret = 1;
            break;
        }
    }
    HAL_FLASH_Lock();

    return ret;
#else
    util_assert(adr >= DAPLINK_XIP_BASE);
    util_assert((adr - DAPLINK_XIP_BASE + sz) <= DAPLINK_XIP_SIZE);
    util_assert(((adr - DAPLINK_XIP_BASE) % W25Q_PAGE_SIZE) == 0);

    while (sz > 0) {
        const uint32_t chunk = (sz > W25Q_PAGE_SIZE) ? W25Q_PAGE_SIZE : sz;
        if (W25Q_PageProgramQuad(xip_to_w25q_addr(adr), (const uint8_t *)buf, chunk) != W25Q_Result_Ok) {
            return 1;
        }
        adr += chunk;
        buf = (uint32_t *)((uint8_t *)buf + chunk);
        sz -= chunk;
    }
    return 0;
#endif
}

/* flash_erase_sector / flash_program_page / flash_is_readable are
 * provided by the weak defaults in daplink/flash_hal.c (which call
 * EraseSector/ProgramPage with interrupts disabled and validate
 * the address range against DAPLINK_ROM_START..DAPLINK_ROM_SIZE).
 * They are sufficient for this port. */