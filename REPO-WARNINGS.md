# Repo warnings (read before staging anything)

Recorded 2026-11-14 by the FPS-Agent session, after a near-miss in this repo.

## Status
- **#1 fixed 2026-11-14:** a root `.gitignore` now ignores `Binaries/`, `Intermediate/`, `Saved/` and `DerivedDataCache/` at any depth, plus `.vs/`, `*.sln`, `*.VC.db` and `.env`. Source, Config, Content and the `.uproject` are still tracked.
- **#2 fixed 2026-11-14:** `git prune` removed the 10,319 orphaned objects (about 783 MB).
- **Still open:** Git LFS for `*.uasset` and `*.umap`. It's the game session's call, because it changes storage and GitHub quota. #3 and #4 stay as reminders.

## 1. There was no `.gitignore` (fixed; see Status)
The Unreal project `GameJam/` produces large generated folders that must never be committed:
- `Binaries/`, `Intermediate/` (it already holds a full Python `PipInstall`), `Saved/`, `DerivedDataCache/`;
- `*.sln`, `.vs/`, `*.VC.db`;
- packaged builds.

A `git add -A` on 2026-11-14 began staging `GameJam/Intermediate/...` before it was stopped. No commit was made, and no files were changed.

**Do:**
- add an Unreal `.gitignore` at the repo root covering the list above;
- use Git LFS for `*.uasset` and `*.umap` (`.gitattributes`) if they are versioned.

Until then, stage files **by name** (`git add -- PLAN.md`), never with `git add -A` or `git add .`.

## 2. Leftover git objects from the interrupted `git add`
- `git` prints "There are too many unreachable loose objects". The interrupted staging wrote blobs that nothing references.
- They are harmless. `git prune` (or `git gc`) removes them once no other git command is running.

## 3. A stale `.git/index.lock` means a git command was killed
- If git says another process is running and **no `git.exe` is running**, delete the empty `.git/index.lock`.
- Never delete it while a git command is still running.

## 4. Contract reminders (from FPS-Agent)
- The game's enemies use the **`walk`** model line: no crouch, no jump. See `FPS-Agent-Constraints-v1.2-addendum.md`.
- The current game snapshot is `run-S1-walk-20261114-152929`. A full-line snapshot with crouch and jump masked is **not** a stand-in: it drops to 62% clear.
