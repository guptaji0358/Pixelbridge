"""Pixelbridge developer tooling (local automation - no network, no AI calls).

    python main.py env                 dependency inventory as JSON (tool, version, path)
    python main.py benchmark DIR       compare Pixelbridge's decoder with OpenCV on a folder of images
    python main.py fixtures DIR        write a set of representative test images into DIR

The desktop application is the C++ program built by build.bat; this script is
not needed to run it.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CLI = ROOT / "build" / "bin" / "qrs_cli.exe"
QT_DIR = Path(os.environ.get("QRS_QT_DIR", r"D:\Apps\Qt6\6.12.0\mingw_64"))
MINGW_DIR = Path(os.environ.get("QRS_MINGW_DIR", r"D:\Apps\Qt6\Tools\mingw1310_64"))
CMAKE_DIR = Path(os.environ.get("QRS_CMAKE_DIR", r"D:\Apps\Qt6\Tools\CMake_64\bin"))
NINJA_DIR = Path(os.environ.get("QRS_NINJA_DIR", r"D:\Apps\Qt6\Tools\Ninja"))
IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".tif", ".tiff"}


def _tool_env() -> dict[str, str]:
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join([str(MINGW_DIR / "bin"), str(QT_DIR / "bin"), env.get("PATH", "")])
    return env


def _version(command: list[str]) -> str | None:
    try:
        out = subprocess.run(command, capture_output=True, text=True, timeout=20, check=False)
    except (OSError, subprocess.TimeoutExpired):
        return None
    text = (out.stdout or out.stderr).strip()
    return text.splitlines()[0] if text else None


def _probe(name: str, exe: Path | str | None, args: list[str], required: bool, role: str) -> dict:
    path = shutil.which(str(exe)) if exe else None
    return {
        "name": name,
        "required": required,
        "role": role,
        "available": path is not None,
        "path": path,
        "version": _version([path, *args]) if path else None,
    }


def cmd_env(_: argparse.Namespace) -> int:
    """Targeted checks only: known tool locations and PATH. Never scans drives."""
    items = [
        _probe("MinGW g++", MINGW_DIR / "bin" / "g++.exe", ["--version"], True, "C / C++ compiler"),
        _probe("Qt qmake", QT_DIR / "bin" / "qmake.exe", ["-query", "QT_VERSION"], True, "GUI framework"),
        _probe("windeployqt", QT_DIR / "bin" / "windeployqt.exe", ["--version"], True, "packaging"),
        _probe("CMake", CMAKE_DIR / "cmake.exe", ["--version"], True, "build system"),
        _probe("Ninja", NINJA_DIR / "ninja.exe", ["--version"], True, "build tool"),
        _probe("cargo", "cargo", ["--version"], True, "Rust encoder build"),
        _probe("rustc", "rustc", ["--version"], True, "Rust compiler"),
        _probe("git", "git", ["--version"], False, "version control"),
        _probe("conan", "conan", ["--version"], False, "not used"),
        _probe("vcpkg", "vcpkg", ["version"], False, "not used"),
        _probe("cl (MSVC)", "cl", [], False, "not used"),
    ]
    items.append({"name": "Python", "required": False, "role": "tests and tooling", "available": True,
                  "path": sys.executable, "version": sys.version.split()[0]})
    for module, role in (("cv2", "interop tests / benchmark"), ("numpy", "tests"), ("pytest", "test runner")):
        try:
            mod = __import__(module)
            items.append({"name": f"python:{module}", "required": False, "role": role, "available": True,
                          "path": str(Path(mod.__file__).parent), "version": getattr(mod, "__version__", None)})
        except ImportError:
            items.append({"name": f"python:{module}", "required": False, "role": role, "available": False,
                          "path": None, "version": None})
    vendored = ROOT / "third_party" / "c" / "quirc" / "quirc.h"
    items.append({"name": "quirc (vendored)", "required": True, "role": "QR decoding (C)",
                  "available": vendored.exists(), "path": str(vendored.parent), "version": "source in tree"})
    print(json.dumps(items, indent=2))
    missing = [i["name"] for i in items if i["required"] and not i["available"]]
    if missing:
        print("MISSING required: " + ", ".join(missing), file=sys.stderr)
    return 1 if missing else 0


def _images(folder: Path) -> list[Path]:
    return sorted(p for p in folder.rglob("*") if p.suffix.lower() in IMAGE_EXTENSIONS)


def cmd_benchmark(args: argparse.Namespace) -> int:
    import cv2
    import numpy as np

    if not CLI.exists():
        print(f"{CLI} not found - run build.bat first", file=sys.stderr)
        return 1
    files = _images(Path(args.folder))
    if not files:
        print("no images found", file=sys.stderr)
        return 1

    rows = []
    for path in files:
        proc = subprocess.run([str(CLI), "decode", str(path)], capture_output=True, env=_tool_env(), check=False)
        ours = json.loads(proc.stdout.decode("utf-8").splitlines()[0])
        our_texts = sorted(c["text"] for c in ours["codes"] if c.get("decoded"))

        image = cv2.imdecode(np.fromfile(str(path), dtype=np.uint8), cv2.IMREAD_COLOR)
        cv_texts: dict[str, list[str]] = {}
        cv_ms: dict[str, float] = {}
        for label, factory in (("opencv", cv2.QRCodeDetector), ("opencv_aruco", cv2.QRCodeDetectorAruco)):
            start = time.perf_counter()
            texts: list[str] = []
            if image is not None:
                ok, found, _, _ = factory().detectAndDecodeMulti(image)
                texts = sorted(t for t in found if t) if ok else []
            cv_ms[label] = round((time.perf_counter() - start) * 1000, 1)
            cv_texts[label] = texts
        rows.append({
            "file": str(path.relative_to(args.folder)),
            "pixelbridge": {"codes": len(our_texts), "ms": ours["elapsed_ms"],
                         "pass": ",".join(sorted({c["strategy"] for c in ours["codes"] if c.get("decoded")}))},
            **{k: {"codes": len(v), "ms": cv_ms[k], "agrees": v == our_texts} for k, v in cv_texts.items()},
        })

    summary = {
        "images": len(rows),
        "decoded_by": {k: sum(1 for r in rows if r[k]["codes"]) for k in ("pixelbridge", "opencv", "opencv_aruco")},
        "total_ms": {k: round(sum(r[k]["ms"] for r in rows), 1) for k in ("pixelbridge", "opencv", "opencv_aruco")},
        "note": "qrstudio ms is decode time inside the process; OpenCV ms excludes image loading as well.",
    }
    print(json.dumps({"summary": summary, "rows": rows} if args.verbose else summary, indent=2))
    return 0


def cmd_fixtures(args: argparse.Namespace) -> int:
    """Representative images: clean, small, rotated, blurred, noisy, dark, inverted, multi, none."""
    import cv2
    import numpy as np

    out = Path(args.folder)
    out.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(42)

    def qr(text: str, scale: int = 8) -> "np.ndarray":
        code = cv2.QRCodeEncoder.create().encode(text)
        code = cv2.copyMakeBorder(code, 4, 4, 4, 4, cv2.BORDER_CONSTANT, value=255)
        return cv2.resize(code, None, fx=scale, fy=scale, interpolation=cv2.INTER_NEAREST)

    def rotate(image: "np.ndarray", angle: float) -> "np.ndarray":
        h, w = image.shape[:2]
        side = int((h * h + w * w) ** 0.5) + 20
        canvas = np.full((side, side), 255, np.uint8)
        y, x = (side - h) // 2, (side - w) // 2
        canvas[y:y + h, x:x + w] = image
        matrix = cv2.getRotationMatrix2D((side / 2, side / 2), angle, 1.0)
        return cv2.warpAffine(canvas, matrix, (side, side), borderValue=255)

    base = qr("https://example.com/fixture")
    noisy = np.clip(base.astype(np.float64) + rng.normal(0, 35, base.shape), 0, 255).astype(np.uint8)
    multi = np.full((base.shape[0] + 40, base.shape[1] * 2 + 60), 255, np.uint8)
    multi[20:20 + base.shape[0], 20:20 + base.shape[1]] = base
    second = qr("second code")
    multi[20:20 + second.shape[0], 40 + base.shape[1]:40 + base.shape[1] + second.shape[1]] = second

    images = {
        "clean.png": base,
        "small_2px.png": qr("https://example.com/fixture", 2),
        "large_4k.png": cv2.copyMakeBorder(qr("https://example.com/fixture", 24), 900, 900, 1400, 1400,
                                           cv2.BORDER_CONSTANT, value=235),
        "rotated_17.png": rotate(base, 17),
        "rotated_45.png": rotate(base, 45),
        "blurred.jpg": cv2.GaussianBlur(base, (9, 9), 0),
        "noisy.png": noisy,
        "dark_low_contrast.png": (base // 6 + 20).astype(np.uint8),
        "inverted.png": 255 - base,
        "jpeg_q20.jpg": base,
        "multi_two_codes.png": multi,
        "no_qr_noise.png": rng.integers(0, 255, (300, 300), dtype=np.uint8),
        "no_qr_blank.png": np.full((300, 400), 255, np.uint8),
    }
    for name, image in images.items():
        params = [cv2.IMWRITE_JPEG_QUALITY, 20] if name == "jpeg_q20.jpg" else []
        cv2.imwrite(str(out / name), image, params)
    print(f"wrote {len(images)} images to {out}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("env").set_defaults(func=cmd_env)
    bench = sub.add_parser("benchmark")
    bench.add_argument("folder")
    bench.add_argument("--verbose", action="store_true", help="include per-image rows")
    bench.set_defaults(func=cmd_benchmark)
    fixtures = sub.add_parser("fixtures")
    fixtures.add_argument("folder")
    fixtures.set_defaults(func=cmd_fixtures)
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
