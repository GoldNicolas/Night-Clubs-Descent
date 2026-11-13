# UE5 Game Master Plan

**Status:** Initial planning draft; no game implementation is implied.
**Written:** November 13, 2026.
**Game title:** Undecided.

This plan records the game concept, proposed architecture, development order, and open decisions discussed so far. **Confirmed** items come from the user's brief. **Proposed** items are recommendations to decide or validate during development. **Open** items are not settled requirements.

## 1. Required companion document

Read **FPS-Agent-Constraints.md** alongside this plan before designing or implementing enemy integration.

- Original location: C:/Users/ng440/Desktop/FPS-Agent/docs/FPS-Agent-Constraints.md
- Local link from this Desktop plan: [FPS-Agent-Constraints.md](FPS-Agent/docs/FPS-Agent-Constraints.md)
- The user will also provide that Markdown file separately when handing this plan to another project or coding agent.
- This draft references contract version 1, dated November 13, 2026, based on FPS-Agent commit b802737 and build-0eef702.

The companion document is the authority for exact observation/action schemas, mechanics, collision rules, timing, snapshot compatibility, and integration acceptance checks. This plan summarizes their design consequences; it does not replace them.

FPS-Agent remains a separate private training project. The game hosts its trained models and adapts to their documented contract. A conflicting feature must be deferred, adapted, or explicitly coordinated with FPS-Agent for new training. Do not quietly change the agent interface to accommodate a game feature.

## 2. Confirmed creative brief

- A first-person run-and-gun game, with the energy of a 3D Hotline Miami.
- Procedurally generated maps.
- The setting is inspired by the Las Vegas underground tunnels, taken over by a mob and turned into an eternal nightclub rager.
- Dark thematic direction, stylized visuals inspired by SIFU; realism is not required.
- Lights, music, and ambient NPCs dancing throughout suitable areas. The crowd should have the rough, "scummy people" appearance described in the brief.
- No additional lore development is required.
- Enemies spawn at randomized locations and use the trained agents produced by FPS-Agent.
- Player kills make enemies ragdoll.
- The player can fire while running; running makes aim less accurate.
- Perks/items, achievements, and kill streaks are planned features. Their exact rules are open.
- The run currently continues until the player dies. This ending rule may change.

The references establish tone and feel. Their exact damage models, movement systems, camera behavior, and progression systems have not been adopted.

## 3. Proposed first scope and gameplay loop

**Proposed:** Start with a single-player PC prototype in Unreal Engine 5.7.4, using C++ for agent integration and core gameplay, with Blueprints and data assets for presentation and content tuning. Multiplayer, release platforms, input support, and hardware targets remain open.

First establish a short playable loop:

1. Start a run in a generated, valid combat space.
2. Move through tunnel rooms and club areas while enemies spawn at valid randomized positions.
3. Shoot, move, manage exposure, and survive.
4. Kills trigger ragdolls, feedback, and kill-streak tracking.
5. Encounter pressure evolves; items or perk opportunities are introduced once the combat loop works.
6. Player death ends the current run and leads to a results/restart flow.

**Proposed:** Begin with a finite generated map that supports an endless survival session. An endless session does not require an infinitely expanding world. Continuous map streaming or progression through generated chunks is a separate open decision.

Do not commit to a fixed development schedule or a promised number of active enemies before measuring the integrated game.

## 4. What the trained agent provides

The game supplies observations and executes actions. The policy chooses actions from the information it receives. Building observations is an integration task; it does not require training another model when the existing contract is reproduced correctly.

A compatible schema alone does not prove that the model has learned the behavior required by the game.

Documented model status in the companion file, as of November 13, 2026:

| Stage | Intended capability | Documented state | Use in this game |
|---|---|---|---|
| S0 | Stationary learner aims and shoots stationary targets | Trained and promoted, 98.5% clear | Initial snapshot/loading and shooting integration checks |
| S1 | Learner movement, moving targets, cover, footstep hearing | Build B shipped; training next | Moving-target and movement integration once a suitable snapshot is promoted |
| S2 | Enemies shoot back; damage and cover use | Planned | Required validation for normal combat where the player attacks the agents |
| S3 | Searching and hunting across maps | Planned | Intended dependency for reliable enemies across procedural room networks |

