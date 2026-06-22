/**
 * @file    sdk.c
 * @brief   Clock / HAL MSP initialisation for STM32H723VG.
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2017-2017, ARM Limited, All Rights Reserved
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

#include "stm32h7xx.h"
#include "cortex_m.h"
#include "util.h"
#include "Debug.h"

TIM_HandleTypeDef timer;
uint32_t time_count;

static uint32_t tim2_clk_div(uint32_t apb1clkdiv);

#ifdef DEBUG_PRINT
/* Decode RCC->RSR reset-cause flags and print them. Cleared after read so
 * the next boot reports only the new cause. The H7 HAL exposes these as
 * RCC_RSR_* (not RCC_CSR_* as on smaller STM32 families). */
static void print_reset_cause(void)
{
    static const struct { uint32_t mask; const char *name; } causes[] = {
        { RCC_RSR_LPWRRSTF,  "LPWR"  },
        { RCC_RSR_WWDG1RSTF, "WWDG"  },
        { RCC_RSR_IWDG1RSTF, "IWDG"  },
        { RCC_RSR_SFTRSTF,   "SFTR"  },
        { RCC_RSR_PORRSTF,   "POR"   },
        { RCC_RSR_BORRSTF,   "BOR"   },
        { RCC_RSR_PINRSTF,   "PIN"   },
    };
    uint32_t rsr = RCC->RSR;
    DebugPrint("RCC->RSR=0x%08lx flags:", rsr);
    for (size_t i = 0; i < sizeof(causes)/sizeof(causes[0]); ++i) {
        if (rsr & causes[i].mask) {
            DebugPrint(" %s", causes[i].name);
        }
    }
    DebugPrint("\r\n");
    __HAL_RCC_CLEAR_RESET_FLAGS();
}
#else
static inline void print_reset_cause(void) {}
#endif

/* MPU configuration - required for memory-mapped XIP at 0x90000000.
 * The Demo's MPU_Config and the MPU region set up in MX_OCTOSPI1_Init
 * are merged into a single function called from sdk_init before any
 * XIP access is attempted. */
