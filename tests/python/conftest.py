"""Shared fixtures for the Python interoperability tests.

These tests drive the real ``qrs_cli.exe`` (C++ + C + Rust) and compare it
against OpenCV, an implementation that shares no code with the project.
"""
from __future__ import annotations

import json
import os
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
CLI = ROOT / "build" / "bin" / "qrs_cli.exe"

# Same defaults as build.bat; build.bat exports the variables when it runs pytest.
QT_DIR = Path(os.environ.get("QRS_QT_DIR", r"D:\Apps\Qt6\6.12.0\mingw_64"))
MINGW_DIR = Path(os.environ.get("QRS_MINGW_DIR", r"D:\Apps\Qt6\Tools\mingw1310_64"))


def _env() -> dict[str, str]:
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join([str(MINGW_DIR / "bin"), str(QT_DIR / "bin"), env.get("PATH", "")])
    return env


class Cli:
    """Thin wrapper around qrs_cli.exe."""

    def run(self, *args: str | os.PathLike) -> subprocess.CompletedProcess:
        return subprocess.run(
            [str(CLI), *map(str, args)],
            capture_output=True,
            env=_env(),
            timeout=120,
            check=False,
        )

    def encode(self, text: str, out: Path, *extra: str) -> subprocess.CompletedProcess:
        # The text goes through a UTF-8 file so console code pages cannot mangle it.
        source = out.with_suffix(".txt")
        source.write_bytes(text.encode("utf-8"))
        return self.run("encode", "--text-file", source, "--out", out, *extra)

    def decode(self, *paths: Path) -> list[dict]:
        proc = self.run("decode", *paths)
        return [json.loads(line) for line in proc.stdout.decode("utf-8").splitlines() if line.strip()]


@pytest.fixture(scope="session")
def cli() -> Cli:
    if not CLI.exists():
        pytest.skip(f"{CLI} not built - run build.bat first")
    return Cli()
