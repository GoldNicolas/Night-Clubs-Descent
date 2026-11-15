#pragma once

#include "CoreMinimal.h"
#include "Common/FPSAgentTypes.h"

/**
 * The agent's observation and action layout (spec §2), as plain float arrays the Learning Agents layer wraps. Pure.
 * Self, enemy, hit and sound vectors have fixed sizes; changing them changes the network layout (no resume across).
 */
namespace FPSCombatObs
{
	inline constexpr int32 SelfSize = 15;
	inline constexpr int32 EnemySize = 15;
	inline constexpr int32 HitSize = 4;
	inline constexpr int32 SoundSize = 5;

	struct FSelfState
	{
		FVector VelocityView = FVector::ZeroVector;
		bool bGrounded = true;
		bool bCrouched = false;
		bool bWalking = false;
		float Health = 0.f;
		float MaxHealth = 100.f;
		float Armor = 0.f;
		float MaxArmor = 50.f;
		int32 Ammo = 0;
		int32 MagazineSize = 25;
		bool bReloading = false;
		float SpreadDeg = 0.f;
		int32 SprayShots = 0;
		float SecondsSinceShot = 1.f;
		float AimPitchDeg = 0.f;
		float TimeLeftFraction = 1.f;
	};
	/** [vel x,y,z / VelocityScale, grounded, crouched, walking, health, armor, ammo (fractions), reloading,
	 *   spread / 10 deg, spray shots / magazine, min(time since shot, 1 s), pitch / 90, time left]. */
	AGENTRUNTIME_API TArray<float> PackSelf(const FSelfState& S, const FCombatPerceptionSettings& P);

	struct FEnemyState
	{
		/** Head position relative to the agent's eye, in the view frame (cm). */
		FVector HeadView = FVector::ZeroVector;
		FVector2D AimErrorDeg = FVector2D::ZeroVector;
		FVector VelocityView = FVector::ZeroVector;
		bool bFacingAgent = false;
		bool bCrouched = false;
		bool bVisible = false;
		float SecondsSinceSeen = 0.f;
	};
	/** [head x,y,z / PositionScale, distance / PositionScale, aim yaw, pitch / 180, vel x,y,z / VelocityScale,
	 *   facing, crouched, visible, min(age / MemorySeconds, 1), fine aim yaw, fine aim pitch (aim error / FineAimScaleDeg, clamped to [-1, 1])]. */
	AGENTRUNTIME_API TArray<float> PackEnemy(const FEnemyState& E, const FCombatPerceptionSettings& P);
	/** [direction x,y,z (view frame, unit), min(age / HitIndicatorSeconds, 1)]. */
	AGENTRUNTIME_API TArray<float> PackHit(const FVector& DirectionView, float SecondsAgo, const FCombatPerceptionSettings& P);
	/** [direction x,y,z (view frame, unit), min(distance / PositionScaleCm, 1), min(age / SoundMemorySeconds, 1)] (spec §2 hearing, D-024). */
	AGENTRUNTIME_API TArray<float> PackSound(const FVector& DirectionView, float DistanceCm, float AgeSeconds, const FCombatPerceptionSettings& P, float SoundMemorySeconds);

	/** One decision's observation, ready for the Learning Agents layer. */
	struct FObservationData
	{
		TArray<float> Self;
		TArray<float> Rays;
		TArray<TArray<float>> Enemies;
		bool bHit = false;
		TArray<float> Hit;
		TArray<TArray<float>> Sounds;

		/** All zeros with the right sizes (fallback when an agent id is unknown). */
		static FObservationData Zero(int32 RayCount);
	};

	/** One decision's action. Move/Aim in [-1, 1]; held for the next TicksPerDecision ticks. */
	struct FAgentAction
	{
		FVector2D Move = FVector2D::ZeroVector;
		FVector2D Aim = FVector2D::ZeroVector;
		bool bFire = false;
		bool bReload = false;
		bool bCrouch = false;
		bool bWalk = false;
		bool bJump = false;
	};
	/** Policy samples can leave [-1, 1]: non-finite components become 0, then Aim is clamped per axis and Move to length 1. */
	AGENTRUNTIME_API FAgentAction Sanitize(const FAgentAction& A);
	/** D-033 mobility line: clears Crouch / Jump when the run's Combat.Abilities switch is off, so that output is ignored (as S0 ignores movement). */
	AGENTRUNTIME_API FAgentAction ApplyAbilities(const FAgentAction& A, const FCombatAbilitySettings& Abilities);
	/** Aim change per tick in degrees (yaw, pitch): Aim * MaxTurnRate * TickSeconds (spread evenly, never snapped). */
	AGENTRUNTIME_API FVector2D AimPerTickDeg(const FVector2D& Aim, const FCombatPerceptionSettings& P, float TickSeconds);
	/** View-frame move input (forward, right) to a world direction (Z = 0). */
	AGENTRUNTIME_API FVector MoveWorld(const FVector2D& Move, float ViewYawDeg);

	/** The nearest visible enemy for the scripted reference agent. */
	struct FScriptedTarget
	{
		bool bHasTarget = false;
		FVector2D AimErrorDeg = FVector2D::ZeroVector;
		float DistanceCm = 0.f;
	};
	/**
	 * -Scripted reference agent for S0: turn toward the target at the turn-rate cap; fire when the aim error is within
	 * the head's angular radius (at least 0.25 deg); scan right when nothing is visible; reload when empty (or below half
	 * with nothing in sight). Uses the same senses as the policy, nothing more.
	 */
	AGENTRUNTIME_API FAgentAction ScriptedAction(const FScriptedTarget& Target, int32 Ammo, int32 MagazineSize, bool bReloading,
		float HeadRadiusCm, const FCombatPerceptionSettings& P, float DecisionSeconds);
}
