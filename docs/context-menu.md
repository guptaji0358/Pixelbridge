# File Explorer right-click integration

**Status: installed on the development machine on 2026-11-15** (with the user's approval), pointing at `dist\QRStudio.exe`. The registry entries were read back and three of the four registered commands were run; nobody has yet clicked the entries in Explorer itself.

## What it adds

A "QR Studio" submenu on the right-click menu of:

| File types | Entries | Runs |
|---|---|---|
| `.png .jpg .jpeg .bmp .gif .webp .tif .tiff` | Open with QR Scanner | `QRStudio.exe --scan "%1"` |
| | Copy file path | `QRStudio.exe --copy-path "%1"` |
| | Open with default application | `QRStudio.exe --open-default "%1"` |
| `.txt` | Generate QR from text | `QRStudio.exe --generate-from-file "%1"` |

On Windows 11 the submenu is under **Show more options** (or Shift+F10). Putting an entry in the short Windows 11 menu requires a packaged, signed shell extension, which this project does not have.

## What it changes on the system

Only keys named `QRStudio`, only for the current user:

```
HKEY_CURRENT_USER\Software\Classes\SystemFileAssociations\<.ext>\shell\QRStudio
```

- No administrator rights. `HKEY_LOCAL_MACHINE` is never touched.
- Default programs, "Open with" lists and double-click behaviour are unchanged.
- Other file types get nothing.
- The entries store the full path of `QRStudio.exe`. If you move the program, install again.

## Install / remove

From the app: **Settings → File Explorer right-click menu → Add to right-click menu…** It shows what will be written and asks before doing anything.

From a terminal (use the `dist\` copy so the path stays valid):

```bat
powershell -ExecutionPolicy Bypass -File dist\context_menu.ps1 -Action status
powershell -ExecutionPolicy Bypass -File dist\context_menu.ps1 -Action install -ExePath "D:\Apps\MyApps\Pendiing\QR\dist\QRStudio.exe" -DryRun
powershell -ExecutionPolicy Bypass -File dist\context_menu.ps1 -Action install -ExePath "D:\Apps\MyApps\Pendiing\QR\dist\QRStudio.exe"
powershell -ExecutionPolicy Bypass -File dist\context_menu.ps1 -Action uninstall
```

`-DryRun` prints every registry change without making it. `status` never changes anything.

## What has and has not been verified

| Check | Result |
|---|---|
| `status` reports correctly and is read-only | tested (automated, real registry) |
| `install -DryRun` lists 77 values, all under HKCU, registry unchanged afterwards | tested (automated) |
| `install` refuses a path that is not `QRStudio.exe` | tested (automated) |
| `--copy-path` leaves the path on the clipboard after exit | tested (automated, real clipboard) |
| `--open-default` rejects missing files and folders | tested (automated) |
| `--scan` opens the window with the image | tested once by hand with `dist\QRStudio.exe` (window opened) |
| Real `install` for all 9 file types, values read back from the registry | done 2026-11-15 |
| Install leaves other programs' entries alone | checked: the existing `Motiva` verb under `.png\shell` is untouched |
| Registered "Open with QR Scanner" command, run with a real path | window opened |
| Registered "Copy file path" command | path on the clipboard, exit code 0 |
| Registered "Generate QR from text" command, file name with spaces | window opened |
| Registered "Open with default application" command on a real image | **not run** (it would open your image viewer); only its rejection of bad paths is tested |
| The submenu being visible in Explorer and clicking each entry there | **not checked** — needs a person at the screen |
| Real `uninstall` | **not run** |