static void MPU_Config(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    /* Disable the MPU before reconfiguring */
    HAL_MPU_Disable();

    /* Region 0: full 4 GB, no access (background region).
       Same as the Demo's MPU_Config. */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x0;
    MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
    MPU_InitStruct.SubRegionDisable = 0x87;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /* Region 1: OCTOSPI1 (0x90000000), 64 MB,
       write-through, shareable, cacheable, bufferable, full access,
       execute-enabled. This is the region set up in the Demo's
       MX_OCTOSPI1_Init for the XIP window. */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER1;
    MPU_InitStruct.BaseAddress = OCTOSPI1_BASE;
    MPU_InitStruct.Size = MPU_REGION_SIZE_64MB;
    MPU_InitStruct.SubRegionDisable = 0x00U;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
  * @brief  System Clock configuration
  *            System Clock source            = PLL (HSE 25 MHz)
  *            SYSCLK(Hz)                     = 300 MHz
  *            HCLK(Hz)                       = 150 MHz   (AHB DIV2)
  *            APB1/2/3/4(Hz)                 = 75 MHz    (DIV2)
  *            PLL1: HSE/PLLM=8, *PLLN=192, /PLLP=2, /PLLQ=2, /PLLR=2
  *            USB clock                      = HSI48 (with CRS)
  *            OCTOSPI1 clock                 = D1HCLK (150 MHz)
  *            MCO1                           = HSE/5 (5 MHz)
  */
void sdk_init()
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /* MPU configuration must happen before any XIP access */
    MPU_Config();

    HAL_Init();

    /** Supply configuration update enable */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    /** Configure the main internal regulator output voltage */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /** Initialise RCC oscillators */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.HSI48State     = RCC_HSI48_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    // PLL1: 25 MHz / 8 = 3.125 MHz (PLLM, in 2-4 MHz range),
    //       * 192 = 600 MHz VCO, / 2 = 300 MHz PLL1P (SYSCLK)
    RCC_OscInitStruct.PLL.PLLM       = 8;
    RCC_OscInitStruct.PLL.PLLN       = 192;
    RCC_OscInitStruct.PLL.PLLP       = 2;
    RCC_OscInitStruct.PLL.PLLQ       = 2;
    RCC_OscInitStruct.PLL.PLLR       = 2;
    RCC_OscInitStruct.PLL.PLLRGE     = RCC_PLL1VCIRANGE_1;
    RCC_OscInitStruct.PLL.PLLVCOSEL  = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN   = 0;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        util_assert(false);
    }

    /** Initialise CPU, AHB and APB bus clocks */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                       | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_HCLK_DIV2;     // 150 MHz
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;     // 75 MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;     // 75 MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;     // 75 MHz
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;     // 75 MHz

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        util_assert(false);
    }

    /** Initialise peripheral clocks */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART2
                                               | RCC_PERIPHCLK_USART3
                                               | RCC_PERIPHCLK_OSPI
                                               | RCC_PERIPHCLK_USB
                                               | RCC_PERIPHCLK_RNG;
    PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
    PeriphClkInitStruct.UsbClockSelection          = RCC_USBCLKSOURCE_HSI48;
    PeriphClkInitStruct.OspiClockSelection        = RCC_OSPICLKSOURCE_D1HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        util_assert(false);
    }

    /** Enable D2 SRAM clocks. Required so the first USB / UART access to a
     *  D2-side variable does not trigger a bus-error pending reset. Cheap
     *  to keep on. The H723 has D2SRAM1 and D2SRAM2 only (no D2SRAM3). */
    __HAL_RCC_D2SRAM1_CLK_ENABLE();
    __HAL_RCC_D2SRAM2_CLK_ENABLE();

    /** HSI48 trim via CRS (USB clock). Matches the Demo. */
    __HAL_RCC_CRS_CLK_ENABLE();
    RCC_CRSInitStruct.Prescaler         = RCC_CRS_SYNC_DIV1;
    RCC_CRSInitStruct.Source            = RCC_CRS_SYNC_SOURCE_USB2;
    RCC_CRSInitStruct.Polarity          = RCC_CRS_SYNC_POLARITY_RISING;
    RCC_CRSInitStruct.ReloadValue       = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
    RCC_CRSInitStruct.ErrorLimitValue   = 34;
    RCC_CRSInitStruct.HSI48CalibrationValue = 32;
    HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);

    SystemCoreClockUpdate();

    InitDebug();
    PRINT("STM32H723VG: sdk_init done, SYSCLK=%lu MHz, HCLK=%lu MHz\r\n",
          SystemCoreClock / 1000000U,
          HAL_RCC_GetHCLKFreq() / 1000000U);
    PRINT("Silicon revision: 0x%04lx (REVID=%lu, DEVID=0x%03lx)\r\n",
          (unsigned long)DBGMCU->IDCODE,
          (unsigned long)((DBGMCU->IDCODE & DBGMCU_IDCODE_REV_ID) >> 16),
          (unsigned long)(DBGMCU->IDCODE & DBGMCU_IDCODE_DEV_ID));
    print_reset_cause();
}

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    HAL_StatusTypeDef ret;
    RCC_ClkInitTypeDef clk_init;
    uint32_t unused;
    uint32_t prescaler;
    uint32_t source_clock;

    HAL_RCC_GetClockConfig(&clk_init, &unused);

    /* Compute the prescaler value to have TIMx counter clock equal to 4000 Hz */
    source_clock = HAL_RCC_GetPCLK1Freq() * tim2_clk_div(clk_init.APB1CLKDivider);
    prescaler = (uint32_t)(source_clock / 4000) - 1;

    /* Set TIMx instance */
    timer.Instance = TIM2;

    timer.Init.Period            = 0xFFFF;
    timer.Init.Prescaler         = prescaler;
    timer.Init.ClockDivision     = 0;
    timer.Init.CounterMode       = TIM_COUNTERMODE_UP;
    timer.Init.RepetitionCounter = 0;

    __HAL_RCC_TIM2_CLK_ENABLE();

    ret = HAL_TIM_Base_DeInit(&timer);
    if (ret != HAL_OK) {
        return ret;
    }

    time_count = 0;
    ret = HAL_TIM_Base_Init(&timer);
    if (ret != HAL_OK) {
        return ret;
    }

    ret = HAL_TIM_Base_Start(&timer);
    if (ret != HAL_OK) {
        return ret;
    }

    return HAL_OK;
}

void HAL_IncTick(void)
{
    // Do nothing - TIM2 is the tick source.
}

