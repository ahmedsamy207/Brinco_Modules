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

### Already installed? (per-user install, e.g. Python 3.14)

With a standard `pip install --user`, `pyocd.exe` ends up in a **version-specific
per-user folder** that Windows does *not* put on PATH, for example:

```text
C:\Users\Ahmed Samy\AppData\Roaming\Python\Python314\Scripts\pyocd.exe
```

Resolve it without hard-coding the version, and check it works:

```powershell
$scripts = & py -c "import sysconfig; print(sysconfig.get_path('scripts','nt_user'))"
$scripts                                  # ...\AppData\Roaming\Python\Python314\Scripts

py -m pyocd --version                     # works even with pyocd NOT on PATH
& "$scripts\pyocd.exe" list --targets     # quotes needed: the path contains a space
```

To get a plain `pyocd` command in **every new terminal**, add that folder to the
*user* PATH (no admin rights needed):

```powershell
$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
if (($userPath -split ';') -notcontains $scripts) {
    [Environment]::SetEnvironmentVariable(
        'Path', $userPath.TrimEnd(';') + ';' + $scripts, 'User')
}
```

Then close **all** terminals and VS Code and reopen them.

With that in place you can skip the installation parts below. Note that this
project also works **without** any PATH change:

* `make flash` / `make erase` fall back to `py -m pyocd`
* `.vscode/settings.json` sets
  `"cortex-debug.pyocdPath.windows": "${env:APPDATA}/Python/Python314/Scripts/pyocd.exe"`
  (change `Python314` there if you upgrade Python)

---

### Fresh install: project-local venv

**Recommended: a project-local virtual environment** - it needs no administrator
rights and cannot be broken by PATH settings:

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

$scripts = & py -c "import sysconfig; print(sysconfig.get_path('scripts','nt_user'))"
[Environment]::SetEnvironmentVariable(
    'Path',
    [Environment]::GetEnvironmentVariable('Path', 'User').TrimEnd(';') + ';' + $scripts,
    'User')
```

Then close **all** terminals and VS Code and reopen them.

> **"pyocd : The term 'pyocd' is not recognized"** means the executable is not on
> PATH - it does not mean the board or the pack is wrong. Verify where it is:
>
> ```powershell
> py -m pyocd --version     # works even when 'pyocd' is not on PATH
> py -c "import sysconfig; print(sysconfig.get_path('scripts','nt_user'))"
> ```
>
> If `py -m pyocd --version` fails, pyOCD is not installed at all; if it works,
> only PATH is missing. The venv route above sidesteps both cases.

pyOCD installs its own `libusb` DLL, so nothing else is needed for USB probes.
`pack-manager` (used by the optional pack installer) lives in the same folder
as `pyocd.exe`.

---

## 5. Debug probe: ST-Link V2 (and alternatives)

**This project is set up for an ST-Link V2.** Two independent paths work with
it, and both are configured already:

| Path | Command | Used by |
|------|---------|---------|
| **pyOCD** (default) | `make flash` | `F5` -> *Debug (pyOCD)* |
| **OpenOCD** (Puya build) | `make flash-openocd` | `F5` -> *Debug (OpenOCD, ST-Link)* |

There is no need to choose up front: try `make flash` first, and switch to
`make flash-openocd` if pyOCD cannot talk to the target (see below).

### Driver

Install **STSW-LINK009** from ST:
<https://www.st.com/en/development-tools/stsw-link009.html>

It is a signed WinUSB driver, so it works for **both** pyOCD (libusb) and the
bundled OpenOCD. No Zadig is needed.

* Device Manager should show `STMicroelectronics STLink dongle` without a
  warning triangle.
* If you already installed Zadig/libusbK for another tool, that also works -
  both paths use libusb.
* If the probe does not appear at all, uninstall any "unknown device" entry,
  replug it, and re-run the STSW-LINK009 installer.

### Wiring (ST-Link V2, 20-pin or 4-pin header)

| ST-Link | Target |
|---------|--------|
| `SWDIO` (pin 7) | `SWDIO` / `DIO` |
| `SWCLK` (pin 9) | `SWCLK` / `CLK` |
| `GND` (pin 4/6/8...) | `GND` |
| `3.3V` (pin 1/2) | `3V3` - **only** if the board is not already powered |
| `NRST` (pin 15) | `NRST` - optional, helps if the chip is in a low-power mode |

Never connect the 5 V pin to the target. Keep the wires short (10 cm) for
reliable SWD; long jumper wires are the usual cause of "cannot connect".

### Check that the probe is visible

```powershell
py -m pyocd list
```

Expected, for an ST-Link V2:

```text
  #   Probe/Board     Unique ID       Target
  ------------------------------------------------------
  0   STLink          <serial>        n/a
