#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "FPSAgentTypes.generated.h"

/** Hitscan trace channel: "Weapon" (ECC_GameTraceChannel2 in GameJam, default Block) in Config/DefaultEngine.ini. */
inline constexpr ECollisionChannel ECC_Weapon = ECC_GameTraceChannel2; // GameJam: channel 1 is the template's Projectile (contract §2 allows remapping)

UENUM(BlueprintType)
enum class EHitZone : uint8 { None, Head, Body, Legs };

UENUM(BlueprintType)
enum class ERangeBotMode : uint8 { Static, Strafe };

/** Movement tuning (cm, cm/s). "verify" = community-sourced estimate, confirm by play-testing. */
USTRUCT(BlueprintType)
struct FMovementTuning
{
	GENERATED_BODY()
	/** ~5.4 m/s with a rifle (community-sourced, verify). */
	UPROPERTY(EditAnywhere, Category="Movement") float RunSpeed = 540.f;
	/** Shift-walk speed, ~55% of run (verify). */
	UPROPERTY(EditAnywhere, Category="Movement") float WalkSpeed = 300.f;
	/** Crouch speed, ~35% of run (verify). */
	UPROPERTY(EditAnywhere, Category="Movement") float CrouchSpeed = 190.f;
	/** High accel/braking = snappy counter-strafe (tune by feel). */
	UPROPERTY(EditAnywhere, Category="Movement") float MaxAcceleration = 4000.f;
	UPROPERTY(EditAnywhere, Category="Movement") float BrakingDeceleration = 6000.f;
	UPROPERTY(EditAnywhere, Category="Movement") float GroundFriction = 8.f;
	/** Jump launch speed (verify jump height). */
	UPROPERTY(EditAnywhere, Category="Movement") float JumpZVelocity = 420.f;
	UPROPERTY(EditAnywhere, Category="Movement") float AirControl = 0.3f;
	/** Fully accurate at or below this fraction of RunSpeed: 27.5% (secondary source, verify). */
	UPROPERTY(EditAnywhere, Category="Movement") float AccurateSpeedFraction = 0.275f;
};

/** Bean geometry (cm). Hitboxes derive from it via ComputeBeanHitboxLayout. */
USTRUCT(BlueprintType)
struct FBeanBody
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Body") float CapsuleRadius = 34.f;
	/** 1.76 m tall standing. */
	UPROPERTY(EditAnywhere, Category="Body") float StandingHalfHeight = 88.f;
	UPROPERTY(EditAnywhere, Category="Body") float CrouchedHalfHeight = 60.f;
	UPROPERTY(EditAnywhere, Category="Body") float HeadRadius = 12.f;
	UPROPERTY(EditAnywhere, Category="Body") float HeadTopClearance = 6.f;
	/** Fraction of total height covered by the legs hitbox. */
	UPROPERTY(EditAnywhere, Category="Body") float LegFraction = 0.386f;
	UPROPERTY(EditAnywhere, Category="Body") float BodyHalfDepth = 18.f;
	UPROPERTY(EditAnywhere, Category="Body") float BodyHalfWidth = 26.f;
	UPROPERTY(EditAnywhere, Category="Body") float LegsHalfDepth = 16.f;
	UPROPERTY(EditAnywhere, Category="Body") float LegsHalfWidth = 20.f;
};

