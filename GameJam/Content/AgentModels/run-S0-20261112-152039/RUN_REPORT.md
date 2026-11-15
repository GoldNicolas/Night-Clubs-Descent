# Run S0-20261112-152039
L-007 (3) and (4): the first S0 (aim gallery) training run on Cradle, and its promotion test.
- Build: `build-10520b5` · Stage: S0 · Node: `gpu001` (`gpuq`, 1× A100 80GB PCIe, 8 cores)
- Slurm jobs:
  - training **152039**: 300 batches from scratch, 86 min;
  - training **152055**: 300 more batches at level 1, 86 min;
  - chain jobs 152040 (exited at once: chain already promoted), 152041 and 152042 (cancelled; not needed);
  - promotion tests, CPU only:
    - 152053 / 152054: attempt 1, policy / scripted;
    - 152056: attempt 2, the gate;
    - 152057 / 152058: seed 2000, policy / scripted;
    - 152073: mid-continuation check.
- Started from: scratch
- Config:
  - D-030: N = 4 games × K = 32 arenas; seeds `JOBID*1000+i`; `train.py --batch-size 4`.
  - Every setting at the build's defaults (no `-Set`): `Combat: ppo discount=0.9950 gae=0.95 entropy=0.0000 lr=0.0001/0.001 network=2x256+mem128 ticksPerDecision=4 resetOnUpdate=0`; replay buffer 10,000 steps per game; 32 gradient iterations per batch.
  - Trainer: the pinned copy with the D-029 polls (repo `d2c860d`).
  - Script: [`cluster/slurm/combat.sbatch`](../../cluster/slurm/combat.sbatch) `MODE=train` (chain) and `MODE=eval`.