```

### If pyOCD cannot connect: use OpenOCD

Recent **genuine ST** firmware only allows STM32 targets and refuses a PY32
part; cheap ST-Link V2 clones are not affected. That is why Puya's own OpenOCD
build is committed in `Tools/openocd-puya/`:

```powershell
make flash-openocd        # program Build\<Project>.hex (verify + reset)
make erase-openocd        # full chip erase
```

or the VS Code tasks **Flash (OpenOCD / ST-Link)** and
**Erase chip (OpenOCD / ST-Link)**, or `F5` with
*Debug (OpenOCD, ST-Link)*.

The config `Tools/openocd-puya/stlink-py32f003.cfg` sets the flash size for this part,
selects `dapdirect_swd` (the vendor script runs SWD at a conservative
100 kHz, which is reliable and still fast enough for these image sizes).

Two OpenOCD errors are worth knowing:

* `Error: open failed` - the USB driver is not libusb-compatible (install
  STSW-LINK009), or another program already owns the probe.
* no DAP / `unable to find a matching transport` - the ST-Link firmware is
  older than V2J24 (2015). Upgrade it with STSW-LINK007, or use a DAPLink.

### Other probes

| Probe | What to install |
|-------|-----------------|
| **CMSIS-DAP / DAPLink** | Nothing - WinUSB/HID on Windows 10/11 is enough. Point `interface/stlink-dap.cfg` at `interface/cmsis-dap.cfg`. |
| **J-Link** | SEGGER J-Link Software Pack. `make flash FLASH_PROGRM=jlink JLINKEXE="C:\Program Files\SEGGER\JLink\JLink.exe"` |

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

# works with a global/pip install, even without PATH
py -m pyocd list --targets | Select-String py32f003x8

# project-local venv
.\.venv\Scripts\pyocd.exe list --targets | Select-String py32f003x8

# if the per-user Scripts folder is on PATH (or the venv is activated)
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

**With an ST-Link V2, the OpenOCD path is the fallback** when pyOCD cannot
connect (see section 5):

```powershell
make flash-openocd    # program, verify, reset
make erase-openocd    # full chip erase
```

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

The OpenOCD configurations use `Tools\openocd-puya\stlink-py32f003.cfg`, which sets the
flash size for this part, selects `dapdirect_swd` for ST-Link and then
sources the Puya `target/py32f0xx.cfg`. For a DAPLink probe, copy that file
and replace `interface/stlink-dap.cfg` with `interface/cmsis-dap.cfg` (and
drop the `transport select` line).

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
| `Makefile` | `PYOCD_EXE` resolves in order: override -> `.venv\Scripts\pyocd.exe` -> `py -m pyocd`, so pyOCD does not need to be on PATH |
| `.vscode/settings.json` | `cortex-debug.pyocdPath.windows` points at `${env:APPDATA}/Python/Python314/Scripts/pyocd.exe` so F5 debugging works without PATH |
| `.gitignore` | ignores the local `.venv\` |
| `rules.mk` | **new** `flash-openocd` / `erase-openocd` targets and `FLASH_PROGRM=openocd`, so the ST-Link V2 can program through the bundled OpenOCD |
| `Tools/openocd-puya/stlink-py32f003.cfg` / `stlink-py32f040.cfg` | **new** ST-Link session: sets the flash size and uses `dapdirect_swd`. The stock `script/py32f0xx.cfg` sources the deprecated `hla` driver, which cannot create the DAP that `target/py32f0xx.cfg` needs |
| `.vscode/launch.json` | OpenOCD configurations point at the new ST-Link session instead of the broken `py32f0xx.cfg` |
| `.vscode/tasks.json` | **new** tasks *Flash (OpenOCD / ST-Link)* and *Erase chip (OpenOCD / ST-Link)* |
| `Tools/check-env.ps1` | also reports the bundled OpenOCD and prints ST-Link hints |

Run the **Check toolchain** task any time to see what is missing.

---

## 11. Troubleshooting

| Symptom | Fix |
|---------|-----|
| `make` / `arm-none-eabi-gcc` / `pyocd` not recognized | PATH was changed in another window. Close **all** terminals and VS Code, then reopen. |
| `pyocd : The term 'pyocd' is not recognized` | pyOCD is not on PATH. It usually lives in `C:\Users\<you>\AppData\Roaming\Python\Python314\Scripts`. Test with `py -m pyocd --version`; if that works only PATH is missing. Add that folder to the user PATH, or just use `make flash` (falls back to `py -m pyocd`) - see section 4. |
| `make flash` says `'py' is not recognized` | The Python launcher is missing, so the `py -m pyocd` fallback cannot work. Install Python from python.org/winget, or pass the exe: `make flash PYOCD_EXE="C:\Users\Ahmed Samy\AppData\Roaming\Python\Python314\Scripts\pyocd.exe"` (quotes belong in the value). |
| `make: *** [..] Error 1` with `The syntax of the command is incorrect` | You are running make from Git Bash/MSYS2; use PowerShell. `rules.mk` pins `cmd.exe` when `OS=Windows_NT`, but a stale checkout may not have the fix. |
| `arm-none-eabi-gcc: error: unrecognized command-line option '-mcpu=cortex-m0plus'` | A non-ARM `gcc` is first in PATH. Put the Arm toolchain `bin` folder first. |
| `pyocd: unrecognized target type: py32f003x8` | Run pyOCD from the project folder (so it reads `pyocd.yaml`), or run the pack installer, or pass `--pack Tools\packs\Puya.PY32F0xx_DFP.1.2.0.pack`. |
| `pyocd list` shows no probe | Install the probe driver (STSW-LINK009 for ST-Link), try another USB cable/port, avoid hubs. |
| pyOCD sees the probe but cannot connect | Check SWDIO/SWCLK/GND/3V3 and target power first, then try `make flash-openocd`. Genuine ST-Link firmware may refuse non-ST targets; clones and DAPLink/J-Link are not affected. |
| OpenOCD: `Error: open failed` | The probe driver is not libusb-compatible - install STSW-LINK009. Another program may also hold the probe (STM32CubeProgrammer, ST-Link Utility, a second debug session). |
| OpenOCD: no DAP / `unable to find a matching transport` | ST-Link firmware older than V2J24; upgrade with STSW-LINK007 or use a DAPLink. |
| OpenOCD: `init mode failed` / target not examined | Target not powered, SWDIO and SWCLK swapped, or the chip is in a low-power mode - connect NRST and retry. |
| OpenOCD: `verify failed` | The flash driver could not read back. Program without verify: `make flash-openocd OPENOCD_PROGRAM_ARGS="reset exit"`. |
| `Cortex-Debug: GDB executable not found` | `arm-none-eabi-gdb` is not on PATH; install the Arm toolchain. You can also set `cortex-debug.gdbPath` / `cortex-debug.armToolchainPath` in VS Code settings. |
| IntelliSense shows "cannot open source file py32f0xx.h" | Select the `PY32F003 (arm-none-eabi)` configuration in the bottom-right C/C++ selector, and make sure `arm-none-eabi-gcc` is on PATH. |
| PowerShell refuses to run the scripts | Run with `-ExecutionPolicy Bypass` (the VS Code tasks already do), or `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`. |
| Linker error about `region RAM overflowed` | You are out of the 8 KB RAM; check large buffers/`printf` usage. |
| Only part of the image is written when using OpenOCD | Use pyOCD (the default) or pass the correct flash size to OpenOCD; the bundled config autodetects it, but old OpenOCD builds may not. |

---

## 12. Working on several modules

There are two generic templates in this repository:

| Template | MCU | Use it for |
|----------|-----|------------|
| `Py32f003_ProjectTemplate` | PY32F003F18P6TU | new PY32F003 modules (this project's MCU) |
| `Py32f040_ProjectTemplate` | PY32F040C1BT6 | new PY32F040 modules |

Both use the same toolchain, make and pyOCD install as this project; only the
MCU type, linker script, SVD and pyOCD target differ. This project is
essentially `Py32f003_ProjectTemplate` with the ultrasonic application added,
which makes it a worked example of TIM input capture and an I2C slave.

To work on both projects in one VS Code window, open the workspace file at the
repository root:

```powershell
code ..\Brinco_Modules.code-workspace
```

More detail, including the multi-root caveats, is in the
[repository README](../README.md#2-open-the-workspace).
