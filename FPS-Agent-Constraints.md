# FPS-Agent Constraints: what a game must provide to host the trained combat agent

**Contract version 1.** Written 2026-11-13 from FPS-Agent commit `b802737` (Linux build `build-0eef702`). It is valid for every snapshot trained with observation/action layout v1. That layout has been frozen since build A (decisions D-023, D-028) and is shared by all curriculum stages S0–S3.

**Who reads this.** An AI coding agent building a full Unreal Engine game into which the FPS-Agent policy will be dropped as enemy bots. FPS-Agent is a separate, private ML project. The game must adapt to the agent; the agent will not adapt to the game.

**Keywords.** **MUST** = break it and the policy sees inputs it was never trained on (it may still run, but behaves badly or erratically). **SHOULD** = strongly recommended; deviations cost performance. **MAY** = free choice.

**Core principle.** The trained policy is a function from a fixed vector of numbers to a fixed vector of actions. It has no idea what game it is in. Integration means one thing: from each agent's point of view, the game must reproduce the world it trained in. That covers:
- the same body, movement, rifle, units and timing;
- the same rules for seeing and hearing an enemy;
- level geometry of the same kind and scale.

The **player** is the enemy and may differ from the training bots, but only inside the envelope in section 7. **Other agents** must be invisible to every agent (section 8).

---

## Table of contents
1. What the policy is and what it has learned
2. Engine, plugin and build requirements
3. Artifacts handed over from FPS-Agent
4. Runtime architecture to implement in the game
5. Timing (decision clock, aim integration)
6. The agent's body, movement, rifle and health (exact values)
7. The player as the enemy: what the player must expose, and the in-distribution envelope
8. Multiple agents: allies are invisible
9. Observation contract (exact layout, formulas, units)
10. Action contract (exact semantics)
11. Level and geometry requirements
12. Spawning, episodes and the "time left" input
13. Difficulty and human-likeness without retraining
14. Networking, performance and packaging
15. What requires retraining (send it back to FPS-Agent)
16. Acceptance tests (with a worked observation example)
17. Source file manifest
18. Quick checklist

---

## 1. What the policy is and what it has learned
- **Algorithm.** PPO through Epic's **Learning Agents** plugin (UE 5.7), trained headless on a cluster. **Inference runs in-process in the game**: CPU, no Python, no trainer.
- **Network.**
  - An encoder (with attention over the enemy and sound sets).
  - A policy MLP of 2 hidden layers × 256 units with ELU activations.
  - A recurrent memory: a GRU with a 128-float state per agent.
  - A decoder.

  The game uses only the three files `encoder.bin`, `policy.bin` and `decoder.bin`. The critic is training-only.
- **Training opponents.** Scripted "bean" bots only, one agent per arena, never another agent (FPS-Agent hard rule 3). The curriculum:

  | Stage | Bots | Agent | Status (2026-11-13) |
  |---|---|---|---|
  | S0 aim gallery | static, 1–5, 5–35 m | can't move | trained and promoted (98.5% clear) |
  | S1 movers | strafe, peek, crouch, patrol, explore and investigate; 1–5; 60–100% run speed; cover | moves; hears footsteps | build B shipped; training next |
  | S2 duels | shoot back; reaction 600 → 200 ms | takes damage, uses cover | planned |
  | S3 full-map hunt | random original maps, several bots including Hunters | searches maps | planned |

  "Has learned to find and kill moving targets" means an **S1-or-later snapshot**. Until an **S2+** snapshot exists, agents have **never been shot at**: the damage-taken inputs exist, but their meaning is untrained.
- **Fair senses** are a design rule of FPS-Agent, not just a training detail:
  - no camera or pixel input;
  - no information through walls, except hearing and the hit indicator.

  The game MUST NOT feed the agent anything else. A "wallhack" is only possible by breaking this contract, and it would also be out of distribution.

---

## 2. Engine, plugin and build requirements
- **Engine.** MUST be **Unreal Engine 5.7** (FPS-Agent uses 5.7.4). Snapshot files are a Learning Agents 5.7 format; another engine version needs re-verification or retraining.
- **Plugin.** MUST enable **Learning Agents** (experimental). Inference needs the modules `Learning` and `LearningAgents`. `LearningTraining` and `LearningAgentsTraining` are only needed for training, so the game SHOULD NOT link them unless needed (verify the build links without them).
- **Project type.** MUST be a **C++ project**: the interactor, perception and rifle code are C++. Blueprints MAY wrap the result.
- **Collision channel.** MUST add a trace channel named **`Weapon`** as `ECC_GameTraceChannel1`, default response **Block**. FPS-Agent's `Config/DefaultEngine.ini` has:
  ```ini
  [/Script/Engine.CollisionProfile]
  +DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Block,bTraceType=True,bStaticObject=False,Name="Weapon")
  ```
  If the game already uses GameTraceChannel1 for something else, remap `ECC_Weapon` in the copied code (it is one `inline constexpr` in `FPSAgentTypes.h`) and keep the same semantics.
- **Units.** Centimetres, degrees and seconds. Z is up, X is forward, Y is right. Yaw increases to the right, pitch increases upward.
- **Build configurations.** FPS-Agent has only ever built **Development** (Windows editor and Linux server-style). Verify that a **Shipping** build links and runs Learning Agents inference before relying on it.
- **Rendering.** It MAY be on in the game. The agent never reads rendering, so all of its code also runs under `-nullrhi`, for example on a dedicated server.

---

## 3. Artifacts handed over from FPS-Agent
1. **A snapshot folder.** It holds `encoder.bin`, `policy.bin` and `decoder.bin`, about 0.5 MB in total.
   - Source: GitHub Release `run-<run-id>` of the FPS-Agent repo, verified with its `SHA256SUMS`.
   - Each run has a report `docs/runs/<run-id>.md` giving:
     - the **stage** (S0/S1/S2/S3), which sets the episode time limit (section 12);
     - the `-Set=` override string the run used (see below);
     - the eval clear rate.
2. **The settings overrides.** Every override in a run's `-Set` string that touches `Movement.*`, `Body.*`, `Rifle.*`, `Health.*`, `Combat.Perception.*`, `Bots.*` (hearing rules) or `Combat.Ppo.HiddenLayers/HiddenSize/MemorySize` **MUST be replicated in the game**.
   - Overrides of `Combat.Reward.*`, other `Combat.Ppo.*` and `Curriculum.*` are training-only; ignore them.
   - FPS-Agent has no `.ini` overrides: the code defaults in `FPSAgentTypes.h` are the trained values, and they are the numbers in this document.
