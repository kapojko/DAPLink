#include "main.h"
#include "Debug.h"
#include "Version.h"
#include "stm32h7xx_hal_gpio.h"
#include "AppMain.h"
#include "W25Q32.h"

static volatile uint8_t timerElapsed = 0;
static volatile uint8_t btnBootPressed = 0;
static volatile uint8_t btnConfPressed = 0;

extern TIM_HandleTypeDef htim2;

#define W25Q_TEST_BUF_SIZE         64U

/* Use three separate sectors for the three demo steps so that each test
   owns its own erase / program / read area. */
#define W25Q_TEST_SECTOR_SINGLE     (16U * W25Q_SECTOR_SIZE)
#define W25Q_TEST_SECTOR_QUAD       (17U * W25Q_SECTOR_SIZE)
#define W25Q_TEST_SECTOR_MMAPPED    (18U * W25Q_SECTOR_SIZE)

static const uint8_t testWritePattern[W25Q_TEST_BUF_SIZE] = {
    'L', 'P', '2', '_', 'O', 'S', 'P', 'I', '_',
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0xFE, 0xDC, 0xBA, 0x09, 0x87, 0x65, 0x43, 0x21,
    'M', 'M', 'O', 'D', 'E', '_', 'T',
    0xDE, 0xAD, 0xBE, 0xEF, 0xC0, 0xFF, 0xEE, 0x00,
    'X', 'I', 'P', '_', 'R', 'E', 'A', 'D',
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0
};

static uint8_t testReadback[W25Q_TEST_BUF_SIZE];

static const char *W25Q_ResultStr(W25Q_Result_t r) {
    switch (r) {
    case W25Q_Result_Ok:         return "OK";
    case W25Q_Result_Timeout:    return "TIMEOUT";
    case W25Q_Result_HalError:   return "HAL_ERROR";
    case W25Q_Result_InvalidId:  return "INVALID_ID";
    default:                     return "?";
    }
}

static uint32_t ComparePattern(const uint8_t *actual, const uint8_t *expected, uint32_t len) {
    uint32_t mismatches = 0U;
    for (uint32_t i = 0U; i < len; ++i) {
        if (actual[i] != expected[i]) {
            ++mismatches;
        }
    }
    return mismatches;
}

/* Step 1: classic single-IO path.
   Erase (0x20 sector erase 1-1-0), Program (0x02 1-1-1), Read (0x03 1-1-1 indirect). */
static void TestSingleMode(void) {
    W25Q_Result_t res;
    uint32_t mismatches;

    PRINT("\r\n=== Step 1: Single-IO (1-1-1) ===\r\n");

    res = W25Q_SectorErase(W25Q_TEST_SECTOR_SINGLE);
    PRINT("SectorErase        [1-1-0] @0x%08X: %s\r\n", W25Q_TEST_SECTOR_SINGLE,
          W25Q_ResultStr(res));
    if (res != W25Q_Result_Ok) {
        return;
    }

    res = W25Q_PageProgram(W25Q_TEST_SECTOR_SINGLE, testWritePattern, W25Q_TEST_BUF_SIZE);
    PRINT("PageProgram        [1-1-1] %u bytes @0x%08X: %s\r\n", W25Q_TEST_BUF_SIZE,
          W25Q_TEST_SECTOR_SINGLE, W25Q_ResultStr(res));
    if (res != W25Q_Result_Ok) {
        return;
    }

    res = W25Q_Read(W25Q_TEST_SECTOR_SINGLE, testReadback, W25Q_TEST_BUF_SIZE);
    PRINT("ReadData           [1-1-1] %u bytes @0x%08X: %s\r\n", W25Q_TEST_BUF_SIZE,
          W25Q_TEST_SECTOR_SINGLE, W25Q_ResultStr(res));
    if (res != W25Q_Result_Ok) {
        return;
    }

    mismatches = ComparePattern(testReadback, testWritePattern, W25Q_TEST_BUF_SIZE);
    PRINT("Single compare: %s (%u mismatches)\r\n",
          (mismatches == 0U) ? "MATCH" : "MISMATCH", mismatches);
}