- Release with snapshots: [**`run-S0-20261112-152039`**](https://github.com/GoldNicolas/FPS-Agent/releases/tag/run-S0-20261112-152039) (prerelease): the 4 `.bin` files from `job_152055/snapshots_final` plus `SHA256SUMS`. These are the exact files the passing gate (job 152056) evaluated.
  - Published 2026-11-13, tag at `3100086`. CLUSTER downloaded it again into an empty folder; `sha256sum -c SHA256SUMS` passes for all 4 files.
  - SHA-256: `policy.bin` 4c899d2a…, `encoder.bin` 93457d75…, `decoder.bin` 8d57659c…, `critic.bin` 1af39368… (full hashes in the release's `SHA256SUMS`).

## Outcome
**PASS: S0 is promoted.** After 600 batches (2 jobs, 2.9 h of training), the final snapshot clears **98.5%** of the 200 fixed-seed level-1 episodes (`-Eval -EvalEpisodes=200 -StartDifficulty=1 -Seed=1000 -Arenas=32`; gate ≥ 70%).
- **Seed 2000** (robustness check): 98.0%. The `-Scripted` reference clears 100% with both seeds.
- **Learning:** the agent first cleared one bot around batch 85. The ramp then climbed from level 0 to 1.0 by about batch 290, and the promotion trigger (mean `level=` ≥ 0.9 for 10 windows, every game) fired at the end of job 152039.
- **Gate attempt 1** on that snapshot: **67.0% (fail)**. The trigger fires as soon as the ramp tops out, before the policy is good at level 1 (observation 4).
- **Continuation:** 300 more batches at level 1 raised the clear rate to 93.5% by its midpoint (seed 3000) and to 98.5% at the end.
- **Against the scripted reference:** the policy now clears in 4.1–4.5 s (scripted: 3.5 s) with 51–56% accuracy (scripted: 29%). The scripted bot fires only when on the head (99.8% headshots); the policy lands 50–54% headshots.

## Promotion test (L-007 (4))
All runs use `-Eval -EvalEpisodes=200 -StartDifficulty=1 -Arenas=32`, with mean actions, no ramp and no review episodes. The episode set depends on `-Seed` and `-Arenas`, so each policy row is compared with the scripted row of the same seed. JSON files are in [`S0-20261112-152039/`](S0-20261112-152039/).

| Agent | Job | Seed | Clear rate | Timeouts | Kills / episode | Accuracy | Headshot % of hits | Clear time | avgReturn | Gate (≥ 70%) |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| Policy after 152039 (300 batches) | 152053 | 1000 | 67.0% | 33.0% | 3.96 | 12.0% | 69.5% | 8.4 s | 9.13 | fail |
| Policy mid-152055 (about 450 batches; copy of the live snapshot) | 152073 | 3000 | 93.5% | 6.5% | 4.83 | 40.8% | 29.0% | 7.0 s | 11.67 | (check only) |
| **Policy after 152055 (600 batches): released** | **152056** | **1000** | **98.5%** | 1.5% | 4.95 | 56.3% | 54.3% | 4.1 s | 12.37 | **pass** |
| same | 152057 | 2000 | 98.0% | 2.0% | 4.94 | 51.0% | 50.3% | 4.5 s | 12.27 | pass |
| `-Scripted` reference | 152054 | 1000 | 100.0% | 0% | 5.00 | 29.3% | 99.8% | 3.5 s | 12.48 | pass |
| `-Scripted` reference | 152058 | 2000 | 100.0% | 0% | 5.00 | 29.2% | 99.8% | 3.5 s | 12.47 | pass |

No episode ended in a death: S0 bots don't shoot.

## Training timeline
From the games' `-StatsCsv` files, which append across both jobs; merged per window in [`stats-per-window.csv`](S0-20261112-152039/stats-per-window.csv) (rates episode-weighted over the 4 games).
- Windows are 10 s of game time per game; there are about 2.3 per batch.
- Windows 1–690 are job 152039 and 691–1,324 are job 152055.
- The level is the mean over the 128 arenas at the end of the block (min..max).

| windows | episodes | clear % | timeout % | avg return | kills/ep | accuracy % | headshot % of hits | clear time s | level |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 1–60 | 3,842 | 0.1 | 99.9 | −0.31 | 0.00 | 0.1 | 0.2 | 2.8 | 0.00 (0.00..0.00) |
| 61–120 | 3,840 | 0.3 | 99.7 | −0.16 | 0.00 | 0.4 | 2.8 | 7.2 | 0.00 (0.00..0.00) |
| 121–180 | 3,845 | 0.8 | 99.2 | −0.13 | 0.01 | 1.1 | 4.6 | 7.9 | 0.00 (0.00..0.00) |
| 181–240 | 4,910 | 39.2 | 60.8 | 1.14 | 0.39 | 6.2 | 6.7 | 8.7 | 0.02 (0.00..0.10) |
| 241–300 | 11,017 | 88.6 | 11.4 | 3.60 | 1.14 | 15.1 | 9.6 | 5.3 | 0.19 (0.10..0.25) |
| 301–360 | 10,539 | 90.5 | 9.5 | 4.94 | 1.77 | 20.4 | 10.0 | 5.9 | 0.35 (0.30..0.40) |
| 361–420 | 9,682 | 90.1 | 9.9 | 6.10 | 2.33 | 24.7 | 10.5 | 6.6 | 0.50 (0.45..0.55) |
| 421–480 | 8,769 | 86.9 | 13.1 | 6.78 | 2.68 | 23.3 | 19.8 | 7.0 | 0.63 (0.60..0.70) |
| 481–540 | 8,158 | 84.3 | 15.7 | 7.67 | 3.11 | 23.0 | 26.0 | 7.4 | 0.76 (0.65..0.80) |
| 541–600 | 7,979 | 83.7 | 16.3 | 8.41 | 3.45 | 21.9 | 38.1 | 7.6 | 0.87 (0.80..0.95) |
| 601–660 | 7,197 | 79.9 | 20.1 | 9.02 | 3.78 | 20.9 | 46.9 | 8.3 | 0.97 (0.90..1.00) |
| 661–720 | 7,032 | 81.8 | 18.2 | 9.61 | 4.04 | 22.2 | 51.5 | 8.7 | 1.00 (1.00..1.00) |
| 721–780 | 9,672 | 94.2 | 5.8 | 10.75 | 4.42 | 31.2 | 49.7 | 7.2 | 1.00 (1.00..1.00) |
| 781–840 | 11,880 | 97.4 | 2.6 | 11.16 | 4.53 | 38.4 | 51.5 | 6.1 | 1.00 (1.00..1.00) |
| 841–900 | 13,275 | 98.3 | 1.7 | 11.31 | 4.56 | 40.5 | 56.4 | 5.5 | 1.00 (1.00..1.00) |
| 901–960 | 13,298 | 98.5 | 1.5 | 11.35 | 4.57 | 45.2 | 47.3 | 5.6 | 1.00 (1.00..1.00) |
| 961–1020 | 11,815 | 97.9 | 2.1 | 11.17 | 4.53 | 44.1 | 40.6 | 6.2 | 1.00 (1.00..1.00) |
| 1021–1080 | 9,493 | 95.2 | 4.8 | 10.84 | 4.46 | 41.1 | 29.5 | 7.5 | 1.00 (1.00..1.00) |
| 1081–1140 | 10,511 | 96.7 | 3.3 | 11.05 | 4.51 | 49.1 | 24.7 | 6.9 | 1.00 (1.00..1.00) |
| 1141–1200 | 12,371 | 98.3 | 1.7 | 11.24 | 4.54 | 47.5 | 37.0 | 6.0 | 1.00 (1.00..1.00) |
| 1201–1260 | 13,912 | 99.1 | 0.9 | 11.36 | 4.57 | 39.6 | 61.2 | 5.4 | 1.00 (1.00..1.00) |
| 1261–1324 | 12,963 | 96.4 | 3.6 | 11.10 | 4.51 | 35.7 | 67.2 | 5.8 | 1.00 (1.00..1.00) |

- The training clear rate includes the 20% review episodes (a random level in [0, L]), so it reads higher than a fixed level-1 test while the policy is still weak at level 1 (windows 601–720 against gate attempt 1).
- The trainer's `Avg Rew Sum` went from −0.49 (batch 0) to +3.4 (batch 89), +6.9 (179) and +9.5 (299) in job 152039, then levelled off at about +11.2 in job 152055. The average episode length went 294 → 88 → 163 → 68 decisions.
- **Speed:** both jobs ran at 17.0–17.1 s per batch: `PPO train` 11.4–11.5 s (median), `Pull Experience` 4.1 s; GPU median 14–15%, peak 27%. Batches took 19 s while the untrained agents timed out in lockstep, and about 16.3 s once episodes de-synchronized, as Spike B finding 4 predicted.

## Play it back (LOCAL)
1. Download: `gh release download run-S0-20261112-152039 -D <folder>`, then `sha256sum -c SHA256SUMS` (or compare the hashes by hand on Windows).
2. Run a game built from `10520b5` with `-Stage=S0 -Infer -ResumeFrom=<folder>` (add `-Arenas=` / `-StartDifficulty=1` as wanted), or with `-Eval -EvalEpisodes=200 -StartDifficulty=1 -Seed=1000 -Arenas=32` to reproduce the gate.
   - `-Infer` / `-Eval` need `encoder.bin`, `policy.bin` and `decoder.bin`; `critic.bin` is only for resuming training.
   - Use the same build and network settings: a mismatched snapshot loads without an error and then scores 0.
3. This is the starting snapshot for S1 (spec §5: the next stage starts from it at L = 0).

## Problems / observations
1. **No failures.** All 8 training games exited 0 and both trainers exited 0, with no errors in the game logs. The only trainer output after each run is the cosmetic `_worker` shutdown traceback ([smoke report](s0smoke-20261112-152030.md), observation 4).
2. **Log volume is fine:** 2 MB (job 152039) and 3 MB (job 152055) per game, mostly `Resetting Agents` lines (about 16,000 and 26,000 per game) once episodes de-synchronized. The script gzips the logs at the end of each job; `-LogCmds` isn't needed.
3. **26–39 Learning Agents warnings per game per job:** `PPOTrainer_0: Agent with id N has completed episode and will be reset but has not generated any experience.`
   - Each comes one decision after that agent's previous reset, so the new episode ended on its first decision. That's about 0.1–0.2% of episodes.
   - The game resets the held action at every episode start (`UCombatArena::StartEpisode`).
   - So these are most likely genuine first-decision clears: the single bot of a low-level or review episode is roughly in front at spawn and dies to the first action within 4 ticks. Game 1's first stats window in job 152039 shows exactly that with an untrained policy: `clearTime=0.1s acc=100% hs=100%`.
   - Learning Agents drops such one-step episodes from the replay buffer. Harmless at this rate; no request filed.
4. **The promotion trigger fires before the policy meets the gate.**
   - "Mean level ≥ 0.9 for 10 windows" is reached as soon as the ramp tops out. The ramp keeps raising L while the training clear rate (with review episodes) is above 70%.
   - At job 152039's end, the level-1 clear rate was still 67%. It took 300 more batches at level 1 to reach 98.5%.
   - For S1+, CLUSTER will keep training for a while after the trigger before the gate. Alternatively, LOCAL could add a gate-like condition to the trigger: e.g. the non-review clear rate at L ≥ 0.9, logged separately.
5. **The headshot share swings while accuracy rises:** 69% in gate attempt 1 → 29% mid-continuation → 54% at the end; in training it varied between 25% and 67% at level 1. Headshot % is a metric, not a reward (CLAUDE.md), and body hits also kill, so the policy drifts between styles. That's worth knowing before S2, when bots shoot back and time-to-kill matters.
6. **One bookkeeping slip, fixed:** gate attempt 2 (152056) reused attempt 1's eval folder name.
   - The engine kept attempt 1's log as `game-backup-*.log`, and attempt 1's JSON was already copied here; only its `info.txt` was overwritten.
   - `combat.sbatch` now adds the job ID to every eval folder name.

## Next
- LOCAL: watch the released snapshot; S0 is promoted, so S1 (build B) can start from it at L = 0.
- CLUSTER: Spike B's revisit list (D-030): N = 4 against N = 8 per wall-clock hour, and `IterationsPerGather` / `PolicyBatchSize`. Do it on S1, where the trainer cost matters more.