3. **Source code** to copy (section 17). The observation and action **schema code MUST be copied verbatim**. The network's compatibility hash is computed from the schema, so any difference in element names, sizes, order or encoder parameters makes the snapshot unusable or silently wrong.

---

## 4. Runtime architecture to implement in the game
Suggested shape, one per game world: an `UAgentDirector` `UWorldSubsystem`, or an actor that owns a `ULearningAgentsManager` component.

```
UAgentDirector (world subsystem / manager actor)
 ├─ ULearningAgentsManager       SetMaxAgentNum(MaxAgents); AddAgent(pawn) per agent; RemoveAgent on despawn
 ├─ UCombatInteractor (copied)   schema = section 9/10, verbatim; Gather/Perform call into the director by AgentId
 ├─ ULearningAgentsPolicy        MakePolicy(..., PolicySettings) + LoadNetworkFromSnapshot ×3 (or baked network assets)
 ├─ per agent: FAgentState       tracks (enemy memory), held action, last hit time+direction, virtual episode start, ids
 ├─ shared: TArray<FSoundEvent>  sound buffer (enemy sounds only), pruned by age
 └─ decision clock               section 5
```

- **Policy settings.** The `FLearningAgentsPolicySettings` MUST be exactly:
  - `HiddenLayerNum = 2`, `HiddenLayerSize = 256`, `MemoryStateSize = 128`;
  - `MemoryCell = LearningAgentsGRU` and `ActivationFunction = ELU` (both defaults);
  - `InitialEncodedActionScale = 0.1` (default); `bUseParallelEvaluation = true` (default).

  FPS-Agent calls `ULearningAgentsPolicy::MakePolicy(Manager, Interactor, ULearningAgentsPolicy::StaticClass(), "Policy", nullptr, nullptr, nullptr, true, true, true, PolicySettings, Seed)`. It then calls `LoadNetworkFromSnapshot(FFilePath)` on `GetEncoderNetworkAsset()`, `GetPolicyNetworkAsset()` and `GetDecoderNetworkAsset()`.
- **Load check.** MUST compare each network's `NeuralNetworkData->GetInputSize()`, `GetOutputSize()` and `GetCompatibilityHash()` **before and after** loading, and refuse to run on a mismatch. Learning Agents silently overwrites them, and a mismatched snapshot then "works" but scores near zero. Copy `UTrainingSession::LoadSnapshots` (`Learning/TrainingSession.cpp`).
- **Inference call.** Use `Policy->RunInference(0.f)`, the mean action and the same call FPS-Agent uses for playback, evaluation and videos. It gathers observations for all agents, evaluates them in one batch and performs actions. Call it **once per decision for all agents together**, never once per agent.
- **Recurrent memory.** Each agent has its own memory state. MUST call `Manager->ResetAgents({AgentId})` (which zeroes it) whenever an agent starts a new life or a new virtual episode (section 12). MUST NOT reset it between decisions.
- **Agent ids.** `AddAgent(Pawn)` returns an id. The interactor's `Gather`/`Perform` callbacks receive that id, so map it to the pawn and its `FAgentState`.

---

