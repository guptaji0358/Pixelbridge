"""Cross-implementation tests: QR Studio (Rust encoder, C decoder) vs OpenCV."""
from __future__ import annotations

import csv
import json

import cv2
import numpy as np
import pytest

TEXTS = {
    "url": "https://example.com/path?q=1&r=two",
    "numeric": "31415926535897932384626433832795",
    "alnum": "HELLO WORLD 123 $%*+-./:",
    "text": "The quick brown fox jumps over the lazy dog.",
    "unicode": "नमस्ते — こんにちは — ünïcödé",
    "wifi": "WIFI:T:WPA;S:Home Net;P:correct horse;;",
    "long": "lorem ipsum dolor sit amet " * 30,
}


def opencv_decode(path) -> list[str]:
    # np.fromfile + imdecode instead of cv2.imread: imread cannot open non-ASCII paths on Windows.
    image = cv2.imdecode(np.fromfile(str(path), dtype=np.uint8), cv2.IMREAD_COLOR)
    assert image is not None, f"OpenCV could not read {path}"
    # OpenCV's classic detector misses some perfectly valid symbols - including ones made by
    # OpenCV's own encoder (verified 2026-11-15 with the "wifi" and "long" texts at ECC M) -
    # so the ArUco-based detector is used as a second opinion before declaring failure.
    for detector in (cv2.QRCodeDetector(), cv2.QRCodeDetectorAruco()):
        ok, texts, _, _ = detector.detectAndDecodeMulti(image)
        found = [t for t in texts if t] if ok else []
        if found:
            return found
    return []


def opencv_encode(text: str, path, scale: int = 8, border: int = 4) -> None:
    code = cv2.QRCodeEncoder.create().encode(text)
    code = cv2.copyMakeBorder(code, border, border, border, border, cv2.BORDER_CONSTANT, value=255)
    code = cv2.resize(code, None, fx=scale, fy=scale, interpolation=cv2.INTER_NEAREST)
    assert cv2.imwrite(str(path), code)


@pytest.mark.parametrize("ecc", ["L", "M", "Q", "H"])
@pytest.mark.parametrize("name", TEXTS)
def test_our_encoder_is_read_by_opencv(cli, tmp_path, name, ecc):
    """Rust encoder + C++ renderer produce symbols an independent decoder accepts."""
    out = tmp_path / f"{name}-{ecc}.png"
    proc = cli.encode(TEXTS[name], out, "--size", "900", "--ecc", ecc)
    assert proc.returncode == 0, proc.stderr.decode(errors="replace")
    assert opencv_decode(out) == [TEXTS[name]]


@pytest.mark.parametrize("name", [n for n in TEXTS if n != "unicode"])
def test_our_decoder_reads_opencv_codes(cli, tmp_path, name):
    """C decoder reads symbols produced by an independent encoder."""
    path = tmp_path / f"cv-{name}.png"
    opencv_encode(TEXTS[name], path)
    (result,) = cli.decode(path)
    assert result["status"] == "found"
    assert [c["text"] for c in result["codes"]] == [TEXTS[name]]


@pytest.mark.parametrize("ext", ["png", "jpg", "bmp", "webp", "tif"])
def test_formats_agree_with_opencv(cli, tmp_path, ext):
    out = tmp_path / f"code.{ext}"
    assert cli.encode("format " + ext, out, "--size", "400").returncode == 0
    (ours,) = cli.decode(out)
    assert [c["text"] for c in ours["codes"]] == ["format " + ext]
    assert opencv_decode(out) == ["format " + ext]


def test_custom_colours_and_margin(cli, tmp_path):
    out = tmp_path / "colour.png"
    proc = cli.encode("colour", out, "--size", "600", "--margin", "6", "--fg", "#0b3d91", "--bg", "#fff7e0")
    assert proc.returncode == 0
    info = json.loads(proc.stdout)
    assert info["pixels"] == 600
    image = cv2.imdecode(np.fromfile(str(out), dtype=np.uint8), cv2.IMREAD_COLOR)
    assert image.shape[:2] == (600, 600)
    assert tuple(int(v) for v in image[0, 0]) == (0xE0, 0xF7, 0xFF)  # BGR of #fff7e0
    assert opencv_decode(out) == ["colour"]


def test_svg_export(cli, tmp_path):
    out = tmp_path / "code.svg"
    assert cli.encode("svg", out).returncode == 0
    svg = out.read_text(encoding="utf-8")
    assert svg.startswith("<?xml") and "<path" in svg and 'viewBox="0 0 29 29"' in svg