S0 is not a complete gameplay enemy. S1 does not establish competent response to incoming fire. S3 is an intended capability milestone, not a guarantee that a new tunnel layout will work without evaluation.

The final game should target validated S2/S3 behavior. Development can proceed with small integration scenes and temporary scripted stand-ins, clearly identified as placeholders. Do not claim stand-in performance as evidence of learned behavior.

Training remains in FPS-Agent. Normal gameplay uses local inference; it does not need the Python PPO trainer or the cluster.

## 5. Proposed runtime architecture

Keep enemy inference, encounter pacing, map generation, and presentation independently understandable.

~~~text
Game Mode / Run Director
  |-- Player Pawn and Player Combat
  |-- Procedural Map Director
  |-- Encounter Director
  |-- Progression and Run Statistics
  |-- Presentation: visuals, audio, ambient crowds, ragdolls
  +-- Agent Runtime
       |-- One Agent Director actor per world
       |-- Learning Agents manager, interactor, and shared policy
       |-- Per-agent memory, tracking, held actions, and episode state
       +-- Compatible enemy pawns
~~~

| System | Responsibility | Boundary |
|---|---|---|
| Game Mode / Run Director | Start, pause, death, restart, and run state | Does not construct model observations |
| Player Pawn / Combat | Input, movement, player weapons, health, and target information | Exposes the player through a target interface |
| Procedural Map Director | Layout, collision, connectivity, spawn candidates, and decoration placement | Produces spaces that satisfy the contract |
| Encounter Director | Spawn timing, randomized spawn selection, active-enemy budget, and pressure | Does not provide hidden player coordinates to the policy |
| Agent Director | Model loading, registration, perception, batched inference, actions, and virtual episodes | Owns the contract implementation |
| Enemy Pawn | Exact trained movement, body, health, rifle, and damage behavior | Visuals may vary while mechanics remain compatible |
| Presentation | Animation, effects, lights, music, dancers, ragdolls, UI | Must not accidentally change agent collision or observations |
| Progression / Statistics | Kills, streaks, items/perks, achievements, results, and persistence | Consumes gameplay events without changing the agent schema |

**Proposed implementation:** A dedicated AgentRuntime module or plugin containing pinned FPS-Agent runtime code and the game's adapters. Prefer an actor owning the Learning Agents manager component for the initial Agent Director. A world subsystem can later locate or coordinate it if needed.

Use a player target interface/component exposing stable identity, head/chest positions, velocity, facing, crouch/alive state, hit zones, and the required sound events.

Keep C++ responsible for compatibility-sensitive behavior. Expose presentation and approved tuning settings through Blueprints/data assets. Avoid copying the training arena, reward system, and curriculum into normal gameplay.

## 6. Agent integration requirements

Use the exact definitions and source manifest in the companion document, including any mechanics/perception/network overrides attached to the selected run.

- UE 5.7, preferably the matching 5.7.4 baseline; C++ project with compatible Learning Agents runtime dependencies.
- Load encoder.bin, policy.bin, and decoder.bin, or equivalent packaged network assets. The critic is training-only.
- Record the model run ID, stage, source/build version, checksums, applicable settings, and schema version.
- Compare network input size, output size, and compatibility hash when loading. Refuse incompatible models.
- Preserve the exact Combat observation/action schema, names, scales, set/optional structures, and encoding settings.
- Baseline policy: two hidden layers of 256, GRU memory of 128, and the other settings required by the contract.
- Run one batched inference call for all active agents at 15 decisions per game-time second.
- Maintain separate recurrent memory and perception history for every agent; reset them at appropriate lifecycle boundaries.
- Apply held aim and movement each frame using elapsed game time. Preserve discrete-action timing and rifle cadence.
- Keep inference, perception, sound events, and authoritative hitscan on game authority. In single-player, that is the local authoritative world.
- Use exact trained enemy body, movement, health, armor, rifle, spread, recoil, and reload behavior.
- Preserve visibility, geometry probes, last-seen memory, footsteps, gunshots, and hit-indicator rules.

