/**
 * @file    flash_hal_hk32f103xb.c
 * @brief
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

#include "flash_hal.h"
#include "target_config.h"
#include "hk32f10x.h"
#include "hk32f10x_flash.h"
#include "util.h"
#include "string.h"
#include "target_board.h"

#define FLASH_PAGE_SIZE 0x800

uint32_t Init(uint32_t adr, uint32_t clk, uint32_t fnc)
{
    return (0);
}

uint32_t UnInit(uint32_t fnc)
{
    return (0);
}

uint32_t EraseChip(void)
{
    FLASH_Status status;
    uint32_t ret = 0;

    if (g_board_info.target_cfg) {
        FLASH_Unlock();

        /* Clear all pending flags */
        FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

        util_assert((g_board_info.target_cfg->flash_regions[0].end - g_board_info.target_cfg->flash_regions[0].start) %
                    FLASH_PAGE_SIZE == 0);

        status = FLASH_EraseAllPages();
        if (status != FLASH_COMPLETE) {
            ret = 1;
        }

        FLASH_Lock();
    } else {
        ret = 1;
    }

    return ret;
}

uint32_t EraseSector(uint32_t adr)
{
    FLASH_Status status;
    uint32_t ret = 0;

    FLASH_Unlock();

    /* Clear all pending flags */
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    status = FLASH_ErasePage(adr);
    if (status != FLASH_COMPLETE) {
        ret = 1;
    }

    FLASH_Lock();
    return ret;
}

uint32_t ProgramPage(uint32_t adr, uint32_t sz, uint32_t *buf)
{
    uint32_t i;
    FLASH_Status status;
    uint32_t ret = 0;

    FLASH_Unlock();

    /* Clear all pending flags */
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    util_assert(sz % 4 == 0);

    for (i = 0; i < sz / 4; i++) {
        status = FLASH_ProgramWord(adr + i * 4, buf[i]);
        if (status != FLASH_COMPLETE) {
            ret = 1;
            break;
        }
    }

    FLASH_Lock();
    return ret;
}
