#pragma once

#include <stdbool.h>
#include <stdint.h>

/* W25Q32JV (Winbond) connected to OCTOSPI1 in standard/quad SPI mode.
   Geometry: 32 Mbit = 4 MB = 16384 pages = 1024 sectors (4 KB). */
#define W25Q_PAGE_SIZE             256U
#define W25Q_SECTOR_SIZE           (4U * 1024U)
#define W25Q_FLASH_SIZE            (4U * 1024U * 1024U)
#define W25Q_END_ADDR              W25Q_FLASH_SIZE

/* Memory-mapped base address for OCTOSPI1 (from RM0468, Section 2.3.2) */
#define W25Q_MM_BASE               0x90000000UL

/* Standard SPI opcodes */
#define W25Q_CMD_WRITE_ENABLE      0x06U
#define W25Q_CMD_READ_STATUS_1     0x05U
#define W25Q_CMD_READ_STATUS_2     0x35U
#define W25Q_CMD_WRITE_STATUS_REG  0x01U
#define W25Q_CMD_READ_DATA         0x03U
#define W25Q_CMD_FAST_READ         0x0BU
#define W25Q_CMD_PAGE_PROGRAM      0x02U
#define W25Q_CMD_SECTOR_ERASE      0x20U
#define W25Q_CMD_BLOCK32K_ERASE    0x52U
#define W25Q_CMD_BLOCK64K_ERASE    0xD8U
#define W25Q_CMD_CHIP_ERASE        0xC7U
#define W25Q_CMD_ENABLE_RESET      0x66U
#define W25Q_CMD_RESET             0x99U
#define W25Q_CMD_READ_JEDEC_ID     0x9FU
#define W25Q_CMD_READ_MFR_DEV_ID   0x90U
#define W25Q_CMD_VOLATILE_SR_WREN  0x50U

/* Quad SPI opcodes (require QE=1 in SR2) */
#define W25Q_CMD_QUAD_PAGE_PROGRAM 0x32U   /* 1-1-4: cmd/addr on 1 line, data on 4 lines */
#define W25Q_CMD_FAST_READ_QUAD_OUT 0x6BU  /* 1-1-4: cmd/addr on 1 line, data on 4 lines, 8 dummy cycles */
#define W25Q_CMD_QUAD_IO_READ      0xEBU   /* 1-4-4: cmd on 1 line, addr+data on 4 lines, 4 dummy cycles (QE=1) */

/* Status register bits */
#define W25Q_SR1_BUSY              0x01U
#define W25Q_SR1_WEL               0x02U
#define W25Q_SR2_QE                0x02U

/* Expected manufacturer/device IDs (JEDEC 0x9F) for W25Q32FV/JV */
#define W25Q_JEDEC_MFR_WINBOND     0xEFU
#define W25Q_JEDEC_TYPE_MEMTYPE    0x40U
#define W25Q_JEDEC_CAP_32MBIT      0x16U

/* Dummy cycles for fast read variants */
#define W25Q_DUMMY_CYCLES_QUAD_OUT 8U      /* 0x6B - matches FrameCam4 */
#define W25Q_DUMMY_CYCLES_QUAD_IO  4U      /* 0xEB with QE=1 - matches FrameCam4 */

typedef struct {
    uint8_t mfrId;
    uint8_t memType;
    uint8_t capacity;
} W25Q_JedecId_t;

typedef enum {
    W25Q_Result_Ok = 0,
    W25Q_Result_Timeout,
    W25Q_Result_HalError,
    W25Q_Result_InvalidId,
} W25Q_Result_t;

W25Q_Result_t W25Q_Init(void);
W25Q_Result_t W25Q_ReadJedecId(W25Q_JedecId_t *id);
W25Q_Result_t W25Q_ReadMfrDevId(uint8_t *mfrId, uint8_t *devId);
W25Q_Result_t W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len);
W25Q_Result_t W25Q_ReadQuad(uint32_t addr, uint8_t *buf, uint32_t len);
W25Q_Result_t W25Q_PageProgram(uint32_t addr, const uint8_t *buf, uint32_t len);
W25Q_Result_t W25Q_PageProgramQuad(uint32_t addr, const uint8_t *buf, uint32_t len);
W25Q_Result_t W25Q_SectorErase(uint32_t addr);
W25Q_Result_t W25Q_ChipErase(void);
W25Q_Result_t W25Q_EnterMemoryMapped(void);
