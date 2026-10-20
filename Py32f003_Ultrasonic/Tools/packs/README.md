# Puya device family pack

`Puya.PY32F0xx_DFP.1.2.0.pack` is Puya's CMSIS Device Family Pack for the
PY32F0 series (release 1.2.0, 2023-08-23), mirrored from
<https://github.com/decaday/PY32_Docs> (`Pack/` folder).

It is used by:

* `pyocd.yaml` (project root) - loaded automatically by pyOCD when it runs from
  the project folder.
* `.vscode/launch.json` - the `cmsisPack` attribute of the pyOCD debug
  configurations.
* `rules.mk` - the `PYOCD_PACK` variable used by `make flash`.

It adds these targets to pyOCD (among others):

| Target | Part | Flash |
|--------|------|-------|
| `py32f003x8` | PY32F003x8 / PY32F003F18P6TU | 64 KB |
| `py32f040xb`, `py32f040cxb` | PY32F040 | up to 128 KB |

The pack is a ZIP archive; it contains the `.pdsc` descriptor, SVD files and
the `CMSIS/Flash/PY32F0xx_*.FLM` flash algorithms. Keep the version in the file
name in sync with `PYOCD_PACK` in `rules.mk` and `pyocd.yaml` if you upgrade it.