The observation contract includes Self (15 values), Rays (72), up to five Enemy records, an optional Hit record, and up to four Sound records. Its matching actions provide Move, Aim, Fire, Reload, Crouch, Walk, and Jump.

The player is the target in each agent's enemy list. Other agent-controlled enemies are allies: exclude them from enemy observations and hearing, let their shots pass through each other, disable friendly fire, and avoid mutual movement blocking.

Five enemy records per policy does not mean five agent NPCs in the game. Each agent can observe the single player independently; the active-agent budget is a performance and game-design decision.

## 7. Player combat and enemy difficulty

**Confirmed:** The player can run and fire simultaneously, with worse aim while running.

**Proposed baseline:**

- Snappy movement and a readable movement-dependent spread indicator.
- Player speed near the trained target range, initially around 540 cm/s and within the contract's 600 cm/s ceiling.
- Player head/body proportions and hit zones aligned with the agent's target expectations.
- One player weapon for the first combat slice; the full player arsenal remains open.
- Clear hit, kill, and incoming-damage feedback, with effects that preserve target readability.

The player does not have to use the enemy's exact rifle, but new player weapons and perks still need gameplay evaluation against the model. The enemy's own rifle and physical rules remain fixed by the contract.

Tune difficulty first through spawn pacing, concurrent enemies, and the documented reaction-delay, perception-noise, damage, and firing modifiers. Establish the unmodified model baseline before applying these changes.

These modifiers preserve the schema but can still affect behavior. Do not treat the contract's risk estimates as a guarantee. Do not use an S0 snapshot as a universal "easy" moving enemy: it lacks required skills.

Dashes, slides, teleportation, wall-running, extreme speed boosts, and other unfamiliar player movement are deferred ideas, not part of the initial movement plan. New enemy weapons, abilities, and coordinated tactics require a separate compatibility/training decision.

## 8. Procedural tunnel and nightclub maps

**Proposed:** Generate a gameplay blockout first, then dress it with a modular tunnel/club art kit.

The structural generator should provide:

- A connected room-and-corridor network on the compatible 4 m grid.
- One walkable floor height per horizontal location: a 2.5D layout viewed and played in first person.
- Corridors generally at least 4 m wide and combat rooms approximately 10-40 m across.
- Cover near the trained low/high heights of 120/250 cm.
- Compatible ramps and steps; no required ladders, swimming, mantling, or jumps beyond the trained abilities.
- Valid spawn candidates, floor support, collision clearance, and reachable gameplay areas.
- A reproducible seed for diagnosing bad layouts.
- Simple structural collision that agrees with weapon and perception blocking.

The model does not use a navmesh. A generator-side connectivity check is still useful, and a connected layout does not prove the policy can navigate it reliably.

**Ceiling proposal:** Use the contract's special ceiling collision channel: ceilings block characters and weapons but are excluded from agent geometry/LOS object queries. Only use this where there is no walkable floor or enemy above the ceiling. Treat this as a transfer approach to validate in a representative tunnel, not a proven substitute for indoor training.

Do not place overhead walkways, stacked tunnels, accessible balconies above combat areas, or overlapping playable floors in the initial generator.

Keep real walls and cover visible to both weapon traces and agent perception. Do not make required cover cosmetic. Avoid bulletproof transparent glass, opaque bullet penetration, and moving/destructible geometry as core mechanics until separately evaluated.

Possible decoration, not new lore: tunnel concrete, pipes, club lighting, dance floors, DJ equipment, temporary barriers, booths, cables, litter, and mob-occupied club dressing. The art kit can vary visually while preserving readable gameplay collision.

## 9. Visuals, music, ambient NPCs, and ragdolls

**Confirmed:** Dark, stylized, nonrealistic presentation inspired by SIFU, nightclub lighting/music, dancing ambient NPCs, and enemy ragdolls caused by player kills.

- Use stylized character meshes over the compatible enemy body and primitive hitboxes.
- Agent animation must not move the authoritative eye or hitboxes. Keep visible silhouettes reasonably aligned with where shots count.
- Darkness, fog, strobes, and smoke effects do not automatically hide the player from geometric perception.
- Nightclub music and ambient crowd/physics sounds are presentation audio. Agent hearing receives only the specified player footsteps/gunshots.
- Keep pickups, small clutter, VFX, debris, dancers, and corpses out of agent geometry queries.
- Dancers must not become invisible movement obstacles or unexplained bullet blockers for agents.

