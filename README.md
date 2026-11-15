# QR Studio

A Windows desktop app for scanning and generating QR codes. It works entirely offline.

**Status (2026-11-15):** builds, all automated tests pass, self-contained package in `dist\`. The File Explorer right-click menu is installed for the current user and points at `dist\QRStudio.exe`.

## What works

| Area | Implemented |
|---|---|
| **QR Scanner** | Open, drag-and-drop or paste an image. Finds several codes per image. Shows the content, its type, and where the code sits. Copy, export (TXT/CSV), open link (with confirmation). Clear "No QR found" state with Copy/Open/Export disabled; damaged codes are highlighted. |
| **QR Generator** | Text, URL, email, phone, SMS, Wi-Fi. Size, margin, error correction, foreground and background colour. Live preview, input validation, low-contrast warning. Save as PNG/JPG/BMP/WebP/SVG, copy image, copy text, save and show in folder. |
| **File Explorer** | Browse a folder, live search by name (wildcards), filter by extension, images-only, include subfolders, sortable, multi-select. Scan, batch scan, open, show in folder, copy path, move, delete (to Recycle Bin). |
| **Batch Processing** | Scan a folder or a selection on all CPU cores with progress and cancel. Export CSV, copy all results. |
| **Settings** | Light / dark / follow Windows. Generator defaults. Link confirmation. Right-click menu install/remove. |
| **Command line** | `qrs_cli.exe encode | decode | batch | formats` |

Reads PNG, JPG, BMP, GIF, WebP, TIFF, ICO, TGA and a few others (whatever the bundled Qt image plugins support).

## Not done yet

- The right-click menu is installed on the development machine and its commands were run, but it has not been clicked through in Explorer by a person. See [docs/context-menu.md](docs/context-menu.md).
- No camera scanning.
- No installer; `dist\` is a copy-anywhere folder.
- QRStudio.exe has no icon resource.
- Everything else that is untested is listed in [docs/testing.md](docs/testing.md#not-tested).

## Build and run

```bat
build.bat all
dist\QRStudio.exe
```

Requirements and their locations are in [docs/build.md](docs/build.md). Nothing needs to be installed on the development machine as it stands.

## Layout

```
CMakeLists.txt        build definition (names the source folder once: QRS_SOURCE_DIR)
build.bat             check / build / test / deploy / clean
main.py               developer tools: env inventory, fixtures, decoder benchmark
gui/
  include/qrs/        public headers
  src/core/           C decoder wrapper, C++ services
  src/app/            Qt Widgets pages
  main/               main.cpp (QRStudio.exe), qrs_cli.cpp
rust/qrs_encode/      QR encoder (Rust, C ABI DLL)
third_party/c/quirc/    QR decoder library (C, ISC licence)
tools/windows/        context_menu.ps1
tests/cpp/            Qt Test suites
tests/python/         pytest: OpenCV interop, Windows environment
docs/                 architecture, build, testing, context menu
```

## Documentation

- [Architecture, languages, decisions, limitations](docs/architecture.md)
- [Build, dependency inventory, troubleshooting](docs/build.md)
- [Testing: what is covered and what is not](docs/testing.md)
- [Right-click menu: effects, install, remove](docs/context-menu.md)

## Licences

quirc is ISC-licensed (`third_party/c/quirc/LICENSE`). Qt is used under the LGPL and linked dynamically.
