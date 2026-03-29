# DAPLink HK32F103XB Port - Agent Instructions

## Major Paths

| Purpose | Path |
|---------|------|
| **HK32 Datasheet** | `D:\Source\DAPLink\example_no_commit\HK32F103x8xB Datasheet V1.6.txt` |
| **HK32 User Manual** | `D:\Source\DAPLink\example_no_commit\HK32F103x8xB User Manual V1.6.txt` |
| **HIC Target Folder** | `D:\Source\DAPLink\source\hic_hal\hk32\hk32f103xb\` |
| **Reference HIC (STM32)** | `D:\Source\DAPLink\source\hic_hal\stm32\stm32f103xb\` |
| **Build Records** | `D:\Source\DAPLink\records\hic_hal\hk32f103xb.yaml` |
| **Projects Config** | `D:\Source\DAPLink\projects.yaml` |
| **HIC ID Definitions** | `D:\Source\DAPLink\source\daplink\daplink.h` |
| **Device Config** | `D:\Source\DAPLink\source\hic_hal\device.h` |
| **Test Info** | `D:\Source\DAPLink\test\info.py` |

## Build Commands

```bash
# Activate virtual environment
.\venv\Scripts\activate

# Build bootloader
python tools/progen_compile.py -t cmake_gcc_arm -g ninja hk32f103xb_bl --clean

# Build interface
python tools/progen_compile.py -t cmake_gcc_arm -g ninja hk32f103xb_if --clean
```

## Hardware notes

HK32F103CBT6 is *partially* compatible with STM32F103CBT6, but **not fully**. Also, there is an upgraded version HK32F103CBT6A, which is also *not fully compatible* with both STM32F103CBT6 and HK32F103CBT6.
