# STM32H723VGT6 DAPLink HIC Port

This directory contains the DAPLink HIC (Hardware Interface Circuit) port for
the **ST STM32H723VGT6** MCU, named `stm32h723vg`.

## Hardware summary

| Item              | Value                                              |
|-------------------|----------------------------------------------------|
| MCU               | STM32H723VGT6 (Cortex-M7 @ 300 MHz)                |
| HIC ID            | `0x97969952` (DAPLINK_HIC_ID_STM32H723VG)         |
| Internal flash    | 1 MB at `0x08000000` (BL 128 KB + IF 256 KB + CFG 128 KB) |
| DTCM              | 128 KB at `0x20000000` (app code + data)           |
| AXI SRAM          | 320 KB at `0x24000000` (sector buffer only)        |
| External flash    | W25Q32JV (4 MB) at XIP `0x90000000` over OCTOSPI1 |
| USB               | OTG_HS in Full-Speed, embedded PHY, HSI48 clock   |
| Debug             | SWD on PE2 (SWDIO_OUT), PE3 (SWDIO_IN), PE5 (SWCLK) |

## Build commands

```bash
.\venv\Scripts\activate
python tools/progen_compile.py -t cmake_gcc_arm -g ninja stm32h723vg_bl --clean
python tools/progen_compile.py -t cmake_gcc_arm -g ninja stm32h723vg_if --clean
```

Or use the PowerShell wrappers at the repo root:

```powershell
.\build_daplink_stm32h723vg.ps1
.\program_daplink_stm32h723vg.ps1
```

## Pin map

Pin assignment matches the CubeMX Demo project shipped in
`example_cubemx/Core/Src/main.c`:

| Function        | Port/Pin   | Notes                                    |
|-----------------|------------|------------------------------------------|
| USB FS DM/DP    | PA11/PA12  | AF10 (OTG_HS embedded PHY)               |
| SWDIO_OUT       | PE2        | push-pull, high-speed                     |
| SWDIO_IN        | PE3        | input, no pull                           |
| SWCLK           | PE5        | push-pull, high-speed                     |
| CONNECTED LED   | PA6        | active-low                               |
| RUNNING LED     | PA7        | active-low                               |
| HID LED         | PD14       | active-low                               |
| CDC LED         | PD12       | active-low                               |
| MSC LED         | PD13       | active-low                               |
| EXTRA LED       | PB0        | active-low                               |
| BTN_BOOT        | PB8        | EXTI rising, no pull                     |
| BTN_CONF        | PB9        | EXTI rising, pulldown                    |
| POWER_EN        | PE0        | output, target power switch              |
| USART2 TX/RX    | PA2/PA3    | AF7 (debug print at 115200 8N1)          |
| W25Q32 NCS      | PB6        | AF10 (OCTOSPIM_P1)                       |
| W25Q32 CLK      | PB2        | AF9  (OCTOSPIM_P1)                       |
| W25Q32 IO0      | PB1        | AF4  (OCTOSPIM_P1)                       |
| W25Q32 IO1      | PC10       | AF9  (OCTOSPIM_P1)                       |
| W25Q32 IO2      | PB13       | AF4  (OCTOSPIM_P1)                       |
| W25Q32 IO3      | PA1        | AF9  (OCTOSPIM_P1)                       |
| UART3 TX/RX     | PB10/11    | AF7  (DAPLink CDC UART)                  |

This board has **no target nRESET pin** - the SWD connector is wired
directly without a reset line.

## Clock tree

- HSE 25 MHz → PLL1 (M=8, N=192, P=2, Q=2, R=2, VCOWIDE, VCIRANGE_1) → 300 MHz SYSCLK
- AHB prescaler /2 → 150 MHz HCLK
- APB1/2/3/4 prescaler /2 → 75 MHz APB
- USB clock = HSI48 with CRS trim (`HAL_PWREx_EnableUSBVoltageDetector` enabled)
- OCTOSPI1 clock = D1HCLK (150 MHz), OSPI ClockPrescaler=1 → 75 MHz OSPI kernel
- USART2/3 clock = D2PCLK1 (75 MHz)

## Directory layout