/** Automatic rifle with Vandal-like stats (generic name). Degrees / seconds / cm. */
USTRUCT(BlueprintType)
struct FRifleStats
{
	GENERATED_BODY()
	/** 160 / 40 / 34, no falloff (2025-26 guides, verify). */
	UPROPERTY(EditAnywhere, Category="Damage") float DamageHead = 160.f;
	UPROPERTY(EditAnywhere, Category="Damage") float DamageBody = 40.f;
	UPROPERTY(EditAnywhere, Category="Damage") float DamageLegs = 34.f;
	UPROPERTY(EditAnywhere, Category="Damage") float MaxRangeCm = 10000.f;
	/** Rounds per second (guides, verify). */
	UPROPERTY(EditAnywhere, Category="Handling") float FireRate = 9.75f;
	UPROPERTY(EditAnywhere, Category="Handling") int32 MagazineSize = 25;
	/** Seconds (verify). */
	UPROPERTY(EditAnywhere, Category="Handling") float ReloadTime = 2.5f;
	/** Standing-still first-shot spread, cone half-angle (guide, verify). */
	UPROPERTY(EditAnywhere, Category="Spread") float FirstShotSpreadDeg = 0.25f;
	/** Spread just above the accurate-speed threshold (tune). */
	UPROPERTY(EditAnywhere, Category="Spread") float WalkSpreadDeg = 2.5f;
	/** Spread at full run speed (tune). */
	UPROPERTY(EditAnywhere, Category="Spread") float RunSpreadDeg = 5.f;
	UPROPERTY(EditAnywhere, Category="Spread") float AirSpreadDeg = 8.f;
	UPROPERTY(EditAnywhere, Category="Spread") float CrouchSpreadMultiplier = 0.85f;
	/** Shots 0..AccurateShots-1 get no spray bloom. */
	UPROPERTY(EditAnywhere, Category="Spread") int32 AccurateShots = 3;
	UPROPERTY(EditAnywhere, Category="Spread") float SprayBloomPerShotDeg = 0.35f;
	UPROPERTY(EditAnywhere, Category="Spread") float SprayBloomMaxDeg = 3.f;
	/** Our own recoil pattern: small kick, then vertical climb to a cap, then horizontal sway. */
	UPROPERTY(EditAnywhere, Category="Recoil") float RecoilKickFirstShotsDeg = 0.15f;
	UPROPERTY(EditAnywhere, Category="Recoil") float RecoilClimbPerShotDeg = 0.6f;
	UPROPERTY(EditAnywhere, Category="Recoil") float RecoilClimbMaxDeg = 6.f;
	UPROPERTY(EditAnywhere, Category="Recoil") float RecoilSwayAmplitudeDeg = 1.5f;
	UPROPERTY(EditAnywhere, Category="Recoil") int32 RecoilSwayPeriodShots = 6;
	/** Idle seconds that recover one step of the spray index (verify feel). */
	UPROPERTY(EditAnywhere, Category="Recoil") float RecoveryTimePerShot = 0.04f;
};

USTRUCT(BlueprintType)
struct FHealthTuning
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Health") float MaxHealth = 100.f;
	/** Heavy-shield equivalent (verify). */
	UPROPERTY(EditAnywhere, Category="Health") float MaxArmor = 50.f;
	/** Fraction of each hit taken by armor while armor remains (verify: 1.0 = armor absorbs everything first). */
	UPROPERTY(EditAnywhere, Category="Health") float ArmorAbsorbFraction = 1.f;
};

USTRUCT(BlueprintType)
struct FRangeLane
{
	GENERATED_BODY()
	/** Distance in front of the player (+X). */
	UPROPERTY(EditAnywhere, Category="Range") float DistanceCm = 1000.f;
	/** Sideways offset (+Y = right of the player). */
	UPROPERTY(EditAnywhere, Category="Range") float LateralCm = 0.f;
	UPROPERTY(EditAnywhere, Category="Range") ERangeBotMode Mode = ERangeBotMode::Static;
};

/** What the arena game mode builds. Precedence: -Map= command line > fps.PlayMap console variable > this setting. */
USTRUCT(BlueprintType)
struct FMapPlaySettings
{
	GENERATED_BODY()
	/** "" or "range" = shooting range; "map01" / "gen_007" = that spec in Content/MapSpecs; "pool" = seeded random pool entry. */
	UPROPERTY(EditAnywhere, Category="Map") FString PlayMap;
	/** Range bots spawned on bot spots when playing a map. */
	UPROPERTY(EditAnywhere, Category="Map") int32 BotCount = 6;
};

USTRUCT(BlueprintType)
struct FRangeSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Range") TArray<FRangeLane> Lanes;
	UPROPERTY(EditAnywhere, Category="Range") float StrafeHalfWidthCm = 300.f;
	UPROPERTY(EditAnywhere, Category="Range") float StrafeMinPeriod = 0.35f;
	UPROPERTY(EditAnywhere, Category="Range") float StrafeMaxPeriod = 1.1f;
	UPROPERTY(EditAnywhere, Category="Range") float BotRespawnDelay = 1.5f;
	UPROPERTY(EditAnywhere, Category="Range") float ArenaLengthCm = 6000.f;
	UPROPERTY(EditAnywhere, Category="Range") float ArenaWidthCm = 3000.f;
	UPROPERTY(EditAnywhere, Category="Range") float WallHeightCm = 400.f;
	UPROPERTY(EditAnywhere, Category="Range") float WallThicknessCm = 50.f;
};