## 5. Timing (decision clock, aim integration)
Training ran on a **fixed 60 Hz game tick**, set by `FApp::SetUseFixedTimeStep` (1/60 s, Learning Agents' `FixedTimeStepFrequency = 60`). It made a **decision every 4th tick, 15 decisions per second** (D-023), and held the action for those 4 ticks. A real game has a variable frame rate. The game MUST reproduce the *game-time* behaviour:

1. **Decision clock.**
   - Accumulate game time (`World->GetDeltaSeconds()`, so time dilation and pause apply naturally).
   - When the accumulator reaches **1/15 s (0.0667 s)**, run one decision: perception update, then `RunInference`. Subtract 1/15 s.
   - At most one decision per frame: if the game falls behind by more than one decision, drop the backlog, as FPS-Agent does on hitches.
2. **Per-decision order** (it must match training):
   1. Update each agent's enemy tracks (section 9.3).
   2. Run `RunInference(0)`, which gathers observations and performs actions.
   3. Apply the discrete actions immediately (section 10).
3. **Every frame,** for each living agent, with the held action:
   - `yaw += Aim.X × 720 °/s × frameDt` and `pitch += Aim.Y × 720 °/s × frameDt`;
   - normalize the yaw and clamp the pitch to ±80°;
   - set this as the controller's control rotation;
   - call `AddMovementInput(Rotate(FVector(Move.X, Move.Y, 0), yaw), 1.0)`, using the **updated** yaw;
   - keep the rifle trigger held while `Fire` is true.

   Aim is a **rate command, not an angle**. The policy expects 48° of turn per decision at full deflection (12° per 1/60 s tick).
4. **Movement simulation.** CharacterMovement SHOULD use `MaxSimulationTimeStep = 1/60 (0.0167)` with enough `MaxSimulationIterations` (8 or more), so movement integrates in steps no longer than training's. The game SHOULD run at 30 fps or more.
5. **Rifle cadence** uses world time, and a catch-up loop fires every shot that is due. It is frame-rate independent as written, so keep the copied `FRifleFireState`.
6. **Never run the policy faster or slower than 15 Hz.** The memory and the aim gains are tuned to 1/15 s steps. Slower agents (a difficulty knob) MUST come from section 13's knobs, not from changing the decision rate.

---

## 6. The agent's body, movement, rifle and health (exact values)
The agent pawn MUST behave exactly like FPS-Agent's `ABeanCharacter`. Copy the class (section 17). Its **visuals may change; its gameplay may not.**

### 6.1 Body and hitboxes (cm; "above floor" ignores CharacterMovement's ~2 cm floor gap)
| Item | Standing | Crouched |
|---|---|---|
| Capsule radius / half-height | 34 / **88** (176 tall) | 34 / **60** (120 tall) |
| Capsule centre above floor | 88 | 60 |
| **Eye** = head centre (`BaseEyeHeight` / `CrouchedEyeHeight`, relative to the capsule centre) | +70 → **158** above floor | +42 → **102** above floor |
| Head hitbox: sphere, radius **12** | centre 158 above floor (top 170) | centre 102 |
| Body hitbox: box, half-extents (X 18, Y 26, Z) | centre 106.97, half Z 39.03 | centre 68.16, half Z 21.84 |
| Legs hitbox: box, half-extents (X 16, Y 20, Z) | centre 33.97, half Z 33.97 | centre 23.16, half Z 23.16 |

All of these come from `ComputeBeanHitboxLayout(Body, CapsuleHalfHeight)`, which is re-applied on crouch and uncrouch. Settings: `CapsuleRadius 34`, `StandingHalfHeight 88`, `CrouchedHalfHeight 60`, `HeadRadius 12`, `HeadTopClearance 6`, `LegFraction 0.386`, `BodyHalfDepth 18`, `BodyHalfWidth 26`, `LegsHalfDepth 16`, `LegsHalfWidth 20`.

- **Hitboxes.**
  - They MUST be **primitive components attached to the capsule**: `USphereComponent` for the head, `UBoxComponent` for the body and legs. They MUST NOT be bones or a physics asset.
  - Each is `QueryOnly`, object type `Pawn`, and ignores every channel except `Weapon = Block`.
  - The capsule MUST set `Weapon = Ignore`, so bullets hit hitboxes, not the capsule.
- **Visuals.**
  - Any mesh MAY replace the bean's cylinder and spheres, including an animated skeletal mesh, but it MUST have `NoCollision`.
  - Animation MUST NOT move the hitboxes, the camera or the eye.
  - The visual SHOULD be shaped like the hitboxes (about 176 cm tall, head about 24 cm across at 146–170 cm), so the player isn't cheated by a visual/hitbox mismatch.
  - The weapon mesh and attachments MUST have no collision. If they blocked `WorldStatic`/`WorldDynamic` queries, the agent's own rays would hit them.
- **Rotation.**
  - `bUseControllerRotationYaw = true` (the body faces the aim yaw); pitch and roll are false.
  - `CharacterMovement->bOrientRotationToMovement = false`.
  - The controller is an AI controller with `bSetControlRotationFromPawnOrientation = false`, and its `UpdateControlRotation` only calls `FaceRotation(ControlRotation)`. Copy `ABeanAIController`.
  - CharacterMovement only simulates **controlled** pawns, so the agent MUST be possessed.

### 6.2 Movement (`UCharacterMovementComponent`)
| Property | Value |
|---|---|
| `MaxWalkSpeed` (run, default) | **540** |
| `MaxWalkSpeed` while the Walk action is held | **300** |
| `MaxWalkSpeedCrouched` | **190** |
| `MaxAcceleration` | 4000 |
| `BrakingDecelerationWalking` | 6000 |
| `GroundFriction` | 8 |
| `JumpZVelocity` | 420 (jump apex ≈ 90 cm) |
| `AirControl` | 0.3 |
| Gravity | engine default (−980), `GravityScale 1` |
| `bCanCrouch` | true; `CrouchedHalfHeight` 60 |
| `MaxStepHeight`, `WalkableFloorAngle` | engine defaults (45 cm, 44.765°) |

The agent never sprints, slides, dashes, mantles, climbs or uses abilities. Adding any of these to *agents* requires retraining.

### 6.3 Rifle (the only weapon the agent knows)
| Property | Value |
|---|---|
| Damage (no falloff) | head **160**, body **40**, legs **34** |
| Range | 10000 (100 m), hitscan |
| Fire rate | **9.75 rounds/s**, automatic while the trigger is held |
| Magazine | **25**; reserve ammo **infinite** |
| Reload | **2.5 s**. On request (ignored when the magazine is full or a reload is running), and automatic when firing on an empty magazine. |

- **Shot origin and direction.** Each shot traces from the **eye** (`GetActorEyesViewPoint`) along the control rotation, plus recoil, plus a random spread cone. It is a `LineTraceSingleByChannel` on `Weapon`, `bTraceComplex = false`, ignoring the shooter (and allies, section 8). A hit on a hitbox applies that zone's damage. A muzzle tracer is cosmetic only.
- **Spread** (cone half-angle, degrees):
  - airborne: **8**;
  - otherwise, horizontal speed ≤ **148.5** cm/s (0.275 × 540): **0.25**;
  - otherwise: `lerp(2.5, 5, (speed − 148.5)/(540 − 148.5))`;
  - crouched and grounded: × **0.85**;
  - plus spray bloom: for spray index `i ≥ 3`, `+ min((i − 2) × 0.35, 3)`.
- **Recoil.**
  - It offsets the **bullet only**. There is **no camera kick**, and the control rotation is never changed by recoil.
  - **Pitch:** `i = 0` → 0; `i = 1, 2` → `0.15 × i`; `i ≥ 3` → `min(0.30 + (i − 2) × 0.6, 6)`.
  - **Yaw:** for `i > 12`, `1.5 × sin(2π (i − 12)/6)`; otherwise 0.
- **Spray recovery.** The spray index `i` recovers one step per **0.04 s** of idle time beyond the fire interval (1/9.75 s).
- The observation reports the current spread and spray index (section 9), so **the formulas MUST match**. Copy `CombatMath.cpp`, `RifleFireState.cpp` and `RifleComponent.cpp`.
- **The agent must not have:** ADS or scopes, weapon switching, grenades, melee, limited reserve ammo, weapon pickups, camera recoil or weapon sway.

### 6.4 Health
- **Values.** `MaxHealth 100`, `MaxArmor 50`, `ArmorAbsorbFraction 1.0`: armor absorbs all damage until it is gone, then health takes the rest.
- **Spawn state.** Agents spawn with 100 health and 50 armor.
- **Kills from full (150).** One headshot; four body shots; five leg shots.
- **Death.** In training the dead agent is hidden with collision disabled. In the game a corpse or ragdoll MAY exist, but it MUST NOT use object type `WorldStatic`/`WorldDynamic` with query collision (section 11). The physics-body ragdoll type is fine.

---

## 7. The player as the enemy
The policy has no notion of "player". **An enemy is whatever the game puts in an agent's enemy list.** The player (or players) MUST be put there, and for each enemy the game MUST supply the values below on every decision.

### 7.1 What each enemy must expose
A suggested interface (`IAgentTargetInterface` or a component on the player pawn):

| Value | Definition |
|---|---|
| `Head` | World position of the **centre of the enemy's head hitbox** (cm). The agent aims here. |
| `Chest` | World position of the centre of the torso hitbox. It is only used as the second line-of-sight point. |
| `Velocity` | `GetVelocity()` in world cm/s. |
| `Forward2D` | The enemy body's facing direction (yaw only, normalized 2D). Used for "facing the agent". |
| `bCrouched` | Crouch state. |
| `bAlive` | Dead enemies drop out of tracking immediately. |
| `Id` | Stable id for the enemy's track. |
| Hitboxes | Hit zones Head/Body/Legs that the agent's `Weapon` trace can hit (7.3). |
| Sounds | Footstep and gunshot events (7.4). |

### 7.2 Visibility rule (exact; the agent sees the player only through this)
On each decision, for the agent's eye `E` and view rotation `V` (its control rotation):
1. `err = AimErrorDeg(E, V, Head)`, where `err.yaw = Normalize((Head−E).Rotation().Yaw − V.Yaw)` and `err.pitch = Normalize((Head−E).Rotation().Pitch − Normalize(V.Pitch))`.
2. **In view** if `|err.yaw| ≤ 51.5°` **and** `|err.pitch| ≤ 35.265°` (a 103° × 70.53° cone). Only the head point is tested.
3. **Line of sight:**
   - `LineTraceSingleByObjectType(E → Head)` with object types **{WorldStatic, WorldDynamic}** and `bTraceComplex = false` finds no hit;
   - **or** the same trace `E → Chest` finds no hit.
4. **Visible** = the agent is alive **and** in view **and** has line of sight.
5. **FacingAgent** = `dot(Forward2D, normalize2D(AgentLocation − EnemyLocation)) > cos(51.5°) = 0.6225`, using actor locations.

Consequences the game MUST respect:
- **The player's pawn and every attachment** (weapon, backpack, cosmetics) MUST NOT block `WorldStatic`/`WorldDynamic` object queries, or the player would hide behind their own gear. Pawns are object type `Pawn`, which these traces ignore.
- **Darkness, fog, camouflage, smoke VFX and lighting do not hide the player** from agents, because perception is geometric. A smoke grenade or stealth mechanic that should hide the player MUST be added as an explicit extra test in step 3. Expect the agent to treat a hidden player as "not visible", which is in distribution (like behind cover).

### 7.3 Player hitboxes and damage
- **Hit zones.** The agent's `Weapon`-channel hitscan MUST be able to hit the player, with zones Head/Body/Legs and damage 160/40/34 before any game-side multiplier (section 13).
- **Recommended setup:** give the player the **same primitive hitbox set** as the bean (6.1), attached to the capsule. Then "head" means exactly what the agent learned.
- **If the player uses a skeletal-mesh physics asset:** map its bones to Head/Body/Legs, and report the **head bone's collision centre** as `Head`. The head shape SHOULD be about a 12 cm-radius sphere. A much smaller head makes the agent miss headshots it "knows" it lined up.
- **Hit indicator.** When the player damages an agent, the game MUST give the agent's hit indicator (9.4) the **player actor as the instigator**.

### 7.4 Player sounds (the agent's only through-wall sense)
The game MUST emit sound events for each enemy (player) as follows:

| Kind | When | Hearing radius |
|---|---|---|
| **Footstep** | While **audible movement**: `!crouched && !walkMode && horizontalSpeed > 315` cm/s (1.05 × 300). One event every **0.35 s** while the condition holds. | **1500** cm (15 m) |
| **Gunshot** | One event **per bullet fired** (every shot, including misses). Bots don't shoot before S2, so enemy gunshots only carry learned meaning in **S2+ snapshots**; emit them anyway. | **4000** cm (40 m) |

- **Event fields:**
  - `Position`: the source actor's location (capsule centre);
  - `Time`: game seconds;
  - `Kind`: footstep or gunshot;
  - `SourceId`: which enemy made it;
  - `HeardOffset`: a random point on a **disc of radius 200 cm** (`angle = U(0, 2π)`, `r = 200·√U(0, 1)`, Z = 0), drawn **once per event, when it is emitted**.
- **Audible to an agent** if `0 ≤ now − Time ≤ 1.0 s` **and** `Distance(Position, AgentActorLocation) ≤ radius(Kind)`. The *true* position decides audibility.
- **The agent observes** the **newest 4** audible enemy events, sorted newest first. Direction and distance are measured from its **eye** to `Position + HeardOffset`, so it knows the position only roughly.
- **Excluded** from an agent's hearing: its own sounds, all other agents' sounds, and any non-enemy sound (ambience, music, explosions, physics). Adding new sound kinds requires retraining.
- **The player's "walk"** is the game's silent-movement mode (for example holding Shift). With no such mode, only the speed and crouch rule applies.

### 7.5 The in-distribution envelope for the player
The player MAY move and behave differently from the bots. Performance degrades smoothly the further the player leaves this envelope.

| Property | Trained range | Guidance |
|---|---|---|
| Height / head centre | standing 158 cm, crouched 102 cm | Keep the standing head at 150–170 cm and the crouched head at 95–110 cm |
| Horizontal speed | 0–540 cm/s (bots at 60–100% of 540; walk 300, crouch 190) | SHOULD stay ≤ 540; MUST stay ≤ 600 (the velocity input is divided by 600 and **not clamped**; values above 1 were never seen) |
| Acceleration | 4000 cm/s², braking 6000 | Similar snappy movement is fine; floaty or very slow acceleration is fine |
| Vertical motion | bots never jumped in S0/S1 | Jumping (about 90 cm apex) is tolerable. Double jumps, jetpacks, wall-runs, grapples, dashes, slides, blinks and teleports are **out of distribution**. Allowed, but agents will track them poorly. |
| Behaviour | static; ADAD strafe (flip every 0.3–0.9 s, ±2.5 m); peek from cover (0.8–2 s); crouch-spam (0.3–1.2 s); patrol; explore; walk to noises; S2+ shooting back | Any human behaviour is fine; these are the "seen before" patterns |
| Distance to agents | 5–35 m at spawn; fights mostly < 30 m | Engagements SHOULD happen within about 40 m (positions are divided by 3000 cm and not clamped for enemies) |
| Number of enemies per agent | 1–5 | Up to 5 tracked (visible ones first). More players than 5 per agent: only 5 are seen at once. |
| Enemy health | bots 100 + 50 armor | The agent doesn't observe enemy health; any value is fine |

---

## 8. Multiple agents: allies are invisible
In training each agent was alone with its bots. It never saw, heard, bumped into or shot another agent. With several agents hunting one player, each agent MUST experience the others as if they did not exist:

1. **Perception.**
   - Other agents MUST NOT be in an agent's enemy list.
   - Their sounds MUST NOT reach its hearing.
   - Rays and line of sight already ignore all pawns (object type `Pawn`); keep agents' capsules, hitboxes and meshes off `WorldStatic`/`WorldDynamic`.
2. **Bullets.**
   - An agent's `Weapon` trace MUST pass through other agents: add every allied agent to the trace's ignored actors (`FCollisionQueryParams::AddIgnoredActors`), or use a separate trace channel for agent bullets that agent hitboxes ignore.
   - There MUST be **no friendly fire**. Otherwise an ally standing in the line of fire silently eats the shots, and the agent cannot perceive why.
3. **Movement collision.**
   - Agents SHOULD NOT physically block each other. Give agent capsules a custom object channel (for example `AgentPawn`) that ignores itself and still blocks the player, `Pawn` and the world.
   - Bumping into an invisible obstacle is out of distribution and can leave agents stuck.
   - If the design needs agents to block each other, accept the risk and add a small steering separation outside the policy. Never feed it to the policy.
4. **Hit indicator:** only enemy (player) damage sets it.
5. **Spawning:** spread agents out (SHOULD be at least 4 m apart, never overlapping) and at least 5 m from the player (section 12).
6. **No coordination.** Agents act independently and will converge on the player from wherever they hear or see them. Pacing, such as a cap on simultaneous attackers or staggered spawns, is the game director's job, outside the policy.
7. **The player sees agents normally.** "Invisible" applies only to the agents' senses of each other.

---

## 9. Observation contract (exact)
**View frame:** the yaw-only frame of the agent's control rotation: `ToViewFrame(v) = FRotator(0, ViewYaw, 0).UnrotateVector(v)`, giving X forward, Y right, Z up. **Eye** = `GetActorEyesViewPoint` = actor location + `(0, 0, BaseEyeHeight)` (crouch-adjusted), and the rotation is the control rotation.

The observation is one Learning Agents **struct** named `"Combat"` with five elements. Names, sizes and parameters are exact:

| Element | Learning Agents type | Size |
|---|---|---|
| `Self` | Continuous, scale 1 | 15 |
| `Rays` | Continuous, scale 1 | 72 |
| `Enemies` | **Set**: `SpecifySetObservation(Schema, Enemy, MaxNum 5, AttentionEncodingSize 32, AttentionHeadNum 4, ValueEncodingSize 32)` of `Enemy`: Continuous, scale 1 | 15 each |
| `Hit` | **Optional**: `SpecifyOptionalObservation(Schema, HitDirection, EncodingSize 32)` (not the default 128) of `HitDirection`: Continuous, scale 1 | 4 |
| `Sounds` | **Set**: `SpecifySetObservation(Schema, Sound, MaxNum 4, 32, 4, 32)` of `Sound`: Continuous, scale 1 | 5 each |

The trainer applies no observation normalization: these raw scaled values are what the network learned.

### 9.1 Self (15)
| # | Value |
|---|---|
| 0–2 | Agent velocity in the view frame / **600** (X, Y, Z) |
| 3 | 1 if grounded (not falling) |
| 4 | 1 if crouched (`bIsCrouched`) |
| 5 | 1 if walk mode is on |
| 6 | health / 100 |
| 7 | armor / 50 |
| 8 | ammo in magazine / 25 |
| 9 | 1 if reloading |
| 10 | current spread (degrees, section 6.3, for the *current* spray index) / **10** |
| 11 | spray index / 25 |
| 12 | `clamp(secondsSinceLastShot, 0, 1)` (1 if never shot) |
| 13 | view pitch (degrees, normalized to [−180, 180]) / **90** |
| 14 | **time left fraction** in [0, 1] (section 12) |

### 9.2 Rays (72)
- Pitch-major: for each pitch in **[−20°, 0°, +15°]**, then for `k = 0…23`, the direction is `FRotator(pitch, ViewYaw + 15°·k, 0).Vector()`. The first ray of each ring points straight ahead.
- Each ray starts at the **eye** and runs 3000 cm: `LineTraceSingleByObjectType` with **{WorldStatic, WorldDynamic}**, `bTraceComplex = false`.
- **Value** = `hitDistance / 3000`, clamped to [0, 1]. A ray that hits nothing gives **1**.
- **Reference values on flat open ground:**
  - a −20° ray reads 158 / sin 20° = 462 cm → **0.154** standing (crouched 102 cm → **0.099**);
  - +15° rays read **1.0** unless a wall or cover is near; there is no ceiling in training (section 11).

### 9.3 Enemies (set of 0–5, 15 each)
- **Tracking**, once per decision, before inference:
  - **Visible** enemies (7.2) create or refresh a track: last-seen head, velocity, facing, crouched and time.
  - **Not-visible** enemies keep their last data.
  - A track is dropped **2.0 s** after it was last seen, or immediately when the enemy dies.
- **Selection:** visible tracks first, then the most recently seen, up to 5.
- **Per track** (`dHead = trackHead − currentEye`, re-computed every decision from the *current* eye and view):

| # | Value |
|---|---|
| 0–2 | `ToViewFrame(dHead) / 3000` |
| 3 | `‖dHead‖ / 3000` |
| 4, 5 | aim error to the head (yaw, pitch; 7.2 step 1) / **180** |
| 6–8 | `ToViewFrame(trackVelocity) / 600` |
| 9 | facing agent (0/1, as last seen) |
| 10 | crouched (0/1, as last seen) |
| 11 | visible now (0/1) |
| 12 | `clamp((now − lastSeen) / 2.0, 0, 1)` |
| 13, 14 | **fine aim**: `clamp(aimError / 5°, −1, 1)` (yaw, pitch) |

### 9.4 Hit indicator (optional, 4)
- **Present** when the agent took enemy damage within the last **1.0 s**; only the most recent hit counts.
- **Values:**
  - 0–2: `ToViewFrame(normalize(InstigatorActorLocation − AgentActorLocation))`, a unit vector;
  - 3: `clamp(age / 1.0, 0, 1)`.
- **Absent** (null optional) otherwise. Non-enemy damage (falling, hazards) SHOULD NOT set it.

### 9.5 Sounds (set of 0–4, 5 each)
For each heard event (7.4), newest first, at most 4, with `d = (Position + HeardOffset) − Eye`:
- 0–2: `normalize(ToViewFrame(d))`;
- 3: `clamp(‖d‖ / 3000, 0, 1)`;
- 4: `clamp(age / 1.0, 0, 1)`.

---

## 10. Action contract (exact)
The action is one Learning Agents **struct** named `"Combat"`:

| Element | Type | Prior | Applied as |
|---|---|---|---|
| `Move` | Continuous 2, scale 1 | – | (forward, right). Non-finite → 0; clamp to length 1. Every frame: `AddMovementInput(RotateByYaw(Move, currentYaw), 1.0)`. |
| `Aim` | Continuous 2, scale 1 | – | (yaw rate, pitch rate). Non-finite → 0; clamp each to [−1, 1]. Every frame: `+Aim × 720°/s × dt`; pitch clamped to ±80°. Positive = right / up. |
| `Fire` | Bool | 0.5 | Rifle trigger held while true (the cadence decides the shots). |
| `Reload` | Bool | 0.05 | When true, request a reload (ignored if full or already reloading). |
| `Crouch` | Bool | 0.1 | true → `Crouch()`, false → `UnCrouch()`, at each decision. |
| `Walk` | Bool | 0.1 | `MaxWalkSpeed` 300 while true, else 540. |
| `Jump` | Bool | 0.05 | true → `Jump()`, false → `StopJumping()`. |

- The priors are part of the schema: copy `UCombatInteractor` verbatim.
- Bool actions and the trigger are applied **at the decision**. Aim and Move are applied **every frame** until the next decision.
- A dead agent ignores actions.

---

## 11. Level and geometry requirements
The agent's whole picture of the world is 72 distance rays plus line-of-sight tests, against **every collision primitive of object type `WorldStatic` or `WorldDynamic`** that blocks queries. That makes level construction part of the contract.

### 11.1 What the agent trained on
- **Construction.** Blockout geometry built from a **4 m tile grid**:
  - floors;
  - walls **6 m** tall (`#`);
  - **low cover 120 cm** (shoot over it standing; a crouched head at 102 cm hides behind it);
  - **high cover 250 cm**;
  - ramps rising **1 m over one 4 m tile** (about 14°);
  - one-way drops;
  - floor levels in 1 m steps (a 1 m step can't be jumped: the apex is about 90 cm).
- **Stages.**
  - **S0/S1:** an 80 × 80 m open floor with a wall ring. S1 adds random cover blocks on up to 12% of the tiles, half low and half high.
  - **S3 (planned):** original maps in the same tile format, with sites, mid and chokes. The reference map `map01` is 30 × 30 tiles (120 × 120 m); the 50 generated pool maps follow the same rules.
- **No ceilings, roofs, overhangs, bridges, tunnels or second storeys.** Every map is 2.5D, with one walkable floor height per tile and open sky.
- **Simple box collision everywhere** (`bTraceComplex = false`).

### 11.2 Requirements
- **2.5D layout.** Levels SHOULD be 2.5D: no walkable surface above another.
  - Ceilings and overhangs within about **9.3 m** of the eye are out of distribution: an up-ray at +15° from 158 cm reaches 3000 cm only if nothing is below about 934 cm. Every indoor up-ray reading would differ from training.
  - Options if the game has roofs or interiors:
    1. Put ceiling and roof collision on a **custom object channel** that agent rays and LOS ignore, while it still blocks pawns and the `Weapon` channel. This is only valid when no walkable floor or enemy is above it.
    2. Keep them, accept the degradation, and ask for retraining on such maps.
- **Scale.** Corridors and gaps SHOULD be **≥ 4 m** wide (one tile); 2–4 m is borderline. Cover SHOULD be about **120 cm** (low) or about **250 cm** (high) tall. Ramps ≤ 14°, steps ≤ 45 cm. Open areas of 10–40 m.
- **What may block perception.** Only true level geometry may block `WorldStatic`/`WorldDynamic` queries:
  - foliage, grass, small clutter, decals, pickups, VFX volumes, triggers, projectiles, shell casings, debris and ragdolls MUST use other object types, or have no query collision;
  - otherwise they fill the rays with noise and block line of sight.
- **Perception and bullets must agree.** In training, anything that blocks line of sight also blocks bullets, and vice versa.
  - Every collider MUST block **both** {WorldStatic/WorldDynamic object queries} **and** the `Weapon` channel, or **neither**.
  - **No** see-through bulletproof glass, **no** shoot-through-but-opaque walls, **no** penetration.
  - Thin fences and grates: decide one way for both.
- **Moving geometry** (doors, platforms) is allowed. The agent re-perceives every decision, but it never trained with moving walls; keep them rare.
- **Not supported for agents:** ladders, jump pads, water/swimming, ziplines, vehicles, teleporters, destructible-wall pathing.
- **Navigation.** The agent needs **no navmesh**: it walks by its own policy. The game only needs valid spawn points and a kill-Z or out-of-bounds handler.

---

## 12. Spawning, episodes and the "time left" input
Training was episodic:
- an episode starts with agent, bots and cover fresh;
- it ends at **agent death**, **all bots dead** (a "clear") or the **time limit**: S0 20 s, S1 30 s, later stages per their run report;
- the agent observes `Self[14] = clamp(1 − elapsed/limit, 0, 1)` throughout.

The agent never experienced life after a clear, or beyond the time limit. The game MUST frame play into **virtual episodes** per agent:

1. **On every (re)spawn of an agent:**
   - full health and armor; full magazine, no reload, spray index 0;
   - standing (uncrouch **before** teleporting); walk off; not jumping; velocity 0;
   - pitch 0, and any yaw (spawn facing away from the player is closest to training);
   - tracks, sound memory and hit indicator cleared;
   - **`Manager->ResetAgents({id})`**;
   - virtual episode start `E = now`.
2. **Time left:** `clamp(1 − (now − E) / T, 0, 1)`, where `T` is the time limit of the snapshot's stage.
3. **Start a new virtual episode** (`E = now` plus `ResetAgents`) when:
   - (a) the player respawns or a new round or wave starts, since the agent's previous enemies are all dead, which in training meant the episode had ended;
   - (b) time left has reached 0 **and** the agent currently tracks no enemy (no "visible" or "remembered" track).
4. **Alternative to A/B-test:** hold `Self[14]` at a constant 0.5–0.8 and never time-reset. Measure which plays better on the acceptance tests (section 16).
5. **Spawn placement:**
   - SHOULD be **≥ 5 m** from the player, out of the player's view, and inside valid level geometry (floor below, no overlap);
   - **≥ 4 m** from other agents.
6. **While no enemy is alive** (the player is dead), agents MAY keep running the policy. With no enemies it explores and listens, which is the in-distribution "searching" state at episode start. They MAY also be paused; pausing skips decisions and keeps memory.

---

## 13. Difficulty and human-likeness without retraining
A fully trained agent is **inhumanly fast and precise**: it turns up to 720°/s, decides every 67 ms and sees the exact head point the moment line of sight opens. For a fun game, apply these game-side knobs. None of them changes the network layout:

| Knob | How | Effect | Distribution risk |
|---|---|---|---|
| **Reaction delay** | An enemy becomes `visible` in tracks only after passing the visibility test continuously for `D` ms (for example 150–500). Optionally feed the track data from `D` ms ago (a history ring buffer). | Slower first shot; most human-feeling | Low |
| **Aim noise on perception** | Add a smooth, slowly varying offset (Perlin or OU noise, σ = 5–40 cm) to each enemy's reported `Head` point, per agent. | Misses and imperfect tracking that look natural | Low |
| **Damage multiplier** | Scale the agent's bullet damage to the player (for example ×0.5). | Longer time-to-kill | None (the agent doesn't observe enemy health) |
| **Turn-rate cap** | Lower `720` in the aim integration to 360–540°/s. | Slower flicks | Medium: the agent expects 48° per decision and will under-turn, then correct |
| **Action noise** | `RunInference(ActionNoiseScale)` with 0.1–0.5 instead of 0. | Jittery, less optimal play | Medium |
| **Fire discipline** | Randomly drop `Fire` on some decisions, or require N consecutive Fire decisions before pulling the trigger. | Fewer bullets | Low |
| **Weaker snapshots** | Use an earlier snapshot of the run, or an earlier stage. | Genuinely less skilled | None |
| **Agent count and pacing** | Director-side. | Pressure | None |

Do not use these as difficulty knobs, because they break the contract: the agent's speed or body, rifle stats (spread or recoil changes alter the inputs), observation scales, the decision rate, or information through walls.

---

## 14. Networking, performance and packaging
- **Authority.** Policy inference, perception, sound buffers and the rifle hitscan MUST run **on the server / authority only**. Agents replicate to clients as ordinary characters. Clients never run the policy.
- **Cost per agent per decision (15 Hz):**
  - 72 line traces;
  - up to 2 line-of-sight traces per enemy;
  - one batched network forward pass (2 × 256 MLP + GRU 128 + small attention encoders, a few hundred kFLOPs).

  For 10 agents that is about 11k simple traces/s and about 150 forward passes/s, trivial on one CPU core. Batch all agents in one `RunInference`.
- **Packaging the snapshot.** Either:
  - stage the three `.bin` files with the build (for example `+DirectoriesToAlwaysStageAsUFS=(Path="AgentSnapshots")`) and load them at startup; **or**
  - load them once in the editor into `ULearningAgentsNeuralNetwork` assets, save those assets, and pass them to `MakePolicy` with the re-initialize flags set to `false`.

  Keep the run id and SHA256 next to it.
- **Determinism** is not required in the game. FPS-Agent seeds everything for training reproducibility; the game MAY use any RNG for spread, the sound offsets and the noise knobs.

---

## 15. What requires retraining (send it back to FPS-Agent)
Change any of these and the snapshot is invalid or badly out of distribution. Ask the FPS-Agent project for a new run:
- **Agent mechanics:** body size or eye height, movement speeds, acceleration, jump or crouch values.
- **Rifle and health:** any rifle stat, health or armor model, or another weapon for agents.
- **Perception layout:** ray count, pitches or range, the field of view, the memory time, the max of 5 enemies or 4 sounds, or the observation scales.
- **Inputs:** new observation inputs (for example enemy health, teammates, objectives, smoke).
- **Abilities:** new actions (abilities, grenades, sprint, ADS).
- **Timing:** a decision rate other than 15 Hz.
- **Levels:** levels that are mostly indoor, multi-storey or narrow (< 4 m), unless section 11.2's ceiling channel solves it.
- **Teamwork:** agents fighting *other agents* as primary enemies at scale, or agents that must coordinate.

---

## 16. Acceptance tests
The game's integration MUST pass these before tuning difficulty.

1. **Snapshot load.** Input size, output size and compatibility hash are unchanged by loading (log both). A deliberately wrong snapshot is refused.
2. **Golden observation.**
   - **Setup:**
     - an open flat floor at Z = 0, nothing within 30 m except the floor;
     - the agent standing at actor location (0, 0, 88), so its eye is (0, 0, 158), with control rotation yaw 0, pitch 0, full health and armor, a full magazine, at rest, never fired, time left 1;
     - the player standing at (1000, 300, 88), so its head is (1000, 300, 158), at rest, facing yaw 180 (toward −X), visible.
   - **Expected Self:** `[0,0,0, 1,0,0, 1,1,1, 0, 0.025, 0, 1, 0, 1]`.
   - **Expected rays:** pitch −20° ring all ≈ **0.154**; 0° and +15° rings all **1.0**.
   - **Expected single enemy:** `[0.3333, 0.1, 0, 0.3480, 0.09277, 0, 0, 0, 0, 1, 0, 1, 0, 1.0, 0]`.
     - aim yaw = atan(300/1000) = 16.70°, so 16.70/180 = 0.09277;
     - the fine yaw channel clamps 16.70/5 to 1.0;
     - facing: dot((−1, 0), (−0.958, −0.287)) = 0.958 > 0.6225.
   - **Expected Hit and Sounds:** Hit absent; Sounds empty.
   - Unit-test the packing functions with this case.
3. **Static kill.** One agent and a static player dummy at 10–20 m in the open, in view. The agent turns, fires and kills, mostly by headshots, within about 1–2 s. FPS-Agent's S0 reference clears 1–5 static bots in about 3.5 s, at 100%.
4. **Moving kill (S1+ snapshot).** The dummy strafes ADAD at 540 cm/s. The agent stops or counter-strafes to shoot and kills it reliably.
5. **Hearing.** The player runs behind the agent at 10 m, out of view. The agent turns toward the player within about 1 s. The same test with the player walking (silent) gives no reaction until the player is seen.
6. **Walls.** The player stands behind a high wall. The player is never in the enemy set except as "remembered" for 2 s after last being seen. No shots go through the wall.
7. **Allies.** Two or three agents and one player. Agents never shoot, track, hear or block each other, and an agent's shots pass through an ally standing in the line of fire.
8. **Frame-rate independence.** Run test 3 at a 30 fps cap and at 144 fps. Turn speed and time-to-kill match within about 10%.
9. **Respawn.** After an agent dies and respawns, its memory is reset (`ResetAgents`), and it behaves like at the episode start.

---

## 17. Source file manifest (copy from FPS-Agent `Game/FPSAgent/Source/FPSAgent/`)
Copy these files, rename the `FPSAGENT_API` export macro and adapt the includes. **Mark the copied block as vendored**, and change behaviour only where this document says so.

| File(s) | Provides | Adapt? |
|---|---|---|
| `Common/FPSAgentTypes.h` | All mechanics structs with the trained default values; `ECC_Weapon`; `EHitZone` | Keep the values. Remove training-only structs if you like. |
| `Common/FPSAgentSettings.h/.cpp` | `UFPSAgentSettings` (DeveloperSettings) holding the structs | Optional: the game MAY hard-wire the values instead, but they MUST be equal |
| `Common/Perception.h/.cpp` | `AimErrorDeg`, `InFieldOfView`, `ToViewFrame`, `RayDirections`, `UpdateTracks`, `SelectTracks`, `HasLineOfSight`, `RayFraction` | Verbatim (replace `BotIndex` with the enemy id) |
| `Learning/CombatObservation.h/.cpp` | `PackSelf`, `PackEnemy`, `PackHit`, `PackSound`, `Sanitize`, `AimPerTickDeg`, `MoveWorld` | **Verbatim** |
| `Learning/CombatLearningClasses.h/.cpp` | `UCombatInteractor`: the schema, gather and perform | **Schema functions verbatim.** Replace only the `TaskOf(...)` glue so `Gather`/`Perform` call the game's director. Drop `UCombatTrainingEnv`. |
| `Learning/TrainingSession.cpp` (`LoadSnapshots`) | Snapshot loading with the shape and hash check | Copy the function |
| `Learning/CombatArena.cpp` | **Reference implementation** of `Sense`, `BuildObservation`, `TickAction`, `SetAction`, `AddSound`, `EmitFootsteps`, `OnAgentDamaged`, `StartEpisode` (spawn reset) | Re-implement in the director, keeping the logic identical. Arena, bot and reward parts are training-only. |
| `Character/BeanCharacter.h/.cpp`, `BeanHitboxLayout.h/.cpp`, `BeanAIController.h/.cpp`, `HealthComponent.h/.cpp` | Agent pawn, hitboxes, controller, health | Visual meshes may change (6.1); gameplay must not |
| `Weapons/CombatMath.h/.cpp`, `RifleFireState.h/.cpp`, `RifleComponent.h/.cpp` | Rifle: damage, spread, recoil, cadence, reload, hitscan | Add ally-ignore to the trace (section 8). The tracer is cosmetic. |
| `Bots/BotSenses.h/.cpp` (`FSoundEvent`, `IsAudibleMovement`, `HearingRadiusCm`, `CanHear`) | Sound rules | Verbatim; the rest of the bot code is training-only |

FPS-Agent docs worth reading alongside this one:
- `README.md` → *RL design*, *Scripted bots*, *Maps → Format v1*;
- `docs/superpowers/specs/2026-11-12-combat-training-design.md` §2;
- `docs/decisions.md` D-023, D-024, D-028, D-031;
- the run report of the snapshot you deploy.

---

## 18. Quick checklist
- [ ] UE 5.7, C++, Learning Agents enabled; `Weapon` trace channel = GameTraceChannel1, default Block.
- [ ] Snapshot (encoder, policy, decoder) from release `run-<id>`, SHA verified; stage and `-Set` read from its run report.
- [ ] Interactor schema copied verbatim; policy settings 2 × 256, GRU 128, ELU; load-time size and hash check.
- [ ] One manager and policy; `RunInference(0)` once per decision for all agents; `ResetAgents` on spawn and on a new virtual episode.
- [ ] 15 Hz decision clock on game time; Aim integrated at 720°/s × Aim every frame; Move applied every frame; CharacterMovement max sim step 1/60.
- [ ] Agent pawn = the bean: capsule 34/88 (60 crouched), eye 158/102, primitive hitboxes; no camera recoil; movement and rifle values exact.
- [ ] Player provides Head, Chest, Velocity, Forward2D, crouched and alive; is in each agent's enemy list; is hittable by the `Weapon` trace with Head/Body/Legs zones.
- [ ] Visibility = 103° × 70.53° cone on the head plus LOS (WorldStatic/WorldDynamic only) to the head or chest; 2 s memory; max 5.
- [ ] Player sounds: footsteps (running > 315 cm/s, not crouched or walking, every 0.35 s, 15 m) and gunshots (every shot, 40 m); 200 cm disc noise per event; 1 s memory; max 4; never ally or own sounds.
- [ ] Hit indicator from enemy damage only (instigator location, 1 s).
- [ ] Allies invisible: not enemies, not heard, shots pass through, no friendly fire, no mutual capsule blocking.
- [ ] Levels: 2.5D, 4 m-scale blockout, cover 120/250 cm, no ceilings near agents (or a ceiling channel), simple collision, perception and bullets blocked by the same things, no clutter on WorldStatic/WorldDynamic.
- [ ] Virtual episodes and time left per section 12; spawns ≥ 5 m from the player, ≥ 4 m apart.
- [ ] Difficulty only via section 13 knobs.
- [ ] Server-authoritative inference.
- [ ] All section 16 acceptance tests pass.
