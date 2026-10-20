# Windows + VS Code setup - PY32F003 ultrasonic module

This is the from-scratch setup for the **Py32f003_Ultrasonic** project
(MCU: **PY32F003F18P6TU** - Cortex-M0+, 64 KB flash, 8 KB SRAM, TSSOP-20).

Everything below is done **once** on a fresh Windows install. After that you
only build (`Ctrl+Shift+B`) and debug (`F5`).

> Run commands in a normal **PowerShell** window (or the VS Code terminal set
> to PowerShell). The Makefile uses `cmd.exe` commands for creating folders;
> `rules.mk` now pins the shell to `cmd.exe` on Windows, but PowerShell is
> still the simplest choice.

---

## 0. What gets installed

| # | Tool | Purpose |
|---|------|---------|
| 1 | VS Code + C/C++, Cortex-Debug, Makefile Tools | editor, IntelliSense, debugging |
| 2 | Arm GNU Toolchain (`arm-none-eabi-gcc`, `g++`, `gdb`, `objcopy`, `objdump`) | cross compiler used by the Makefile |
| 3 | GNU Make for Windows | runs `Makefile` / `rules.mk` |
| 4 | Python 3 + **pyOCD** | flashing and the GDB server used by Cortex-Debug |
| 5 | Puya PY32F0xx device family pack | lets pyOCD know the PY32F003x8 (already committed in this repo) |
| 6 | Debug probe driver (ST-Link / DAPLink / J-Link) | talks SWD to the board |

---

## 1. VS Code + extensions

```powershell
winget install --id Microsoft.VisualStudioCode -e

code --install-extension ms-vscode.cpptools
code --install-extension marus25.cortex-debug
code --install-extension ms-vscode.makefile-tools
```

The workspace also recommends these in `.vscode/extensions.json`, so a fresh
clone will offer to install them when you open the folder.

Open the folder **`Py32f003_Ultrasonic`** itself as the workspace root (not the
repository root) - the build and debug paths are relative to it.

---

## 2. Arm GNU Toolchain

Download page: <https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads>

Pick **Windows (mingw-w64-i686) hosted cross toolchains -> AArch32 bare-metal
target (arm-none-eabi)**:

* `arm-gnu-toolchain-14.3.rel1-mingw-w64-i686-arm-none-eabi.exe` (recommended,
  mature GCC 14.3 toolchain), **or**
* `arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi.msi` (newest,
  GCC 15.2 - uses the new MSI installer).

Run the installer and keep **"Add path to environment variable"** checked.
Close the installer, open a **new** PowerShell and check:

```powershell
arm-none-eabi-gcc --version
arm-none-eabi-gdb --version
arm-none-eabi-objcopy --version
```

If Windows still does not find them, add the toolchain `bin` folder to PATH
manually (default locations):

```text
C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.3 Rel1\bin
C:\Program Files\Arm GNU Toolchain arm-none-eabi\15.2 Rel1\bin
```

(`Settings -> System -> About -> Advanced system settings -> Environment
Variables -> Path -> Edit -> New`), then restart VS Code.

> Do not use the `gcc` from MSYS2/MinGW or Visual Studio for this project - it
> must be the **arm-none-eabi** toolchain.

---

## 3. GNU Make for Windows

The simplest route:

```powershell
winget install --id ezwinports.make -e
```

Then open a new terminal:

```powershell
make --version      # should print GNU Make 4.4.1
```

Alternatives:

* **Manual:** download `make-4.4.1-without-guile-w32-bin.zip` from
  <https://sourceforge.net/projects/ezwinports/files/>, extract it to e.g.
  `C:\Tools\make`, and add `C:\Tools\make\bin` to PATH.
* **Chocolatey:** `choco install make -y`
* **MSYS2:** works too (`pacman -S make`), because `rules.mk` now forces
  `SHELL := cmd.exe` on Windows. Just do not put MSYS2's `usr\bin` ahead of the
  Arm toolchain in PATH.

> Build serially. `make -j` is not safe here because several recipes create the
> same `Temp\...` folders at the same time.

---

## 4. Python + pyOCD

