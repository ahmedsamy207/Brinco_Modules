# Windows + VS Code setup - PY32F040 project template

This is the from-scratch setup for the **Py32f_ProjectTemplate**
(MCU: **PY32F040C1BT6** - Cortex-M0+, up to 72 MHz, 128 KB flash, 16 KB SRAM,
48-pin LQFP).

It is the same toolchain as the `Py32f003_Ultrasonic` project, so if you already
followed that guide, only sections 5-7 are new to you.

> Run commands in a normal **PowerShell** window. The Makefile uses `cmd.exe`
> syntax for creating/removing folders, and `rules.mk` pins the shell to
> `cmd.exe` on Windows, but PowerShell is still the simplest choice.

---

## 0. What gets installed

| # | Tool | Purpose |
|---|------|---------|
| 1 | VS Code + C/C++, Cortex-Debug, Makefile Tools | editor, IntelliSense, debugging |
| 2 | Arm GNU Toolchain (`arm-none-eabi-gcc`, `g++`, `gdb`, `objcopy`, `objdump`) | cross compiler used by the Makefile |
| 3 | GNU Make for Windows | runs `Makefile` / `rules.mk` |
| 4 | Python 3 + **pyOCD** | flashing and the GDB server used by Cortex-Debug |
| 5 | Puya PY32F0xx device family pack | teaches pyOCD the PY32F040 (committed in this repo) |
| 6 | Debug probe driver (ST-Link / DAPLink / J-Link) | talks SWD to the board |

---

## 1. VS Code + extensions

```powershell
winget install --id Microsoft.VisualStudioCode -e

code --install-extension ms-vscode.cpptools
code --install-extension marus25.cortex-debug
code --install-extension ms-vscode.makefile-tools
```

`.vscode/extensions.json` recommends the same list, so a fresh clone offers to
install them.

Open the folder **`Py32f_ProjectTemplate`** itself as the workspace root - build
and debug paths are relative to it.

---

## 2. Arm GNU Toolchain

<https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads> ->
**Windows (mingw-w64-i686) hosted cross toolchains -> AArch32 bare-metal target
(arm-none-eabi)**:

* `arm-gnu-toolchain-14.3.rel1-mingw-w64-i686-arm-none-eabi.exe`, or
* `arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi.msi`

Keep **"Add path to environment variable"** checked, then in a **new** terminal:

```powershell
arm-none-eabi-gcc --version
arm-none-eabi-gdb --version
```

Manual PATH entries, if needed:

```text
C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.3 Rel1\bin
C:\Program Files\Arm GNU Toolchain arm-none-eabi\15.2 Rel1\bin
```

---

## 3. GNU Make for Windows

```powershell
winget install --id ezwinports.make -e
make --version        # GNU Make 4.4.1
```

Alternatives: ezwinports zip from <https://sourceforge.net/projects/ezwinports/files/>,
`choco install make -y`, or MSYS2 (`pacman -S make`).

> Build serially (`make`, not `make -j`): several recipes create the same
> `Temp\...` folders.

---

## 4. Python + pyOCD

```powershell
winget install --id Python.Python.3.12 -e
```

**Already have pyOCD installed?** A `pip install --user` puts `pyocd.exe` in a
version-specific folder that is **not** on PATH, for example:

```text
C:\Users\Ahmed Samy\AppData\Roaming\Python\Python314\Scripts\pyocd.exe
```

This project works with that as-is:

* `make flash` / `make erase` fall back to `py -m pyocd`
* `.vscode/settings.json` sets
  `"cortex-debug.pyocdPath.windows": "${env:APPDATA}/Python/Python314/Scripts/pyocd.exe"`
  (edit `Python314` if you upgrade Python)

To get a plain `pyocd` command in every terminal:

```powershell
$scripts = & py -c "import sysconfig; print(sysconfig.get_path('scripts','nt_user'))"
$userPath = [Environment]::GetEnvironmentVariable('Path','User')
if (($userPath -split ';') -notcontains $scripts) {
    [Environment]::SetEnvironmentVariable('Path', $userPath.TrimEnd(';') + ';' + $scripts, 'User')
}
```

Then close **all** terminals and VS Code and reopen them.

**Fresh install (recommended): project-local venv**

```powershell
cd <path-to-repo>\Py32f_ProjectTemplate
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\setup-pyocd.ps1
```

