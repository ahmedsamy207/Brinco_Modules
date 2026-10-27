#Requires -Version 5.1
<#
    Install pyOCD into a project-local Python virtual environment.

    This avoids every PATH problem: the venv lives in <project>\.venv and the
    Makefile / VS Code tasks pick <project>\.venv\Scripts\pyocd.exe
    automatically when it exists. Nothing is installed globally and no
    administrator rights are needed.

    Usage (from the project folder):
        powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\setup-pyocd.ps1

    Options:
        -AddToPath   Also append <project>\.venv\Scripts to your *user* PATH so
                     the bare 'pyocd' command works in new terminals.
        -Force       Recreate the venv from scratch.
#>

[CmdletBinding()]
param(
    [string]$VenvPath = (Join-Path (Split-Path -Parent $PSScriptRoot) '.venv'),
    [switch]$AddToPath,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
Write-Host "Project: $projectRoot" -ForegroundColor Cyan
Write-Host "Venv:    $VenvPath" -ForegroundColor Cyan

# A per-user install may already work; this script is only needed to avoid
# touching PATH. Tell the user instead of silently installing a second copy.
if (Get-Command py -ErrorAction SilentlyContinue) {
    $existing = & py -m pyocd --version 2>$null
    if ($existing) {
        $scripts = & py -c "import sysconfig; print(sysconfig.get_path('scripts', 'nt_user'))" 2>$null
        Write-Host ("pyOCD {0} is already installed for the default Python." -f (($existing | Select-Object -First 1))) -ForegroundColor Yellow
        if ($scripts) {
            Write-Host ("Existing executable folder: {0}" -f ([string]$scripts).Trim()) -ForegroundColor DarkGray
        }
        Write-Host "You only need this script if you want the project to be independent of PATH." -ForegroundColor DarkGray
        Write-Host ""
    }
}

# ---------------------------------------------------------------- Python -----
$pyExe = $null
$pyArgs = @()

if (Get-Command py -ErrorAction SilentlyContinue) {
    $pyExe = 'py'
    $pyArgs = @('-3')
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    $pyExe = 'python'
} else {
    throw "Python was not found. Install it first: winget install --id Python.Python.3.12 -e"
}

$version = & $pyExe @pyArgs --version 2>&1
Write-Host "Python:  $version"

# ------------------------------------------------------------------ Venv -----
$venvPython = Join-Path $VenvPath 'Scripts\python.exe'
$pyocdExe = Join-Path $VenvPath 'Scripts\pyocd.exe'

if ($Force -and (Test-Path -LiteralPath $VenvPath)) {
    Write-Host "Removing existing venv (-Force)..." -ForegroundColor Yellow
    Remove-Item -LiteralPath $VenvPath -Recurse -Force
}

if (-not (Test-Path -LiteralPath $venvPython)) {
    Write-Host "Creating virtual environment..." -ForegroundColor Cyan
    & $pyExe @pyArgs -m venv $VenvPath
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to create the virtual environment (exit code $LASTEXITCODE)."
    }
} else {
    Write-Host "Reusing existing virtual environment." -ForegroundColor Cyan
}

# ------------------------------------------------------------------ pyOCD ----
Write-Host "Installing / updating pyOCD..." -ForegroundColor Cyan
& $venvPython -m pip install --disable-pip-version-check --quiet --upgrade pip
if ($LASTEXITCODE -ne 0) {
    Write-Warning "pip self-upgrade failed; continuing anyway."
}

& $venvPython -m pip install --disable-pip-version-check --upgrade pyocd
if ($LASTEXITCODE -ne 0) {
    throw "pip install pyocd failed (exit code $LASTEXITCODE)."
}

if (-not (Test-Path -LiteralPath $pyocdExe)) {
    throw "pyocd.exe was not created in $VenvPath\Scripts"
}

Write-Host ""
$pyocdVersion = & $pyocdExe --version
Write-Host "pyOCD:   $pyocdVersion" -ForegroundColor Green

# ------------------------------------------------------- target visibility ---
Push-Location $projectRoot
try {
    $targets = & $pyocdExe list --targets 2>&1 | Out-String
    if ($targets -match 'py32f040cxb') {
        Write-Host "Target:  py32f040cxb is available (loaded from pyocd.yaml)" -ForegroundColor Green
    } else {
        Write-Warning "py32f040cxb is not listed. Check pyocd.yaml and Tools\packs\Puya.PY32F0xx_DFP.1.2.0.pack."
    }

    Write-Host ""
    Write-Host "Connected debug probes:" -ForegroundColor Cyan
    & $pyocdExe list 2>&1 | Out-Host
}
finally {
    Pop-Location
}

# --------------------------------------------------------------- PATH --------
if ($AddToPath) {
    $scripts = Join-Path $VenvPath 'Scripts'
    $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    if (-not $userPath) { $userPath = '' }
    if ($userPath -split ';' -notcontains $scripts) {
        [Environment]::SetEnvironmentVariable('Path', ($userPath.TrimEnd(';') + ';' + $scripts), 'User')
        Write-Host "Added $scripts to your user PATH (open a new terminal to use it)." -ForegroundColor Yellow
    } else {
        Write-Host "$scripts is already on your user PATH." -ForegroundColor Green
    }
    $env:Path = $env:Path + ';' + $scripts
}

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host ""
Write-Host "Use it in this terminal:" -ForegroundColor Cyan
Write-Host "    .\.venv\Scripts\Activate.ps1"
Write-Host "    pyocd list --targets"
Write-Host ""
Write-Host "Or just let the Makefile use it (it auto-detects .venv):" -ForegroundColor Cyan
Write-Host "    make flash"
Write-Host ""
if (-not $AddToPath) {
    Write-Host "To make the bare 'pyocd' command work in every new terminal, re-run with -AddToPath," -ForegroundColor Yellow
    Write-Host "or add this folder to PATH manually:" -ForegroundColor Yellow
    Write-Host "    $(Join-Path $VenvPath 'Scripts')" -ForegroundColor Yellow
    Write-Host ""
}
Write-Host "For VS Code debugging, either use -AddToPath or add this to your workspace settings:" -ForegroundColor Cyan
Write-Host '    "cortex-debug.pyocdPath.windows": "${workspaceFolder}/.venv/Scripts/pyocd.exe"' -ForegroundColor DarkGray