Install Python first:

```powershell
winget install --id Python.Python.3.12 -e
py --version
```

Then install pyOCD. **Recommended: a project-local virtual environment** - it
needs no administrator rights and cannot be broken by PATH settings:

```powershell
cd <path-to-repo>\Py32f003_Ultrasonic
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\setup-pyocd.ps1
```

That creates `.venv\` inside the project, installs/updates pyOCD in it, checks
that the `py32f003x8` target resolves and lists connected probes. `.venv\` is
ignored by git.

Afterwards everything works **without** `pyocd` being on PATH:

* the Makefile auto-detects `.venv\Scripts\pyocd.exe` (`make flash`, `make erase`)
* the VS Code tasks use the same detection
* in a terminal, activate it once per session:

  ```powershell
  .\.venv\Scripts\Activate.ps1
  pyocd list --targets
  ```

To make the bare `pyocd` command available in **every** terminal (and for the
Cortex-Debug `pyocd` server), re-run the script with `-AddToPath`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\setup-pyocd.ps1 -AddToPath
```

### Alternative: global (per-user) install

```powershell
py -m pip install --user -U pyocd
```

The `pyocd` launcher then lands in the per-user **Scripts** folder, which is
usually **not** on PATH. Add it once:

```powershell
$scripts = py -c "import site; print(site.USER_BASE + r'\Scripts')"
[Environment]::SetEnvironmentVariable(
    'Path',
    [Environment]::GetEnvironmentVariable('Path', 'User') + ';' + $scripts,
    'User')
```

Then close **all** terminals and VS Code and reopen them. Alternatively point
VS Code at the exact executable by adding this to `.vscode\settings.json`:

```jsonc
"cortex-debug.pyocdPath.windows": "C:/Users/<you>/AppData/Roaming/Python/Python312/Scripts/pyocd.exe"
```

> **"pyocd : The term 'pyocd' is not recognized"** means the executable is not on
> PATH - it does not mean the board or the pack is wrong. Verify where it is:
>
> ```powershell
> py -m pyocd --version          # works even when 'pyocd' is not on PATH
> py -m site --user-base         # ...\AppData\Roaming\Python > Scripts lives here
> Get-ChildItem "$env:APPDATA\Python\Python312\Scripts\pyocd.exe"
> ```
>
> If `py -m pyocd --version` fails, pyOCD is not installed at all; if it works,
> only PATH is missing. The venv route above sidesteps both cases.

pyOCD installs its own `libusb` DLL, so nothing else is needed for USB probes.
`pack-manager` (used by the optional pack installer) lives in the same folder
as `pyocd.exe`.

---

## 5. Debug probe driver

| Probe | What to install |
|-------|-----------------|
| **ST-Link V2 / V2-1 / V3** | STSW-LINK009 USB driver from ST: <https://www.st.com/en/development-tools/stsw-link009.html>. pyOCD and the bundled OpenOCD both use it (WinUSB). |
| **CMSIS-DAP / DAPLink** | Nothing - WinUSB/HID on Windows 10/11 is enough. |
| **J-Link** | SEGGER J-Link Software Pack (pyOCD uses the installed J-Link DLL). |

Check that the probe is visible:

```powershell
pyocd list
```

You should see one line with the probe type and serial number.

> Note: recent **official** ST-Link firmware only allows STM32 targets, so it
> may refuse a PY32. Most inexpensive ST-Link V2 clones work fine. If connecting
> fails while `pyocd list` sees the probe, try a DAPLink/CMSIS-DAP probe or a
> J-Link.

---

## 6. pyOCD target support (Puya DFP)

This repository already contains the pack:

```text
Tools\packs\Puya.PY32F0xx_DFP.1.2.0.pack
```

and `pyocd.yaml` in the project root points pyOCD at it, so the `py32f003x8` target resolves
automatically when pyOCD is started from the project folder. No download is
needed.

Verify from the project folder:

```powershell
cd <path-to-repo>\Py32f003_Ultrasonic

# venv install
.\.venv\Scripts\pyocd.exe list --targets | Select-String py32f003x8

# or with pyocd on PATH / an activated venv
pyocd list --targets | Select-String py32f003x8
```