/**
 * Spike A toy task (-Stage=SpikeA): walk the bean to a random point on a walled floor. It proves the training stack
 * (packaged Linux build -> external trainer -> snapshots -> resume), not gameplay. All values are toy choices with no
 * external source; tune freely.
 */
USTRUCT(BlueprintType)
struct FReachTaskSettings
{
	GENERATED_BODY()
	/** Side of each square arena floor (walled). */
	UPROPERTY(EditAnywhere, Category="SpikeA") float ArenaSizeCm = 2000.f;
	/** Distance between arena centers. Arenas never touch, so agents never meet (hard rule 3). */
	UPROPERTY(EditAnywhere, Category="SpikeA") float ArenaSpacingCm = 4000.f;
	/** Starts and targets keep this far from the walls. */
	UPROPERTY(EditAnywhere, Category="SpikeA") float WallMarginCm = 150.f;
	UPROPERTY(EditAnywhere, Category="SpikeA") float TargetMinDistanceCm = 300.f;
	UPROPERTY(EditAnywhere, Category="SpikeA") float TargetMaxDistanceCm = 1200.f;
	/** Success when the bean's center is within this 2D distance of the target. */
	UPROPERTY(EditAnywhere, Category="SpikeA") float ReachRadiusCm = 100.f;
	/** Reward per meter closed in a step (negative when moving away). */
	UPROPERTY(EditAnywhere, Category="SpikeA") float ProgressRewardPerMeter = 1.f;
	/** One-off reward on reaching the target. */
	UPROPERTY(EditAnywhere, Category="SpikeA") float ReachBonus = 5.f;
	/** Cost per second of episode time, so the shortest path wins. */
	UPROPERTY(EditAnywhere, Category="SpikeA") float TimePenaltyPerSecond = 0.5f;
	/** Episode cap in steps (one step per 60 Hz tick: 360 = 6 s). Hitting it is a truncation, not a failure. */
	UPROPERTY(EditAnywhere, Category="SpikeA") int32 MaxEpisodeSteps = 360;
	/** Trainer writes snapshots every N iterations. */
	UPROPERTY(EditAnywhere, Category="SpikeA") int32 IterationsPerSnapshot = 10;
	/** Trainer stops after N iterations and tells the games to exit. -Iterations= overrides. */
	UPROPERTY(EditAnywhere, Category="SpikeA") int32 MaxIterations = 1000000;
	/** Ask the trainer for GPU (it falls back to CPU without CUDA). */
	UPROPERTY(EditAnywhere, Category="SpikeA") bool bTrainOnGpu = true;
	/** Ask the trainer for TensorBoard logs (skipped with a warning if the package is missing). */
	UPROPERTY(EditAnywhere, Category="SpikeA") bool bUseTensorboard = true;
	/** Game-side stats line in the log every N seconds of game time. */
	UPROPERTY(EditAnywhere, Category="SpikeA") float StatsLogIntervalSeconds = 10.f;
};

/**
 * Combat reward weights (spec 2026-11-12-combat-training-design §3). Design values agreed with the user, no external
 * source: tune by sweeps (-Set=Combat.Reward.<Field>=<value>).
 */
