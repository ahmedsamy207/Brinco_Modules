#Requires -Version 5.1
<#
    Register the Puya PY32F0xx device family pack with pyOCD.

    This is OPTIONAL for this project: pyocd.yaml and the Makefile already pass
    the bundled .pack file explicitly. Installing it into pyOCD's pack cache
    additionally makes 'pyocd list --targets' and manual pyocd commands work
    from any folder, without --pack.

    Usage (from the project root):
        powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\install-py32-pack.ps1
#>

param(
    [string]$PackPath = (Join-Path $PSScriptRoot 'packs\Puya.PY32F0xx_DFP.1.2.0.pack')
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $PackPath)) {
    throw "Pack file not found: $PackPath"
}

# Prefer the project-local venv, then PATH, then the per-user Scripts folder.
$projectRoot = Split-Path -Parent $PSScriptRoot
$venvScripts = Join-Path $projectRoot '.venv\Scripts'
$venvPyocd = Join-Path $venvScripts 'pyocd.exe'
$venvPackManager = Join-Path $venvScripts 'pack-manager.exe'

# %APPDATA%\Python\PythonXXX\Scripts - resolved without knowing the version
$userScripts = $null
if (Get-Command py -ErrorAction SilentlyContinue) {
    $candidate = & py -c "import sysconfig; print(sysconfig.get_path('scripts', 'nt_user'))" 2>$null
    if ($candidate) { $userScripts = ([string]$candidate).Trim() }
}
$userPackManager = if ($userScripts) { Join-Path $userScripts 'pack-manager.exe' } else { $null }
$userPyocd = if ($userScripts) { Join-Path $userScripts 'pyocd.exe' } else { $null }

if (Test-Path -LiteralPath $venvPyocd) {
    $pyocdExe = $venvPyocd
} elseif (Get-Command pyocd -ErrorAction SilentlyContinue) {
    $pyocdExe = 'pyocd'
} elseif ($userPyocd -and (Test-Path -LiteralPath $userPyocd)) {
    $pyocdExe = $userPyocd
} else {
    throw "pyocd was not found. Run Tools\setup-pyocd.ps1 or: py -m pip install --user -U pyocd"
}

if (Test-Path -LiteralPath $venvPackManager) {
    $packManagerExe = $venvPackManager
} elseif (Get-Command pack-manager -ErrorAction SilentlyContinue) {
    $packManagerExe = 'pack-manager'
} elseif ($userPackManager -and (Test-Path -LiteralPath $userPackManager)) {
    $packManagerExe = $userPackManager
} else {
    throw "pack-manager was not found. It is installed together with pyOCD."
}

Write-Host "pyOCD:        $pyocdExe"
Write-Host "pack-manager: $packManagerExe"

$cacheDir = [string](& $packManagerExe print-cache-dir | Select-Object -First 1)
$cacheDir = $cacheDir.Trim()
if (-not $cacheDir) {
    throw "Could not determine the cmsis-pack-manager cache directory."
}

Write-Host "pyOCD pack cache: $cacheDir"

# The pack is a ZIP container; extract the .pdsc descriptor from it.
$tmpDir = Join-Path $env:TEMP ('py32-dfp-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmpDir | Out-Null

try {
    $zipPath = Join-Path $tmpDir 'dfp.zip'
    Copy-Item -LiteralPath $PackPath -Destination $zipPath -Force

    $unpackDir = Join-Path $tmpDir 'unpacked'
    Expand-Archive -LiteralPath $zipPath -DestinationPath $unpackDir -Force

    $pdsc = Get-ChildItem -Path $unpackDir -Filter '*.pdsc' -File | Select-Object -First 1
    if (-not $pdsc) {
        throw "No .pdsc descriptor found inside $PackPath"
    }

    # cmsis-pack-manager expects:
    #   <cache>\Puya.PY32F0xx_DFP.1.2.0.pdsc
    #   <cache>\Puya\PY32F0xx_DFP\1.2.0.pack
    $packDir = Join-Path $cacheDir 'Puya\PY32F0xx_DFP'
    New-Item -ItemType Directory -Path $packDir -Force | Out-Null

    $packTarget = Join-Path $packDir '1.2.0.pack'
    $pdscTarget = Join-Path $cacheDir 'Puya.PY32F0xx_DFP.1.2.0.pdsc'

    Copy-Item -LiteralPath $PackPath -Destination $packTarget -Force
    Copy-Item -LiteralPath $pdsc.FullName -Destination $pdscTarget -Force

    & $packManagerExe add-packs $pdscTarget
    if ($LASTEXITCODE -ne 0) {
        throw "pack-manager add-packs failed with exit code $LASTEXITCODE"
    }
}
finally {
    Remove-Item -LiteralPath $tmpDir -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "Installed packs:" -ForegroundColor Cyan
& $pyocdExe pack show

Write-Host ""
Write-Host "Puya targets now visible to pyOCD:" -ForegroundColor Cyan
& $pyocdExe list --targets | Select-String -Pattern 'py32f003', 'py32f040'

Write-Host ""
Write-Host "Done. You can now omit --pack from pyocd commands." -ForegroundColor Green
