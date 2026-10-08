# Claude Code notes

The rules for this repository are in `AGENTS.md` (canonical, harness-neutral). Read it, then `docs/STATUS.md`.

Claude-specific:

- Use the Write and Edit tools for files; never route file content through a shell command.
- Shell commands take forward-slash paths. Build with `tools/build/msvc.cmd all` through the PowerShell tool or Bash.
- `oracle/js/CLAUDE.md` belongs to the frozen oracle; it describes that package, not this repository.
- Before ending a session, update `docs/STATUS.md` and write `docs/sessions/<date>-<slug>.md`.