Creates `.venv\`, installs pyOCD there, verifies that `py32f040cxb` resolves and
lists probes. `.venv\` is ignored by git. The Makefile picks it up
automatically, so nothing is added to PATH. Add `-AddToPath` to also append
`.venv\Scripts` to your user PATH.

pyOCD ships its own `libusb` DLL; nothing else is needed for USB probes.

---

## 5. Debug probe driver

| Probe | What to install |
|-------|-----------------|
| **ST-Link V2 / V2-1 / V3** | STSW-LINK009 from ST: <https://www.st.com/en/development-tools/stsw-link009.html> |
| **CMSIS-DAP / DAPLink** | Nothing - WinUSB/HID on Windows 10/11 is enough |
| **J-Link** | SEGGER J-Link Software Pack |

```powershell
pyocd list          # or: py -m pyocd list
```

> Recent **official** ST-Link firmware only allows STM32 targets. Many clones
> work fine with the PY32; if connecting fails while the probe is listed, try a
> DAPLink/CMSIS-DAP or a J-Link.

---

## 6. pyOCD target for the PY32F040C1BT6

The pack `Tools\packs\Puya.PY32F0xx_DFP.1.2.0.pack` is committed and
`pyocd.yaml` points pyOCD at it, so no download is needed.

Verify from the project folder:

```powershell
py -m pyocd list --targets | Select-String py32f040
```

Expected:

```text
py32f040cxb    Puya    PY32F040CxB    PY32F0 Series, PY32F040C    pack
py32f040xb     Puya    PY32F040xB     PY32F0 Series, PY32F040     pack
```

**Which target?**

| Item | Value | Reason |
|------|-------|--------|
| `MCU_TYPE` (build) | `PY32F040xB` | the CMSIS device header only defines `PY32F040x6/x8/xB`; it selects the 128 KB flash map |
| `PYOCD_TARGET` (flash/debug) | `PY32F040CxB` | `PY32F040C1BT6` is the C (48-pin) package; same memory map and the same `PY32F040xx_128.FLM` flash algorithm |
| SVD | `Misc/SVD/py32f040cxx.svd` | extracted from the pack, matches the C package |

`make PYOCD_TARGET=PY32F040xB flash` also works if you want the generic target.

**Optional:** register the pack in pyOCD's global cache so it works from any
folder and without `--pack`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\install-py32-pack.ps1
```

---

## 7. Build

```powershell
cd <path-to-repo>\Py32f_ProjectTemplate
make clean
make all
```

