# Build, environment and packaging

## Quick start

```bat
build.bat            configure + compile → build\bin\QRStudio.exe
build.bat test       compile, then run Rust, C++ and Python tests
build.bat deploy     compile, then create a self-contained dist\ folder
build.bat all        build + test + deploy
build.bat check      only verify the toolchain
build.bat clean      delete build\ and dist\
```

`dist\QRStudio.exe` runs on a machine without Qt, MinGW, Rust or Python installed.
To run `build\bin\QRStudio.exe` directly, the Qt and MinGW `bin` folders must be on `PATH` (the test scripts do this themselves).

## Dependency inventory

Verified on the development machine on 2026-11-15. `python main.py env` prints the current state as JSON.

| Dependency | Version | Location | Used from | Required | Needs installing |
|---|---|---|---|---|---|
| Qt (MinGW 64-bit kit) | 6.12.0 | `D:\Apps\Qt6\6.12.0\mingw_64` | C++ | yes | no |
| MinGW-w64 GCC (Qt's) | 13.1.0, msvcrt | `D:\Apps\Qt6\Tools\mingw1310_64` | C, C++ | yes | no |
| CMake | Qt-bundled | `D:\Apps\Qt6\Tools\CMake_64\bin` | build | yes | no |
| Ninja | Qt-bundled | `D:\Apps\Qt6\Tools\Ninja` | build | yes | no |
| Rust / cargo | 1.96.1 (MSYS2, `x86_64-pc-windows-gnu`) | `D:\Apps\msy32\ucrt64\bin` | Rust | yes | no |
| quirc | vendored source | `third_party\quirc` | C | yes | no |
| Python | 3.13.15 | `D:\Apps\python` | tests, tools | tests only | no |
| opencv-python-headless | 5.0.0 | Python site-packages | tests, benchmark | tests only | no |
| numpy / pytest | 2.5.4 / 9.1.1 | Python site-packages | tests | tests only | no |
| Git | 2.56 | `C:\Apps\Git\cmd` (on PATH) | — | no | no |

Looked for and **not used**:

| Item | Finding |
|---|---|
| MSVC compiler | Not installed. `D:\Apps\Microsoft Visual Studio Community` contains only the IDE; there is no `VC\Tools\MSVC`. The Qt `msvc2022_64` kit therefore cannot be used. |
| MSYS2 GCC 16.1 | Installed at `D:\Apps\msy32\ucrt64\bin` and earlier on `PATH` than Qt's compiler. It must **not** compile the C++ code (different runtime from the Qt kit); `build.bat` puts Qt's MinGW first. It is used only as the linker for the Rust DLL. |
| Second Rust install | `D:\Library\cargo` + `D:\Library\rustup` (MSVC toolchain) exists but is not on `PATH` and cannot link without MSVC. |
| vcpkg | Not found. |
| Conan | 2.34.0 installed; not needed. |
| OpenCV / ZXing / zbar / libqrencode for C++ | Not installed (checked MSYS2 packages). Not needed. |
| PySide6 | Installed; not used. |

No drive was scanned recursively to produce this table: it comes from `PATH`, environment variables, the top level of `D:\`, `D:\Apps`, `D:\Library`, and the package managers' own listings.

## Using different tool locations

Set any of these before calling `build.bat`:

```bat
set QRS_QT_DIR=D:\Apps\Qt6\6.12.0\mingw_64
set QRS_MINGW_DIR=D:\Apps\Qt6\Tools\mingw1310_64
set QRS_CMAKE_DIR=D:\Apps\Qt6\Tools\CMake_64\bin
set QRS_NINJA_DIR=D:\Apps\Qt6\Tools\Ninja
set QRS_PYTHON=python
```

`cargo` is taken from `PATH`.

## How the build fits together

The root `CMakeLists.txt` defines:

| Target | Kind | Sources |
|---|---|---|
| `quirc` | static C library | `third_party/quirc` |
| `qrs_decode` | static C library | `gui/src/core/*.c` |
| `qrs_encode` | imported DLL | built by `cargo build --release --offline` into `build/rust` |
| `qrs_core` | static C++ library | `gui/src/core/*.cpp` |
| `qrs_app` | static C++ library | `gui/src/app/*.cpp` (listed in `gui/gui.cmake`) |
| `QRStudio`, `qrs_cli` | executables | `gui/main/` |
| `tst_core`, `tst_files`, `tst_gui` | tests | `tests/cpp/` |

### Renaming the source folder

The folder name `gui` appears once, as the default of `QRS_SOURCE_DIR` in the root `CMakeLists.txt`. To rename: rename the folder, change that default (or pass `-DQRS_SOURCE_DIR=<name>`), run `build.bat clean` then `build.bat`. C/C++ includes use `qrs/...` and need no change.

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `[check] MISSING ...` | A tool is not where `build.bat` expects. Set the matching `QRS_*` variable. |
| Link errors mentioning `__imp_` or `std::` symbols from Qt | The C++ code was compiled by MSYS2's GCC instead of Qt's. Run `build.bat clean` and build through `build.bat`, not a bare `cmake`. |
| Rust step fails to link | The Rust linker must be the `gcc.exe` beside `cargo.exe`; CMake sets `CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER` for this. Check that MSYS2's `gcc` still exists there. |
| `QRStudio.exe` from `build\bin` reports a missing DLL | Use `dist\QRStudio.exe`, or add the Qt and MinGW `bin` folders to `PATH`. |
| A newly added source file is not compiled | Add it to `gui/gui.cmake` (app) or the root `CMakeLists.txt` (core). Source lists are explicit. |
| `windeployqt` warns about `dxcompiler.dll` | Harmless; Direct3D 12 is not used. |
