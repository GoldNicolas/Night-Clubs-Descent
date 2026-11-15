# Run S1-20261113-L009
L-009 (3)–(5): S1 ("Movers") training on Cradle and its promotion test.
- Build: `build-0eef702` · Stage: S1 · Partition **`gpul40q`** (`gpu011`, 1× L40S 48GB, 5 cores; `gpuq` is off-limits)
- Slurm jobs:
  - training 152848 (700 batches, 2.5 h) and continuation 152872 (700 batches at level 1);
  - chain jobs 152849–152851 exited at once (trigger already fired);
  - evals 152869, 152870, 152871 and 152873 (CPU only).
- Started from: the promoted S0 snapshot `run-S0-20261112-152039`, all 4 files. L-008 hasn't run. `-StartDifficulty=0`, no `-Set`.
- Config: D-030 shape, 4 games × 32 arenas, `--batch-size 4`; build defaults (30 s episodes, agent moves); pinned trainer with the D-029 polls.
- The run ID uses `L009` instead of a job ID: the chain was resubmitted with a fixed ID after a submit slip.
- Release: [**`run-S1-20261113-L009`**](https://github.com/GoldNicolas/FPS-Agent/releases/tag/run-S1-20261113-L009) (prerelease, tag at `442616a`): the 4 `.bin` files from `job_152872/snapshots_final` and `SHA256SUMS`, the files gate job 152873 evaluated. Re-downloaded and checked with `sha256sum -c`.

## Outcome
**PASS: S1 is promoted, at 76.0% clear on the gate** (≥ 70%), after 1,400 batches.
- Job 152848 climbed the ramp from level 0 to 1.0, and the trigger fired (mean level ≥ 0.9 and pooled `clearTop` 75.5%, n = 486).
- Its gate attempt failed at 65.0%: same pattern as S0, where the trigger fires before the gate is met.
- 700 more batches at level 1 raised the training `clearTop` from about 66% to 72–73%, and the gate to 76.0%.
- The policy is still well behind the scripted reference: 17.6 s per clear against 9.7 s, accuracy 30% against 44%.

## Promotion test
`-Stage=S1 -Eval -EvalEpisodes=200 -StartDifficulty=1 -Seed=1000 -Arenas=32`. JSON files are in [`S1-20261113-L009/`](S1-20261113-L009/).

| Agent | Job | Clear | Kills / ep | Accuracy | Headshot % of hits | Clear time | Gate |
|---|---|---:|---:|---:|---:|---:|---|
| S0 snapshot, before S1 training | 152870 | 24.0% | — | 13.5% | 25.9% | 18.2 s | fail |
| Policy after 152848 (700 batches) | 152869 | 65.0% | — | 29.5% | 16.5% | 16.5 s | fail |
| **Policy after 152872 (1,400 batches): released** | **152873** | **76.0%** | 4.62 | 29.5% | 14.1% | 17.6 s | **pass** |
| `-Scripted` reference | 152871 | 85.0% | — | 44.4% | 98.8% | 9.7 s | pass |

`botSpeed` is 243–303 cm/s in every eval; the bots move. No deaths, since S1 bots don't shoot.

## Training timeline
From `combat_report.py progress` on the run's stats CSVs. Each block is 200 windows of 10 s of game time per game, over 4 games. Windows 1–1,332 are job 152848; 1,333–2,821 are job 152872.

| windows | clear % | avg return | kills/ep | accuracy % | clear time s | level | clearTop % (n) | botSpeed |
|---|---:|---:|---:|---:|---:|---|---:|---:|
| 1–100 | 98.5 | 4.36 | 1.41 | 21.2 | 6.7 | 0.26 | — | 202 |
| 401–500 | 76.8 | 7.37 | 3.24 | 21.6 | 15.9 | 0.74 | — | 238 |
| 801–900 | 67.7 | 8.05 | 3.61 | 21.7 | 16.9 | 0.87 | 67.4 (1,261) | 252 |
| 1201–1300 | 71.0 | 9.35 | 4.18 | 27.9 | 16.8 | 0.99 | 66.0 (4,682) | 271 |
| 1601–1800 | 74.2 | 9.63 | 4.28 | 28.8 | 16.6 | 1.00 | 69.7 (10,234) | 275 |
| 2201–2400 | 77.0 | 9.75 | 4.30 | 29.2 | 16.2 | 1.00 | 73.1 (10,523) | 279 |
| 2601–2800 | 77.1 | 9.77 | 4.31 | 29.7 | 16.2 | 1.00 | 73.2 (10,577) | 275 |

The second job levelled off by about window 2,000. More of the same training would add little; the next gains likely need changes, not more batches.

## Speed (this run doubles as Spike B on S1, L-009 (2))
- **12.4–12.7 s per batch** at 4 × 32 on `gpul40q`: `PPO train` 6.3–6.4 s (median), `Pull Experience` 5.1–5.3 s. GPU median 10–16%, peak 26%.
- About 1,330–1,350 decisions/s per game at level 1 (2,500 at the low levels).
- Against S0 on `gpuq`'s A100 node (17 s per batch, `PPO train` 11.4 s): the update is CPU-overhead-bound, and `gpu011`'s CPU halves it. The games are slower (moving bots, A*, hearing), so filling the buffer takes longer.
- 700 batches is about 2.5 h, so a 4 h job holds about 1,000 batches.

## Observations
1. All 8 games and both trainers exited 0, with no errors.
2. The promotion trigger fired before the gate passed again (65%), as in S0. The new `clearTop` condition helps, but it was met on only the last 10 windows (75.5%, n = 486); over 1,000+ windows it was 66%. **A longer trigger window (e.g. 100 windows) would match the gate better.** That's LOCAL's call (D-031 (6)); CLUSTER's `PROMOTE_WINDOWS` can do it on our side.
3. The headshot share fell to 14% (S0 gate: 54%). The moving policy body-shoots; headshots aren't rewarded (CLAUDE.md).

## Next
- LOCAL: watch the release. S2 (bots shoot) would start from it.
- L-008 (S0 with movement) is still open; S1 already trains a moving agent, so L-008 may no longer be needed (LOCAL's call).
