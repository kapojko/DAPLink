param (
    [switch]$bl,
    [switch]$if,
    [switch]$erase
)

if (-not $ENV:OPENOCD_PATH) {
    $OPENOCD_PATH = "D:\Programs\openocd-v0.12.0-i686-w64-mingw32"
} else {
    $OPENOCD_PATH = $ENV:OPENOCD_PATH
}

function Invoke-OpenOCD {
    param (
        [string]$hexFile,
        [string]$address
    )

    & "$OPENOCD_PATH\bin\openocd.exe" `
        -f "$OPENOCD_PATH\share\openocd\scripts\interface\cmsis-dap.cfg" `
        -f "$OPENOCD_PATH\share\openocd\scripts\target\stm32h7x.cfg" `
        -c "program $hexFile $address verify reset exit"

    if ($LASTEXITCODE -ne 0) {
        Write-Error "OpenOCD failed for file: $hexFile (exit code: $LASTEXITCODE)"
        exit $LASTEXITCODE
    }
}

function Invoke-Erase {
    & "$OPENOCD_PATH\bin\openocd.exe" `
        -f "$OPENOCD_PATH\share\openocd\scripts\interface\cmsis-dap.cfg" `
        -f "$OPENOCD_PATH\share\openocd\scripts\target\stm32h7x.cfg" `
        -c "init" `
        -c "reset halt" `
        -c "stm32h7x mass_erase 0" `
        -c "shutdown"

    if ($LASTEXITCODE -ne 0) {
        Write-Error "OpenOCD erase failed (exit code: $LASTEXITCODE)"
        exit $LASTEXITCODE
    }
}

if ($erase) {
    Write-Host "Erasing chip..."
    Invoke-Erase
    Write-Host "Erase complete."
    exit
}

if ($if) {
    Write-Host "Flashing interface firmware at 0x08020000..."
    Invoke-OpenOCD "projectfiles/cmake_gcc_arm/stm32h723vg_if/build/stm32h723vg_if.hex"
    Write-Host "Done."
    exit
}

if ($bl) {
    Write-Host "Flashing bootloader at 0x08000000..."
    Invoke-OpenOCD "projectfiles/cmake_gcc_arm/stm32h723vg_bl/build/stm32h723vg_bl.hex"
    Write-Host "Done."
    exit
}

# Default: flash both bootloader and interface
Write-Host "Flashing bootloader at 0x08000000..."
Invoke-OpenOCD "projectfiles/cmake_gcc_arm/stm32h723vg_bl/build/stm32h723vg_bl.hex"
Write-Host "Flashing interface firmware at 0x08020000..."
Invoke-OpenOCD "projectfiles/cmake_gcc_arm/stm32h723vg_if/build/stm32h723vg_if.hex"
Write-Host "Done."
