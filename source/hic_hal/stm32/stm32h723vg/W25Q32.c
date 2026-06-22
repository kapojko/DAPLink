#include "stm32h7xx.h"
#include "W25Q32.h"

extern OSPI_HandleTypeDef hospi1;

#define W25Q_TIMEOUT_MS            3000U
#define W25Q_CHIP_ERASE_TIMEOUT_MS 60000U

typedef enum {
    W25Q_Dir_Write = 0,
    W25Q_Dir_Read  = 1,
} W25Q_Dir_t;

static W25Q_Result_t W25Q_WaitBusy(uint32_t timeoutMs);
static W25Q_Result_t W25Q_WriteEnable(void);
static HAL_StatusTypeDef W25Q_SendCommand(uint8_t instruction,
                                          uint32_t address,
                                          uint32_t addressSize,
                                          uint8_t dummyCycles,
                                          W25Q_Dir_t dir,
                                          uint8_t *data,
                                          uint32_t dataSize);

static HAL_StatusTypeDef W25Q_SendCommand(uint8_t instruction,
                                          uint32_t address,
                                          uint32_t addressSize,
                                          uint8_t dummyCycles,
                                          W25Q_Dir_t dir,
                                          uint8_t *data,
                                          uint32_t dataSize) {
    OSPI_RegularCmdTypeDef cmd = {0};

    cmd.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
    cmd.FlashId            = HAL_OSPI_FLASH_ID_1;
    cmd.Instruction        = instruction;
    cmd.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
    cmd.Address            = address;
    cmd.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    cmd.DummyCycles        = dummyCycles;
    cmd.DQSMode            = HAL_OSPI_DQS_DISABLE;
    cmd.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

    if (addressSize > 0U) {
        cmd.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
        cmd.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
    } else {
        cmd.AddressMode = HAL_OSPI_ADDRESS_NONE;
    }

    if (dataSize > 0U) {
        cmd.DataMode = HAL_OSPI_DATA_1_LINE;
        cmd.NbData   = dataSize;
    } else {
        cmd.DataMode = HAL_OSPI_DATA_NONE;
    }

    HAL_StatusTypeDef status = HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);
    if (status != HAL_OK) {
        return status;
    }

    if (dataSize > 0U) {
        if (dir == W25Q_Dir_Read) {
            return HAL_OSPI_Receive(&hospi1, data, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);
        }
        return HAL_OSPI_Transmit(&hospi1, data, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);
    }
    return HAL_OK;
}

static W25Q_Result_t W25Q_WriteEnable(void) {
    if (W25Q_SendCommand(W25Q_CMD_WRITE_ENABLE, 0U, 0U, 0U,
                         W25Q_Dir_Write, NULL, 0U) != HAL_OK) {
        return W25Q_Result_HalError;
    }
    return W25Q_Result_Ok;
}

static W25Q_Result_t W25Q_WaitBusy(uint32_t timeoutMs) {
    uint8_t statusReg = W25Q_SR1_BUSY;
    uint32_t startTick = HAL_GetTick();

    while ((statusReg & W25Q_SR1_BUSY) != 0U) {
        if ((HAL_GetTick() - startTick) > timeoutMs) {
            return W25Q_Result_Timeout;
        }
        if (W25Q_SendCommand(W25Q_CMD_READ_STATUS_1, 0U, 0U, 0U,
                             W25Q_Dir_Read, &statusReg, 1U) != HAL_OK) {
            return W25Q_Result_HalError;
        }
    }
    return W25Q_Result_Ok;
}

