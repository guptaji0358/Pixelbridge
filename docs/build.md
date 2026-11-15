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
`build\bin\` is self-contained too: CMake copies the runtime DLLs next to every executable after each build (see below), so `build\bin\QRStudio.exe`, `qrs_cli.exe` and the tests run with no tool folders on `PATH`.

### Runtime DLLs copied by CMake

`cmake/RuntimeDlls.cmake` defines two functions, called from the root `CMakeLists.txt`, `gui/gui.cmake` and `tests/cpp/CMakeLists.txt`:

| Function | Copies | Used for |
|---|---|---|
| `qrs_copy_runtime_dlls(<target>)` | `qrs_encode.dll` and the Qt DLLs the target links (via `$<TARGET_RUNTIME_DLLS>`), plus `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` from the compiler's folder | `qrs_cli`, `QRStudio`, every test |
| `qrs_deploy_qt_plugins(<target>)` | Qt plugins (`platforms\`, `styles\`, `imageformats\`, …) through `windeployqt` | `QRStudio` |

`tst_gui` additionally gets `platforms\qoffscreen.dll`, because ctest runs it with `QT_QPA_PLATFORM=offscreen` and Qt looks for plugins beside the DLLs it loaded. To add a DLL for a new executable, call `qrs_copy_runtime_dlls(<target>)` after defining it.

## Dependency layout

```
third_party\c\quirc\   vendored C source (tracked in git)
deps\                  tool installs (git-ignored)
  qt6\                 Qt 6.12.0 + MinGW, CMake, Ninja   (junction D:\Apps\Qt6 points here)
  msys2\               Rust/cargo + MSYS2 linker         (junction D:\Apps\msy32 points here)
```

The old `D:\Apps\...` paths still work through the junctions, so PATH entries and other projects are unaffected. Python (`D:\Apps\python`) was **not** moved: VS Code Jupyter kernels were using it. Close them, move it to `deps\python`, leave a junction, and update `QRS_PYTHON` if wanted.

## Dependency inventory

Verified on the development machine on 2026-11-15. `python main.py env` prints the current state as JSON.

| Dependency | Version | Location | Used from | Required | Needs installing |
|---|---|---|---|---|---|
| Qt (MinGW 64-bit kit) | 6.12.0 | `deps\qt6\6.12.0\mingw_64` | C++ | yes | no |
| MinGW-w64 GCC (Qt's) | 13.1.0, msvcrt | `deps\qt6\Tools\mingw1310_64` | C, C++ | yes | no |
| CMake | Qt-bundled | `deps\qt6\Tools\CMake_64\bin` | build | yes | no |
| Ninja | Qt-bundled | `deps\qt6\Tools\Ninja` | build | yes | no |
| Rust / cargo | 1.96.1 (MSYS2, `x86_64-pc-windows-gnu`) | `deps\msys2\ucrt64\bin` (junction `D:\Apps\msy32`) | Rust | yes | no |
| quirc | vendored source | `third_party\c\quirc` | C | yes | no |
| Python | 3.13.15 | `D:\Apps\python` | tests, tools | tests only | no |
| opencv-python-headless | 5.0.0 | Python site-packages | tests, benchmark | tests only | no |
| numpy / pytest | 2.5.4 / 9.1.1 | Python site-packages | tests | tests only | no |
| Git | 2.56 | `C:\Apps\Git\cmd` (on PATH) | — | no | no |

Looked for and **not used**:

| Item | Finding |
|---|---|
| MSVC compiler | Not installed. `D:\Apps\Microsoft Visual Studio Community` contains only the IDE; there is no `VC\Tools\MSVC`. The Qt `msvc2022_64` kit therefore cannot be used. |
| MSYS2 GCC 16.1 | Installed at `deps\msys2\ucrt64\bin` and earlier on `PATH` than Qt's compiler. It must **not** compile the C++ code (different runtime from the Qt kit); `build.bat` puts Qt's MinGW first. It is used only as the linker for the Rust DLL. |
| Second Rust install | `D:\Library\cargo` + `D:\Library\rustup` (MSVC toolchain) exists but is not on `PATH` and cannot link without MSVC. |
| vcpkg | Not found. |
| Conan | 2.34.0 installed; not needed. |
| OpenCV / ZXing / zbar / libqrencode for C++ | Not installed (checked MSYS2 packages). Not needed. |
| PySide6 | Installed; not used. |

No drive was scanned recursively to produce this table: it comes from `PATH`, environment variables, the top level of `D:\`, `D:\Apps`, `D:\Library`, and the package managers' own listings.

## Using different tool locations

Set any of these before calling `build.bat`:

```bat
set QRS_QT_DIR=%CD%\deps\qt6\6.12.0\mingw_64
set QRS_MINGW_DIR=%CD%\deps\qt6\Tools\mingw1310_64
set QRS_CMAKE_DIR=%CD%\deps\qt6\Tools\CMake_64\bin
set QRS_NINJA_DIR=%CD%\deps\qt6\Tools\Ninja
set QRS_PYTHON=python
```

`cargo` is taken from `PATH`.

## How the build fits together

The root `CMakeLists.txt` defines:

| Target | Kind | Sources |
|---|---|---|
| `quirc` | static C library | `third_party/c/quirc` |
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
| `QRStudio.exe` from `build\bin` reports a missing DLL | Rebuild with `build.bat build`; the post-build step copies the DLLs. If a new library was linked, check that `qrs_copy_runtime_dlls` is called for that target. |
| A test fails with "could not find the Qt platform plugin" | The plugin is missing from `build\bin\platforms`. Offscreen tests need `qoffscreen.dll` (copied for `tst_gui`); see above. |
| A newly added source file is not compiled | Add it to `gui/gui.cmake` (app) or the root `CMakeLists.txt` (core). Source lists are explicit. |
| `windeployqt` warns about `dxcompiler.dll` | Harmless; Direct3D 12 is not used. |
