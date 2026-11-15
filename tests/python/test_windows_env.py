"""Actual-Windows tests: real Pixelbridge.exe process, real clipboard, real registry (read-only).

Nothing here installs the right-click menu; the install path is exercised only
with -DryRun, and the test asserts the registry is unchanged afterwards.
Note: test_copy_path overwrites the clipboard.
"""
from __future__ import annotations

import os
import subprocess
import sys
import winreg
from pathlib import Path

import pytest

from conftest import MINGW_DIR, QT_DIR, ROOT

pytestmark = pytest.mark.skipif(sys.platform != "win32", reason="Windows only")

APP = ROOT / "build" / "bin" / "Pixelbridge.exe"
SCRIPT = ROOT / "tools" / "windows" / "context_menu.ps1"
VERB_KEY = r"Software\Classes\SystemFileAssociations\.png\shell\QRStudio"


def _env() -> dict[str, str]:
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join([str(MINGW_DIR / "bin"), str(QT_DIR / "bin"), env.get("PATH", "")])
    env.pop("QT_QPA_PLATFORM", None)  # the real "windows" platform plugin
    return env


def _powershell(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", *args],
                          capture_output=True, text=True, timeout=60, check=False)


def _menu_installed() -> bool:
    try:
        winreg.CloseKey(winreg.OpenKey(winreg.HKEY_CURRENT_USER, VERB_KEY))
        return True
    except FileNotFoundError:
        return False


@pytest.fixture(scope="module")
def app() -> Path:
    if not APP.exists():
        pytest.skip(f"{APP} not built")
    return APP


def test_copy_path_survives_process_exit(app, tmp_path):
    """Regression: the path vanished from the clipboard as soon as Pixelbridge.exe exited."""
    target = tmp_path / "some image.png"
    target.write_bytes(b"x")
    _powershell("-Command", "Set-Clipboard -Value 'stale'")
    proc = subprocess.run([str(app), "--copy-path", str(target)], env=_env(), timeout=30, check=False)
    assert proc.returncode == 0
    assert _powershell("-Command", "Get-Clipboard").stdout.strip() == str(target)


def test_open_default_rejects_missing_and_folders(app, tmp_path):
    for bad in (tmp_path / "missing.png", tmp_path):
        proc = subprocess.run([str(app), "--open-default", str(bad)], env=_env(), timeout=30, check=False)
        assert proc.returncode == 1


def test_context_menu_script_status_is_read_only():
    before = _menu_installed()
    proc = _powershell("-File", str(SCRIPT), "-Action", "status")
    assert proc.returncode == 0
    assert proc.stdout.startswith("status: installed" if before else "status: not installed")
    assert _menu_installed() == before


def test_context_menu_dry_run_changes_nothing(app):
    before = _menu_installed()
    proc = _powershell("-File", str(SCRIPT), "-Action", "install", "-ExePath", str(app), "-DryRun")
    assert proc.returncode == 0, proc.stderr
    lines = [line for line in proc.stdout.splitlines() if "[dry-run]" in line]
    assert len(lines) == 77  # 8 image types x 9 values + .txt x 5
    assert all("HKCU:\\Software\\Classes\\SystemFileAssociations\\" in line for line in lines)
    assert not any("HKLM" in line for line in lines)
    assert f'"{app}" --scan "%1"' in proc.stdout
    assert _menu_installed() == before

    proc = _powershell("-File", str(SCRIPT), "-Action", "uninstall", "-DryRun")
    assert proc.returncode == 0
    assert _menu_installed() == before


def test_context_menu_install_refuses_other_executables():
    cli = ROOT / "build" / "bin" / "qrs_cli.exe"
    proc = _powershell("-File", str(SCRIPT), "-Action", "install", "-ExePath", str(cli), "-DryRun")
    assert proc.returncode != 0
    assert "Not Pixelbridge.exe" in proc.stderr
    missing = _powershell("-File", str(SCRIPT), "-Action", "install", "-DryRun")
    assert missing.returncode != 0
