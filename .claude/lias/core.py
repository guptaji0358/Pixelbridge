from __future__ import annotations

import hashlib
import json
import os
import re
import sqlite3
import subprocess
import sys
import time
import uuid
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
LIAS = Path(__file__).resolve().parent
CONFIG_PATH = LIAS / "config" / "config.json"
DATABASE_PATH = LIAS / "database" / "lias.sqlite3"
DEFAULT_CONFIG = {
    "schema_version": 1,
    "max_timeout_seconds": 60,
    "max_output_bytes": 65536,
    "max_search_results": 10,
    "retry_limit": 0,
}
SECRET_PATTERN = re.compile(r"(?i)(api[_-]?key|secret|password|token)\s*[:=]")


def utcnow() -> str:
    return datetime.now(UTC).isoformat(timespec="seconds")


def project_id(root: Path = ROOT) -> str:
    return hashlib.sha256(str(root.resolve()).lower().encode()).hexdigest()[:16]


def atomic_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def config() -> dict[str, Any]:
    if not CONFIG_PATH.exists():
        atomic_json(CONFIG_PATH, DEFAULT_CONFIG)
    data = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
    if data.get("schema_version") != 1:
        raise RuntimeError("unsupported LIAS config schema")
    return {**DEFAULT_CONFIG, **data}


def connect() -> sqlite3.Connection:
    config()
    DATABASE_PATH.parent.mkdir(parents=True, exist_ok=True)
    con = sqlite3.connect(DATABASE_PATH)
    con.row_factory = sqlite3.Row
    con.execute("PRAGMA foreign_keys = ON")
    con.executescript("""
        CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
        CREATE TABLE IF NOT EXISTS knowledge (
          id TEXT PRIMARY KEY, category TEXT NOT NULL, content TEXT NOT NULL,
          source TEXT NOT NULL, verification TEXT NOT NULL CHECK(verification IN ('verified','unverified','invalid')),
          created_at TEXT NOT NULL, updated_at TEXT NOT NULL, expires_at TEXT,
          project_id TEXT NOT NULL, supersedes TEXT
        );
        CREATE VIRTUAL TABLE IF NOT EXISTS knowledge_fts USING fts5(id UNINDEXED, content);
        CREATE TABLE IF NOT EXISTS automation (
          id TEXT PRIMARY KEY, name TEXT NOT NULL, description TEXT NOT NULL,
          command_json TEXT NOT NULL, working_directory TEXT NOT NULL,
          timeout_seconds INTEGER NOT NULL, risk_tier INTEGER NOT NULL,
          version TEXT NOT NULL, status TEXT NOT NULL CHECK(status IN ('active','inactive','quarantined')),
          created_at TEXT NOT NULL, updated_at TEXT NOT NULL, project_id TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS execution (
          id TEXT PRIMARY KEY, automation_id TEXT NOT NULL, status TEXT NOT NULL,
          started_at TEXT NOT NULL, duration_ms INTEGER NOT NULL, result_json TEXT NOT NULL,
          FOREIGN KEY(automation_id) REFERENCES automation(id)
        );
    """)
    con.execute("INSERT OR IGNORE INTO metadata(key,value) VALUES('schema_version','1')")
    con.commit()
    return con


def add_knowledge(category: str, content: str, source: str, verification: str = "unverified", expires_at: str | None = None) -> str:
    if verification not in {"verified", "unverified"}:
        raise ValueError("verification must be verified or unverified")
    if not category.strip() or not content.strip() or not source.strip():
        raise ValueError("category, content, and source are required")
    if SECRET_PATTERN.search(content):
        raise ValueError("refusing content that appears to contain a secret")
    record_id, now = str(uuid.uuid4()), utcnow()
    con = connect()
    try:
        con.execute("INSERT INTO knowledge VALUES(?,?,?,?,?,?,?,?,?,NULL)",
                    (record_id, category.strip(), content.strip(), source.strip(), verification, now, now, expires_at, project_id()))
        con.execute("INSERT INTO knowledge_fts(id,content) VALUES(?,?)", (record_id, content.strip()))
        con.commit()
    finally:
        con.close()
    return record_id


def search_knowledge(query: str, limit: int | None = None) -> list[dict[str, Any]]:
    words = re.findall(r"[\w-]+", query)
    if not words:
        raise ValueError("query must contain a word")
    limit = min(limit or config()["max_search_results"], config()["max_search_results"])
    match = " AND ".join(f'"{word}"' for word in words)
    con = connect()
    try:
        rows = con.execute("""SELECT k.*, bm25(knowledge_fts) AS score FROM knowledge_fts
            JOIN knowledge k ON k.id=knowledge_fts.id
            WHERE knowledge_fts MATCH ? AND k.project_id=? AND k.verification != 'invalid'
              AND (k.expires_at IS NULL OR k.expires_at > ?) ORDER BY score LIMIT ?""",
            (match, project_id(), utcnow(), limit)).fetchall()
    finally:
        con.close()
    return [dict(row) for row in rows]