```
stm32h723vg/
├── DAP_config.h              CMSIS-DAP pin functions (Demo pin map)
├── IO_Config.h               board-level pin definitions
├── Debug.c, Debug.h          PRINT / DebugPrint / delays over USART2 (PA2/PA3)
├── W25Q32.c, W25Q32.h        external flash driver over OCTOSPI1 (from Demo)
├── daplink_addr.h            memory map (flash + RAM + XIP window)
├── flash.c                   HIC flash.c, calls into W25Q32_* driver
├── gpio.c                    LED / SWD / power GPIO init
├── read_uid.c                unique ID reader (H7 0x1FF1E800)
├── sdk.c                     clock tree + USB/OSPI/UART MSP + MPU + boot PRINT
├── uart.c                    USART3 CDC UART driver
├── usb_buf.h                 MSC scratch buffer
├── usb_config.c              USB device descriptors (FS)
├── usbd_STM32H7xx.c          USB device driver (OTG_HS in FS mode)
├── cmsis/                    CMSIS device header + HAL config + system file
│   ├── stm32h7xx.h
│   ├── stm32h723xx.h
│   ├── stm32h7xx_hal_conf.h
│   ├── system_stm32h7xx.c
│   └── system_stm32h7xx.h
├── gcc/                      GCC startup
│   └── startup_stm32h723xx.S
├── stm32h723vg.ld            port linker script (sector buffer in AXI SRAM)
└── example_cubemx/           original CubeMX project (reference only, not compiled)
    ├── Core/
    ├── USB_DEVICE/
    ├── Middlewares/
    ├── User/                  Demo's original Debug.c (kept for reference only)
    ├── startup_stm32h723xx.s
    └── STM32H723XG_FLASH.ld
```

## Debug print over USART2

`Debug.c` / `Debug.h` provide `PRINT(...)` (and the lower-level
`DebugPrint(fmt, ...)`), mirroring the API of the demo's
`User/Debug.c` so the same code works in both. USART2 is
initialised lazily on the first call (PA2/AF7 TX, PA3/AF7 RX, 115200
8N1, clock source D2PCLK1).

PRINT is a no-op unless the port is built with `-DDEBUG_PRINT` (set in
`records/hic_hal/stm32h723vg.yaml`). The macro was renamed from `DEBUG`
to `DEBUG_PRINT` because `DEBUG` is already defined by newlib headers
and would conflict.

`sdk.c::sdk_init` emits a one-line banner so the host UART (e.g. a
USB-UART bridge on PA2/PA3) immediately confirms the chip booted:

```
STM32H723VG: sdk_init done, SYSCLK=300 MHz, HCLK=150 MHz
```

## External flash model

The DAPLink firmware itself runs from internal flash. The external W25Q32
serves as the **drag-n-drop IAP target only**:

- Reads/writes go through `flash.c`, which calls `W25Q32_Init`,
  `W25Q32_SectorErase` (4 KB), `W25Q32_PageProgramQuad` (1-1-4 mode).
- After init, the chip is left in memory-mapped mode so XIP reads at
  `0x90000000` work transparently for the host.
- The `sectors_info[]` table in the board file declares the W25Q32 sector
  size (4 KB) for the target-side flash algorithm lookup.
- The MPU region for the XIP window (region 1: 0x90000000, 64 MB,
  write-through, shareable, cacheable, bufferable) is set up in
  `sdk.c::MPU_Config` and is required for memory-mapped reads to work.

## Known caveats

- Both `stm32h723vg_bl` and `stm32h723vg_if` build cleanly with
  `progen_compile.py -t cmake_gcc_arm -g ninja` on GCC 12.3.1. Build sizes:
  - bootloader: 50 KB text + 80 KB data + 12 KB bss (142 KB total)
  - interface:   78 KB text + 184 KB data + 16 KB bss (278 KB total)
- PA11/PA12 USB AF10 mux is **not** explicitly configured in
  `HAL_PCD_MspInit` (matches the demo). If enumeration fails in field
  test, add an explicit `HAL_GPIO_Init(GPIOA, ...)` for pins 11/12
  with `Alternate = GPIO_AF10_OTG1_FS` in `sdk.c`.
- `PRINT` is no-op unless `-DDEBUG_PRINT` is set. To silence the
  banner, remove the macro from `records/hic_hal/stm32h723vg.yaml`.
- Vector table is at flash start; no boot-mode configuration is required
  (BOOT0 = 0).
- No QSPI / OCTOSPI boot from external flash - DAPLink code always
  executes from internal flash.
- The CubeMX-generated files in `example_cubemx/` are kept for reference
  only and are **not compiled** into either the BL or IF build.
- USART2 is **not** the DAPLink CDC UART - USART3 (PB10/PB11) is.
  USART2 is dedicated to the debug print port.