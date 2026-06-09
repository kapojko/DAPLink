# DAPLink HK32F103 Port - Agent Instructions

## Major Paths

| Purpose | Path |
|---------|------|
| **HK32 Datasheet** | `D:\Source\DAPLink\example_no_commit\HK32F103x8xB Datasheet V1.6.txt` |
| **HK32 User Manual** | `D:\Source\DAPLink\example_no_commit\HK32F103x8xB User Manual V1.6.txt` |
| **HK32A Datasheet** | `D:\Source\DAPLink\example_no_commit\HK32F103A\HK32F103x8xBT6A数据手册V1.8.md` |
| **HK32A User Manual** | `D:\Source\DAPLink\example_no_commit\HK32F103A\HK32F103x8xBT6A用户手册V1.7.md` |
| **HK32A SDK Examples** | `D:\Source\DAPLink\example_no_commit\HK32F103A\HK32F103(39A)VExxx_Ver1_Examples` |
| **HIC Target Folder (B)** | `D:\Source\DAPLink\source\hic_hal\hk32\hk32f103xb\` |
| **HIC Target Folder (BA)** | `D:\Source\DAPLink\source\hic_hal\hk32\hk32f103xba\` |
| **Reference HIC (STM32)** | `D:\Source\DAPLink\source\hic_hal\stm32\stm32f103xb\` |
| **Build Records (B)** | `D:\Source\DAPLink\records\hic_hal\hk32f103xb.yaml` |
| **Build Records (BA)** | `D:\Source\DAPLink\records\hic_hal\hk32f103xba.yaml` |
| **Projects Config** | `D:\Source\DAPLink\projects.yaml` |
| **HIC ID Definitions** | `D:\Source\DAPLink\source\daplink\daplink.h` |
| **Device Config** | `D:\Source\DAPLink\source\hic_hal\device.h` |
| **Test Info** | `D:\Source\DAPLink\test\info.py` |

## Build Commands

```bash
# Activate virtual environment
.\venv\Scripts\activate

# HK32F103XB (legacy B-suffix)
python tools/progen_compile.py -t cmake_gcc_arm -g ninja hk32f103xb_bl --clean
python tools/progen_compile.py -t cmake_gcc_arm -g ninja hk32f103xb_if --clean

# HK32F103XBA (new A-suffix, HK32F103CBT6A)
python tools/progen_compile.py -t cmake_gcc_arm -g ninja hk32f103xba_bl --clean
python tools/progen_compile.py -t cmake_gcc_arm -g ninja hk32f103xba_if --clean
```

## Hardware notes

HK32F103CBT6 is *partially* compatible with STM32F103CBT6, but **not fully**. Also, there is an upgraded version HK32F103CBT6A, which is also *not fully compatible* with both STM32F103CBT6 and HK32F103CBT6.

HK32F103CBT6A has its own official SDK (`HK32F103xxxxA`) with different register layout from the older `HK32F103xB` SDK. The two ports must be kept separate.

## Editing notes

Do NOT modify SDK files except explicitly granted by user! The only SDK files that can be modified according to the task is system... .c/.h and startup assembly.