**Proposed crowd baseline:** Ambient dancers are presentation actors outside the combat target lists, with no agent-blocking collision. Whether the player can harm them, whether they react to combat, and whether they affect scoring remain open. Do not add a civilian morality or lore system by assumption.

On enemy death, stop inference/actions for that enemy, remove its active combat hitboxes, and enable the visual ragdoll. Configure corpse collision so it does not affect agent observations or absorb agent shots. Cosmetic physics interaction can be evaluated separately.

Bound the number of active dancers, lights, ragdolls, and debris. Pooling, settling corpses, or cleanup rules are implementation options to choose after profiling.

## 10. Encounters, spawning, and endless duration

**Proposed:** An Encounter Director controls encounter pressure while each trained agent controls its own combat actions.

Random spawning must be constrained:

- Valid floor and capsule clearance.
- At least 5 m from the player and preferably outside the player's view.
- At least 4 m between agent spawn positions.
- Only in loaded, connected gameplay areas.
- Appropriate engagement distances, generally within about 40 m.
- A measured active-enemy budget and staggered arrivals.

The director may know player location to choose spawn candidates. That knowledge must not be passed to the policy as hidden target information.

Agents do not know about allies and cannot coordinate. Account for convergence, visual overlap, and shots passing through allies through room design and spawn pacing. Adding steering or navigation overrides would change action execution and needs separate evaluation.

**Open encounter format:** Continuous replenishment, distinct waves, or room-based encounters. The initial survival ending remains player death.

The player run clock and each agent's virtual episode clock are separate. Follow section 12 of the companion document, using the deployed snapshot's stage time limit and reset rules. Do not use an arbitrary global countdown, change the decision rate, or automatically kill/heal an agent when its model timer expires.

Endless deployment is a material transfer risk. An agent can remain in combat after the trained time limit; resetting memory/time without resetting health, ammo, and surroundings does not recreate a training episode. Compare the documented virtual-episode approach against alternatives only in dedicated gameplay evaluation. Holding time-left at a constant is an experiment, not an accepted solution.

Avoid wiping an active agent's memory merely because another enemy was spawned. Define safe encounter boundaries explicitly.

## 11. Perks, items, achievements, and kill streaks

**Confirmed feature categories; mechanics remain open.**

Use gameplay events such as PlayerDamaged, EnemyKilled, ItemCollected, and RunEnded to drive score, UI, progression, audio, and effects. Keep these systems outside the agent observation/action schema.

Candidate ideas for discussion:

- Kill streaks based on consecutive kills within a time window.
- Score multipliers and cosmetic/audio feedback for longer streaks.
- Player ammunition/reload or accuracy perks.
- Player healing/armor items.
- Unlocks or achievements for survival, kills, streaks, or play styles.

These are suggestions, not approved rules. Decide whether upgrades last only for a run or persist between runs. Player upgrades may change encounter difficulty even when the agent does not directly observe them.

Do not modify enemy speed, body, weapon, health model, or senses through a generic perk system. Player movement upgrades must also respect or explicitly revisit the target envelope.

## 12. Development milestones and exit criteria

These are future implementation and validation tasks, not work completed by this plan.

| Milestone | Deliverable | Exit condition |
|---|---|---|
| 0. Runtime proof | UE5 C++ project, AgentRuntime integration, one plain test room, pinned S0 snapshot | Compatible snapshot loads; wrong model is rejected; golden observations and actions match; early packaged inference feasibility is checked |
| 1. Combat slice | Player movement/shooting, one enemy, damage, death, ragdoll, restart | Stationary-target integration works; running worsens player aim while firing remains possible; death fires one consistent kill event |
| 2. Learned gameplay | Promoted S1/S2 models as available, hearing, moving targets, player return fire | Verify skills appropriate to each stage; establish a repeatable baseline before difficulty modifiers |
| 3. Procedural encounters | Connected layouts, ceiling approach, valid spawns, multiple agents, Encounter Director | Generated spaces support reliable combat/navigation with suitable S2/S3 snapshots; agents ignore allies; clutter and ceilings do not corrupt perception |
| 4. Survival run | Chosen wave/continuous format, results, streaks, initial items/perks | Stable repeated encounters and long-lived-agent behavior; bounded performance and reliable death/restart |
| 5. Representative presentation | Styled tunnel/club art, lighting, music, dancers, effects, achievements/persistence | Readable combat, intended atmosphere, acceptable target-hardware performance, and packaged runtime validation |

