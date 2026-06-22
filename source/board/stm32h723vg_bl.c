/**
 * @file    stm32h723vg_bl.c
 * @brief   Board file for STM32H723VG bootloader build.
 *
 * DAPLink Interface Firmware
 * SPDX-License-Identifier: Apache-2.0
 */

#include "daplink_addr.h"
#include "target_config.h"
#include "target_board.h"
#include "target_family.h"

static const sector_info_t sectors_info[] = {
    {DAPLINK_ROM_IF_START, DAPLINK_SECTOR_SIZE},
    {DAPLINK_ROM_IF_START + DAPLINK_SECTOR_SIZE, DAPLINK_SECTOR_SIZE},
};

target_cfg_t target_device = {
    .version                    = kTargetConfigVersion,
    .sectors_info               = sectors_info,
    .sector_info_length         = (sizeof(sectors_info))/(sizeof(sector_info_t)),
    .flash_regions[0].start     = DAPLINK_ROM_IF_START,
    .flash_regions[0].end       = DAPLINK_ROM_IF_START + DAPLINK_ROM_IF_SIZE,
    .flash_regions[0].flags     = kRegionIsDefault,
    .ram_regions[0].start       = DAPLINK_RAM_APP_START,
    .ram_regions[0].end         = DAPLINK_RAM_APP_START + DAPLINK_RAM_APP_SIZE,
};

const target_family_descriptor_t *g_target_family = NULL;

const board_info_t g_board_info = {
    .info_version = kBoardInfoVersion,
    .board_id = "0000",
    .daplink_url_name       = "HELP_FAQHTM",
    .daplink_drive_name 		= "MAINTENANCE",
    .daplink_target_url = "https://daplink.io",
    .target_cfg = &target_device,
};