W25Q_Result_t W25Q_Init(void) {
    OSPI_RegularCmdTypeDef cmd = {0};
    uint8_t sr[2] = { 0x00U, W25Q_SR2_QE };

    cmd.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
    cmd.FlashId            = HAL_OSPI_FLASH_ID_1;
    cmd.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
    cmd.AddressMode        = HAL_OSPI_ADDRESS_NONE;
    cmd.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode           = HAL_OSPI_DATA_NONE;
    cmd.DummyCycles        = 0U;
    cmd.DQSMode            = HAL_OSPI_DQS_DISABLE;
    cmd.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

    cmd.Instruction = W25Q_CMD_ENABLE_RESET;
    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Q_Result_HalError;
    }

    cmd.Instruction = W25Q_CMD_RESET;
    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Q_Result_HalError;
    }

    HAL_Delay(10);

    if (W25Q_WriteEnable() != W25Q_Result_Ok) {
        return W25Q_Result_HalError;
    }

    cmd.Instruction = W25Q_CMD_VOLATILE_SR_WREN;
    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Q_Result_HalError;
    }

    if (W25Q_SendCommand(W25Q_CMD_WRITE_STATUS_REG, 0U, 0U, 0U,
                         W25Q_Dir_Write, sr, 2U) != HAL_OK) {
        return W25Q_Result_HalError;
    }

    HAL_Delay(10);

    uint8_t sr1Verify = 0U;
    uint8_t sr2Verify = 0U;
    if (W25Q_SendCommand(W25Q_CMD_READ_STATUS_1, 0U, 0U, 0U,
                         W25Q_Dir_Read, &sr1Verify, 1U) != HAL_OK) {
        return W25Q_Result_HalError;
    }
    if (W25Q_SendCommand(W25Q_CMD_READ_STATUS_2, 0U, 0U, 0U,
                         W25Q_Dir_Read, &sr2Verify, 1U) != HAL_OK) {
        return W25Q_Result_HalError;
    }
    if ((sr2Verify & W25Q_SR2_QE) == 0U) {
        return W25Q_Result_HalError;
    }
    (void)sr1Verify;
    return W25Q_Result_Ok;
}

W25Q_Result_t W25Q_ReadJedecId(W25Q_JedecId_t *id) {
    if (id == NULL) {
        return W25Q_Result_HalError;
    }

    uint8_t buf[3] = {0U, 0U, 0U};
    if (W25Q_SendCommand(W25Q_CMD_READ_JEDEC_ID, 0U, 0U, 0U,
                         W25Q_Dir_Read, buf, 3U) != HAL_OK) {
        return W25Q_Result_HalError;
    }

    id->mfrId    = buf[0];
    id->memType  = buf[1];
    id->capacity = buf[2];

    if ((id->mfrId != W25Q_JEDEC_MFR_WINBOND) ||
        (id->capacity != W25Q_JEDEC_CAP_32MBIT)) {
        return W25Q_Result_InvalidId;
    }
    return W25Q_Result_Ok;
}

W25Q_Result_t W25Q_ReadMfrDevId(uint8_t *mfrId, uint8_t *devId) {
    if ((mfrId == NULL) || (devId == NULL)) {
        return W25Q_Result_HalError;
    }

    /* 0x90: 3 dummy address bytes (don't care), then 2 data bytes.
       Use address-mode=24 bits with address=0 to clock out the dummy address. */
    uint8_t buf[2] = {0U, 0U};
    if (W25Q_SendCommand(W25Q_CMD_READ_MFR_DEV_ID, 0U, 24U, 0U,
                         W25Q_Dir_Read, buf, 2U) != HAL_OK) {
        return W25Q_Result_HalError;
    }

    *mfrId = buf[0];
    *devId = buf[1];
    return W25Q_Result_Ok;
}

W25Q_Result_t W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len) {
    if ((buf == NULL) || (len == 0U) || ((addr + len) > W25Q_END_ADDR)) {
        return W25Q_Result_HalError;
    }

    OSPI_RegularCmdTypeDef cmd = {0};
    cmd.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
    cmd.FlashId            = HAL_OSPI_FLASH_ID_1;
    cmd.Instruction        = W25Q_CMD_READ_DATA;
    cmd.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
    cmd.Address            = addr;
    cmd.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;
    cmd.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;
    cmd.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode           = HAL_OSPI_DATA_1_LINE;
    cmd.NbData             = len;
    cmd.DummyCycles        = 0U;
    cmd.DQSMode            = HAL_OSPI_DQS_DISABLE;
    cmd.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Q_Result_HalError;
    }
    if (HAL_OSPI_Receive(&hospi1, buf, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Q_Result_HalError;
    }
    return W25Q_Result_Ok;
}

