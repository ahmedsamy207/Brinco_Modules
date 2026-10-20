#Requires -Version 5.1
<#
    Quick environment check for the PY32F003 VS Code setup.

    Run from the project root:
        powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\check-env.ps1
    or use the "Check toolchain" task in VS Code.
#>

$ErrorActionPreference = 'Continue'
$script:problems = 0

function Write-Check {
    param(
        [string]$Name,
        [string]$Command,
        [string[]]$Arguments
    )

    $found = Get-Command $Command -ErrorAction SilentlyContinue
    if (-not $found) {
        Write-Host ("[MISSING] {0} - '{1}' was not found on PATH" -f $Name, $Command) -ForegroundColor Red
        $script:problems++
        return
    }

    $output = & $Command @Arguments 2>&1 | Select-Object -First 1
    Write-Host ("[OK]      {0}: {1}" -f $Name, $output) -ForegroundColor Green
}

Write-Host "=== PY32F003 toolchain check ==="
Write-Host ""

Write-Check 'Arm GNU Toolchain' 'arm-none-eabi-gcc' @('--version')
Write-Check 'GNU Make'          'make'              @('--version')
Write-Check 'Python launcher'   'py'                @('--version')
Write-Check 'pyOCD'             'pyocd'             @('--version')

# Device family pack shipped with this project
$projectRoot = Split-Path -Parent $PSScriptRoot
$packPath = Join-Path $PSScriptRoot 'packs\Puya.PY32F0xx_DFP.1.2.0.pack'
if (Test-Path -LiteralPath $packPath) {
    Write-Host ("[OK]      Puya DFP: {0}" -f $packPath) -ForegroundColor Green
} else {
    Write-Host ("[MISSING] Puya DFP: {0}" -f $packPath) -ForegroundColor Red
    $script:problems++
}

# pyOCD target visibility (uses pyocd.yaml because cwd is the project root)
$pyocd = Get-Command pyocd -ErrorAction SilentlyContinue
if ($pyocd) {
    Push-Location $projectRoot
    try {
        $targets = & pyocd list --targets 2>&1 | Out-String
        if ($targets -match 'py32f003x8') {
            Write-Host "[OK]      pyOCD knows the target py32f003x8" -ForegroundColor Green
        } else {
            Write-Host "[WARN]    pyOCD does not list py32f003x8." -ForegroundColor Yellow
            Write-Host "          Check that pyocd.yaml exists and points at Tools\packs\Puya.PY32F0xx_DFP.1.2.0.pack."
            $script:problems++
        }

        Write-Host ""
        Write-Host "--- Connected debug probes ---"
        & pyocd list 2>&1 | Out-Host
    }
    finally {
        Pop-Location
    }
}

# Last build output
$elf = Join-Path $projectRoot 'Build\Py32f003_Ultrasonic.elf'
Write-Host ""
if (Test-Path -LiteralPath $elf) {
    $item = Get-Item -LiteralPath $elf
    Write-Host ("[OK]      Firmware built: {0} ({1:N1} KB, {2})" -f $item.FullName, ($item.Length / 1KB), $item.LastWriteTime) -ForegroundColor Green
} else {
    Write-Host "[INFO]    No Build\Py32f003_Ultrasonic.elf yet - run the Build task first." -ForegroundColor Cyan
}

Write-Host ""
if ($script:problems -eq 0) {
    Write-Host "Environment looks good. Build with 'make all', flash with 'make flash'." -ForegroundColor Green
} else {
    Write-Host ("{0} problem(s) found. See SETUP_WINDOWS.md for the installation steps." -f $script:problems) -ForegroundColor Yellow
}