def test_multiple_codes_in_one_image(cli, tmp_path):
    tiles = []
    for i, text in enumerate(["left code", "right code"]):
        path = tmp_path / f"tile{i}.png"
        assert cli.encode(text, path, "--size", "300").returncode == 0
        tiles.append(cv2.imread(str(path)))
    sheet = np.full((400, 760, 3), 255, np.uint8)
    sheet[50:350, 40:340] = tiles[0]
    sheet[50:350, 420:720] = tiles[1]
    path = tmp_path / "sheet.png"
    cv2.imwrite(str(path), sheet)
    (result,) = cli.decode(path)
    assert sorted(c["text"] for c in result["codes"]) == ["left code", "right code"]


def test_photo_like_distortions(cli, tmp_path):
    """Perspective warp, blur and noise - closer to a phone photo than a clean render."""
    src = tmp_path / "clean.png"
    assert cli.encode("https://example.com/photo", src, "--size", "500", "--ecc", "Q").returncode == 0
    image = cv2.imread(str(src))
    canvas = np.full((800, 900, 3), 190, np.uint8)
    corners = np.float32([[0, 0], [500, 0], [500, 500], [0, 500]])
    target = np.float32([[180, 140], [660, 90], [720, 640], [130, 590]])
    warped = cv2.warpPerspective(image, cv2.getPerspectiveTransform(corners, target), (900, 800),
                                 dst=canvas, borderMode=cv2.BORDER_TRANSPARENT)
    warped = cv2.GaussianBlur(warped, (5, 5), 0)
    noise = np.random.default_rng(7).normal(0, 9, warped.shape)
    noisy = np.clip(warped.astype(np.float64) + noise, 0, 255).astype(np.uint8)
    path = tmp_path / "photo.jpg"
    cv2.imwrite(str(path), noisy, [cv2.IMWRITE_JPEG_QUALITY, 70])
    (result,) = cli.decode(path)
    assert result["status"] == "found", result
    assert result["codes"][0]["text"] == "https://example.com/photo"


def test_no_qr_and_invalid_inputs(cli, tmp_path):
    blank = tmp_path / "blank.png"
    cv2.imwrite(str(blank), np.full((300, 400, 3), 230, np.uint8))
    fake = tmp_path / "fake.png"
    fake.write_text("not an image")
    proc = cli.run("decode", blank, fake, tmp_path / "missing.png")
    assert proc.returncode == 1
    statuses = [json.loads(line)["status"] for line in proc.stdout.decode().splitlines()]
    assert statuses == ["no_qr", "error", "error"]


def test_encode_errors(cli, tmp_path):
    out = tmp_path / "x.png"
    too_long = cli.encode("x" * 3000, out)
    assert too_long.returncode == 1 and b"too long" in too_long.stderr
    assert not out.exists()
    same = cli.encode("abc", out, "--fg", "#123456", "--bg", "#123456")
    assert same.returncode == 1
    assert cli.run("encode", "--out", out).returncode == 2  # usage error
    assert cli.run("bogus").returncode == 2


def test_batch_scan_and_csv(cli, tmp_path):
    folder = tmp_path / "batch"
    (folder / "nested").mkdir(parents=True)
    assert cli.encode("one, with comma", folder / "a.png").returncode == 0
    assert cli.encode('=SUM(A1) "quoted"', folder / "b.jpg", "--size", "500").returncode == 0
    assert cli.encode("deep", folder / "nested" / "c.png").returncode == 0
    for txt in folder.rglob("*.txt"):  # helper files written by Cli.encode
        txt.unlink()
    cv2.imwrite(str(folder / "empty.png"), np.full((200, 200, 3), 255, np.uint8))
    (folder / "broken.png").write_text("garbage")
    (folder / "readme.md").write_text("ignored: not an image")

    flat_csv = tmp_path / "flat.csv"
    proc = cli.run("batch", folder, "--csv", flat_csv)
    assert proc.returncode == 0, proc.stderr
    assert json.loads(proc.stdout) == {"files": 4, "found": 2, "no_qr": 1, "errors": 1}

    raw = flat_csv.read_bytes()
    assert raw.startswith(b"\xef\xbb\xbf")
    rows = list(csv.DictReader(raw.decode("utf-8-sig").splitlines(keepends=True)))
    by_file = {r["file"]: r for r in rows}
    assert by_file["a.png"]["content"] == "one, with comma"
    assert by_file["a.png"]["status"] == "ok"
    # Formula-looking content is neutralised with a leading apostrophe.
    assert by_file["b.jpg"]["content"] == "'=SUM(A1) \"quoted\""
    assert by_file["empty.png"]["status"] == "no_qr"
    assert by_file["broken.png"]["status"] == "error" and by_file["broken.png"]["error"]

    proc = cli.run("batch", folder, "--recursive")
    assert json.loads(proc.stdout)["files"] == 5
    assert cli.run("batch", tmp_path / "nope").returncode == 1
