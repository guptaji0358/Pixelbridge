# LIAS runtime

LIAS is a lightweight, project-local knowledge and safe-automation runtime. Its
state remains below `.claude/lias/`; it has no cloud dependency, no daemon, and
no automatic prompt interception.

## Use

```powershell
python .claude/lias/cli.py bootstrap
python .claude/lias/cli.py doctor
python .claude/lias/cli.py knowledge add build "build.bat test is verified" --source docs/build.md --verified
python .claude/lias/cli.py knowledge search "build test"
python .claude/lias/cli.py automation run source-statistics
```

`bootstrap` registers only the read-only `source-statistics` automation. Every
automation is an argument array, has a project-root working directory and bounded
timeout, and is checked against the project-local registry before execution.

Knowledge is explicitly marked `verified` or `unverified`. LIAS rejects likely
secrets and never treats unverified facts as confirmed. SQLite FTS5 provides local
keyword retrieval; there is no external embedding service.

## Integration and limits

`.claude/CLAUDE.md` is the client-facing linker. The current client environment
does not expose a verified Claude hook for automatic request interception, so
retrieval and automation run through this CLI (or an agent that chooses to invoke
it). Hooks are deliberately not installed.

Run all project tests with `build.bat test`; that includes the isolated LIAS unit
tests. Runtime databases, logs and caches are intentionally untracked local state.