USTRUCT(BlueprintType)
struct FCombatRewardSettings
{
	GENERATED_BODY()
	/** Damage is divided by this: full health + armor (100 + 50), so a kill from full is worth 1.0. */
	UPROPERTY(EditAnywhere, Category="Reward") float DamageNormalizer = 150.f;
	/** Multiplies damage dealt / DamageNormalizer (health + armor actually removed). */
	UPROPERTY(EditAnywhere, Category="Reward") float DamageDealtWeight = 1.f;
	UPROPERTY(EditAnywhere, Category="Reward") float Kill = 1.f;
	/** Once per bot per episode, on the first decision it passes the line-of-sight-in-FOV check. */
	UPROPERTY(EditAnywhere, Category="Reward") float FirstSighting = 0.2f;
	/** Clear bonus = ClearBase + ClearSpeed * (time left / time limit): once, when every bot is dead and the agent lives. */
	UPROPERTY(EditAnywhere, Category="Reward") float ClearBase = 0.5f;
	UPROPERTY(EditAnywhere, Category="Reward") float ClearSpeed = 1.5f;
	/** The aim-helper potential reaches 0 at this aim error (degrees). Its weight is per stage (FCombatStageSpec). */
	UPROPERTY(EditAnywhere, Category="Reward") float AimHelperRangeDeg = 30.f;
	/** Multiplies -(damage taken / DamageNormalizer). */
	UPROPERTY(EditAnywhere, Category="Reward") float DamageTakenWeight = 1.f;
	UPROPERTY(EditAnywhere, Category="Reward") float Death = -1.f;
	/** Per fired bullet that hits no enemy. */
	UPROPERTY(EditAnywhere, Category="Reward") float MissPerBullet = -0.01f;
	/** Per second of episode time (anti-camping). */
	UPROPERTY(EditAnywhere, Category="Reward") float TimePerSecond = -0.01f;
};

/** Trainer and network settings for combat stages (D-023). The game sends them to the trainer; sweep with -Set=Combat.Ppo.*. */
USTRUCT(BlueprintType)
struct FCombatPpoSettings
{
	GENERATED_BODY()
	/** A decision every N fixed 60 Hz ticks: 4 = 15 decisions per second (D-023). */
	UPROPERTY(EditAnywhere, Category="PPO") int32 TicksPerDecision = 4;
	/** 0.995 at 15 Hz looks about 13 s ahead (research note §2). Sweep 0.99-0.997. */
	UPROPERTY(EditAnywhere, Category="PPO") float Discount = 0.995f;
	UPROPERTY(EditAnywhere, Category="PPO") float GaeLambda = 0.95f;
	/** Learning Agents default 0; sweep 0 / 0.001 / 0.01 (research note §5). */
	UPROPERTY(EditAnywhere, Category="PPO") float EntropyWeight = 0.f;
	UPROPERTY(EditAnywhere, Category="PPO") float LearningRatePolicy = 0.0001f;
	UPROPERTY(EditAnywhere, Category="PPO") float LearningRateCritic = 0.001f;
	/** Gradient work per update (Learning Agents defaults). Scale with the number of games via -Set=Combat.Ppo.* (C-007, Spike B). */
	UPROPERTY(EditAnywhere, Category="PPO") int32 IterationsPerGather = 32;
	UPROPERTY(EditAnywhere, Category="PPO") int32 PolicyBatchSize = 1024;
	UPROPERTY(EditAnywhere, Category="PPO") int32 CriticBatchSize = 4096;
	/** Experience per update: the replay buffer is sent when either limit is reached (Learning Agents defaults; C-007). */
	UPROPERTY(EditAnywhere, Category="PPO") int32 ReplayMaxSteps = 10000;
	UPROPERTY(EditAnywhere, Category="PPO") int32 ReplayMaxEpisodes = 1000;
	/** Policy and critic: HiddenLayers x HiddenSize; policy memory MemorySize. Changing them breaks resume from older snapshots. */
	UPROPERTY(EditAnywhere, Category="PPO") int32 HiddenLayers = 2;
	UPROPERTY(EditAnywhere, Category="PPO") int32 HiddenSize = 256;
	UPROPERTY(EditAnywhere, Category="PPO") int32 MemorySize = 128;
	/** Trainer snapshot interval in gradient iterations (README -> Snapshot timing). */
	UPROPERTY(EditAnywhere, Category="PPO") int32 IterationsPerSnapshot = 10;
	/** Run length when neither -Batches nor -Iterations is given (gradient iterations). */
	UPROPERTY(EditAnywhere, Category="PPO") int32 MaxIterations = 1000000;
	UPROPERTY(EditAnywhere, Category="PPO") bool bTrainOnGpu = true;
	UPROPERTY(EditAnywhere, Category="PPO") bool bUseTensorboard = true;
	/** Game-side stats line (and -StatsCsv row) every N seconds of game time. */
	UPROPERTY(EditAnywhere, Category="PPO") float StatsLogIntervalSeconds = 10.f;
};

