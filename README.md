# Brinco Modules

Firmware for the Brinco hardware modules that plug into the custom main board
(a Quarky-style carrier).

| Project | MCU | Core | Flash / RAM | Purpose |
|---------|-----|------|-------------|---------|
| `Py32f003_Ultrasonic` | **PY32F003F18P6TU** | Cortex-M0+ | 64 KB / 8 KB | ultrasonic distance sensor, I2C slave |
| `Py32f_ProjectTemplate` | **PY32F040C1BT6** | Cortex-M0+ @ 72 MHz | 128 KB / 16 KB | template for new PY32F040 modules |

Both projects are **standalone**: open the project folder itself in VS Code (not
the repository root), and everything (toolchain, Makefile, pyOCD pack, SVD,
debug configuration) is already inside it.

## Toolchain (Windows)

| Tool | Version used |
|------|--------------|
| VS Code + C/C++, Cortex-Debug, Makefile Tools | latest |
| Arm GNU Toolchain (`arm-none-eabi-gcc`) | 14.3.rel1 or 15.2.rel1 (mingw-w64 i686) |
| GNU Make for Windows | ezwinports 4.4.1 |
| Python + pyOCD | pyOCD 0.46.x |
| Puya `PY32F0xx_DFP` | 1.2.0 (committed in each project) |

Step-by-step installation, build, flash and debug instructions:

* **PY32F003 ultrasonic module** -> [`Py32f003_Ultrasonic/SETUP_WINDOWS.md`](Py32f003_Ultrasonic/SETUP_WINDOWS.md)
* **PY32F040 template** -> [`Py32f_ProjectTemplate/SETUP_WINDOWS.md`](Py32f_ProjectTemplate/SETUP_WINDOWS.md)

## Quick start

```powershell
cd Py32f003_Ultrasonic      # or Py32f_ProjectTemplate
make all                    # Build\<Project>.elf/.hex/.bin/.lst
make flash                  # erase + program via pyOCD
```

In VS Code: `Ctrl+Shift+B` builds, `F5` flashes and starts a pyOCD debug session
with the SVD peripheral view.

## Layout of each project

```text
.vscode/            build/debug tasks, IntelliSense, extension list
Libraries/          CMSIS, HAL drivers, FreeRTOS, E-Paper, linker scripts
Misc/               SVD files for the debugger, J-Link scripts
Tools/              pyOCD pack, setup/check PowerShell helpers, OpenOCD (Puya build)
User/               application code (main.c, MSP, interrupt handlers, HAL config)
Makefile, rules.mk  build system (run from the project folder)
pyocd.yaml          pyOCD configuration (bundled device family pack, SWD clock)
```

## Creating a new module

1. Copy `Py32f_ProjectTemplate` to a new folder and rename it.
2. Open the folder in VS Code and write your application in `User/Src`.
3. `make all` produces `Build\<FolderName>.elf` automatically - the Makefile
   derives `PROJECT` from the folder name, and `launch.json` uses
   `${workspaceFolderBasename}`, so debugging works without edits.
4. Change `MCU_TYPE` (build) and `PYOCD_TARGET` (flash/debug) in the `Makefile`
   if the new module uses a different PY32F0 variant. Both projects carry the
   full Puya device family pack, so all PY32F0 parts are covered.