Output in `Build\`:

```text
Py32f_ProjectTemplate.elf    <- for the debugger
Py32f_ProjectTemplate.hex    <- for pyOCD / other flash tools
Py32f_ProjectTemplate.bin
Py32f_ProjectTemplate.map
Py32f_ProjectTemplate.lst
```

`PROJECT` defaults to **the folder name**, so if you copy this template to
`Brinco_LED_Module`, the outputs become `Build\Brinco_LED_Module.elf`
automatically - which is exactly the path `.vscode/launch.json` uses
(`${workspaceFolderBasename}`). You can still override it:
`make PROJECT=MyBoard all`.

`Temp\` and `Build\` are ignored by git. `Ctrl+Shift+B` runs the default
**Build** task.

---

## 8. Flash

```powershell
make flash        # erase + program
make erase        # erase only
```

Equivalent to:

```powershell
pyocd erase -t py32f040cxb --pack .\Tools\packs\Puya.PY32F0xx_DFP.1.2.0.pack --chip --config .\pyocd.yaml
pyocd load  -t py32f040cxb --pack .\Tools\packs\Puya.PY32F0xx_DFP.1.2.0.pack .\Build\Py32f_ProjectTemplate.elf --config .\pyocd.yaml
```

Or use the VS Code tasks **Flash (pyOCD)** / **Erase chip (pyOCD)**.

J-Link instead of pyOCD:

```powershell
make flash FLASH_PROGRM=jlink JLINKEXE="C:\Program Files\SEGGER\JLink\JLink.exe"
```

`Misc\jlink-command` loads `Build/Py32f_ProjectTemplate.hex`; update that file if
you rename `PROJECT`.

---

## 9. Debug in VS Code

1. Connect the probe and the board.
2. `Ctrl+Shift+D` -> **Debug (pyOCD)** -> `F5`.

It builds, flashes and stops at `main`. `Misc/SVD/py32f040cxx.svd` gives you
TIM1/TIM2, USART, SPI, I2C, ADC, LCD, OPA... registers in the **Cortex
Peripherals** view.

| Configuration | Use |
|---------------|-----|
| **Debug (pyOCD)** | build + flash + debug (default) |
| **Attach (pyOCD)** | attach without flashing |
| **Debug (OpenOCD, ST-Link)** | bundled `Tools\openocd-puya\bin\openocd.exe` |
| **Attach (OpenOCD, ST-Link)** | attach with OpenOCD |

The OpenOCD configs use `Tools\openocd-puya\script\py32f0xx.cfg`, which sources
`interface/stlink.cfg`. For DAPLink, change that line to
`source [find interface/cmsis-dap.cfg]`.

---

## 10. What was fixed in this project

| File | Change |
|------|--------|
| `Makefile` | `LDSCRIPT` no longer uses `tr` (it does not exist on Windows) - the MCU name is lower-cased with PowerShell on Windows and `tr` elsewhere |
| `Makefile` | `MCU_TYPE_LOWER/UPPER` computed once; `PYOCD_TARGET` (default `PY32F040CxB`) separated from `MCU_TYPE` (`PY32F040xB`) |
| `Makefile` | `PYOCD_EXE` resolves: override -> `.venv\Scripts\pyocd.exe` -> `py -m pyocd` |
| `Makefile` | `PROJECT` defaults to the folder name, so a copied template builds/debugs without editing anything |
| `rules.mk` | `SHELL := cmd.exe` on Windows; `MKDIR`/`RMDIR` use `if not exist ... mkdir` / `rmdir /S /Q` (the old `mkdir -p` created a stray `-p` folder and `rm` does not exist in cmd) |
| `rules.mk` | flash/erase pass `--pack` and a real `pyocd.yaml`; new `erase` target |
| `rules.mk` | output paths quoted; `-include $(OBJS:.o=.d)` so header changes trigger rebuilds |
| `pyocd.yaml` | **new** - packed DFP + 1 MHz SWD |
| `Tools/packs/` | **new** - Puya DFP 1.2.0 |
| `Misc/SVD/py32f040cxx.svd` | **new** - extracted from the DFP, replaces the wrong `py32f002axx.svd` reference |
| `Misc/jlink-command` | loads `Build/Py32f_ProjectTemplate.hex` instead of the non-existent `Build/app.hex` |
| `.vscode/*` | `cl.exe`-less ARM IntelliSense config, working pyOCD/OpenOCD launch configs, Build/Clean/Flash/Erase/setup/check tasks, removed the hard-coded `C:/Users/Ahmed Samy/Downloads/...` CMake path and the hard-coded remote IP configs |
| `Tools/*.ps1` | setup/check/install helpers adapted to `py32f040cxb` |

Run the **Check toolchain** task any time to see what is missing.

---

## 11. Troubleshooting

| Symptom | Fix |
|---------|-----|
| `make` / `arm-none-eabi-gcc` / `pyocd` not recognized | PATH changed in another window - close all terminals and VS Code. |
| `arm-none-eabi-gcc: error: unrecognized command-line option '-mcpu=cortex-m0plus'` | A non-ARM `gcc` is first in PATH. |
| `cannot open linker script ... .ld` | `MCU_TYPE` is not one of the names that exist in `Libraries/LDScripts/`. Check the value and the comment block at the top of the Makefile. |
| `The system cannot find the path specified` during build | Run `make` in PowerShell/cmd, not Git Bash. `rules.mk` pins `cmd.exe`, but a stale checkout may not have the fix. |
| `pyocd: unrecognized target type: py32f040cxb` | Run pyOCD from the project folder (so `pyocd.yaml` is read), pass `--pack Tools\packs\Puya.PY32F0xx_DFP.1.2.0.pack`, or run `Tools\install-py32-pack.ps1`. |
| `pyocd list` shows no probe | Install the probe driver (STSW-LINK009 for ST-Link), try another cable/port, avoid hubs. |
| Probe listed but cannot connect | Official ST-Link firmware may block non-ST targets; check SWDIO/SWCLK/GND/3V3 and target power. |
| `Cortex-Debug: GDB executable not found` | `arm-none-eabi-gdb` is not on PATH; install the Arm toolchain or set `cortex-debug.gdbPath`. |
| IntelliSense cannot open `py32f0xx.h` | Select the `PY32F040 (arm-none-eabi)` config in the C/C++ status bar item; make sure `arm-none-eabi-gcc` is on PATH. |
| F5 uses the wrong ELF after renaming the folder | `PROJECT` follows the folder name; run `make clean` so the old `Build\<OldName>.elf` is removed. |
| `region RAM overflowed` at link | 16 KB RAM is exhausted - check buffers and `printf` usage. |

---

## 12. Using this template for a new module

1. Copy the whole `Py32f_ProjectTemplate` folder and rename it
   (e.g. `Brinco_LED_Module`).
2. Open the new folder in VS Code.
3. Replace `User/Src/main.c` with your application. Keep
   `py32f040_hal_conf.h`, `py32f040_hal_msp.c` and `py32f040_it.c`.
4. If your MCU is a different flash/package variant, set `MCU_TYPE` (build) and
   `PYOCD_TARGET` (flash/debug) at the top of the Makefile. See section 6.
5. `make all` -> `Build\<FolderName>.elf`, `F5` -> debug. Nothing else to change.

If you add peripherals, enable the matching `HAL_*_MODULE_ENABLED` in
`User/Inc/py32f040_hal_conf.h`; the HAL driver sources are already all compiled,
so no Makefile change is needed.

## 13. Working on several modules

Open the workspace file at the repository root to get this template and
`Py32f003_Ultrasonic` in the same window:

```powershell
code ..\Brinco_Modules.code-workspace
```

See the [repository README](../README.md#2-open-the-workspace) for the
multi-root details (window-scoped settings, duplicate task labels).
