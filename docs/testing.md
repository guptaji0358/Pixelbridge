# Testing

Run everything with `build.bat test`. Results below are from 2026-11-15.

| Suite | Kind | Count | Result |
|---|---|---|---|
| `cargo test` (rust/qrs_encode) | Unit | 11 | pass |
| `tst_core` | Integration: Rust → C++ → C | 32 | pass |
| `tst_files` | Unit + integration (temp folders) | 33 passed, 1 skipped | pass |
| `tst_gui` | GUI (real widgets, offscreen under ctest) | 11 | pass |
| `tests/python/test_interop.py` | Interop against OpenCV | 46 | pass |
| `tests/python/test_windows_env.py` | Actual Windows (process, clipboard, registry read) | 5 | pass |

## What each level covers

**Unit (Rust)** — Galois-field maths, a published Reed-Solomon generator, the published "HELLO WORLD" 1-M codewords and ECC bytes, capacity limits (41 digits / 17 bytes at 1-L, 2953 bytes at 40-L), alignment positions, function-module counts for all 40 versions, C ABI argument checks.

**Integration (`tst_core`)** — every round trip crosses all three languages: text, URL, numeric, alphanumeric, Unicode, Wi-Fi and multi-line payloads at all four ECC levels; every version 1–40; custom colours and margin; validation errors; SVG. Scanning: blank and noise images, a damaged code, three codes in one image, rotated 90° and 33°, mirrored, inverted, 2-pixel modules, low contrast, transparent background, 4000×3000, seven file formats, missing file, text file renamed `.png`, PNG renamed `.jpg`.

**`tst_files`** — 21 URL-classification cases including `javascript:`, `file:`, `data:` and custom schemes that must not be openable; link warnings; payload builders; Wi-Fi escaping round trip; CSV quoting and formula neutralising; CSV export with BOM; extension parsing; search by name, wildcard, extension, recursion, cancellation and result cap; move never overwrites; bad move targets.

**GUI (`tst_gui`)** — five-page navigation; scanner decodes, copies to the real clipboard, disables Copy/Open/Export when there is no QR or the file is unreadable, does not offer "Open link" for plain text, pastes an image from the clipboard; generator preview, copy image (decoded back from the clipboard), copy text, too-long error; file list live search and multi-selection; batch run with found / none / error rows.
Set `QRS_SCREENSHOT_DIR` (and optionally `QRS_TEST_THEME=dark`) to save a picture of every page.

**Interop (Python)** — our encoder read by OpenCV (7 payloads × 4 ECC levels), OpenCV's encoder read by our decoder, five file formats, colours, SVG, two codes in one image, a perspective-warped, blurred, noisy JPEG, error exit codes, batch scan with CSV parsed by Python's `csv` module.

**Actual Windows** — see [context-menu.md](context-menu.md) for the table of what is and is not verified.

## Not tested

- Real installation of the right-click menu and using it from Explorer (needs approval).
- Sending a file to the real Recycle Bin: `tst_files::trashRealFile` exists but is skipped unless `QRS_TEST_RECYCLE_BIN=1`, because it leaves a file in your Recycle Bin.
- The confirmation dialogs for delete, move and open-link (they are modal; the tests check the buttons' enabled state and the underlying functions, not the dialogs).
- Save dialogs and "Open link" actually launching a browser.
- Photographs from a real camera; the distorted images are synthetic.
- Any machine other than the development one.

## Decoder comparison

`python main.py fixtures build\fixtures` then `python main.py benchmark build\fixtures --verbose`:

| Image | Pixelbridge | OpenCV classic | OpenCV ArUco |
|---|---|---|---|
| clean | 1 | 0 | 1 |
| small (2 px modules) | 1 | 0 | 1 |
| large 4K | 1 | 0 | 1 |
| rotated 17° / 45° | 1 / 1 | 0 / 0 | 1 / 1 |
| blurred | 1 | 1 | 1 |
| noisy | 1 | 0 | 1 |
| dark, low contrast | 1 | 1 | 1 |
| inverted | 1 (invert pass) | 0 | 0 |
| JPEG quality 20 | 1 | 0 | 1 |
| two codes | 2 | 2 | 2 |
| noise / blank (no QR) | 0 / 0 | 0 / 0 | 0 / 0 |
| **Images decoded (of 11)** | **11** | **3** | **10** |
| Total decode time | 66 ms | 273 ms | 406 ms |

These are 13 synthetic images, so treat the numbers as a sanity check rather than a general accuracy claim. Timings exclude file loading on all three.

## Bugs found by tests (kept as regression checks)

| Bug | Caught by |
|---|---|
| Rust `alignment_positions` subtracted past zero | `function_module_count_matches_capacity` |
| ECC level reported in quirc's raw order (M/L/H/Q) | `tst_core::roundTrip` compares `eccLevel` |
| `--copy-path` exited 0 but the clipboard was empty afterwards | `test_copy_path_survives_process_exit` |
| `build.bat` printed "MISSING" for tools that exist (parentheses in labels) | `build.bat check` |
