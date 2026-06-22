# STM32H723VG

STM32H723VGT6 from STMicroelectronics is a Cortex-M7 MCU running up to 550 MHz
with 1 MB of internal flash, 320 KB of D1 SRAM (AXI), 128 KB of DTCM, and
USB 2.0 FS via the OTG_HS peripheral with the embedded FS PHY.

## Porting

Porting was performed using the STM32H743XX port as a reference for the
Cortex-M7 + HAL + Keil RL-USB stack, and the HK32F103XBA port as a reference
for build-system conventions (PowerShell helpers, docs layout, AGENTS.md,
project records). The CubeMX-generated Demo project shipped in
`source/hic_hal/stm32/stm32h723vg/example_cubemx/` was used as the source of
truth for the clock tree, MPU configuration, OCTOSPI1 init parameters, and
GPIO pin map. The external W25Q32 driver is the Demo's
`User/W25Q32.c`/`User/W25Q32.h` lifted into the port root.

The H723 has only one OTG controller (`USB1_OTG_HS`) and uses it in
Full-Speed mode with the embedded FS PHY (`GUSBCFG.PHYSEL = 1`). USB clock is
HSI48 with CRS trim.

The drag-n-drop IAP target is the **external** W25Q32 (4 MB) in quad
command mode, accessed through OCTOSPI1. The DAPLink firmware itself runs
from internal flash; the W25Q32 is the data sink only.

## Configuration

- **HIC ID**: `0x97969952` (STM32H723VG)
- **OS_CLOCK**: 300 MHz (HSE 25 MHz × PLL1: M=8, N=192, P=2, Q=2, R=2)
- **Flash layout** (internal): 128 KB bootloader + 256 KB interface + 128 KB
  user config (total 512 KB, 1 sector for BL, 2 sectors for IF, 1 sector for CFG)
- **RAM**:
  - DTCM 128 KB (0x20000000) - app code + data, leaving 256 B for shared
  - AXI SRAM 320 KB (0x24000000) - drag-n-drop sector buffer
- **External flash**: W25Q32JV (4 MB) at XIP 0x90000000 over OCTOSPI1
  (single port, AF10 NCS, AF9 CLK, AF4 IO0/IO2, AF9 IO1/IO3)
- **USB clock**: HSI48 with CRS, PWR USB voltage detector enabled

## Hardware Notes

Pin assignment matches the CubeMX Demo project shipped in
`example_cubemx/Core/Src/main.c`:

- LEDs (all active-low): PA6 = CONNECTED, PA7 = RUNNING, PB0 = EXTRA,
  PD12 = CDC, PD13 = MSC, PD14 = HID
- SWD direct bit-bang: PE2 = SWDIO_OUT, PE3 = SWDIO_IN, PE5 = SWCLK
  (no level shifter, no buffer)
- Buttons: PB8 = BTN_BOOT (EXTI rising), PB9 = BTN_CONF (EXTI rising, pulldown),
  both on `EXTI9_5_IRQn`. PB8 is used as the alternate reset-button source.
- Power: PE0 = POWER_EN (default off in interface, off in bootloader)
- USB FS: PA11/PA12, AF10 (OTG_HS in embedded FS PHY mode)
- MCO1: PA8 = HSE / 5 = 5 MHz (exported for the target)
- This board has **no target nRESET pin** - the SWD connector is wired
  directly without a reset line. `target_forward_reset` and `RESET_TARGET`
  are stubs.

The Demo's `User/Debug.c` provides USART2-based debug printf, but is kept
in `example_cubemx/User/` for reference only - the DAPLink port uses
vfs_user/ASSERT.TXT for diagnostics instead.

## SDK Notes

- The H7 HAL revision included in `Drivers/STM32H7xx_HAL_Driver/` is older
  (uses the `HAL_OSPI_Init` 1-arg + `HAL_OSPIM_Config` 2-step API, not
  the newer 3-arg single-call API). The W25Q32 driver follows the older
  API to match.
- `HAL_OSPI_DELAY_BLOCK_BYPASSED` is the correct symbol (not
  `HAL_OSPI_DELAY_BLOCK_BYPASS`).
- The OCTOSPI1 IO manager is configured with port 1 only
  (`ClkPort=1, NCSPort=1, IOLowPort=PORT_1_LOW`); the high-half IO port
  is left at its default.
- `system_stm32h7xx.c` (the only SDK file the port is permitted to
  modify per AGENTS.md) already has `HSE_VALUE = 25000000`, matching the
  board's 25 MHz HSE.

## MPU Configuration

The XIP window at `0x90000000` requires an MPU region with
write-through, shareable, cacheable, bufferable, full access, and
execute-enabled attributes. The port's `sdk.c::MPU_Config` sets up two
regions:

- Region 0: full 4 GB, no access (background)
- Region 1: OCTOSPI1_BASE (0x90000000), 64 MB, write-through

without which memory-mapped reads from the W25Q32 will not work.

## Known Issues and Limitations

- USB FS via OTG_HS embedded PHY has not been exercised by DAPLink before.
  The `usbd_STM32H7xx.c` is adapted from the H743 OTG_HS driver and relies
  on the same RL-USB call sequence. First-iteration risks: HSI48 startup
  (mitigated by `HAL_PWREx_EnableUSBVoltageDetector` + CRS), and
  `GUSBCFG.PHYSEL = 1` selection in `USBD_Init`.
- W25Q32 memory-mapped mode is used for XIP reads only. Writes are
  performed through the indirect command path because the W25Q32 clears
  WEL after every program instruction and rejects further programs
  while BUSY=1; the OCTOSPI's memory-mapped FSM cannot inject 0x06
  (Write Enable) for us. See `W25Q32.c::W25Q_PageProgramQuad` for the
  rationale.
- The Demo's `User/Debug.c` (USART2-based printf) is not compiled into
  the port. It is preserved in `example_cubemx/User/` for reference.

## Build Commands

```bash
# Activate virtual environment
.\venv\Scripts\activate

# Build bootloader
python tools/progen_compile.py -t cmake_gcc_arm -g ninja stm32h723vg_bl --clean

# Build interface
python tools/progen_compile.py -t cmake_gcc_arm -g ninja stm32h723vg_if --clean
```

Or use the PowerShell wrapper:

```powershell
.\build_daplink_stm32h723vg.ps1         # builds both bl and if
.\build_daplink_stm32h723vg.ps1 -clean # clean rebuild
.\build_daplink_stm32h723vg.ps1 -compile_commands  # also generate compile_commands.json for the if build
```

## Flash Commands

```powershell
.\program_daplink_stm32h723vg.ps1            # flash both bl and if
.\program_daplink_stm32h723vg.ps1 -bl       # flash bootloader only (0x08000000)
.\program_daplink_stm32h723vg.ps1 -if       # flash interface only (0x08020000)
.\program_daplink_stm32h723vg.ps1 -erase    # mass-erase internal flash
```
