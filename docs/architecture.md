# Architecture

Pixelbridge is a local, offline Windows desktop application. Nothing in it opens a network connection.

## Layers

```
 gui/main/main.cpp ── Pixelbridge.exe          gui/main/qrs_cli.cpp ── qrs_cli.exe
        │                                             │
 gui/src/app/  (C++, Qt Widgets)  pages, theme        │
        └──────────────┬──────────────────────────────┘
                       ▼
 gui/src/core/ + gui/include/qrs/  (C++, Qt Core/Gui)        "qrs_core"
   Generator   Scanner   Content   Files
        │          │
        │          ▼
        │   gui/src/core/qrs_decode.c, qrs_preprocess.c  (C)  "qrs_decode"
        │          │
        │          ▼
        │   third_party/c/quirc  (C, vendored, ISC licence)
        ▼
 rust/qrs_encode  (Rust, no crates)  →  qrs_encode.dll, C ABI
```

## Language responsibilities

| Language | Responsibility | Why this language |
|---|---|---|
| Rust | QR encoding: data analysis, Reed-Solomon, module placement, mask selection | Bit-level algorithm with many index calculations; memory safety and built-in unit tests. Written from the specification with no external crates, so the build is offline. |
| C | QR decoding: multi-pass preprocessing and the wrapper around quirc | quirc is C; the wrapper works on raw pixel buffers and is a stable C interface any language can call. |
| C++ (Qt 6) | Everything the user sees, plus rendering, content rules, CSV, search, file operations | Native Windows GUI, image I/O, threading. |
| Python | Tests against an independent implementation (OpenCV), fixtures, benchmark, environment inventory | OpenCV bindings; not needed at run time. |
| PowerShell / BAT | Build driver, right-click menu installer | Native Windows scripting; no extra runtime. |

## Module interfaces

- `qrs/qrs_encode.h` — `qrs_encode(data, len, ecl, out, cap)` returns the side length in modules or a negative error. The caller owns the buffer; no memory crosses the DLL boundary.
- `qrs/qrs_decode.h` — `qrs_decode_gray(gray, w, h, stride, strategies, out, max, stats)`. Tries passes in order (raw, contrast stretch, invert, 2× upscale, downscale, blur) and stops at the first that decodes something. If nothing decodes, it reports QR-like regions it located so the UI can highlight them. The input buffer is never modified.
- `qrs/Generator.h` — `generate(text, options)` → image, matrix, error or warning. `renderSvg()` for vector export.
- `qrs/Scanner.h` — `scanImage()`, `scanFile()`, `loadImage()`. File format is detected from content, EXIF orientation is honoured, transparent pixels are composited onto white.
- `qrs/Content.h` — `classify()` decides what a decoded string is and whether it may be opened; payload builders for URL, email, phone, SMS and Wi-Fi.
- `qrs/Files.h` — CSV export, filename search, move, and move-to-Recycle-Bin.

## Safety rules built into the code

- **Links**: only `http`, `https`, `mailto`, `tel`, `sms` and `geo` can ever be opened. `file:`, `javascript:`, `data:` and custom app schemes cannot. Opening is always a button press followed by a confirmation that shows the real destination host (the confirmation can be turned off in Settings).
- **Delete** means "move to the Recycle Bin", after a confirmation listing the files. There is no permanent delete.
- **Move** asks first and never overwrites; a clashing name gets " (2)", " (3)", …
- **CSV**: cells starting with `=`, `+`, `-` or `@` are prefixed with `'` so a spreadsheet does not run decoded QR text as a formula.
- **Registry**: the application never writes to the registry. Only `tools/windows/context_menu.ps1` does, per user, after an explicit confirmation. See [context-menu.md](context-menu.md).
- **Settings** are an INI file in `%LOCALAPPDATA%\Pixelbridge`.

## Decisions

| # | Decision | Reason |
|---|---|---|
| 1 | Qt Widgets with the Qt 6.12 MinGW kit | Only complete C++ GUI toolchain on the machine. No MSVC compiler is installed. |
| 2 | Rust encoder is a DLL, not a static library | The installed Rust (MSYS2) links the UCRT runtime; Qt's MinGW 13.1 links msvcrt. A DLL with a plain C ABI keeps the two runtimes apart. The DLL imports only Windows system DLLs. |
| 3 | quirc for decoding instead of OpenCV or ZXing | Small, vendored, no install step. OpenCV exists only as a Python package here; no C++ ZXing or zbar is installed. Measured results are in [testing.md](testing.md). |
| 4 | `QUIRC_MAX_REGIONS=65534` | The default of 254 is too low for photographs and sheets with several codes. |
| 5 | Source folder name lives in one CMake variable (`QRS_SOURCE_DIR`) | The folder was renamed from `scripts/` to `gui/`; a future rename is a one-line change. |
| 6 | App pages are a static library (`qrs_app`) | Lets `tst_gui` link the real widgets. |

## Known limitations

- "Scan folder" on the Batch page lists the folder on the UI thread before scanning starts; a very large recursive tree makes the window pause briefly. Scanning itself runs on worker threads.
- Kanji-mode and ECI output are not produced by the encoder; all text is encoded as UTF-8 bytes (numeric and alphanumeric modes are used when the text allows).
- One mode per symbol: mixed text is not split into segments, so some codes are slightly larger than the theoretical minimum.
- No camera/webcam scanning.
- Pixelbridge.exe has no embedded icon resource yet (the window icon is drawn in code).
- Spin-box text sits close to the left edge (cosmetic).
