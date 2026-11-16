# Pixelbridge — instructions for Claude

## Project memory

- Memory lives in `.claude/memory/`; start at [memory/index.md](memory/index.md).
- Read the index and the relevant memory file before inspecting source broadly.
- Memory is a map, not the truth: verify against the source before relying on it.
- After a meaningful change (modules, interfaces, build, dependencies, decisions, known issues), update the affected memory file.
- Every Markdown file in `.claude/memory/` must stay at or under 200 lines; split by topic when needed and update the index.

## Project skills

- Canonical reusable workflows live in `.claude/Skills/`. Read the relevant `SKILL.md` before doing LIAS or project-memory work; do not duplicate the skill instructions elsewhere.
- `lias-local-intelligence-automation` is implementation guidance only. Do not claim that `.claude/lias/`, hooks, or background automation are active unless they have been implemented and verified.
- LIAS commands are in `.claude/lias/cli.py`; start with `python .claude/lias/cli.py doctor`. Its client integration is on-demand only unless a supported hook is explicitly configured and tested.

## Local-first workflow

- For repeatable or deterministic work, use LIAS before spending model effort: search relevant verified knowledge with `python .claude/lias/cli.py knowledge search "<keywords>"`, then inspect or run a registered safe automation with `python .claude/lias/cli.py automation list` and `... automation run <id>`.
- Prefer the compact structured result from local execution. Use reasoning only to choose, validate, interpret, or extend a capability, and do not claim that an unsupported client hook automatically intercepted the request.
- Keep Python automations project-local under `.claude/lias/`; do not create background processes, cloud dependencies, or unverified generated scripts.

## Working rules

- Build and test only through `build.bat` (it orders PATH so Qt's MinGW is used, not MSYS2's GCC).
- Prefer the local tools over ad-hoc work: `build.bat check|build|test|deploy`, `python main.py env|fixtures|benchmark`.
- Never scan `C:` or `D:` recursively; dependency locations are recorded in `docs/build.md`.
- Never run `tools/windows/context_menu.ps1 -Action install` or `uninstall` without the user's explicit approval. `status` and `-DryRun` are safe.
- Do not commit or push without explicit approval.
- Do not document something as working unless a test or a manual check in this repo shows it.
