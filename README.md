# Brinco Modules

**Project handover and module status:** start with the
[module development index](docs/modules/README.md). It links one reference
page per board/module, including implemented behavior and unresolved decisions.

Firmware for the Brinco hardware modules that plug into the custom main board
(a Quarky-style carrier).

**New to this repository?** Follow [Set up VS Code for the first time](#set-up-vs-code-for-the-first-time)
below, then read [Day-to-day workflow](#day-to-day-workflow). Everything is
already configured inside each project - you do not need to create a project,
write a Makefile or install a device pack.

---

## Firmware code documentation

**Clock policy:** always use the fitted external crystal. PY32F003F18P6TU
runs directly from 32 MHz HSE; PY32F040C1BT6 uses 24 MHz HSE with PLL x3
for 72 MHz. Keep HSE_VALUE consistent in HAL and CMSIS, configure flash wait
states, and recalculate peripheral clocks. Crystal startup failure is an error,
not a reason to silently fall back to HSI. The legacy I2C ultrasonic project
is retained unchanged as historical firmware.

All new module application code should include file-level purpose and hardware
mapping, function comments describing inputs/outputs, units, errors and calling
context, and field/constant comments for protocol and configuration values.
Explain timing assumptions, ISR/foreground data sharing, calibration, state
transitions and recovery where they affect behavior. Keep comments accurate
when changing code; explain design decisions rather than repeating statements.
Apply this convention to each new module copied from the MCU templates.

## Projects

[`py32f003_IR_Array_Module`](py32f003_IR_Array_Module/README.md) supports the
5-sensor IR array and the cut 3-sensor variant. Its UART `READ` command returns
one byte containing the sensor states, sensor-count flag, and validity flag.

The current ultrasonic UART firmware is in
[`py32f003_Ultrasonic_Module`](py32f003_Ultrasonic_Module/README.md).
`Py32f003_Ultrasonic` is the legacy I2C version. Current module hardware uses
UART for sensor modules; TFT and LED matrix use direct ESP32 SPI, and the
I2S board uses ESP32 audio interfaces. For a new module, copy the matching MCU
template and name the folder `py32f0xx_<ModuleName>_Module`.

| Project | MCU | Core | Flash / RAM | Purpose |
|---------|-----|------|-------------|---------|
| `py32f003_Color_Sensor_Module` | **PY32F003F18P6TU** | Cortex-M0+ @ 32 MHz | 64 KB / 8 KB | VEML6040 RGBW and illumination, USART2 at 115200 baud; board rework required for missing I2C pull-ups and level shifting |
| `py32f003_Sound_Sensor_Module` | **PY32F003F18P6TU** | Cortex-M0+ @ 32 MHz | 64 KB / 8 KB | Relative sound level and comparator detection, USART2 at 115200 baud |
| `py32f040_LED_Matrix_Module` | **PY32F040C1BT6** | Cortex-M0+ @ 72 MHz | 128 KB / 16 KB | 48 RGB LEDs, direct ESP32 SPI, TIM15/DMA output |
| `py32f003_Touch_Sensor_Module` | **PY32F003F18P6TU** | Cortex-M0+ @ 32 MHz | 64 KB / 8 KB | XMXW 223 touch state, USART2 at 115200 baud |
| `py32f003_IR_Array_Module` | **PY32F003F18P6TU** | Cortex-M0+ @ 32 MHz | 64 KB / 8 KB | 5/3-sensor line-following IR array, USART1 at 115200 baud |
| `py32f003_Ultrasonic_Module` | **PY32F003F18P6TU** | Cortex-M0+ @ 32 MHz | 64 KB / 8 KB | current ultrasonic UART module, USART2 at 115200 baud |
| `Py32f003_Ultrasonic` | **PY32F003F18P6TU** | Cortex-M0+ @ 24 MHz | 64 KB / 8 KB | ultrasonic distance sensor, I2C slave at `0x08` |
| `Py32f003_ProjectTemplate` | **PY32F003F18P6TU** | Cortex-M0+ @ 32 MHz | 64 KB / 8 KB | **template** for new PY32F003 modules |
| `Py32f040_ProjectTemplate` | **PY32F040C1BT6** | Cortex-M0+ @ 72 MHz | 128 KB / 16 KB | **template** for new PY32F040 modules |

### Which template should I copy?

| Your board's MCU | Copy this folder |
|------------------|------------------|
| PY32F003F18P6TU (TSSOP-20, 64 KB / 8 KB) | `Py32f003_ProjectTemplate` |
| PY32F040C1BT6 (LQFP-48, 128 KB / 16 KB) | `Py32f040_ProjectTemplate` |

Both templates are minimal but complete: clock setup, empty main loop, HAL
configuration, debug configuration, bundled device family pack. You only write
`User/Src/main.c`.

Every project is **standalone**: it contains its own `.vscode` configuration,
Makefile, bundled Puya device family pack, SVD and `pyocd.yaml`. Projects can be
opened, built, flashed and debugged one at a time, or together through
`Brinco_Modules.code-workspace`.

Repository layout:

```text
Brinco_Modules/
├─ README.md                        this file
├─ Brinco_Modules.code-workspace    multi-root VS Code workspace (all projects)
├─ Py32f003_Ultrasonic/             finished PY32F003 module (worked example)
│   ├─ .vscode/                     build/debug tasks, IntelliSense, extensions
│   ├─ User/                        application code  <- you work here
│   ├─ Libraries/                   CMSIS, HAL drivers, linker scripts
│   ├─ Misc/                        SVD files for the debugger, J-Link scripts
│   ├─ Tools/                       device family pack, PowerShell helpers, OpenOCD
│   ├─ Makefile, rules.mk           build system
│   ├─ pyocd.yaml                   pyOCD config (bundled pack, SWD clock)
│   └─ SETUP_WINDOWS.md             detailed setup guide
├─ Py32f003_ProjectTemplate/        PY32F003 template (same layout)
│   └─ SETUP_WINDOWS.md
└─ Py32f040_ProjectTemplate/        PY32F040 template (same layout)
    └─ SETUP_WINDOWS.md
```

All three folders share the same structure, so anything you learn in one applies
to the others.

---

## Set up VS Code for the first time

Target environment: **Windows 10/11**. No administrator rights are needed for
anything except the Arm toolchain and probe driver installers.

### 0. Checklist

| # | Install | Needed for | Quick check |
|---|---------|-----------|-------------|
| 1 | [VS Code](https://code.visualstudio.com/) | everything | `code --version` |
| 2 | [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) | compiling | `arm-none-eabi-gcc --version` |
| 3 | [GNU Make 4.4.1](https://sourceforge.net/projects/ezwinports/files/) | running the build | `make --version` |
| 4 | [Python 3](https://www.python.org/downloads/windows/) + pyOCD | flashing, debugging | `py -m pyocd --version` |
| 5 | **ST-Link V2** driver (STSW-LINK009), or DAPLink / J-Link | talking to the board | `py -m pyocd list` |
| 6 | VS Code extensions | IntelliSense, debugging | Extensions view |

The detailed, copy-pasteable instructions for each item (with the exact
installer names, PATH entries and driver links) are in:

* [`Py32f003_ProjectTemplate/SETUP_WINDOWS.md`](Py32f003_ProjectTemplate/SETUP_WINDOWS.md)
* [`Py32f040_ProjectTemplate/SETUP_WINDOWS.md`](Py32f040_ProjectTemplate/SETUP_WINDOWS.md)
* [`Py32f003_Ultrasonic/SETUP_WINDOWS.md`](Py32f003_Ultrasonic/SETUP_WINDOWS.md)

They are equivalent; read the one for the project you start with (the template
guides also cover the ST-Link wiring and the OpenOCD fallback). Condensed
version:

```powershell
# 2. Arm GNU Toolchain: download the Windows (mingw-w64-i686) arm-none-eabi
#    installer and keep "Add path to environment variable" checked.
#    Default install paths:
#      C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.3 Rel1\bin
#      C:\Program Files\Arm GNU Toolchain arm-none-eabi\15.2 Rel1\bin

# 3. GNU Make
winget install --id ezwinports.make -e

# 4. Python
winget install --id Python.Python.3.12 -e
py -m pip install --user -U pyocd

# 6. VS Code extensions
code --install-extension ms-vscode.cpptools
code --install-extension marus25.cortex-debug
code --install-extension ms-vscode.makefile-tools
```

> **`pyocd` is not recognized?** A `pip install --user` puts `pyocd.exe` in a
> version-specific folder that Windows does not add to PATH, e.g.
> `C:\Users\<you>\AppData\Roaming\Python\Python314\Scripts`. The projects work
> anyway (`py -m pyocd`, and `.vscode/settings.json` already points
> Cortex-Debug at that folder). To get a plain `pyocd` command:
>
> ```powershell
> $scripts = & py -c "import sysconfig; print(sysconfig.get_path('scripts','nt_user'))"
> $userPath = [Environment]::GetEnvironmentVariable('Path','User')
> if (($userPath -split ';') -notcontains $scripts) {
>     [Environment]::SetEnvironmentVariable('Path', $userPath.TrimEnd(';') + ';' + $scripts, 'User')
> }
> ```
>
> Then close **all** terminals and VS Code and reopen them.

### 1. Clone the repository

```powershell
git clone https://github.com/ahmedsamy207/Brinco_Modules.git
cd Brinco_Modules
```

Line endings: the build works with both CRLF and LF, but keep `rules.mk` and
`Makefile` untouched by editors that convert line endings - if a build suddenly
fails with strange `cmd.exe` errors, check `git diff` for a whole-file rewrite.

### 2. Open the workspace

Two ways, both supported. **Start with option A**; use option B once you are
comfortable.

#### A. One project at a time (recommended)

```powershell
code Py32f003_Ultrasonic          # or: code Py32f003_ProjectTemplate
                                  # or: code Py32f040_ProjectTemplate
```

Opening the **project folder itself** is important: the build and debug paths
are relative to it. This is the mode used by `F5` and `Ctrl+Shift+B`.

#### B. Both projects in one window

```powershell
code Brinco_Modules.code-workspace
```

Gives you the repo docs, both projects and git in one Explorer. Each folder
keeps its own IntelliSense config and launch configurations; pick the
configuration from the **Run and Debug** dropdown (they are grouped per folder
as `PY32F003 - Ultrasonic: Debug (pyOCD)` etc.).

> When running a task in this mode, VS Code asks which folder's task to use if
> the labels collide (they are the same in both projects). Choose the folder you
> are working on. Building/debugging from option A avoids the question entirely.

### 3. What the `.vscode` folder does

Every project ships the same setup - you should not need to edit it:

| File | Contents |
|------|----------|
| `c_cpp_properties.json` | ARM GCC IntelliSense: `arm-none-eabi-gcc`, `gcc-arm` mode, `-mcpu=cortex-m0plus`, the project's defines (`PY32F003x8` or `PY32F040xB` + `USE_HAL_DRIVER`) and include paths |
| `settings.json` | `cortex-debug.pyocdPath.windows` (points at the per-user pyOCD), file associations, excludes for `Build`/`Temp` |
| `launch.json` | **Debug (pyOCD)**, **Attach (pyOCD)**, **Debug (OpenOCD, ST-Link)**, **Attach (OpenOCD, ST-Link)** |
| `tasks.json` | Build, Clean, Flash, Erase chip, Set up pyOCD, Register Puya DFP, Check toolchain |
| `extensions.json` | recommended extensions (C/C++, Cortex-Debug, Makefile Tools) |

The debugger uses the bundled SVD (`Misc/SVD/*.svd`), so **Cortex Peripherals**
shows the real names of every timer, USART, I2C, ADC register while you debug.

### 4. Verify the setup

In VS Code: `Ctrl+Shift+P` -> **Tasks: Run Task** -> **Check toolchain**
(or run it directly):

```powershell
cd Py32f003_Ultrasonic
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\check-env.ps1
```

Expected output: `[OK]` for the compiler, make, Python, pyOCD, the device pack
and the target, then the list of connected probes. Anything `[MISSING]` means
that row of the checklist is not installed or not on PATH.

Check the pyOCD target by hand:

```powershell
py -m pyocd list --targets | Select-String py32f
# py32f003x8 ... (ultrasonic)   py32f040cxb / py32f040xb ... (template)
```

### 5. First build

```powershell
cd Py32f003_Ultrasonic
make clean
make all
```

or press `Ctrl+Shift+B` in VS Code. Output lands in `Build\`:

```text
<Project>.elf   debugger / pyOCD
<Project>.hex   hex dump for other tools
<Project>.bin   raw image
<Project>.map   link map (what ends up in flash and why)
<Project>.lst   disassembly with source
```

`Temp\`, `Build\` and `.venv\` are ignored by git - never commit them.

---

## Flashing with the ST-Link V2

The modules are programmed with an **ST-Link V2**. Everything for it is already
configured; there are two independent paths and you can use whichever works:

| Path | Command | VS Code |
|------|---------|---------|
| pyOCD (default) | `make flash` | `F5` -> *Debug (pyOCD)* |
| OpenOCD (Puya build) | `make flash-openocd` | `F5` -> *Debug (OpenOCD, ST-Link)* |

Try `make flash` first. If pyOCD reports that it cannot connect, use the
OpenOCD path - it is Puya's own OpenOCD build with their PY32 flash driver.

### Driver

Install **STSW-LINK009** (<https://www.st.com/en/development-tools/stsw-link009.html>).
It is a signed WinUSB driver, so both pyOCD (libusb) and OpenOCD can use it -
**no Zadig needed**.

### Wiring

| ST-Link V2 | Target |
|------------|--------|
| `SWDIO` | `SWDIO` / `DIO` |
| `SWCLK` | `SWCLK` / `CLK` |
| `GND` | `GND` |
| `3.3V` | `3V3`, only if the board is not already powered |
| `NRST` | `NRST`, optional; helps when the chip is in a low-power mode |

Never connect the ST-Link's 5 V pin to the target. Keep the wires short - long
jumper wires are the most common cause of connection failures.

### Why there are two tools

Recent **genuine ST** firmware only allows STM32 targets and refuses a PY32
part. Cheap ST-Link V2 clones (and older firmware) are not restricted, so
pyOCD usually works. When it does not, OpenOCD with Puya's flash driver does:

```powershell
make flash-openocd     # program + verify + reset
make erase-openocd     # full chip erase
```

Both use `Tools/openocd-puya/stlink-py32f003.cfg` (or `…-py32f040.cfg`), which sets
the correct flash size and selects `dapdirect_swd`. The vendor script keeps SWD
at a conservative 100 kHz - slow, but tolerant of wiring and still only a few
seconds for these image sizes.

---

## Day-to-day workflow

| Action | VS Code | Command line |
|--------|---------|--------------|
| Build | `Ctrl+Shift+B` | `make all` |
| Clean | `Ctrl+Shift+P` -> Tasks: Run Task -> Clean | `make clean` |
| Flash | Tasks: Run Task -> **Flash (pyOCD)** | `make flash` |
| Erase chip | Tasks: Run Task -> **Erase chip (pyOCD)** | `make erase` |
| Flash via ST-Link with OpenOCD | Tasks: Run Task -> **Flash (OpenOCD / ST-Link)** | `make flash-openocd` |
| Erase via ST-Link with OpenOCD | Tasks: Run Task -> **Erase chip (OpenOCD / ST-Link)** | `make erase-openocd` |
| Debug | `F5` (**Debug (pyOCD)**) - builds and flashes first | - |
| Attach to running target | Run and Debug -> **Attach (pyOCD)** | - |
| See the build command | - | `make -n all` |
| Full command output | - | `make V=1 all` |
| Which sources/includes are used | - | `make echo` |

### Make targets and variables

| Target | Effect |
|--------|--------|
| `all` | `.elf`, `.hex`, `.bin`, `.lst` in `Build\` |
| `clean` | removes `Temp\` and `Build\` |
| `flash` | erase + program over SWD via pyOCD |
| `erase` | erase the whole chip (pyOCD) |
| `flash-openocd` | program + verify + reset through OpenOCD and the ST-Link |
| `erase-openocd` | full chip erase through OpenOCD |
| `echo` | prints the resolved source file, include and object lists |

| Variable | Default | Meaning |
|----------|---------|---------|
| `PROJECT` | **folder name** | output file base name (`Build\<PROJECT>.elf`) |
| `MCU_TYPE` | `PY32F003x8` / `PY32F040xB` | selects startup file, system file, HAL and linker script |
| `PYOCD_TARGET` | `py32f003x8` / `PY32F040CxB` | CMSIS-Pack target used for flash/debug |
| `PYOCD_EXE` | `.venv\Scripts\pyocd.exe`, else `py -m pyocd` | how pyOCD is invoked |
| `PYOCD_PACK` | `Tools/packs/Puya.PY32F0xx_DFP.1.2.0.pack` | device family pack |
| `FLASH_PROGRM` | `pyocd` | or `jlink`, or `openocd` (`make flash FLASH_PROGRM=openocd`) |
| `OPENOCD_EXE` | `Tools/openocd-puya/bin/openocd.exe` | bundled Puya OpenOCD 0.12 |
| `OPENOCD_CFG` | `Tools/openocd-puya/stlink-py32f003.cfg` / `…-py32f040.cfg` | ST-Link session (flash size + `dapdirect_swd`) |
| `ARM_TOOLCHAIN` | empty | set to the toolchain `bin` folder if it is not on PATH |
| `V` | `0` | `1` prints every command |

Examples:

```powershell
make ARM_TOOLCHAIN="C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.3 Rel1/bin" all
make flash PYOCD_EXE="C:\Users\Ahmed Samy\AppData\Roaming\Python\Python314\Scripts\pyocd.exe"
make PROJECT=MyBoard all
```

Build **serially** (`make`, not `make -j`): several recipes create the same
`Temp\...` folders.

---

## Where to write code

```text
User/Inc/main.h                  board-level definitions, handles
User/Inc/py32f0xx_hal_conf.h     enable/disable HAL modules
User/Src/main.c                  main(), peripheral init, application loop
User/Src/py32f0xx_hal_msp.c      low-level pin/clock setup for each peripheral
User/Src/py32f0xx_it.c           interrupt handlers
```

Typical flow for a new peripheral: enable its `HAL_*_MODULE_ENABLED` in
`User/Inc/py32f0xx_hal_conf.h` (all HAL sources are already compiled, so no
Makefile edit is needed), add the `HAL_xxx_MspInit()` code in
`py32f0xx_hal_msp.c`, then use the HAL from `main.c`.

The templates start you off with `HAL_Init()`, a working `SystemClock_Config()`
(32 MHz HSE on PY32F003, 72 MHz via 24 MHz HSE and PLL x3 on PY32F040) and an empty main loop.

`Py32f003_Ultrasonic` is a complete worked example of the same skeleton:
TIM1 input capture, I2C1 slave at address `0x08` (command `0x35` returns the
distance), exponential smoothing with alpha `0.25`, HSI at 24 MHz. Comparing it
with `Py32f003_ProjectTemplate` shows exactly what an application adds.

---

## Creating a new module

1. Copy the template for your MCU (see [Which template should I copy?](#which-template-should-i-copy))
   to a new folder inside the repository and rename it, e.g. `Brinco_LED_Module`:

   ```powershell
   Copy-Item -Recurse Py32f003_ProjectTemplate Brinco_LED_Module
   ```

2. Open the new folder in VS Code (`code Brinco_LED_Module`).
3. Write the application in `User/Src/main.c` (and the MSP/IRQ files as needed).
4. `make all` produces `Build\<FolderName>.elf` automatically: the Makefile
   derives `PROJECT` from the folder name and `launch.json` uses
   `${workspaceFolderBasename}`, so `F5` works with no edits.
5. If the module uses a different PY32F0 part, change `MCU_TYPE` (build) and
   `PYOCD_TARGET` (flash/debug) at the top of the `Makefile`, and update
   `svdFile` in `.vscode/launch.json`. Every part of the family is covered by the
   bundled pack.

`Build\` and `Temp\` are ignored by git, but the copy itself is a normal
folder in the repository: commit it, or add it to `.gitignore` if it is only a
scratch experiment.

---

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| `make : The term 'make' is not recognized` | GNU Make is not installed or not on PATH - see checklist item 3, then reopen the terminal. |
| `arm-none-eabi-gcc : not recognized` | Toolchain not on PATH; reinstall with the "Add to PATH" option or pass `ARM_TOOLCHAIN=...`. |
| `pyocd : not recognized` | See the note in [section 0](#0-checklist); the projects do not require it. |
| `make` fails with `The system cannot find the path specified` | You are running it in Git Bash. Use PowerShell or `cmd.exe` (the Makefile pins `cmd.exe` itself), or delete a stale `Temp\` folder. |
| `pyocd: unrecognized target type` | Run pyOCD from the project folder (so `pyocd.yaml` is found), or pass `--pack Tools\packs\Puya.PY32F0xx_DFP.1.2.0.pack`. |
| `pyocd list` shows no probe | Install the ST-Link driver (STSW-LINK009), try another cable/port, avoid USB hubs, check target power. In Device Manager the probe must appear without a warning triangle. |
| pyOCD sees the probe but cannot connect | Check SWDIO/SWCLK/GND/3V3 wiring first, then use `make flash-openocd`. Recent **genuine** ST-Link firmware only allows STM32 parts; clones are not affected. |
| OpenOCD: `Error: open failed` | The probe driver is not libusb-compatible (install STSW-LINK009), or another program holds the probe (STM32CubeProgrammer, ST-Link Utility, a second debug session). |
| OpenOCD: no DAP / `unable to find a matching transport` | ST-Link firmware older than V2J24; upgrade with STSW-LINK007 or use a DAPLink. |
| OpenOCD: `init mode failed` | Target not powered, SWDIO and SWCLK swapped, or the chip is in a low-power mode - connect NRST and retry. |
| `F5` stops with "GDB executable not found" | `arm-none-eabi-gdb` is not on PATH (comes with the Arm toolchain). |
| IntelliSense shows red squiggles on `py32f0xx.h` | Pick `PY32F003 (arm-none-eabi)` / `PY32F040 (arm-none-eabi)` in the C/C++ status bar item; make sure `arm-none-eabi-gcc` is on PATH. |
| Wrong SVD/peripherals in the debug session | Check `svdFile` in `launch.json` - `py32f003xx.svd` for PY32F003 projects, `py32f040cxx.svd` for PY32F040. |
| After renaming a project folder, `F5` looks in the old path | `PROJECT` follows the folder name; run `make clean` to drop the old `Build\<OldName>.elf`. |
| `region RAM overflowed` at link | Out of RAM (8 KB on PY32F003, 16 KB on PY32F040). Check buffers, `printf` usage and large stack arrays. |

Per-project guides have longer troubleshooting tables:
[PY32F003 template](Py32f003_ProjectTemplate/SETUP_WINDOWS.md#11-troubleshooting) ·
[PY32F040 template](Py32f040_ProjectTemplate/SETUP_WINDOWS.md#11-troubleshooting) ·
[PY32F003 ultrasonic](Py32f003_Ultrasonic/SETUP_WINDOWS.md#11-troubleshooting).

---

## Toolchain versions used

| Tool | Version |
|------|---------|
| Arm GNU Toolchain (`arm-none-eabi-gcc`) | 14.3.rel1 or 15.2.rel1 (mingw-w64 i686) |
| GNU Make for Windows | ezwinports 4.4.1 |
| Python | 3.12+ (3.14 tested) |
| pyOCD | 0.46.x |
| OpenOCD (bundled, ST-Link) | Puya build 0.12.0 with the `puyaf0x` flash driver |
| Puya `PY32F0xx_DFP` | 1.2.0, committed in `Tools/packs/` of each project |

The device family pack is in the repository on purpose: no `pyocd pack install`,
no network access and no global pyOCD configuration are needed to build, flash
or debug.
