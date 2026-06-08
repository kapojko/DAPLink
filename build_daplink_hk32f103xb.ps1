param (
    [switch]$bl,
    [switch]$if,
    [switch]$clean,
    [switch]$compile_commands
)

$venvPython = ".\venv\Scripts\python.exe"

function Invoke-Build {
    param (
        [string]$project,
        [switch]$clean
    )

    $args = @("tools/progen_compile.py", "-t", "cmake_gcc_arm", "-g", "ninja", $project)
    if ($clean) {
        $args += "--clean"
    }

    Write-Host "Building $project..."
    & $venvPython @args
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed for $project (exit code: $LASTEXITCODE)"
        exit $LASTEXITCODE
    }
}

function New-CompileCommands {
    param (
        [string]$project
    )

    $buildDir = "projectfiles/cmake_gcc_arm/$project/build"
    $sourceDir = "projectfiles/cmake_gcc_arm/$project"

    if (-not (Test-Path $buildDir)) {
        Write-Error "Build directory not found: $buildDir. Run build first."
        return
    }

    Write-Host "Generating compile_commands.json for $project..."
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G Ninja -S $sourceDir -B $buildDir
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake failed for $project (exit code: $LASTEXITCODE)"
        exit $LASTEXITCODE
    }

    if (Test-Path "$buildDir/compile_commands.json") {
        Write-Host "Created: $buildDir/compile_commands.json"
    } else {
        Write-Warning "compile_commands.json was not created"
    }
}

if (-not $bl -and -not $if) {
    # Build both if no specific target selected
    Invoke-Build -project "hk32f103xb_bl" -clean:$clean
    Invoke-Build -project "hk32f103xb_if" -clean:$clean
    if ($compile_commands) {
        New-CompileCommands -project "hk32f103xb_if"
    }
} else {
    if ($bl) {
        Invoke-Build -project "hk32f103xb_bl" -clean:$clean
    }
    if ($if) {
        Invoke-Build -project "hk32f103xb_if" -clean:$clean
        if ($compile_commands) {
            New-CompileCommands -project "hk32f103xb_if"
        }
    }
}