W25Q_Result_t W25Q_ReadQuad(uint32_t addr, uint8_t *buf, uint32_t len) {
    if ((buf == NULL) || (len == 0U) || ((addr + len) > W25Q_END_ADDR)) {
        return W25Q_Result_HalError;
    }

    /* 0xEB Fast Read Quad I/O (1-4-4): instruction on 1 line, address and
       data on 4 lines, 4 dummy cycles between mode byte and data.
       The W25Q32 inserts a Mode byte (M7-M0, used for "Continuous Read
       Mode" - don't care in this code path) between the address and the
       dummy cycles; see W25Q32JV datasheet Figure 24a. The OCTOSPI maps
       that to its alternate-bytes phase: 1 byte on 4 lines (2 SCK edges),
       then the dummy cycles, then the data. */
    OSPI_RegularCmdTypeDef cmd = {0};
    cmd.OperationType          = HAL_OSPI_OPTYPE_COMMON_CFG;
    cmd.FlashId                = HAL_OSPI_FLASH_ID_1;
    cmd.Instruction            = W25Q_CMD_QUAD_IO_READ;
    cmd.InstructionMode        = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd.InstructionSize        = HAL_OSPI_INSTRUCTION_8_BITS;
    cmd.Address                = addr;
    cmd.AddressMode            = HAL_OSPI_ADDRESS_4_LINES;
    cmd.AddressSize            = HAL_OSPI_ADDRESS_24_BITS;
    cmd.AlternateBytesMode     = HAL_OSPI_ALTERNATE_BYTES_4_LINES;
    cmd.AlternateBytesSize     = HAL_OSPI_ALTERNATE_BYTES_8_BITS;
    cmd.AlternateBytes         = 0xFFU;
    cmd.AlternateBytesDtrMode  = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
    cmd.DataMode               = HAL_OSPI_DATA_4_LINES;
    cmd.NbData                 = len;
    cmd.DummyCycles            = W25Q_DUMMY_CYCLES_QUAD_IO;
    cmd.DQSMode                = HAL_OSPI_DQS_DISABLE;
    cmd.SIOOMode               = HAL_OSPI_SIOO_INST_EVERY_CMD;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Q_Result_HalError;
    }
    if (HAL_OSPI_Receive(&hospi1, buf, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Q_Result_HalError;
    }
    return W25Q_Result_Ok;
}

W25Q_Result_t W25Q_PageProgram(uint32_t addr, const uint8_t *buf, uint32_t len) {
    if ((buf == NULL) || (len == 0U) || ((addr + len) > W25Q_END_ADDR)) {
        return W25Q_Result_HalError;
    }

    uint32_t pageRemain;
    uint32_t chunk;
    uint32_t offset = 0U;

    while (len > 0U) {
        pageRemain = W25Q_PAGE_SIZE - (addr & (W25Q_PAGE_SIZE - 1U));
        chunk = (len < pageRemain) ? len : pageRemain;

        if (W25Q_WriteEnable() != W25Q_Result_Ok) {
            return W25Q_Result_HalError;
        }

        if (W25Q_SendCommand(W25Q_CMD_PAGE_PROGRAM, addr, 24U, 0U,
                             W25Q_Dir_Write,
                             (uint8_t *)&buf[offset], chunk) != HAL_OK) {
            return W25Q_Result_HalError;
        }

        W25Q_Result_t busyRes = W25Q_WaitBusy(W25Q_TIMEOUT_MS);
        if (busyRes != W25Q_Result_Ok) {
            return busyRes;
        }

        addr   += chunk;
        offset += chunk;
        len    -= chunk;
    }
    return W25Q_Result_Ok;
}

W25Q_Result_t W25Q_PageProgramQuad(uint32_t addr, const uint8_t *buf, uint32_t len) {
    if ((buf == NULL) || (len == 0U) || ((addr + len) > W25Q_END_ADDR)) {
        return W25Q_Result_HalError;
    }

    /* Note on memory-mapped writes: the W25Q32 clears WEL after every
       program instruction and rejects further programs while BUSY=1,
       so a true "memcpy into XIP" would need 0x06 Write Enable before
       every byte. The HAL issues HAL_OSPI_Command (used for that 0x06)
       with FMODE=0, which aborts the memory-mapped mode, so the only
       sound way to do memory-mapped writes on STM32H7 is to re-enter
       the mode after every WREN - slow enough that the demo uses this
       indirect path for writes and saves the XIP window for reads only.
       See docs/README.md "Memory-mapped mode in this demo (Step 3)". */

    uint32_t pageRemain;
    uint32_t chunk;
    uint32_t offset = 0U;

    while (len > 0U) {
        pageRemain = W25Q_PAGE_SIZE - (addr & (W25Q_PAGE_SIZE - 1U));
        chunk = (len < pageRemain) ? len : pageRemain;

        if (W25Q_WriteEnable() != W25Q_Result_Ok) {
            return W25Q_Result_HalError;
        }

        /* 0x32 Quad Input Page Program: instruction and address on 1 line,
           data on 4 lines. Requires QE=1 in SR2 (set by W25Q_Init). */
        OSPI_RegularCmdTypeDef cmd = {0};
        cmd.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
        cmd.FlashId            = HAL_OSPI_FLASH_ID_1;
        cmd.Instruction        = W25Q_CMD_QUAD_PAGE_PROGRAM;
        cmd.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
        cmd.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
        cmd.Address            = addr;
        cmd.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;
        cmd.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;
        cmd.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
        cmd.DataMode           = HAL_OSPI_DATA_4_LINES;
        cmd.NbData             = chunk;
        cmd.DummyCycles        = 0U;
        cmd.DQSMode            = HAL_OSPI_DQS_DISABLE;
        cmd.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

        if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
            return W25Q_Result_HalError;
        }
        if (HAL_OSPI_Transmit(&hospi1, (uint8_t *)&buf[offset], HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
            return W25Q_Result_HalError;
        }

        W25Q_Result_t busyRes = W25Q_WaitBusy(W25Q_TIMEOUT_MS);
        if (busyRes != W25Q_Result_Ok) {
            return busyRes;
        }

        addr   += chunk;
        offset += chunk;
        len    -= chunk;
    }
    return W25Q_Result_Ok;
}