/** What the agent perceives and how fast it turns (spec §2). Changing ray or set sizes changes the network layout. */
USTRUCT(BlueprintType)
struct FCombatPerceptionSettings
{
	GENERATED_BODY()
	/** Horizontal field of view for the line-of-sight check (README: 103°, verify). */
	UPROPERTY(EditAnywhere, Category="Perception") float FieldOfViewDeg = 103.f;
	/** Vertical view angle: 103 deg horizontal at 16:9 = 70.53 deg vertical (2*atan(tan(51.5 deg) * 9/16)). */
	UPROPERTY(EditAnywhere, Category="Perception") float VerticalFieldOfViewDeg = 70.53f;
	/** Fine aim channels: aim error / FineAimScaleDeg, clamped to [-1, 1], so sub-degree errors are visible to the policy (no observation normalization in the trainer). */
	UPROPERTY(EditAnywhere, Category="Perception") float FineAimScaleDeg = 5.f;
	/** Enemies seen within this many seconds stay in the observation as "remembered" (spec §2). */
	UPROPERTY(EditAnywhere, Category="Perception") float MemorySeconds = 2.f;
	UPROPERTY(EditAnywhere, Category="Perception") int32 MaxEnemies = 5;
	UPROPERTY(EditAnywhere, Category="Perception") int32 MaxSounds = 4;
	/** Ray ring from eye height: RayYawCount directions x RayPitchesDeg (degrees, + = up). Level geometry only. */
	UPROPERTY(EditAnywhere, Category="Perception") int32 RayYawCount = 24;
	UPROPERTY(EditAnywhere, Category="Perception") TArray<float> RayPitchesDeg = { -20.f, 0.f, 15.f };
	UPROPERTY(EditAnywhere, Category="Perception") float RayRangeCm = 3000.f;
	/** Observation scales: positions and distances / PositionScaleCm, velocities / VelocityScaleCmPerS, angles / 180°. */
	UPROPERTY(EditAnywhere, Category="Perception") float PositionScaleCm = 3000.f;
	UPROPERTY(EditAnywhere, Category="Perception") float VelocityScaleCmPerS = 600.f;
	/** The hit indicator shows the direction of damage for this long (D-024). */
	UPROPERTY(EditAnywhere, Category="Perception") float HitIndicatorSeconds = 1.f;
	/** Aim turn-rate cap (human-likeness, verify): 720°/s = 48° per decision at 15 Hz. */
	UPROPERTY(EditAnywhere, Category="Perception") float MaxTurnRateDegPerSec = 720.f;
	UPROPERTY(EditAnywhere, Category="Perception") float MaxPitchDeg = 80.f;
};

/** Sampling weights of the scripted-bot behaviours (spec §4), lerped between a stage's easy and hard mix by the difficulty level. */
USTRUCT(BlueprintType)
struct FBotBehaviourMix
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Bots") float Static = 1.f;
	UPROPERTY(EditAnywhere, Category="Bots") float Strafe = 0.f;
	UPROPERTY(EditAnywhere, Category="Bots") float Peek = 0.f;
	UPROPERTY(EditAnywhere, Category="Bots") float Crouch = 0.f;
	UPROPERTY(EditAnywhere, Category="Bots") float Patrol = 0.f;
	UPROPERTY(EditAnywhere, Category="Bots") float Explore = 0.f;
};