Use placeholders early while exploring visual direction. Avoid making completion of the full training curriculum a prerequisite for all player, UI, art, or progression work. Gate learned-enemy claims on actual stage-appropriate results.

Before broader integration, use the companion document's acceptance checks for observation packing, static/moving targets, hearing, walls, allies, frame-rate behavior, and respawn memory. Apply hearing/movement/damage expectations only to snapshots trained for those capabilities.

Add game-specific checks for ceilings, crowds, ragdolls, generated layouts, player weapon/perk combinations, and prolonged combat. The document's performance estimates are not a measured budget for this game.

## 13. Open decisions

| Topic | Proposed starting point | Still to choose |
|---|---|---|
| Game name | Working title only | Final name |
| Player count | Single-player first | Whether multiplayer/co-op belongs in later scope |
| Platform/input | PC development baseline | Release platforms, input devices, minimum hardware |
| World duration | Finite generated map with endless encounters | Fixed arena, connected rooms, or streamed chunks |
| Run structure | Death ends a run | Waves versus continuous spawning, breaks, extraction, or alternate endings |
| Procedural design | Compatible grid and modular rooms | Room grammar, branching, map size, transition rules, seed handling |
| Player weapons | One weapon in the first slice | Arsenal, swapping, ammo scarcity, recoil/camera behavior |
| Player survivability | Tune after baseline enemy evaluation | Health/armor, healing, damage, desired time-to-kill |
| Player movement | Run-and-gun within the target envelope | Speeds, jump/crouch/walk controls, future special movement |
| Enemy pressure | Measured cap with staggered spawns | Concurrency, spawn cadence, scaling over a run |
| Enemy variation | Visual variants sharing compatible mechanics | Model selection and tested difficulty presets |
| Time-left handling | Contract's virtual episodes as first candidate | Behavior during uninterrupted combat; alternatives if transfer is poor |
| Perks/items | Small player-focused set | Acquisition, stacking, duration, choices, rarity, compatibility limits |
| Kill streaks | Timed chain as a candidate | Timeout, reset rules, rewards, UI/music response |
| Achievements | Event-driven tracking | Achievement list, persistent unlocks, platform integration |
| Ambient crowd | Presentation-only dancers initially | Density, reactions, player interaction, damage and scoring |
| Art/audio | Dark stylized tunnel nightclub | Palette, silhouettes, readability, music style and acquisition |
| Bodies/effects | Cosmetic ragdolls with a bounded budget | Cleanup, pooling, persistence, player interaction |
| Difficulty | Baseline model first, then documented modifiers | Challenge, reaction/aim/damage settings, accessibility options |

A recommended starting point is not a user-approved final decision. Resolve the decisions that affect the first slice before expanding content.

## 14. Handoff guidance

Provide this PLAN.md and FPS-Agent-Constraints.md together.

The implementing project should first establish the selected model's run report, stage, compatible runtime source, required overrides, and packaging approach. Preserve a small integration scene for comparing model versions and diagnosing perception/action problems.

Treat the following as unresolved risks until demonstrated in the game:

- Reliable combat and navigation require capabilities beyond the currently documented S0 snapshot.
- New tunnel layouts may differ materially from the training maps even when their schema is compatible.
- The ceiling-channel approach, virtual episodes, and difficulty modifiers require gameplay evaluation.
- Independent agents can create visual overlap and concentrated fire without understanding one another.
- Nightclub visuals, crowds, and physics can dominate frame cost; the active-agent budget must be measured.
- Shipping-build support and packaged network loading must be demonstrated early.

Future work may refine this plan through explicit decisions. Do not invent additional lore or silently promote open ideas into requirements.