/* Step 2: quad-IO path.
   Erase (0x20 sector erase 1-1-0 - the W25Q32 has no quad erase command),
   Program (0x32 1-1-4), Read (0xEB Fast Read Quad I/O 1-4-4, 4 dummy cycles). */
static void TestQuadMode(void) {
    W25Q_Result_t res;
    uint32_t mismatches;

    PRINT("\r\n=== Step 2: Quad-IO (1-4-4) ===\r\n");

    res = W25Q_SectorErase(W25Q_TEST_SECTOR_QUAD);
    PRINT("SectorErase        [1-1-0] @0x%08X: %s\r\n", W25Q_TEST_SECTOR_QUAD,
          W25Q_ResultStr(res));
    if (res != W25Q_Result_Ok) {
        return;
    }

    res = W25Q_PageProgramQuad(W25Q_TEST_SECTOR_QUAD, testWritePattern, W25Q_TEST_BUF_SIZE);
    PRINT("QuadPageProgram    [1-1-4] %u bytes @0x%08X: %s\r\n", W25Q_TEST_BUF_SIZE,
          W25Q_TEST_SECTOR_QUAD, W25Q_ResultStr(res));
    if (res != W25Q_Result_Ok) {
        return;
    }

    res = W25Q_ReadQuad(W25Q_TEST_SECTOR_QUAD, testReadback, W25Q_TEST_BUF_SIZE);
    PRINT("FastReadQuadIO     [1-4-4] %u bytes @0x%08X: %s\r\n", W25Q_TEST_BUF_SIZE,
          W25Q_TEST_SECTOR_QUAD, W25Q_ResultStr(res));
    if (res != W25Q_Result_Ok) {
        return;
    }

    mismatches = ComparePattern(testReadback, testWritePattern, W25Q_TEST_BUF_SIZE);
    PRINT("Quad compare: %s (%u mismatches)\r\n",
          (mismatches == 0U) ? "MATCH" : "MISMATCH", mismatches);
}

/* Step 3: memory-mapped read path.
   Erase and program are command-driven (indirect mode) for the same
   reason as the rest of the project: the W25Q32 requires 0x06 Write
   Enable before every program and clears WEL after each one, and
   the OCTOSPI's memory-mapped FSM cannot inject 0x06 for us. The
   read goes through the 0x90000000 XIP window:
     - W25Q_EnterMemoryMapped configures READ_CFG = 0x6B (1-1-4, 8
       dummy) and leaves the WRITE_CFG in place for the rare case
       where it might be used.
     - The read is a plain dereference of the XIP pointer; the
       OCTOSPI's memory-mapped FSM emits 0x6B on every CPU load. */
static void TestMemoryMappedMode(void) {
    W25Q_Result_t res;
    uint32_t mismatches;

    PRINT("\r\n=== Step 3: Memory-Mapped read (1-1-4) ===\r\n");

    res = W25Q_SectorErase(W25Q_TEST_SECTOR_MMAPPED);
    PRINT("SectorErase        [1-1-0] @0x%08X: %s\r\n", W25Q_TEST_SECTOR_MMAPPED,
          W25Q_ResultStr(res));
    if (res != W25Q_Result_Ok) {
        return;
    }

    res = W25Q_PageProgramQuad(W25Q_TEST_SECTOR_MMAPPED, testWritePattern, W25Q_TEST_BUF_SIZE);
    PRINT("QuadPageProgram    [1-1-4] %u bytes @0x%08X: %s\r\n", W25Q_TEST_BUF_SIZE,
          W25Q_TEST_SECTOR_MMAPPED, W25Q_ResultStr(res));
    if (res != W25Q_Result_Ok) {
        return;
    }

    res = W25Q_EnterMemoryMapped();
    PRINT("EnterMemoryMapped  [R:1-1-4]      : %s\r\n", W25Q_ResultStr(res));
    if (res != W25Q_Result_Ok) {
        return;
    }

    /* Flush any stale DCache line that may already be held for the
       0x90000000 window (e.g. from a prior run, debugger prefetch, or
       a previous boot's memory-mapped access). The indirect reads in
       Steps 1 and 2 do not populate DCache for the XIP window - they
       fill a CPU buffer in SRAM via HAL_OSPI_Receive - so this is
       purely defensive, but cheap and correct. */
    SCB_InvalidateDCache_by_Addr((void *)(W25Q_MM_BASE + W25Q_TEST_SECTOR_MMAPPED),
                                 W25Q_TEST_BUF_SIZE);

    const volatile uint8_t *mmPtr =
        (const volatile uint8_t *)(W25Q_MM_BASE + W25Q_TEST_SECTOR_MMAPPED);

    mismatches = 0U;
    for (uint32_t i = 0U; i < W25Q_TEST_BUF_SIZE; ++i) {
        if (mmPtr[i] != testWritePattern[i]) {
            ++mismatches;
            if (mismatches <= 4U) {
                PRINT("  MM mismatch [%u]: read=0x%02X expected=0x%02X\r\n",
                      (unsigned)i, mmPtr[i], testWritePattern[i]);
            }
        }
    }
    PRINT("MemoryMapped compare: %s (%u mismatches)\r\n",
          (mismatches == 0U) ? "MATCH" : "MISMATCH", mismatches);
}

