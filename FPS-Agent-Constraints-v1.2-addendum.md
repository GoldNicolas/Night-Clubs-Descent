# FPS-Agent contract v1.2 addendum (hand this to the game project)

Apply this on top of the game's `FPS-Agent-Constraints.md` **v1.1**. Together they make **contract v1.2**. Date: 2026-11-14. Source: FPS-Agent decision D-033, build `build-25a7900`. **The observation/action layout is unchanged:** same network shape, and every v1.1 snapshot stays valid.

## 1. Mobility lines (new)
- FPS-Agent now trains two **model lines**, using two settings switches:
  - `Combat.Abilities.bCrouch`, default true;
  - `Combat.Abilities.bJump`, default true.
- When a switch is off in a run's `-Set`, the policy's output for that action was **ignored in training**, exactly like S0 ignores movement. The game MUST drop that output too.

| Mobility tag | Meaning | `-Set` in the run report | Game applies |
|---|---|---|---|
| `static` | No movement (default S0) | none (`Curriculum.S0.bAgentCanMove` false) | Aim, Fire, Reload only |
| **`walk`** | Move + walk + shoot; **no crouch, no jump** | `Combat.Abilities.bCrouch=false;Combat.Abilities.bJump=false` | Move, Walk, Aim, Fire, Reload |
| `full` | Everything | none | All actions |

- **The game's enemies use the `walk` line.** They move, walk, aim, fire and reload, and never crouch or jump.
- Add a `mobility` field to the model registry. The Agent Director drops Crouch and Jump for `walk` and `static` snapshots.

## 2. Run IDs carry the tag
- Format: `<stage>-<mobility>-<YYYYMMDD>-<jobid>`, for example `S1-walk-20261114-153001`. The release is `run-<run-id>`.
- Older runs keep their names: `S0-20261112-152039` is `static`, and `S1-20261113-L009` is `full`.
- To confirm a snapshot's line:
  - its run report lists the `-Set`;
  - the FPS-Agent log line `Combat: ready ... mobility=walk` shows the tag;
  - its eval JSON has `"mobility": "walk"`.

## 3. Using a `full` snapshot with crouch and jump dropped
This was measured for S1: the `full` snapshot `S1-20261113-L009` cleared **77.5%** with crouch and jump masked (40 episodes, level 1, local check), against 76% unmasked (200 episodes, cluster gate). It barely uses them, so it works as a stand-in. Prefer a `walk` snapshot once one is promoted.

## 4. Changes to v1.1 sections
- **§1.1 stage capability matrix:** the Crouch and Jump outputs are additionally dropped for `walk` snapshots, at every stage.
- **§10 action contract:** Crouch and Jump are applied only when the snapshot's mobility allows them. Otherwise the agent stays uncrouched and never jumps.
- **§16 acceptance tests,** add: **10. Walk line.** A `walk` snapshot's agent never crouches or jumps, even when the policy outputs Crouch or Jump.
- **§18 checklist,** add: mobility read from the registry, and Crouch/Jump dropped for `walk` and `static`.