/** One curriculum stage (spec §5): each knob has an easy (difficulty 0) and a hard (difficulty 1) value. Build A: S0. */
USTRUCT(BlueprintType)
struct FCombatStageSpec
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Stage") float TimeLimitSeconds = 20.f;
	/** Aim-helper potential weight W (spec §3): 0.2 in S0/S1, 0.1 in S2, 0 in S3. */
	UPROPERTY(EditAnywhere, Category="Stage") float AimHelperWeight = 0.2f;
	/** S0: false (movement, crouch, walk and jump actions are ignored). */
	UPROPERTY(EditAnywhere, Category="Stage") bool bAgentCanMove = false;
	/** Open floor side in 4 m tiles (20 = 80 m), inside a one-tile wall ring. */
	UPROPERTY(EditAnywhere, Category="Stage") int32 FloorTiles = 20;
	UPROPERTY(EditAnywhere, Category="Stage") int32 BotsEasy = 1;
	UPROPERTY(EditAnywhere, Category="Stage") int32 BotsHard = 5;
	UPROPERTY(EditAnywhere, Category="Stage") float BotDistanceMinCm = 500.f;
	UPROPERTY(EditAnywhere, Category="Stage") float BotDistanceMaxCmEasy = 1500.f;
	UPROPERTY(EditAnywhere, Category="Stage") float BotDistanceMaxCmHard = 3500.f;
	/** Bots stand within ± this many degrees of the agent's facing (180 = all around). */
	UPROPERTY(EditAnywhere, Category="Stage") float BotSpreadDegEasy = 45.f;
	UPROPERTY(EditAnywhere, Category="Stage") float BotSpreadDegHard = 180.f;
	UPROPERTY(EditAnywhere, Category="Stage") float CrouchedFractionEasy = 0.f;
	UPROPERTY(EditAnywhere, Category="Stage") float CrouchedFractionHard = 0.3f;
	/** Cover tiles as a fraction of the interior floor (spec §5, S1: 0 -> "medium"; verify in S1 training). S0: none. */
	UPROPERTY(EditAnywhere, Category="Stage") float CoverDensityEasy = 0.f;
	UPROPERTY(EditAnywhere, Category="Stage") float CoverDensityHard = 0.f;
	/** Share of the cover tiles that are low (FPSMapFormat::LowCoverCm, crouch to hide); the rest are high (HighCoverCm). Starting guess (D-031); verify in S1 training. */
	UPROPERTY(EditAnywhere, Category="Stage") float LowCoverFraction = 0.5f;
	/** Bot movement speed as a fraction of the run speed (spec §5, S1: 60% -> 100%). A scale of 0 freezes a bot. */
	UPROPERTY(EditAnywhere, Category="Stage") float BotSpeedScaleEasy = 1.f;
	UPROPERTY(EditAnywhere, Category="Stage") float BotSpeedScaleHard = 1.f;
	/** Behaviour mix at difficulty 0 and 1 (default: static only, as in S0). */
	UPROPERTY(EditAnywhere, Category="Stage") FBotBehaviourMix BehaviourMixEasy;
	UPROPERTY(EditAnywhere, Category="Stage") FBotBehaviourMix BehaviourMixHard;
	/** Promotion gate: the -Eval clear rate at difficulty 1 (spec §5). */
	UPROPERTY(EditAnywhere, Category="Stage") float PromotionClearRate = 0.7f;
};

/** S1 "Movers" (spec §5): the agent moves; 1 -> 5 bots that strafe/peek/crouch, then also patrol/explore; speed 60% -> 100%; cover 0 -> medium; 30 s. */
inline FCombatStageSpec MakeS1StageSpec()
{
	FCombatStageSpec S;
	S.TimeLimitSeconds = 30.f;
	S.AimHelperWeight = 0.2f;
	S.bAgentCanMove = true;
	S.CoverDensityEasy = 0.f;
	S.CoverDensityHard = 0.12f; // "medium": 48 of the 400 interior tiles (verify in S1 training)
	S.BotSpeedScaleEasy = 0.6f;
	S.BotSpeedScaleHard = 1.f;
	S.BehaviourMixEasy.Static = 0.f;
	S.BehaviourMixEasy.Strafe = 1.f;
	S.BehaviourMixEasy.Peek = 1.f;
	S.BehaviourMixEasy.Crouch = 1.f;
	S.BehaviourMixHard = S.BehaviourMixEasy;
	// Mix weights: spec §5 names the behaviours, not their shares. Starting guess (D-031); verify in S1 training.
	S.BehaviourMixHard.Patrol = 1.5f;
	S.BehaviourMixHard.Explore = 1.5f;
	return S;
}

