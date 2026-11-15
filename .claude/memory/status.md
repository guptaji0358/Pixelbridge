# Status and open work

Last updated 2026-11-15 (end of session 1). Nothing has been committed to git yet.

## Verified working

- `build.bat all` succeeds: build, tests, `dist\` package.
- Tests: Rust 11, `tst_core` 32, `tst_files` 33 (+1 skipped by design), `tst_gui` 11, pytest 51. All pass.
- `dist\QRStudio.exe` and `dist\qrs_cli.exe` run with only `C:\WINDOWS\system32` on PATH.
- UI reviewed from screenshots in light and dark themes (`build\screenshots\`).
- Context-menu script: `status` and `install -DryRun` verified; registry confirmed unchanged.

## Not verified / waiting on the user

- Right-click menu: **installed 2026-11-15 with user approval**, target `dist\QRStudio.exe`.
  Registry read back; scan / copy-path / generate commands run OK. Still unchecked: the submenu
  seen and clicked in Explorer, "Open with default application" on a real file, real uninstall.
  Deleting or moving `dist\` breaks the entries until reinstalled. `build.bat clean` deletes `dist\`.
- Real Recycle Bin delete test (`QRS_TEST_RECYCLE_BIN=1 build\bin\tst_files.exe`) — never run.
- Modal dialogs (delete / move / open-link confirmations, save dialogs) — not automated.
- Real camera photos — only synthetic distortions tested.

## Open tasks (suggested order)

1. Ask the user to confirm the submenu in Explorer (right-click a .png → Show more options →
   QR Studio) and record the result in `docs/context-menu.md`.
2. Embed an icon resource (.ico + .rc) in QRStudio.exe; it also becomes the menu icon.
3. Batch page: move the folder listing off the UI thread (currently synchronous in `startFolder`).
4. Spin-box text sits too close to the left edge (QSS padding is not reaching the inner line edit).
5. Optional: installer; multi-segment encoding for smaller codes; camera input.
6. LIAS runtime (`.claude/lias/`) is not installed. Local automation so far is `build.bat` and
   `main.py`. Install the full system only if the user asks for it.

## Known bugs

None open that affect function. Cosmetic: item 4 above.

## Fixed bugs worth remembering

- Rust `alignment_positions`: usize underflow after the final insert → `saturating_sub`.
- quirc reports ECC as M=0, L=1, H=2, Q=3 → mapped to L/M/Q/H in `qrs_decode.c`.
- `build.bat`: text containing `(` `)` inside an `if ( ) else ( )` block breaks parsing → `:need`
  uses single-line `if` statements.
- `--copy-path`: Qt's OLE clipboard content vanished when the process exited without an event
  loop → `copyTextAndExit()` uses the Win32 clipboard API directly.
- CMake did not notice newly added `gui.cmake` / `tests/cpp` → `build.bat` now always configures.

## Test-writing lessons

- OpenCV's classic `QRCodeDetector` fails on some valid codes, including OpenCV's own. Use
  `QRCodeDetectorAruco` as a second opinion before blaming the encoder.
- ECC H holds at most 1273 bytes; ECC L 2953. Size test payloads accordingly.
- `QStringList::filter("QR found")` also matches "No QR found"; use `count()` for exact matches.
- PowerShell pipes add a BOM; pipe JSON between Python processes through `cmd /c`.
