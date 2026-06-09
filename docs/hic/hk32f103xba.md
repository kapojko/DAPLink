# HK32F103XBA

HK32F103XBA from Hangshun (A-suffix, e.g. HK32F103CBT6A) is an upgraded version of HK32F103XB. It uses a different SDK (`HK32F103xxxxA`) with different register layout and is **not** compatible with HK32F103XB.

## Porting

Porting was performed using the HK32F103XB port as a reference, but using the official A-suffix SDK from Hangshun (`HK32F103(39A)VExxx_Ver1_Examples`). The USB stack, GCC startup, and clock/USB clock initialization all required updates.

## Configuration

- **HIC ID**: `0x97969951` (HK32F103XBA)
- **OS_CLOCK**: 72 MHz (HSE 8 MHz × PLL9, default)
- **Flash**: 128 KB (0x08000000 + 48 KB bootloader)
- **RAM**: 20 KB (0x20000000)
- **JTAG TAP ID**: `0x3BA00477` (ARM Cortex-M3 r1p1 default)

## Hardware Notes

Pin assignment matches the HK32F103XB port:

- The nRESET pin (PB0) is configured as *Input with Pull-Up (IPU)* for reset button detection
- USB control pin (PA15) for D+ pullup
- Main LED (PA9) in two modes (VDD and GND) for RUNNING, HID, CDC and MSC activity indication
- Extra LED (PB6) for USB connection indication
- Output SWD on PB13 (SWCLK), PB14 (SWDIO OUT) and PB12 (SWDIO IN).

## SDK Differences from HK32F103XB

The A-suffix SDK has a number of changes from the older B-suffix SDK:

1. Different CMSIS device header (`hk32f103xxxxa.h` vs `hk32f10x.h`)
2. Different StdPeriphDriver file names (`hk32f103xxxxa_*.c/h` vs `hk32f10x_*.c/h`)
3. RCC_TypeDef is significantly larger (HSECTL moved to offset 0xC0, new CFGR2/CFGR3/CR2/PLLCTL/CFGR4/CFGR5 fields)
4. New API: `RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5)` for USB clock selection (replaces old `RCC->LPCLK_CTL |= 0x100;`)
5. USB IRQn renamed: `USB_LP_CAN1_RX0_IRQn` → `USB_LP_CAN_RX0_IRQn` (handler symbol is unchanged)
6. New peripheral: CACHE controller (gated by `RCC_AHBPeriph2_CACHE`, not used by DAPLink)
7. GPIO_InitTypeDef adds a `GPIO_Schmit` field (DAPLink doesn't use it)
8. New HSI28 / HSI56 / EXTCLK clock sources (not used by DAPLink)
9. New USB-FS Device Driver library provided (not used; DAPLink continues to use Keil RL-USB + custom register access)

## Known Issues and Limitations

The same USB BULK hardware issue observed on the HK32F103XB chip is expected to also affect HK32F103XBA (the USB peripheral block is the same). See `docs/hic/hk32f103xb.md` for the detailed analysis.

### Target Adapter Speed

Default adapter speed set to 1MHz, since on test setup 5MHz was sometimes unreliable.