def register_automation(manifest: dict[str, Any]) -> None:
    required = {"id", "name", "description", "command", "working_directory", "timeout_seconds", "risk_tier", "version"}
    if missing := required - manifest.keys():
        raise ValueError(f"manifest missing: {', '.join(sorted(missing))}")
    if not re.fullmatch(r"[a-z0-9-]+", manifest["id"]):
        raise ValueError("automation id must use lowercase letters, digits, and hyphens")
    if not isinstance(manifest["command"], list) or not manifest["command"] or not all(isinstance(x, str) for x in manifest["command"]):
        raise ValueError("command must be a non-empty argument array")
    work = (ROOT / manifest["working_directory"]).resolve()
    if ROOT not in work.parents and work != ROOT:
        raise ValueError("working_directory escapes the project root")
    timeout = int(manifest["timeout_seconds"])
    if not 1 <= timeout <= config()["max_timeout_seconds"]:
        raise ValueError("timeout is outside policy")
    if int(manifest["risk_tier"]) != 0:
        raise ValueError("only read-only Tier 0 automations may be activated automatically")
    now = utcnow()
    con = connect()
    try:
        existing = con.execute("SELECT command_json FROM automation WHERE id=?", (manifest["id"],)).fetchone()
        encoded = json.dumps(manifest["command"])
        if existing and existing["command_json"] != encoded:
            raise ValueError("automation id already has a different command")
        con.execute("""INSERT INTO automation VALUES(?,?,?,?,?,?,?,?,?,?,?,?)
          ON CONFLICT(id) DO UPDATE SET name=excluded.name,description=excluded.description,updated_at=excluded.updated_at
        """, (manifest["id"], manifest["name"], manifest["description"], encoded, manifest["working_directory"], timeout,
              0, manifest["version"], "active", now, now, project_id()))
        con.commit()
    finally:
        con.close()


def bootstrap() -> None:
    script = LIAS / "automation" / "scripts" / "source_stats.py"
    register_automation({"id": "source-statistics", "name": "Source statistics", "description": "Counts project source files and lines.",
        "command": [sys.executable, str(script)], "working_directory": ".", "timeout_seconds": 20, "risk_tier": 0, "version": "1"})
    maintenance = LIAS / "automation" / "scripts" / "maintenance.py"
    names = ("repository-health", "cmake-cache-doctor", "test-result-summary", "memory-audit", "docs-link-check", "build-config-audit", "context-menu-audit", "release-readiness", "knowledge-candidate-scan", "automation-candidate-score", "failure-quarantine", "benchmark-history")
    for name in names:
        register_automation({"id": name, "name": name.replace("-", " ").title(), "description": f"Local report for {name}.", "command": [sys.executable, str(maintenance), name], "working_directory": ".", "timeout_seconds": 20, "risk_tier": 0, "version": "1"})


def list_automations() -> list[dict[str, Any]]:
    con = connect()
    try:
        return [dict(row) for row in con.execute("SELECT id,name,description,status,version,timeout_seconds FROM automation WHERE project_id=? ORDER BY id", (project_id(),))]
    finally:
        con.close()


def run_automation(automation_id: str) -> dict[str, Any]:
    con = connect()
    try:
        row = con.execute("SELECT * FROM automation WHERE id=? AND project_id=?", (automation_id, project_id())).fetchone()
    finally:
        con.close()
    if not row or row["status"] != "active":
        raise ValueError("active automation not found")
    work = (ROOT / row["working_directory"]).resolve()
    if ROOT not in work.parents and work != ROOT:
        raise ValueError("stored automation working directory is unsafe")
    start = time.perf_counter()
    try:
        proc = subprocess.run(json.loads(row["command_json"]), cwd=work, capture_output=True, text=True, timeout=row["timeout_seconds"], shell=False)
        status = "success" if proc.returncode == 0 else "failed"
        result = {"returncode": proc.returncode, "stdout": proc.stdout[:config()["max_output_bytes"]], "stderr": proc.stderr[:config()["max_output_bytes"]]}
    except subprocess.TimeoutExpired as exc:
        status, result = "timeout", {"stdout": (exc.stdout or "")[:config()["max_output_bytes"]], "stderr": (exc.stderr or "")[:config()["max_output_bytes"]]}
    output = {"automation_id": automation_id, "status": status, "duration_ms": round((time.perf_counter() - start) * 1000), "result": result}
    con = connect()
    try:
        con.execute("INSERT INTO execution VALUES(?,?,?,?,?,?)", (str(uuid.uuid4()), automation_id, status, utcnow(), output["duration_ms"], json.dumps(output)))
        con.commit()
    finally:
        con.close()
    return output


def doctor() -> dict[str, Any]:
    con = connect()
    try:
        integrity = con.execute("PRAGMA integrity_check").fetchone()[0]
        knowledge = con.execute("SELECT count(*) FROM knowledge WHERE project_id=?", (project_id(),)).fetchone()[0]
        automations = con.execute("SELECT count(*) FROM automation WHERE project_id=?", (project_id(),)).fetchone()[0]
    finally:
        con.close()
    return {"version": "0.1.0", "root": str(ROOT), "database": str(DATABASE_PATH), "integrity": integrity, "knowledge_records": knowledge, "automations": automations, "hooks": "not configured; no supported automatic client hook was detected"}
