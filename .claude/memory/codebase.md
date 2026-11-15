# Codebase map and toolchain facts

## Where things are

| Path | Contents |
|---|---|
| `CMakeLists.txt` | All targets except the app; `QRS_SOURCE_DIR` (default `gui`) is the only place the source folder is named |
| `gui/gui.cmake` | `qrs_app` library + `QRStudio` executable; explicit source list |
| `gui/include/qrs/` | Public headers: `qrs_encode.h`, `qrs_decode.h` (C); `Generator.h`, `Scanner.h`, `Content.h`, `Files.h` (C++) |
| `gui/src/core/` | `qrs_decode.c`, `qrs_preprocess.c/.h` (C); `Generator.cpp`, `Scanner.cpp`, `Content.cpp`, `Files.cpp` |
| `gui/src/app/` | `MainWindow`, `ScannerPage`, `GeneratorPage` (+`GeneratorPageActions.cpp`), `FilesPage` (+`FilesPageActions.cpp`, `FileTableModel.cpp`), `BatchPage`, `SettingsPage`, `Theme`, `Widgets`, `Actions`, `AppSettings` |
| `gui/main/` | `main.cpp` (GUI + command-line options), `qrs_cli.cpp` |
| `rust/qrs_encode/src/` | `lib.rs` (encode + C ABI), `segment.rs`, `matrix.rs`, `reed_solomon.rs`, `tables.rs` |
| `third_party/quirc/` | Vendored decoder; do not edit |
| `tools/windows/context_menu.ps1` | HKCU-only right-click installer (`status` / `install` / `uninstall`, `-DryRun`) |
| `tests/cpp/`, `tests/python/` | Qt Test and pytest suites |
| `build.bat`, `main.py` | Build driver; dev tools (`env`, `fixtures`, `benchmark`) |

The source folder was `scripts/` in the original brief; it had already been renamed to `gui/`
before session 1.

## Language boundaries

- Rust → C++: `qrs_encode()` in `qrs_encode.dll`. Caller-owned buffer, one byte per module.
  Keep `gui/include/qrs/qrs_encode.h` in sync with `lib.rs` by hand; `QRS_ENCODE_ABI_VERSION`
  is checked by `tst_core`.
- C → C++: `qrs_decode_gray()`; `qrs_result` is ~9 KB each, allocated on the heap in `Scanner.cpp`.
- C++ → Python: only through `qrs_cli.exe` (JSON lines on stdout). Python is never embedded.

## Toolchain (verified)

- Qt 6.12.0 MinGW kit `D:\Apps\Qt6\6.12.0\mingw_64`; compiler `D:\Apps\Qt6\Tools\mingw1310_64`
  (GCC 13.1, msvcrt); CMake `D:\Apps\Qt6\Tools\CMake_64\bin`; Ninja `D:\Apps\Qt6\Tools\Ninja`.
- Rust 1.96.1 from MSYS2 at `D:\Apps\msy32\ucrt64\bin` (target `x86_64-pc-windows-gnu`, UCRT).
- Python 3.13 at `D:\Apps\python` with OpenCV 5.0 (headless), numpy, pytest.
- No MSVC compiler, no vcpkg. Full table in `docs/build.md`.

## Pitfalls

- MSYS2's GCC 16 is earlier on the user's PATH than Qt's GCC 13. Compiling C++ with it breaks
  linking against Qt. Always go through `build.bat`.
- The Rust DLL must be linked by MSYS2's gcc (CMake sets
  `CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER`). That is why Rust is a DLL and not a static lib.
- `cmake/RuntimeDlls.cmake` copies runtime DLLs (Rust DLL, Qt, MinGW runtime) next to each
  executable after every build, and `windeployqt` deploys Qt plugins for QRStudio, so
  `build\bin` and `dist\` both run without Qt/MinGW on PATH. New executables must call
  `qrs_copy_runtime_dlls(<target>)`. Once Qt DLLs live in `build\bin`, Qt looks for plugins in
  `build\bin\platforms`, so `tst_gui` also gets `qoffscreen.dll` copied there.
- `ctest` runs GUI tests with `QT_QPA_PLATFORM=offscreen` (set in `build.bat`). Screenshots need
  the real platform: run `build\bin\tst_gui.exe` directly with `QRS_SCREENSHOT_DIR` set.
- `QStandardPaths::setTestModeEnabled(true)` in `tst_gui` keeps tests away from the real settings.

## Conventions

- Namespaces: `qrs` (core, no widgets), `app` (GUI).
- Core functions return result structs with an `error` string; they never show UI.
- Confirmation dialogs belong to the pages; `qrs::moveToTrash` / `moveFiles` just do the work.
- Style via tokens in `Theme.cpp`; widgets use dynamic properties `role` (labels) and `kind`
  (buttons: `primary`, `danger`). Call `repolish()` after changing them.
- New app source files must be added to `gui/gui.cmake`.
- One header per module that others use; private helpers stay in the `.cpp`.
