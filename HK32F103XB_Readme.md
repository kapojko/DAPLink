# HK32F103XB Port - Build Guide

## SDK Usage

The HK32F103XB port uses the **HK32F103 SDK** (taken from Github: https://github.com/JimpleM/594642987-HK32F103CBT6A-Mini-Bored-System/tree/main/HK32F103 ).

**Important Compatibility Notes:**

1. **HK32F103CBT6 is NOT fully compatible with HK32F103CBT6A:**
   - SDK examples for HK32F103CBT6A may not work on HK32F103CBT6

2. **HK32F103CBT6 is NOT fully compatible with STM32F103CBT6:**
   - Different USB register access requirements
   - Different clock configuration requirements
   - HAL vs StdPeriphDriver API differences
   - Unmodified code doesn't work for flash and USB at least

3. **Startup Files:**
   - The SDK includes ARM/Keil format startup files in `cmsis/ARM/` - these are NOT used
   - The project uses `gcc/startup_hk32f103xxxxa.S` which was rewritten for GCC
   - The GCC startup includes DAPLINK-specific initialization (.data/.bss copy, VTOR setup)

## Dependencies

### Python Packages

See `requirements.txt` file.

### Additional packages

Install old version of `setuptools`, otherwise `ModuleNotFoundError: No module named 'pkg_resources'` error will raise on `pyocd` run"

```powershell
pip install --force-reinstall setuptools==81.0.0
```

### Separate System-wide Installs

Required for post-build script (run via `cmd.exe` outside venv):

```bash
pip install jinja2 pyelftools
```

Note: `intelhex` is already in requirements.txt but may need system-wide install if venv Python isn't used by post-build scripts.

### Virtual Environment

```bash
python -m venv venv
./venv/Scripts/pip.exe install -r requirements.txt
```

## Build Commands

### Bootloader

```bash
./venv/Scripts/python.exe tools/progen_compile.py -t cmake_gcc_arm -g ninja hk32f103xb_bl --clean
```

### Interface Firmware

```bash
./venv/Scripts/python.exe tools/progen_compile.py -t cmake_gcc_arm -g ninja hk32f103xb_if --clean
```

## Build Outputs

| Target     | Binary                                   | Location                                          |
| ---------- | ---------------------------------------- | ------------------------------------------------- |
| Bootloader | `hk32f103xb_bl.bin`, `hk32f103xb_bl.hex` | `projectfiles/cmake_gcc_arm/hk32f103xb_bl/build/` |
| Interface  | `hk32f103xb_if.bin`, `hk32f103xb_if.hex` | `projectfiles/cmake_gcc_arm/hk32f103xb_if/build/` |

## Configuration

- **HIC ID**: `0x97969950` (HK32F103XB)
- **OS_CLOCK**: 48 MHz (HSI) or 72 MHz (HSE)
- **Flash**: 128 KB (0x08000000 + 48KB bootloader)
- **RAM**: 20 KB (0x20000000)

### Flash Layout

The HK32F103 flash page size is **2 KB** (unlike STM32F103 medium-density which is 1 KB). The layout uses 2 KB sector alignment throughout:

| Region  | Start      | Size     | Description                  |
|---------|------------|----------|------------------------------|
| BL      | 0x08000000 | 48 KB    | Bootloader (24 × 2 KB pages) |
| IF      | 0x0800C000 | 78 KB    | Interface firmware (39 × 2 KB pages) |
| Config  | 0x0801F800 | 2 KB     | User config (1 × 2 KB page)  |
| **Total** |          | **128 KB** |                          |

`DAPLINK_SECTOR_SIZE` and `DAPLINK_MIN_WRITE_SIZE` are both `0x800` (2 KB) in `source/hic_hal/hk32/hk32f103xb/daplink_addr.h`.

## Hardware Notes

### nRESET Pin (PB0)

The nRESET pin (PB0) is configured as **Input with Pull-Up (IPU)** for reset button detection. This is different from STM32F103XB which uses Output Open-Drain with external pull-up.

If no reset button is connected to PB0, the internal pull-up ensures the pin reads HIGH (button not pressed).

### LED Configuration

| LED | Port | Pin | Function |
|-----|------|-----|----------|
| RUNNING | PA9 | Pin 9 | Bootloader running indicator |
| CONNECTED | PB6 | Pin 6 | USB connected indicator |
| MSC | PA9 | Pin 9 | Mass storage activity |
| CDC | PA9 | Pin 9 | CDC activity |
| HID | PA9 | Pin 9 | HID activity |

Note: All activity LEDs (MSC, CDC, HID) share PA9.

### USB Configuration

- **USB Connect**: PA15 (via NPN transistor)
- **USB DM**: PA11
- **USB DP**: PA12
- **USB Clock**: 48 MHz (PLL/1.5 from 72MHz or PLL/1 from 48MHz system clock)

## Flashing

### Using OpenOCD

See `program_daplink_hk32f103xb.ps1` for instructions.

## SDK File Differences

This section documents all differences between the SDK files in `source/hic_hal/hk32/hk32f103xb/` and the original HK32 SDK in `example_no_commit/HK32F103/`.

### Summary

| Category | Count |
|----------|-------|
| **Identical files** | 47 |
| **Modified files** | 3 |
| **Deleted files** | 2 |
| **Total files compared** | 52 |

### Identical Files (No Changes)

The following SDK files were copied without modification:

**CMSIS files:**
- `cmsis/system_hk32f10x.h` (identical)
- `cmsis/Core/core_cm3.h` (identical)
- `cmsis/Core/core_cm3.c` (identical)
- `cmsis/ARM/startup_hk32f103xB.s` (deleted - Keil/ARM syntax, not used)
- `cmsis/ARM/startup_hk32f103xE.s` (deleted - Keil/ARM syntax, not used)

**Note:** The ARM startup files in `cmsis/ARM/` are for Keil MDK-ARM toolchain and are NOT used. The project uses `gcc/startup_hk32f103xxxxa.S` which is GCC-compatible and was rewritten for DAPLink.

**StdPeriphDriver source files (22 files - all identical):**
- `misc.c`, `hk32f10x_wwdg.c`, `hk32f10x_usart.c`, `hk32f10x_tim.c`
- `hk32f10x_spi.c`, `hk32f10x_sdio.c`, `hk32f10x_rtc.c`, `hk32f10x_rcc.c`
- `hk32f10x_pwr.c`, `hk32f10x_iwdg.c`, `hk32f10x_i2c.c`, `hk32f10x_gpio.c`
- `hk32f10x_fsmc.c`, `hk32f10x_flash.c`, `hk32f10x_exti.c`, `hk32f10x_dma.c`
- `hk32f10x_dbgmcu.c`, `hk32f10x_dac.c`, `hk32f10x_crc.c`, `hk32f10x_can.c`
- `hk32f10x_bkp.c`, `hk32f10x_adc.c`

**StdPeriphDriver header files (22 files - all identical):**
- `misc.h`, `hk32f10x_wwdg.h`, `hk32f10x_usart.h`, `hk32f10x_tim.h`
- `hk32f10x_spi.h`, `hk32f10x_sdio.h`, `hk32f10x_rtc.h`, `hk32f10x_rcc.h`
- `hk32f10x_pwr.h`, `hk32f10x_iwdg.h`, `hk32f10x_i2c.h`, `hk32f10x_gpio.h`
- `hk32f10x_fsmc.h`, `hk32f10x_flash.h`, `hk32f10x_exti.h`, `hk32f10x_dma.h`
- `hk32f10x_dbgmcu.h`, `hk32f10x_dac.h`, `hk32f10x_crc.h`, `hk32f10x_can.h`
- `hk32f10x_bkp.h`, `hk32f10x_adc.h`

**Template files:**
- `StdPeriphDriver/inc/hk32f10x_it.h` (copied from templates, unchanged)

---

### File: cmsis/hk32f10x.h

**Status:** Modified

**Differences:**

1. **Line 45:** Added `HK32F103CBT6` to device preprocessor check:
   - Original: `&& !defined (HK32F103VDXX) && !defined (HK32F103VEXX)`
   - Modified: `&& !defined (HK32F103VDXX) && !defined (HK32F103VEXX) && !defined (HK32F103CBT6)`

2. **Line 49:** Added `HK32F103CBT6` to device family #if condition:
   - Original: `#if defined (HK32F103C8XX) || defined (HK32F103CBXX) || ...`
   - Modified: `#if defined (HK32F103C8XX) || defined (HK32F103CBXX) || ... || defined (HK32F103CBT6)`

3. **Line 962-964:** Added HK32-specific registers to `RCC_TypeDef`:
   ```c
   __IO uint32_t HSECTL;
   __IO uint32_t RESERVED;
   __IO uint32_t LPCLK_CTL;
   ```

---

### File: cmsis/system_hk32f10x.c

**Status:** Modified

**Differences:**

1. **Lines 87-89:** Changed default system clock selection:
   - Original: `#define SYSCLK_HSI_PLL_FREQ_64MHz  64000000` (enabled)
   - Modified: `#define SYSCLK_HSI_PLL_FREQ_48MHz  48000000` (enabled), 64MHz and 72MHz commented out

2. **Lines 123-128:** Added additional SystemCoreClock definitions for 48MHz, 64MHz, and 72MHz HSI PLL options:
   ```c
   #elif defined SYSCLK_HSI_PLL_FREQ_48MHz
       uint32_t SystemCoreClock = SYSCLK_HSI_PLL_FREQ_48MHz;
   #elif defined SYSCLK_HSI_PLL_FREQ_64MHz
       uint32_t SystemCoreClock = SYSCLK_HSI_PLL_FREQ_64MHz;
   #elif defined SYSCLK_HSI_PLL_FREQ_72MHz
       uint32_t SystemCoreClock = SYSCLK_HSI_PLL_FREQ_72MHz;
   ```

3. **Lines 154-159:** Added function prototypes for new clock configuration functions:
   ```c
   #elif defined SYSCLK_HSI_PLL_FREQ_48MHz
       static void SetSysClockToHSI48(void);
   #elif defined SYSCLK_HSI_PLL_FREQ_64MHz
       static void SetSysClockToHSI64(void);
   #elif defined SYSCLK_HSI_PLL_FREQ_72MHz
       static void SetSysClockToHSI72(void);
   ```

4. **Lines 326-330:** Modified SetSysClock() to call SetSysClockToHSI48():
   - Original: Calls `SetSysClockToHSI64()`
   - Modified: Calls `SetSysClockToHSI48()`

5. **Lines 844-925:** Added entirely new `SetSysClockToHSI48()` function:
   - Configures HSI (8MHz) divided by 2, multiplied by 12 = 48MHz PLL
   - **Critical:** Sets USB prescaler bit (RCC_CFGR bit 22) for proper USB clock
   - Uses `RCC_CFGR_PLLSRC_HSI_Div2 | RCC_CFGR_PLLMULL12`
   - Flash latency set to 1 wait state

6. **Lines 908-910:** Added USB clock prescaler configuration:
   ```c
   // Set USB prescaler to 1 (PLL时钟直接作为USB时钟（PLL输出为48 MHz）)
   // NOTE: 在RCC_APB1ENR寄存器中使能USB时钟之前，必须保证该位已经有效
   RCC->CFGR |= ((uint32_t)1 << 22);
   ```

7. **Lines 1006-1083:** Added `SetSysClockToHSI72()` function (for 72MHz operation)

---

### File: StdPeriphDriver/inc/hk32f10x_conf.h

**Status:** Modified

**Differences:**

1. **Line ~20:** Commented out CEC peripheral include:
   - Original: `#include "hk32f10x_cec.h"`
   - Modified: `/* CEC not available in HK32F103CBT6 - #include "hk32f10x_cec.h" */`

---

### File: StdPeriphDriver/inc/hk32f10x_it.h

**Status:** Copied (Identical to template)

This file was copied from `example_no_commit/HK32F103/HK32F103X_lib/templates/hk32f10x_it.h` to the `inc/` directory. No modifications were made.

---

## Known USB Hardware Issues and Limitations

### Overview

The HK32F103XB USB peripheral has a hardware quirk or bug that affects BULK endpoint data transmission. This issue manifests when receiving multiple back-to-back OUT packets from the USB host.

### Symptoms

1. **CDC (Virtual COM Port)**:
   - Reception works correctly for single packets and small data transfers
   - Large data reception fails after the first 16-byte packet
   - Data becomes corrupted or lost when host sends multiple packets in quick succession

2. **MSC (Mass Storage)**: Likely affected by the same issue, appearing non-functional

### Root Cause Analysis

The USB protocol requires that when an endpoint returns NAK status, the host should wait before retrying. However, some USB host controllers (observed in testing) send back-to-back OUT packets without properly waiting for NAK responses.

According to the HK32F103XB User Manual (Section 18.3.2.5), when a BULK OUT packet is received correctly:
1. Hardware sets STAT_RX to NAK (0b10)
2. Hardware sets CTR_RX flag to trigger interrupt
3. Software should process the data and set STAT_RX back to VALID (0b11) to accept the next packet

**The Issue**: When multiple OUT packets arrive in rapid succession (within USB inter-packet timing), the HK32 USB hardware does not properly maintain NAK status for subsequent packets. This causes packet data corruption or loss.

### USB Sniffer Trace Example

The following trace shows the issue when sending 20 bytes to CDC endpoint (16 bytes + 4 bytes split into two packets):

```
Time[s]       Direction  PID   Data
19804	1.242160433	host	3.4	USBLL	19	DATA1
19805	1.242174350	3.4	host	USBLL	1	ACK
19806	1.242176516	host	3.4	USBLL	3	IN
19807	1.242179850	3.4	host	USBLL	1	NAK
19808	1.242182100	host	3.4	USBLL	3	OUT
19809	1.242185350	host	3.4	USBLL	17	DATA0
19810	1.242198016	3.4	host	USBLL	1	ACK
19811	1.242200183	host	3.4	USBLL	3	IN
19812	1.242203433	3.4	host	USBLL	1	NAK
19813	1.242206766	host	3.4	USBLL	3	IN
... (device keeps NAKing, host keeps polling)
```

**Key Observation**: After the ACK at `19810`, the device continues to NAK subsequent IN requests indefinitely, indicating the data was lost or corrupted during reception.

### Current Status

| Feature | Status | Notes |
|---------|--------|-------|
| DAPLINK SWD Programming | ✅ Working | SWD protocol works correctly |
| CDC Reception (single packets) | ✅ Working | Single 16-byte packets work |
| CDC Reception (multi-packet) | ❌ Broken | Data lost/corrupted after first packet |
| CDC Transmission | ⚠️ Uncertain | May have issues with back-to-back IN requests |
| MSC | ❌ Likely Broken | Same BULK endpoint issue |

### Workarounds Investigated

The following approaches were investigated but did not resolve the issue:

1. **Endpoint processing in ISR**: Moving BULK OUT callback processing directly into USB interrupt handler
2. **Explicit NAK setting**: Attempting to force STAT_RX to NAK before processing
3. **COUNT retry reading**: Reading COUNT_RX multiple times in case of zero value
4. **Endpoint number changes**: Moving CDC BULK from EP4 to EP2

None of these workarounds successfully mitigated the hardware behavior.

### Potential Next Steps

1. **Hardware Analysis**: Further investigation with official HK32 SDK examples and HongZhun technical support
2. **Alternative USB HAL**: Try using STM32 USB driver stack adapted for HK32 (requires significant porting)
3. **Software-emulated USB**: Implement USB BULK handling in software (not practical for DAPLINK performance requirements)
4. **Different USB Transceiver**: External USB PHY if hardware limitation is in USB analog front-end

### References

- HK32F103XB User Manual Section 18.5: USB Registers
- USB 2.0 Specification Section 5.8: BULK Transfers
- USB 2.0 Specification Section 8.5: Packet Buffer Layout