W25Q_Result_t W25Q_SectorErase(uint32_t addr) {
    if (addr >= W25Q_END_ADDR) {
        return W25Q_Result_HalError;
    }

    if (W25Q_WriteEnable() != W25Q_Result_Ok) {
        return W25Q_Result_HalError;
    }

    if (W25Q_SendCommand(W25Q_CMD_SECTOR_ERASE, addr, 24U, 0U,
                         W25Q_Dir_Write, NULL, 0U) != HAL_OK) {
        return W25Q_Result_HalError;
    }
    return W25Q_WaitBusy(W25Q_TIMEOUT_MS);
}

W25Q_Result_t W25Q_ChipErase(void) {
    if (W25Q_WriteEnable() != W25Q_Result_Ok) {
        return W25Q_Result_HalError;
    }
    if (W25Q_SendCommand(W25Q_CMD_CHIP_ERASE, 0U, 0U, 0U,
                         W25Q_Dir_Write, NULL, 0U) != HAL_OK) {
        return W25Q_Result_HalError;
    }
    return W25Q_WaitBusy(W25Q_CHIP_ERASE_TIMEOUT_MS);
}

W25Q_Result_t W25Q_EnterMemoryMapped(void) {
    OSPI_RegularCmdTypeDef cmd = {0};
    OSPI_MemoryMappedTypeDef mmCfg = {0};

    /* WRITE_CFG: 0x32 Quad Input Page Program (1-1-4).
       Configures the profile used by the OCTOSPI if a CPU store hits the
       memory-mapped window. Matches FrameCam4 (ExternalFlash.c
       w25q64_xip_init_para write side). */
    cmd.OperationType      = HAL_OSPI_OPTYPE_WRITE_CFG;
    cmd.FlashId            = HAL_OSPI_FLASH_ID_1;
    cmd.Instruction        = W25Q_CMD_QUAD_PAGE_PROGRAM;
    cmd.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
    cmd.Address            = 0U;
    cmd.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;
    cmd.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;
    cmd.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode           = HAL_OSPI_DATA_4_LINES;
    cmd.NbData             = 1U;
    cmd.DummyCycles        = 0U;
    cmd.DQSMode            = HAL_OSPI_DQS_DISABLE;
    cmd.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Q_Result_HalError;
    }

    /* READ_CFG: 0x6B Fast Read Quad Output (1-1-4) with 8 dummy cycles.
       0x6B has no mode byte between the address and the dummy cycles
       (datasheet Figure 20), so AlternateBytes stays NONE here. */
    cmd.OperationType      = HAL_OSPI_OPTYPE_READ_CFG;
    cmd.Instruction        = W25Q_CMD_FAST_READ_QUAD_OUT;
    cmd.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;
    cmd.DataMode           = HAL_OSPI_DATA_4_LINES;
    cmd.DummyCycles        = W25Q_DUMMY_CYCLES_QUAD_OUT;
    cmd.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Q_Result_HalError;
    }

    mmCfg.TimeOutActivation = HAL_OSPI_TIMEOUT_COUNTER_DISABLE;
    mmCfg.TimeOutPeriod     = 0U;

    if (HAL_OSPI_MemoryMapped(&hospi1, &mmCfg) != HAL_OK) {
        return W25Q_Result_HalError;
    }
    return W25Q_Result_Ok;
}