/**
 * Scripted-bot senses and behaviour tuning (spec §4). The senses use the same rules as the agent's (hard rule 4, D-024).
 * Sources: the view cone, memory and hearing radii come from spec §4 (as noted per field). Every other value here is
 * a starting guess (D-031), with no external source: the hearing noise, the footstep cadence and the running threshold,
 * and all Behaviour timings, widths, counts and path-following tolerances. Verify them in S1 training, from the
 * -Scripted video and the CombatStats.
 */
USTRUCT(BlueprintType)
struct FBotBehaviourSettings
{
	GENERATED_BODY()
	/** View cone for spotting the agent: the same 103 deg as the agent's (spec §4). */
	UPROPERTY(EditAnywhere, Category="Senses") float SightFovDeg = 103.f;
	/** A sighting or a heard sound is acted on only after this delay (S1 constant; S2 makes it a stage knob; verify). */
	UPROPERTY(EditAnywhere, Category="Senses") float ReactionSeconds = 0.4f;
	/** S2+: how long a bot that shoots keeps aiming at the last seen position (about 3 s, spec §4). Not used in S1: a bot that loses sight keeps walking to the last stimulus (Investigate). */
	UPROPERTY(EditAnywhere, Category="Senses") float SightMemorySeconds = 3.f;
	/** Gunshots are heard within 40 m and running footsteps within 15 m; walking and crouching are silent (spec §4; verify). */
	UPROPERTY(EditAnywhere, Category="Senses") float GunshotHearingCm = 4000.f;
	UPROPERTY(EditAnywhere, Category="Senses") float FootstepHearingCm = 1500.f;
	/** A heard sound gives its position with this much seeded noise (radius; verify). */
	UPROPERTY(EditAnywhere, Category="Senses") float HearingNoiseCm = 200.f;
	/** A running bean makes one footstep sound event per this many seconds (about a running stride; verify). */
	UPROPERTY(EditAnywhere, Category="Senses") float FootstepIntervalSeconds = 0.35f;
	/** "Running" = faster than WalkSpeed x this, while not walking and not crouched (verify). */
	UPROPERTY(EditAnywhere, Category="Senses") float SilentSpeedFraction = 1.05f;
	/** The agent keeps a heard enemy sound in its observation this long (D-031). */
	UPROPERTY(EditAnywhere, Category="Senses") float SoundMemorySeconds = 1.f;
	/** Strafe: ADAD flip period and how far from the spawn spot it may drift sideways. */
	UPROPERTY(EditAnywhere, Category="Behaviour") float StrafePeriodMinSeconds = 0.3f;
	UPROPERTY(EditAnywhere, Category="Behaviour") float StrafePeriodMaxSeconds = 0.9f;
	UPROPERTY(EditAnywhere, Category="Behaviour") float StrafeHalfWidthCm = 250.f;
	/** Peek: time spent out of cover / behind cover before switching. */
	UPROPERTY(EditAnywhere, Category="Behaviour") float PeekPeriodMinSeconds = 0.8f;
	UPROPERTY(EditAnywhere, Category="Behaviour") float PeekPeriodMaxSeconds = 2.f;
	/** Crouch: crouch-spam toggle period. */
	UPROPERTY(EditAnywhere, Category="Behaviour") float CrouchPeriodMinSeconds = 0.3f;
	UPROPERTY(EditAnywhere, Category="Behaviour") float CrouchPeriodMaxSeconds = 1.2f;
	/** Patrol: number of waypoints in the loop and the pause at each. */
	UPROPERTY(EditAnywhere, Category="Behaviour") int32 PatrolWaypoints = 4;
	UPROPERTY(EditAnywhere, Category="Behaviour") float PatrolPauseMinSeconds = 0.5f;
	UPROPERTY(EditAnywhere, Category="Behaviour") float PatrolPauseMaxSeconds = 2.f;
	/** Explore: random candidate tiles per pick; the least recently visited wins. */
	UPROPERTY(EditAnywhere, Category="Behaviour") int32 ExploreCandidates = 6;
	/** Investigate: look around for this long at the target, turning at this rate. */
	UPROPERTY(EditAnywhere, Category="Behaviour") float InvestigateLookSeconds = 3.f;
	UPROPERTY(EditAnywhere, Category="Behaviour") float InvestigateTurnDegPerSec = 120.f;
	/** Path following: a waypoint counts as reached within this distance; waypoints get a seeded offset up to this size. */
	UPROPERTY(EditAnywhere, Category="Behaviour") float WaypointToleranceCm = 60.f;
	UPROPERTY(EditAnywhere, Category="Behaviour") float PathOffsetCm = 50.f;
	/** No progress toward a waypoint for this long: skip it. */
	UPROPERTY(EditAnywhere, Category="Behaviour") float StuckSeconds = 2.f;
	/** How fast a bot turns its body toward where it wants to look. */
	UPROPERTY(EditAnywhere, Category="Behaviour") float TurnRateDegPerSec = 360.f;
};

