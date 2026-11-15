# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An Unreal Engine 5.7 C++ first-person run-and-gun game (game jam; title undecided) whose enemies are **trained RL policies from a separate private project, FPS-Agent**, run through UE's Learning Agents plugin (inference only). The UE project lives in `GameJam/` (`GameJam.uproject`); the repo root holds the design docs.

Read before touching enemy/agent code, player body/movement/sounds, collision, spawning or level geometry:
- [PLAN.md](PLAN.md): game design, proposed architecture, milestones, open decisions. Confirmed vs Proposed vs Open is deliberate; don't promote open ideas to requirements or invent lore.
- [FPS-Agent-Constraints.md](FPS-Agent-Constraints.md) (v1.1) + [FPS-Agent-Constraints-v1.2-addendum.md](FPS-Agent-Constraints-v1.2-addendum.md) = **contract v1.2**, the authority on observation/action schema, body/rifle/movement numbers, perception, hearing, timing, spawning and acceptance tests. Code comments cite it as `§N` and PLAN as `PLAN §N`.
- [MAPGEN.md](MAPGEN.md): the procedural map generator (families, levels, rules, Unreal integration, measured stats).
- [REPO-WARNINGS.md](REPO-WARNINGS.md) and [GameJam/Source/AgentRuntime/VENDORED.md](GameJam/Source/AgentRuntime/VENDORED.md).

## Build / run

- UE 5.7 (5.7.4 baseline), C++ project; toolchain per `GameJam/.vsconfig` (Visual Studio, Windows). Modules: `GameJam` and `AgentRuntime` (targets `GameJam` / `GameJamEditor` in `GameJam/Source/`).
- On the Linux dev box, UE 5.7.4 (installed build, with Learning Agents and the Python plugin) is at `~/Downloads/unreal_engine`. Build: `~/Downloads/unreal_engine/Engine/Build/BatchFiles/Linux/Build.sh GameJamEditor Linux Development -Project=$PWD/GameJam/GameJam.uproject -WaitMutex` (from the repo root; about 1 min). On Windows: `Build.bat ... Win64`.
- Run a level headless: `~/Downloads/unreal_engine/Engine/Binaries/Linux/UnrealEditor <abs>/GameJam.uproject /Game/GJ/Lvl_MapGen -game -nullrhi -nosound -unattended -nosplash -stdout -ExecCmds="..."`. Editor Python scripts run with `-run=pythonscript -script=<file> -EnablePlugins=PythonScriptPlugin -unattended -nullrhi` (how `Lvl_MapGen` was made).
- The only automated tests are for the map generator. They run without Unreal: `Tools/MapGen/build.sh /tmp/mapgen_test && /tmp/mapgen_test stats 30` (exit 1 on any invalid or non-deterministic layout); `/tmp/mapgen_test show graph 2.5 42` prints one map. Everything else is verified in-editor (PIE) against the contract's §16 acceptance tests. Packaged Development **and Shipping** inference must be checked (PLAN §12).
- Default map: `/Game/FirstPerson/Lvl_FirstPerson` (World Partition). The procedural map's test level is `/Game/GJ/Lvl_MapGen`. The `GJ` testing ground needs no map changes: `UGJEncounterSubsystem` runs in every Game/PIE world, and the player is `Content/GJ/BP_GJPlayer` / `BP_GJPlayerController`.
- Console variables for testing:
  - `agent.Model <run id>` (before Play), `agent.Mobility static|walk|full` (live), `agent.Paused`, `agent.Debug`, `agent.ShowBean`, `agent.ShowHitboxes`, `agent.Tracers`
  - `gj.Enemies N`, `gj.SpawnRadius <cm>`, `gj.SpawnInterval <s>`, `gj.KillEnemies`, `gj.Tracers`
  - `gj.Map <graph|wfc> <level> [seed]` (rebuild the procedural map), `gj.MapInfo`, `gj.MapCheck`, `gj.MapSelfTest [delay] [quit]` (in-game map test; see MAPGEN.md §7)

## Architecture

