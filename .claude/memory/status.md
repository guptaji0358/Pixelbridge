# Status and open work

Last updated 2026-11-16. Nothing has been committed to git yet.

## Verified working

- `build.bat all` succeeds: build, tests, `dist\` package.
- Tests: Rust 11, `tst_core` 32, `tst_files` 33 (+1 skipped by design), `tst_gui` 11, pytest 51. All pass.
- `dist\Pixelbridge.exe` and `dist\qrs_cli.exe` run with only `C:\WINDOWS\system32` on PATH.
- UI reviewed from screenshots in light and dark themes (`build\screenshots\`).
- Context-menu script: `status` and `install -DryRun` verified; registry confirmed unchanged.
- LIAS v0.1: the isolated suite passes (4 tests). SQLite FTS5 knowledge storage, secret
  rejection, the safe Tier-0 registry/runner, diagnostics, and `source-statistics` work.
- LIAS now also has 12 safe, report-first maintenance automations (repository, cache,
  memory, docs, build, context-menu, release, candidate, quarantine, and benchmark checks).

## Not verified / waiting on the user

- Right-click menu: **installed 2026-11-15 with user approval**, target was `dist\QRStudio.exe` (old name; `dist\` is rebuilt as `Pixelbridge.exe`), so the entries are stale until reinstalled with user approval.
  Registry read back; scan / copy-path / generate commands run OK. Still unchecked: the submenu
  seen and clicked in Explorer, "Open with default application" on a real file, real uninstall.
  Deleting or moving `dist\` breaks the entries until reinstalled. `build.bat clean` deletes `dist\`.
- Real Recycle Bin delete test (`QRS_TEST_RECYCLE_BIN=1 build\bin\tst_files.exe`) — never run.
- Modal dialogs (delete / move / open-link confirmations, save dialogs) — not automated.
- Real camera photos — only synthetic distortions tested.

## Open tasks (suggested order)

1. Ask the user to confirm the submenu in Explorer (right-click a .png → Show more options →
   Pixelbridge) and record the result in `docs/context-menu.md`.
2. Embed an icon resource (.ico + .rc) in Pixelbridge.exe; it also becomes the menu icon.
3. Batch page: move the folder listing off the UI thread (currently synchronous in `startFolder`).
4. Spin-box text sits too close to the left edge (QSS padding is not reaching the inner line edit).
5. Optional: installer; multi-segment encoding for smaller codes; camera input.
6. The full `build.bat test` command is blocked by a stale `build/CMakeCache.txt` referring to
   `D:/Apps/MyApps/Pendiing/QR`. Resolving it requires destructive `build.bat clean` approval.

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