Expected:

```text
py32f003x8    Puya    PY32F003x8    PY32F0 Series, PY32F003    pack
```

**Optional:** register the pack in pyOCD's global pack cache so it also works
from other folders / without `--pack`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\install-py32-pack.ps1
```

(Pack provenance: Puya `PY32F0xx_DFP` v1.2.0, released 2023-08-23. The same
pack also covers `PY32F040xB`/`PY32F040CxB`, which is what the second project
template needs.)

---

## 7. Build

```powershell
cd <path-to-repo>\Py32f003_Ultrasonic

make clean
make all
```

Output in `Build\`:

```text
Py32f003_Ultrasonic.elf    <- for the debugger
Py32f003_Ultrasonic.hex    <- for pyOCD / other flash tools
Py32f003_Ultrasonic.bin
Py32f003_Ultrasonic.map
Py32f003_Ultrasonic.lst
```

The link step prints the memory usage; the chip has 64 KB flash and 8 KB RAM.
`Temp\` and `Build\` are ignored by git.

In VS Code: `Ctrl+Shift+B` runs the default **Build** task (same `make all`).

---

## 8. Flash

With pyOCD and the bundled pack:

```powershell
make flash
```

This is equivalent to:

```powershell
pyocd erase -t py32f003x8 --pack .\Tools\packs\Puya.PY32F0xx_DFP.1.2.0.pack --chip --config .\pyocd.yaml
pyocd load  -t py32f003x8 --pack .\Tools\packs\Puya.PY32F0xx_DFP.1.2.0.pack .\Build\Py32f003_Ultrasonic.hex --config .\pyocd.yaml
```

Or use the VS Code task **Flash (pyOCD)**, or **Erase chip (pyOCD)**
(`make erase`) for a full chip erase without programming.

J-Link instead of pyOCD (needs the SEGGER software and the J-Link device name):

```powershell
make flash FLASH_PROGRM=jlink JLINKEXE="C:\Program Files\SEGGER\JLink\JLink.exe"
```

---

## 9. Debug in VS Code

1. Connect the probe and the board.
2. Open **Run and Debug** (`Ctrl+Shift+D`).
3. Pick **Debug (pyOCD)** and press `F5`.

It builds, flashes and stops at `main`. `Misc/SVD/py32f003xx.svd` is loaded, so
you get peripheral registers (TIM1, I2C1, GPIO, ADC...) in the **Cortex
Peripherals** view, and the variables/breakpoints work normally.

Other configurations:

| Configuration | Use |
|---------------|-----|
| **Debug (pyOCD)** | build + flash + debug (default) |
| **Attach (pyOCD)** | attach to a running target without flashing |
| **Debug (OpenOCD, ST-Link)** | use the bundled `Tools\openocd-puya\bin\openocd.exe` |
| **Attach (OpenOCD, ST-Link)** | attach with OpenOCD |

The OpenOCD configurations use `Tools\openocd-puya\script\py32f0xx.cfg`, which
sources `interface/stlink.cfg`. For a DAPLink probe, change that line in
`py32f0xx.cfg` to `source [find interface/cmsis-dap.cfg]`.

---

## 10. What was fixed in this project

The old `.vscode` files were copied from a Windows/MSVC project and did not work
with the ARM build:

| File | Change |
|------|--------|
| `.vscode/c_cpp_properties.json` | replaced the `Win32`/`cl.exe` config with `arm-none-eabi-gcc`, `gcc-arm`, `PY32F003x8` + `FORBID_VECT_TAB_MIGRATION`, and the four include folders used by the Makefile |
| `.vscode/launch.json` | SVD corrected from `py32f002axx` to `py32f003xx`; pyOCD configs use `targetId: py32f003x8` + the bundled pack; OpenOCD uses the bundled `openocd.exe` and script folder; removed the stale hard-coded remote IP configuration |
| `.vscode/settings.json` | removed the hard-coded `D:/Origin-Labs/...` CMake path and the unused `openocd.basePath` keys |
| `.vscode/tasks.json` | Build (default), Clean, Flash (pyOCD), Erase chip, Register Puya DFP, Check toolchain |
| `.vscode/extensions.json` | recommends C/C++, Cortex-Debug, Makefile Tools |
| `pyocd.yaml` | **new** - points pyOCD at the bundled pack and sets 1 MHz SWD |
| `Tools/packs/Puya.PY32F0xx_DFP.1.2.0.pack` | **new** - Puya device family pack (v1.2.0) |
| `Tools/setup-pyocd.ps1` | **new** - installs pyOCD into a project-local `.venv\` (optional `-AddToPath`) |
| `Tools/check-env.ps1` | **new** - verifies compiler, make, Python, pyOCD (venv or PATH), pack, target and probe |
| `Tools/install-py32-pack.ps1` | **new** - optional global pack registration |
| `rules.mk` | pins `SHELL` to `cmd.exe` on Windows, adds `PYOCD_PACK` and an `erase` target, quotes paths, passes `--pack` to pyOCD |
| `Makefile` | `PYOCD_EXE` auto-detects `.venv\Scripts\pyocd.exe`, so pyOCD does not need to be on PATH |
| `.gitignore` | ignores the local `.venv\` |

Run the **Check toolchain** task any time to see what is missing.

---

## 11. Troubleshooting

| Symptom | Fix |
|---------|-----|
| `make` / `arm-none-eabi-gcc` / `pyocd` not recognized | PATH was changed in another window. Close **all** terminals and VS Code, then reopen. |
| `pyocd : The term 'pyocd' is not recognized` | pyOCD is not on PATH. Test with `py -m pyocd --version`. Either run `Tools\setup-pyocd.ps1` (recommended, no PATH needed) or add the per-user Scripts folder to PATH - see section 4. |
| `make: *** [..] Error 1` with `The syntax of the command is incorrect` | You are running make from Git Bash/MSYS2; use PowerShell. `rules.mk` pins `cmd.exe` when `OS=Windows_NT`, but a stale checkout may not have the fix. |
| `arm-none-eabi-gcc: error: unrecognized command-line option '-mcpu=cortex-m0plus'` | A non-ARM `gcc` is first in PATH. Put the Arm toolchain `bin` folder first. |
| `pyocd: unrecognized target type: py32f003x8` | Run pyOCD from the project folder (so it reads `pyocd.yaml`), or run the pack installer, or pass `--pack Tools\packs\Puya.PY32F0xx_DFP.1.2.0.pack`. |
| `pyocd list` shows no probe | Install the probe driver (STSW-LINK009 for ST-Link), try another USB cable/port, avoid hubs. |
| pyOCD sees the probe but cannot connect | Official ST-Link firmware may block non-ST targets; use a clone ST-Link, DAPLink or J-Link. Also check SWDIO/SWCLK/GND/3V3 wiring and target power. |
| `Cortex-Debug: GDB executable not found` | `arm-none-eabi-gdb` is not on PATH; install the Arm toolchain. You can also set `cortex-debug.gdbPath` / `cortex-debug.armToolchainPath` in VS Code settings. |
| IntelliSense shows "cannot open source file py32f0xx.h" | Select the `PY32F003 (arm-none-eabi)` configuration in the bottom-right C/C++ selector, and make sure `arm-none-eabi-gcc` is on PATH. |
| PowerShell refuses to run the scripts | Run with `-ExecutionPolicy Bypass` (the VS Code tasks already do), or `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`. |
| Linker error about `region RAM overflowed` | You are out of the 8 KB RAM; check large buffers/`printf` usage. |
| Only part of the image is written when using OpenOCD | Use pyOCD (the default) or pass the correct flash size to OpenOCD; the bundled config autodetects it, but old OpenOCD builds may not. |

---

## 12. Next project

`Py32f_ProjectTemplate` (PY32F040C1BT6) uses the same toolchain, make and pyOCD
install; only the target (`PY32F040xB` / `PY32F040CxB`) and a few paths in its
`.vscode` and `Makefile` differ. The same Puya DFP already contains the
PY32F040 devices. That project is the next step.