**`Source/AgentRuntime/`** hosts the policy:
- `Public/FPSAgent/` is **vendored** from FPS-Agent at a pinned commit (see VENDORED.md + `VENDORED.sha256`). The observation/action schema code (`Learning/CombatObservation.*`, the `Specify*`/`Gather*`/`Perform*` bodies of `UCombatInteractor`) must stay **byte-identical**. Only the six listed local changes are allowed, each marked `GameJam` in the code. Vendored files include each other relative to `Public/FPSAgent/` (e.g. `"Common/FPSAgentTypes.h"`); keep those includes verbatim.
- `AAgentDirector` (one per world, `AAgentDirector::Get`) is the game's own contract implementation: snapshot loading with size/hash checks that refuse mismatches, a 15 Hz batched-inference decision clock on game time, per-frame held Aim/Move, perception (view cone + LOS, 2 s memory), hearing, hit indicator, virtual episodes/time-left (§12), ally invisibility, and **stage + mobility gating**. All gating belongs here, never in vendored files.
- `AEnemyBean` is the vendored `ABeanCharacter` plus a cosmetic skin, the `AgentPawn` capsule channel, AI-controller possession, and ragdoll-on-death. Gameplay mechanics are untouched.
- `UAgentTargetComponent` (on the player) is **everything agents may know about the player**: head/chest points, velocity, facing, crouch, alive, hitboxes, footstep/gunshot sound events, and cloak hooks (`bHiddenFromAgents`, `bSilentToAgents`).
- `UAgentRuntimeSettings` (Project Settings > Game > Agent Runtime) is the **model registry**: run id, stage, mobility, time limit T, snapshot dir, `-Set` string, SHA-256s, and `ActiveRunId`. Defaults are in `Private/AgentRuntimeSettings.cpp`. Snapshots live in `Content/AgentModels/run-<id>/` (`encoder.bin`, `policy.bin`, `decoder.bin`, `RUN_REPORT.md`, `SHA256SUMS`).

**`Source/GameJam/`** is the UE First Person template (base classes plus `Variant_Shooter` / `Variant_Horror`). The game's own code is in `GJ/`:
- `AGJPlayerCharacter` (extends `AShooterCharacter`): bean-identical capsule and hitboxes, the bean's movement numbers, walk and crouch keys (silent to agents), armor and regen.
- `AGJRifle`: mirrors the enemy rifle using the vendored `FRifleFireState`/`CombatMath`, hitscan on the Weapon channel.
- `AGJHUD`: canvas HUD.
- `UGJEncounterSubsystem`: a placeholder Encounter Director that applies the contract's spawn rules. On a procedural map it probes tiles by walking distance from the player.

`Source/GameJam/MapGen/` is the procedural map generator (MAPGEN.md):
- `GJLayoutGen.*` (namespace `GJMap`) is **plain C++17 with no Unreal types or exceptions**, so `Tools/MapGen` can build it with g++. Keep it that way. All randomness goes through `GJMap::FRng`, never `std::` distributions, so a `(family, level, seed)` tuple gives the same map everywhere.
- `AGJMapGenerator` turns a layout into merged `BlockAll` box colliders plus collision-free instanced meshes, replaces the level's PlayerStarts with one at the layout start, and hands spawn candidates to the encounter subsystem. Its location is the map's south-west corner; row 0 is north (+X), column 0 is west.

Custom collision channels (`Config/DefaultEngine.ini`): `GameTraceChannel1` = Projectile (template), `GameTraceChannel2` = **Weapon** (`ECC_Weapon`), `GameTraceChannel3` = **AgentPawn**.

## Rules that are easy to break

- **Never change the agent interface to fit a game feature.** No changes to observations, actions, enemy body, speed, rifle, health or senses. Such features are deferred or sent back to FPS-Agent for retraining (contract §15). FPS-Agent is read-only for this project; never modify, build or run it, and requests go through the user.
- **Mobility:** the game's enemies use the **`walk`** line (no crouch/jump). A `full` snapshot with crouch and jump masked is *not* a stand-in. The intended game snapshot is `run-S1-walk-20261114-152929`, but it **isn't copied in yet**: the registry currently holds S0 (`static`) and `S1-20261113-L009` (`full`, active).
- **Stage gating:** S0 runs as a turret (no movement, no sounds, no Hit). S1 hears footsteps only. Gunshots and Hit are fed only from S2.
- Presentation (dancers, ragdolls, clutter, VFX, music, darkness) must not enter agent geometry/LOS queries or block the Weapon or AgentPawn channels. Ragdolls use `PhysicsBody` and ignore Weapon. "Hide in the dark/smoke" must be an explicit not-visible test in the Agent Director, never weakened LOS.
- Player speed stays within the trained envelope (run about 540, ceiling 600 cm/s). Use global time dilation only, never per-actor time dilation on agents.
- Levels: 4 m grid, corridors at least 4 m wide, a single walkable floor per XY location, cover at 120/250 cm, ramps instead of jumpable steps (PLAN §8, map format v1 in §8.1).

## Git

- `*.uasset` and `*.umap` go through **Git LFS**. `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `GameJam/Plugins/UnrealMCP/` and `/.mcp.json` are ignored.
- Stage files **by name**. Avoid `git add -A` / `git add .` (it once began staging `Intermediate/`).
- A stale `.git/index.lock` can be deleted only when no git process is running.
