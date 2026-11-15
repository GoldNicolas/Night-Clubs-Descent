# Run S1-walk-20261114-152929
L-011 (2)–(4): S1 on the walk line (D-033), retrained from the full-line S1 snapshot, and its promotion test.
- Build: `build-25a7900` · Stage: S1 · Mobility: **walk** (`MOBILITY=walk` = `-Set="Combat.Abilities.bCrouch=false;Combat.Abilities.bJump=false"` on every train and eval process; every game logged `mobility=walk`)
- Partition `gpul40q` (`gpu011`, 1× L40S, 5 cores; not `gpuq`)
- Slurm jobs:
  - training 152929: 1,000 batches, 3.4 h;
  - chain jobs 152930–152931 exited at once (trigger already fired);
  - evals: 152928 (before-number), 153758 (gate) and 153759 (scripted), all CPU only.
- Started from: `run-S1-20261113-L009` (full line, promoted), all 4 files, `-StartDifficulty=1`.
- Config: 4 games × 32 arenas, `--batch-size 4`, build defaults plus the walk `-Set`. Trigger over **100 windows** (L-010).
- Release: [**`run-S1-walk-20261114-152929`**](https://github.com/GoldNicolas/FPS-Agent/releases/tag/run-S1-walk-20261114-152929) (prerelease, tag at `a75e9f4`): the 4 `.bin` files from `job_152929/snapshots_final` and `SHA256SUMS`, the files gate job 153758 evaluated. Re-downloaded and checked with `sha256sum -c`.

## Outcome
**PASS, narrowly: 73.0% clear on the walk line** (gate ≥ 70%).
- Without crouch and jump, the full-line S1 snapshot dropped to 62.0%. 1,000 batches on the walk line brought it back to 73.0%.
- The trigger fired in the first job: level 1.0 and pooled `clearTop` 75% over the last 100 windows (n = 5,731).
- Training `clearTop` rose slowly, from 70% to 75–76%. It was still rising at the end, so more walk-line training would likely add a few points.

## Promotion test
`-Stage=S1 -Eval -EvalEpisodes=200 -StartDifficulty=1 -Seed=1000 -Arenas=32`. JSON files are in [`S1-walk-20261114-152929/`](S1-walk-20261114-152929/).

| Agent | Mobility | Job | Clear | Kills / ep | Accuracy | Headshot % of hits | Clear time | Gate |
|---|---|---|---:|---:|---:|---:|---:|---|
| `S1-20261113-L009`, before (full-line snapshot) | walk | 152928 | 62.0% | 4.49 | 18.6% | 41.1% | 16.6 s | fail |
| **After 1,000 walk batches: released** | **walk** | **153758** | **73.0%** | 4.63 | 25.0% | 33.8% | 14.8 s | **pass** |
| `-Scripted` reference | walk | 153759 | 85.0% | 4.83 | 44.4% | 98.8% | 9.7 s | pass |
| `S1-20261113-L009` (for comparison) | full | 152873 | 76.0% | 4.62 | 29.5% | 14.1% | 17.6 s | pass |

- Against the full-line S1 snapshot: 3 points lower clear rate, but faster clears (14.8 s against 17.6 s) and more headshots (34% against 14%).
- The scripted reference doesn't crouch or jump, so its numbers match the full-line reference exactly.
- `botSpeed` was 286 cm/s in the gate; no deaths, since S1 bots don't shoot.

## Training timeline (job 152929)
Each block is 300 windows of 10 s of game time per game, over 4 games.

| windows | clear % | kills/ep | accuracy % | headshot % | clear time s | clearTop % (n) |
|---|---:|---:|---:|---:|---:|---:|
| 1–300 | 74.6 | 4.26 | 22.4 | 33.3 | 14.6 | 70.2 (16,399) |
| 601–900 | 75.3 | 4.26 | 22.6 | 32.4 | 14.9 | 71.2 (16,509) |
| 1201–1500 | 77.0 | 4.31 | 21.0 | 34.1 | 14.6 | 73.1 (17,024) |
| 1801–2100 | 79.3 | 4.33 | 20.7 | 38.8 | 14.4 | 75.7 (17,467) |

- Speed: **12.3 s per batch** (`PPO train` 6.4 s, `Pull Experience` 5.2 s); GPU median 20%, peak 26%.
- All 4 games and the trainer exited 0.

## Next
- LOCAL: watch the release; this is the walk-line agent the game uses (D-033).
- A second walk-line job would likely push the clear rate above the current 73%: training was still improving (+5 points of `clearTop` over the job).