static void RunW25Q32Test(void) {
    W25Q_JedecId_t jedec = {0U, 0U, 0U};
    uint8_t mfrId = 0U;
    uint8_t devId = 0U;
    W25Q_Result_t res;

    PRINT("\r\n--- W25Q32 OSPI1 test ---\r\n");

    res = W25Q_ReadMfrDevId(&mfrId, &devId);
    PRINT("ReadMfrDevID  [1-1-1]: %s, MFR=0x%02X DEV=0x%02X\r\n",
          W25Q_ResultStr(res), mfrId, devId);

    res = W25Q_ReadJedecId(&jedec);
    if (res == W25Q_Result_Ok) {
        PRINT("ReadJEDEC     [1-1-1]: %s, MFR=0x%02X TYPE=0x%02X CAP=0x%02X\r\n",
              W25Q_ResultStr(res), jedec.mfrId, jedec.memType, jedec.capacity);
    } else {
        PRINT("ReadJEDEC     [1-1-1]: %s (expected 0xEF/0x40/0x16)\r\n",
              W25Q_ResultStr(res));
    }

    res = W25Q_Init();
    PRINT("W25Q_Init (set QE bit): %s\r\n", W25Q_ResultStr(res));
    if (res != W25Q_Result_Ok) {
        return;
    }

    TestSingleMode();
    TestQuadMode();
    TestMemoryMappedMode();
}

void AppInit(void) {
    PRINT("LinkPlus2_Demo started (FW %s, HW %s), SYSCLK=%luMHz\r\n",
          FW_VERSION, HW_VERSION, SystemCoreClock / 1000000);

    HAL_TIM_Base_Start_IT(&htim2);

    RunW25Q32Test();
}

void AppDeal(void) {
    if (timerElapsed) {
        timerElapsed = 0;
    }

    if (btnBootPressed) {
        PRINT("Boot button pressed\r\n");
        btnBootPressed = 0;

        HAL_GPIO_TogglePin(LED_CONNECTED_GPIO_Port, LED_CONNECTED_Pin);
        HAL_GPIO_TogglePin(LED_RUNNING_GPIO_Port, LED_RUNNING_Pin);
        HAL_GPIO_TogglePin(LED_EXTRA_GPIO_Port, LED_EXTRA_Pin);

        HAL_GPIO_TogglePin(LED_HID_GPIO_Port, LED_HID_Pin);
        HAL_GPIO_TogglePin(LED_CDC_GPIO_Port, LED_CDC_Pin);
        HAL_GPIO_TogglePin(LED_MSC_GPIO_Port, LED_MSC_Pin);
    }

    if (btnConfPressed) {
        PRINT("Conf button pressed\r\n");
        btnConfPressed = 0;

        HAL_GPIO_TogglePin(POWER_EN_GPIO_Port, POWER_EN_Pin);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim == &htim2) {
        static uint16_t counter = 0;
        if (++counter == 4000) {
            timerElapsed = 1;
            counter = 0;
        }
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == BTN_BOOT_Pin) {
        btnBootPressed = 1;
    } else if (GPIO_Pin == BTN_CONF_Pin) {
        btnConfPressed = 1;
    }
}
