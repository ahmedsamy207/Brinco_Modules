# Puya device family pack

`Puya.PY32F0xx_DFP.1.2.0.pack` is Puya's CMSIS Device Family Pack for the
PY32F0 series (release 1.2.0, 2023-08-23), mirrored from
<https://github.com/decaday/PY32_Docs> (`Pack/` folder).

It is used by:

* `pyocd.yaml` (project root) - loaded automatically by pyOCD when it runs from
  the project folder.
* `.vscode/launch.json` - the `cmsisPack` attribute of the pyOCD debug
  configurations.
* `rules.mk` - the `PYOCD_PACK` variable used by `make flash` / `make erase`.

Targets relevant to this template:

| Target | Part | Flash | RAM |
|--------|------|-------|-----|
| `py32f040cxb` | PY32F040C1BT6 (C package, 48 pins) | 128 KB | 16 KB |
| `py32f040xb` | PY32F040xB (generic) | 128 KB | 16 KB |
| `py32f040cx6/cx8/cx9` | smaller flash variants | 32/64/96 KB | 4/8/16 KB |

The pack also covers every other PY32F0 part used by the modules in this repo
(PY32F002A/B, PY32F003, PY32F030, PY32F031, PY32F071, PY32F072).

Notes:

* The pack is a ZIP archive containing the `.pdsc` descriptor, SVD files
  (`CMSIS/SVD/py32f040cxx.svd` was extracted to `Misc/SVD/`) and the flash
  algorithms (`CMSIS/Flash/PY32F040xx_128.FLM`).
* Keep the version in the file name in sync with `PYOCD_PACK` in `rules.mk`,
  `pyocd.yaml` and `.vscode/launch.json` if you upgrade it.
* This folder is copied together with the project, so a new module created from
  this template keeps working without any download.
