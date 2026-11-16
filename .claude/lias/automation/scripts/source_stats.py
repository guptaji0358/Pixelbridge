from __future__ import annotations
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[4]
EXTENSIONS = {".c", ".cc", ".cpp", ".h", ".hpp", ".py", ".rs", ".cmake", ".md", ".bat"}
SKIP = {".git", "build", "dist", ".pytest_cache", "__pycache__"}
files = [p for p in ROOT.rglob("*") if p.is_file() and p.suffix.lower() in EXTENSIONS and not any(part in SKIP for part in p.parts)]
print(json.dumps({"files_scanned": len(files), "total_lines": sum(len(p.read_text(encoding="utf-8", errors="ignore").splitlines()) for p in files)}))