/** The automatic difficulty ramp (spec §5) and the stage table. */
USTRUCT(BlueprintType)
struct FCurriculumSettings
{
	GENERATED_BODY()
	/** Counted (non-review) episodes per rolling window. */
	UPROPERTY(EditAnywhere, Category="Curriculum") int32 WindowEpisodes = 20;
	UPROPERTY(EditAnywhere, Category="Curriculum") float RaiseAboveClearRate = 0.7f;
	UPROPERTY(EditAnywhere, Category="Curriculum") float LowerBelowClearRate = 0.4f;
	UPROPERTY(EditAnywhere, Category="Curriculum") float Step = 0.05f;
	/** Fraction of episodes sampled at a random easier level (anti-forgetting). They are not counted by the ramp. */
	UPROPERTY(EditAnywhere, Category="Curriculum") float ReviewFraction = 0.2f;
	/** Knob jitter as a fraction of the easy-hard range. */
	UPROPERTY(EditAnywhere, Category="Curriculum") float JitterFraction = 0.1f;
	UPROPERTY(EditAnywhere, Category="Curriculum") FCombatStageSpec S0;
	UPROPERTY(EditAnywhere, Category="Curriculum") FCombatStageSpec S1 = MakeS1StageSpec();
	/** Non-review episodes at or above this level count toward the gate-level clear rate `clearTop` (promotion trigger, D-031). */
	UPROPERTY(EditAnywhere, Category="Curriculum") float GateLevel = 0.95f;

	/** The spec of combat stage Index (0 = S0, 1 = S1). */
	const FCombatStageSpec& Stage(int32 Index) const { return Index == 1 ? S1 : S0; }
};

/**
 * The agent abilities a policy may use (D-033). These are the switches of a mobility line, the same in every stage, applied on top
 * of the stage's bAgentCanMove. Off = the policy's output for that action is ignored, as S0 ignores movement; the network layout
 * doesn't change. Design switches, not mechanics (no external source). The `walk` line (the game's) turns both off.
 */
USTRUCT(BlueprintType)
struct FCombatAbilitySettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Abilities") bool bCrouch = true;
	UPROPERTY(EditAnywhere, Category="Abilities") bool bJump = true;
};

/** Mobility tag for run IDs and logs (D-033): static = no movement (default S0), walk = move + walk only, full = crouch and jump too. */
inline const TCHAR* MobilityTag(bool bCanMove, const FCombatAbilitySettings& A)
{
	if (!bCanMove) { return TEXT("static"); }
	if (A.bCrouch && A.bJump) { return TEXT("full"); }
	if (!A.bCrouch && !A.bJump) { return TEXT("walk"); }
	return A.bCrouch ? TEXT("walk+crouch") : TEXT("walk+jump");
}

/** Combat stages (S0-S3): rewards, trainer, perception, arena spacing. */
USTRUCT(BlueprintType)
struct FCombatSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Combat") FCombatRewardSettings Reward;
	UPROPERTY(EditAnywhere, Category="Combat") FCombatPpoSettings Ppo;
	UPROPERTY(EditAnywhere, Category="Combat") FCombatPerceptionSettings Perception;
	/** Mobility line switches (D-033), e.g. -Set="Combat.Abilities.bCrouch=false;Combat.Abilities.bJump=false" for the `walk` line. */
	UPROPERTY(EditAnywhere, Category="Combat") FCombatAbilitySettings Abilities;
	/** Distance between arena centres: more than the largest arena plus 50 m (spec §5). Arenas never interact anyway. */
	UPROPERTY(EditAnywhere, Category="Combat") float ArenaSpacingCm = 15000.f;
};
