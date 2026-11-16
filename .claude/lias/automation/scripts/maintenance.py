"""Read-only LIAS maintenance automations. Prints exactly one JSON object."""
from __future__ import annotations
import argparse, json, re, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SKIP = {".git", "build", "dist", "__pycache__", ".pytest_cache"}

def files(*suffixes: str):
    return [p for p in ROOT.rglob("*") if p.is_file() and p.suffix.lower() in suffixes and not any(x in SKIP for x in p.parts)]

def repo_health():
    status = subprocess.run(["git", "status", "--porcelain"], cwd=ROOT, text=True, capture_output=True, check=False)
    return {"changed": len(status.stdout.splitlines()), "untracked": sum(x.startswith("??") for x in status.stdout.splitlines()), "large_files": [str(p.relative_to(ROOT)) for p in files(".cpp", ".py", ".md") if p.stat().st_size > 250_000]}

def cmake_cache_doctor():
    cache = ROOT / "build" / "CMakeCache.txt"
    if not cache.exists(): return {"cache": "absent", "action": "configure through build.bat"}
    text = cache.read_text(encoding="utf-8", errors="ignore")
    stale = [line for line in text.splitlines() if "_DIR:" in line and "Pixelbridge" not in line and "Pendiing" in line]
    return {"cache": "present", "stale_project_paths": stale, "clean_required": bool(stale)}

def memory_audit():
    memory = ROOT / ".claude" / "memory"; markdown = list(memory.glob("*.md"))
    over = [p.name for p in markdown if len(p.read_text(encoding="utf-8").splitlines()) > 200]
    index = (memory / "index.md").read_text(encoding="utf-8") if (memory / "index.md").exists() else ""
    missing = [p.name for p in markdown if p.name != "index.md" and p.name not in index]
    return {"files": len(markdown), "over_line_limit": over, "missing_from_index": missing}

def docs_links():
    bad=[]
    for p in files(".md"):
        text=p.read_text(encoding="utf-8", errors="ignore")
        for target in re.findall(r"\[[^]]+\]\(([^)#]+)(?:#[^)]+)?\)", text):
            if "://" not in target and not (p.parent / target).exists(): bad.append(f"{p.relative_to(ROOT)} -> {target}")
    return {"broken_local_links": bad, "checked_markdown": len(files(".md"))}

def build_config_audit():
    cmake="\n".join(p.read_text(encoding="utf-8", errors="ignore") for p in [ROOT/"CMakeLists.txt", ROOT/"gui"/"gui.cmake"] if p.exists())
    sources=files(".cpp", ".c")
    return {"unlisted_sources": [str(p.relative_to(ROOT)) for p in sources if p.name not in cmake]}

def context_menu_audit():
    doc=ROOT/"docs"/"context-menu.md"; text=doc.read_text(encoding="utf-8", errors="ignore") if doc.exists() else ""
    return {"mentions_old_name": "QRStudio.exe" in text, "mentions_old_root": "Pendiing/QR" in text or "Pendiing\\QR" in text, "installation_changes_registry": True}

def release_readiness():
    return {"repository": repo_health(), "build_directory_exists": (ROOT/"build").exists(), "dist_directory_exists": (ROOT/"dist").exists(), "docs": docs_links()}

def knowledge_candidates():
    return {"candidates": [{"source": "docs/build.md", "category": "build", "requires_verification": True}], "note": "No knowledge is stored automatically."}

def automation_score():
    return {"candidates": [{"id": x, "risk": "Tier 0", "promotion": "implemented"} for x in ["repository-health", "cmake-cache-doctor", "memory-audit", "docs-link-check", "build-config-audit", "context-menu-audit", "release-readiness"]]}

def failure_quarantine():
    return {"proposed_quarantines": [], "note": "Report-only: LIAS does not disable automations automatically."}

def benchmark_history():
    return {"runs_recorded": 0, "note": "Report-only until benchmark input and retention policy are explicitly configured."}

OPS={"repository-health":repo_health,"cmake-cache-doctor":cmake_cache_doctor,"memory-audit":memory_audit,"docs-link-check":docs_links,"build-config-audit":build_config_audit,"context-menu-audit":context_menu_audit,"release-readiness":release_readiness,"knowledge-candidate-scan":knowledge_candidates,"automation-candidate-score":automation_score,"failure-quarantine":failure_quarantine,"benchmark-history":benchmark_history,"test-result-summary":lambda:{"note":"Provide a build log path in a later revision; no log is read by default."}}
parser=argparse.ArgumentParser(); parser.add_argument("operation", choices=OPS); args=parser.parse_args(); print(json.dumps(OPS[args.operation](), sort_keys=True))
