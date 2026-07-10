---
name: append-local-changes
description: >-
  After making code or config changes in this repo, append concise bullet
  summaries to local-changes-summary.md grouped by date. Use on every task
  that modifies project files, when finishing implementation, or when the
  user mentions local change log, change summary, or 改动说明.
---

# Append Local Changes Summary

## File

- Path: `local-changes-summary.md` (project root)
- **Never commit** this file; it is listed in `.gitignore`

## When to run

After **every** task that changes project files (code, config, assets wiring, etc.), before ending the turn:

1. Summarize what changed in 1–5 short bullets (one line each).
2. Append those bullets under **today's date** in `local-changes-summary.md`.

Skip only if the task made **zero** file changes.

## Format

```markdown
# Local Changes Summary

## YYYY-MM-DD

- 简短说明改动 1
- 简短说明改动 2
```

Rules:

- Date heading: `## YYYY-MM-DD` (use the machine's current local date).
- Each change: one `- ` bullet, one line, concise Chinese (unless the user wrote the task in English).
- **Same day**: append new bullets **below** the last bullet under that date; do not duplicate the date heading.
- **New day**: insert a new `## YYYY-MM-DD` section **immediately below** `# Local Changes Summary` (newest date on top).
- Do not rewrite or delete existing bullets unless the user explicitly asks.

## Example

Today is 2026-07-10 and the file already has that section:

```markdown
## 2026-07-10

- 已有条目 A
```

After fixing a bug in `home_screen.cc`:

```markdown
## 2026-07-10

- 已有条目 A
- 修复主屏空闲计时在超时为 0 时仍打印剩余时间的问题
```

## Checklist

- [ ] Bullets describe **why/what**, not raw file lists
- [ ] Appended under today's date (create section if missing)
- [ ] Did not stage or commit `local-changes-summary.md`