uint32_t HAL_GetTick(void)
{
    cortex_int_state_t state;
    state = cortex_int_get_and_disable();
    const uint32_t ticks = __HAL_TIM_GET_COUNTER(&timer) / 4;
    time_count += (ticks - time_count) & 0x3FFF;
    cortex_int_restore(state);
    return time_count;
}

void HAL_SuspendTick(void)
{
    HAL_TIM_Base_Stop(&timer);
}

void HAL_ResumeTick(void)
{
    HAL_TIM_Base_Start(&timer);
}

static uint32_t tim2_clk_div(uint32_t apb1clkdiv)
{
    switch (apb1clkdiv)
    {
        case RCC_APB1_DIV1:
            return 1;
        case RCC_APB1_DIV2:
            return 1;
        case RCC_APB1_DIV4:
            return 2;
        case RCC_APB1_DIV8:
            return 4;
        case RCC_APB1_DIV16:
            return 8;
        default:
            return 1;
    }
}

/* MSP layer ------------------------------------------------------------------
 * The HAL calls into these to bring up GPIO / clocks for peripherals we use.
 */

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
}

/**
 * USB OTG FS on PA11/PA12 via the USB1_OTG_HS peripheral with embedded FS PHY.
 * The H723 has only one OTG controller (USB1_OTG_HS); FS mode is selected by
 * the GUSBCFG.PHYSEL bit in usbd_STM32H7xx.c. Clock is HSI48 (configured
 * above via RCC_USBCLKSOURCE_HSI48).
 *
 * NOTE: HAL_PCD_MspInit is intentionally NOT provided. This port drives
 * USB at the register level (see usbd_STM32H7xx.c). The IRQ vector entry
 * OTG_HS_IRQHandler is in usbd_STM32H7xx.c:602 and the NVIC is enabled
 * there via USBD_IntrEna -> NVIC_EnableIRQ(OTG_HS_IRQn).
 */

/*
 * USART3 - CDC UART on PB10 (TX) / PB11 (RX), AF7.
 * USART2 is reserved for debug printing (configured in the Demo's
 * example_cubemx/User/Debug.c which is not compiled into the port).
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (huart->Instance != USART3) {
        return;
    }

    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio_init.Pin       = GPIO_PIN_10 | GPIO_PIN_11;
    gpio_init.Mode      = GPIO_MODE_AF_PP;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &gpio_init);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART3) {
        return;
    }
    __HAL_RCC_USART3_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);
}

/*
 * OCTOSPI1 (single-port, used for W25Q32 over OCTOSPIM_P1).
 * Pins: PB2 (CLK AF9), PB6 (NCS AF10), PB1 (IO0 AF4), PB13 (IO2 AF4),
 *       PC10 (IO1 AF9), PA1 (IO3 AF9).
 * ChipSelectHighTime = 2 (matches the Demo's MX_OCTOSPI1_Init).
 */
void HAL_OSPI_MspInit(OSPI_HandleTypeDef *hospi)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (hospi->Instance != OCTOSPI1) {
        return;
    }

    __HAL_RCC_OSPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PB6 = NCS (AF10) */
    gpio_init.Pin       = GPIO_PIN_6;
    gpio_init.Mode      = GPIO_MODE_AF_PP;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Alternate = GPIO_AF10_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* PB2 = CLK (AF9) */
    gpio_init.Pin       = GPIO_PIN_2;
    gpio_init.Alternate = GPIO_AF9_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* PB1 = IO0 (AF4), PB13 = IO2 (AF4) */
    gpio_init.Pin       = GPIO_PIN_1;
    gpio_init.Alternate = GPIO_AF4_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    gpio_init.Pin       = GPIO_PIN_13;
    gpio_init.Alternate = GPIO_AF4_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* PC10 = IO1 (AF9) */
    gpio_init.Pin       = GPIO_PIN_10;
    gpio_init.Alternate = GPIO_AF9_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    /* PA1 = IO3 (AF9) */
    gpio_init.Pin       = GPIO_PIN_1;
    gpio_init.Alternate = GPIO_AF9_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOA, &gpio_init);
}

void HAL_OSPI_MspDeInit(OSPI_HandleTypeDef *hospi)
{
    if (hospi->Instance != OCTOSPI1) {
        return;
    }
    __HAL_RCC_OSPI1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_1  | GPIO_PIN_2  | GPIO_PIN_6  | GPIO_PIN_13);
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10);
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1);
}
