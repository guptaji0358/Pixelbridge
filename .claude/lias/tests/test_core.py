from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

LIAS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(LIAS_DIR))
import core  # noqa: E402


class LiasCoreTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / "project"
        self.root.mkdir()
        self.old = core.ROOT, core.LIAS, core.CONFIG_PATH, core.DATABASE_PATH
        core.ROOT = self.root
        core.LIAS = self.root / ".claude" / "lias"
        core.CONFIG_PATH = core.LIAS / "config" / "config.json"
        core.DATABASE_PATH = core.LIAS / "database" / "lias.sqlite3"

    def tearDown(self) -> None:
        core.ROOT, core.LIAS, core.CONFIG_PATH, core.DATABASE_PATH = self.old
        self.temp.cleanup()

    def test_verified_knowledge_persists_and_searches(self) -> None:
        record = core.add_knowledge("build", "Build with build.bat test", "test", "verified")
        found = core.search_knowledge("build test")
        self.assertEqual([record], [item["id"] for item in found])
        self.assertEqual("verified", found[0]["verification"])

    def test_secret_like_knowledge_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            core.add_knowledge("note", "api_key=do-not-store", "test", "verified")

    def test_registry_rejects_unsafe_working_directory(self) -> None:
        manifest = {"id": "unsafe", "name": "unsafe", "description": "unsafe", "command": [sys.executable, "-V"],
                    "working_directory": "..", "timeout_seconds": 1, "risk_tier": 0, "version": "1"}
        with self.assertRaises(ValueError):
            core.register_automation(manifest)

    def test_registered_tier_zero_automation_returns_structured_output(self) -> None:
        script = self.root / "stats.py"
        script.write_text("import json; print(json.dumps({'ok': True}))", encoding="utf-8")
        core.register_automation({"id": "stats", "name": "stats", "description": "test", "command": [sys.executable, str(script)],
                                  "working_directory": ".", "timeout_seconds": 5, "risk_tier": 0, "version": "1"})
        result = core.run_automation("stats")
        self.assertEqual("success", result["status"])
        self.assertTrue(json.loads(result["result"]["stdout"])["ok"])

    def test_bootstrap_registers_local_maintenance_reports(self) -> None:
        core.bootstrap()
        ids = {item["id"] for item in core.list_automations()}
        self.assertTrue({"source-statistics", "repository-health", "cmake-cache-doctor", "memory-audit", "release-readiness", "failure-quarantine"} <= ids)


if __name__ == "__main__":
    unittest.main()
