# Vendored FPS-Agent sources

`Public/FPSAgent/` is copied from the private FPS-Agent repository, read-only (PLAN.md §4.1, contract §17).

- **Pinned commit:** `25a7900` (build `build-25a7900`, decision D-033: the source of contract **v1.2**).
- **Source path:** `FPS-Agent/Game/FPSAgent/Source/FPSAgent/<same relative path>`.
- **Copied on:** 2026-11-14, with `git show 25a7900:<path>`. The pristine hashes are in `VENDORED.sha256`.
- **Updating:** copy the same file list from a newer commit, re-apply the changes below, update this file, and update `FPS-Agent-Constraints*.md` if the contract version changed.

## Files

| Folder | Files |
|---|---|
| `Common/` | `FPSAgentTypes.h`, `FPSAgentSettings.h/.cpp`, `Perception.h/.cpp` |
| `Learning/` | `CombatObservation.h/.cpp`, `CombatLearningClasses.h/.cpp` |
| `Character/` | `BeanCharacter.h/.cpp`, `BeanHitboxLayout.h/.cpp`, `BeanAIController.h/.cpp`, `HealthComponent.h/.cpp` |
| `Weapons/` | `CombatMath.h/.cpp`, `RifleFireState.h/.cpp`, `RifleComponent.h/.cpp` |
| `Bots/` | `BotSenses.h/.cpp` (only `FSoundEvent`, `IsAudibleMovement`, `HearingRadiusCm`, `CanHear` are used) |

`UTrainingSession::LoadSnapshots` is not vendored as a file. Its logic is copied into `AAgentDirector::LoadSnapshots` (`Private/AgentDirector.cpp`), without the critic.

## Local changes (every one is marked `GameJam` in the code)

The observation and action **schema code is byte-identical**: `CombatObservation.*`, and the `Specify*` / `Gather*` / `Perform*` bodies of `UCombatInteractor`. Snapshot loading compares input size, output size and compatibility hash, and refuses a mismatch.

| # | Change | Files | Allowed by |
|---|---|---|---|
| 1 | Export macro `FPSAGENT_API` → `AGENTRUNTIME_API` | all headers | PLAN.md §4.1 |
| 2 | `TaskOf` glue: the interactor calls `AAgentDirector` instead of `ACombatTaskActor` | `Learning/CombatLearningClasses.cpp` | PLAN.md §4.1 |
| 3 | `UCombatTrainingEnv` removed (training stays in FPS-Agent) | `Learning/CombatLearningClasses.h/.cpp` | contract §17 ("Drop `UCombatTrainingEnv`") |
| 4 | Ally ignore-list on the rifle trace (`SetIgnoredAllies`): no friendly fire | `Weapons/RifleComponent.h/.cpp` | PLAN.md §4.1, contract §8 |
| 5 | `ECC_Weapon` = `ECC_GameTraceChannel2`, because the template already uses channel 1 for `Projectile` | `Common/FPSAgentTypes.h` | contract §2 (remapping allowed, same semantics) |
| 6 | **Target adapter:** when the rifle hits a non-bean actor with a `UAgentTargetComponent` (the player), the zone and the same zone damage go through that component. Zones and damage values are unchanged. | `Weapons/RifleComponent.cpp` | Approved 2026-11-14 (the player is the template's character, not a bean). Contract §7.3 requires that agents can hit the player. |

Stage gating, mobility gating (Crouch/Jump), virtual episodes, hearing and the hit indicator live in the game's own `AAgentDirector`. They are not edits to vendored files.
