# QR Studio — instructions for Claude

## Project memory

- Memory lives in `.claude/memory/`; start at [memory/index.md](memory/index.md).
- Read the index and the relevant memory file before inspecting source broadly.
- Memory is a map, not the truth: verify against the source before relying on it.
- After a meaningful change (modules, interfaces, build, dependencies, decisions, known issues), update the affected memory file.
- Every Markdown file in `.claude/memory/` must stay at or under 200 lines; split by topic when needed and update the index.

## Working rules

- Build and test only through `build.bat` (it orders PATH so Qt's MinGW is used, not MSYS2's GCC).
- Prefer the local tools over ad-hoc work: `build.bat check|build|test|deploy`, `python main.py env|fixtures|benchmark`.
- Never scan `C:` or `D:` recursively; dependency locations are recorded in `docs/build.md`.
- Never run `tools/windows/context_menu.ps1 -Action install` or `uninstall` without the user's explicit approval. `status` and `-DryRun` are safe.
- Do not commit or push without explicit approval.
- Do not document something as working unless a test or a manual check in this repo shows it.
