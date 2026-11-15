#include "Learning/CombatObservation.h"

namespace
{
	/** Observation normalizers that aren't mechanics: spread in units of 10 deg, pitch in units of 90 deg, time since shot capped at 1 s. */
	constexpr float SpreadScaleDeg = 10.f;
	constexpr float PitchScaleDeg = 90.f;
	constexpr float AngleScaleDeg = 180.f;
	constexpr float MinFireToleranceDeg = 0.25f;

	float Safe(float Num, float Den) { return Den > 0.f ? Num / Den : 0.f; }
}

namespace FPSCombatObs
{
	TArray<float> PackSelf(const FSelfState& S, const FCombatPerceptionSettings& P)
	{
		const FVector V = S.VelocityView / FMath::Max(1.f, P.VelocityScaleCmPerS);
		return {
			static_cast<float>(V.X), static_cast<float>(V.Y), static_cast<float>(V.Z),
			S.bGrounded ? 1.f : 0.f, S.bCrouched ? 1.f : 0.f, S.bWalking ? 1.f : 0.f,
			Safe(S.Health, S.MaxHealth), Safe(S.Armor, S.MaxArmor), Safe(static_cast<float>(S.Ammo), static_cast<float>(S.MagazineSize)),
			S.bReloading ? 1.f : 0.f,
			S.SpreadDeg / SpreadScaleDeg,
			Safe(static_cast<float>(S.SprayShots), static_cast<float>(S.MagazineSize)),
			FMath::Clamp(S.SecondsSinceShot, 0.f, 1.f),
			S.AimPitchDeg / PitchScaleDeg,
			FMath::Clamp(S.TimeLeftFraction, 0.f, 1.f) };
	}

	TArray<float> PackEnemy(const FEnemyState& E, const FCombatPerceptionSettings& P)
	{
		const float Pos = FMath::Max(1.f, P.PositionScaleCm);
		const FVector H = E.HeadView / Pos;
		const FVector V = E.VelocityView / FMath::Max(1.f, P.VelocityScaleCmPerS);
		return {
			static_cast<float>(H.X), static_cast<float>(H.Y), static_cast<float>(H.Z),
			static_cast<float>(E.HeadView.Size() / Pos),
			static_cast<float>(E.AimErrorDeg.X / AngleScaleDeg), static_cast<float>(E.AimErrorDeg.Y / AngleScaleDeg),
			static_cast<float>(V.X), static_cast<float>(V.Y), static_cast<float>(V.Z),
			E.bFacingAgent ? 1.f : 0.f, E.bCrouched ? 1.f : 0.f, E.bVisible ? 1.f : 0.f,
			FMath::Clamp(Safe(E.SecondsSinceSeen, P.MemorySeconds), 0.f, 1.f),
			FMath::Clamp(Safe(static_cast<float>(E.AimErrorDeg.X), P.FineAimScaleDeg), -1.f, 1.f),
			FMath::Clamp(Safe(static_cast<float>(E.AimErrorDeg.Y), P.FineAimScaleDeg), -1.f, 1.f) };
	}

	TArray<float> PackHit(const FVector& DirectionView, float SecondsAgo, const FCombatPerceptionSettings& P)
	{
		const FVector D = DirectionView.GetSafeNormal();
		return { static_cast<float>(D.X), static_cast<float>(D.Y), static_cast<float>(D.Z),
			FMath::Clamp(Safe(SecondsAgo, P.HitIndicatorSeconds), 0.f, 1.f) };
	}

	TArray<float> PackSound(const FVector& DirectionView, float DistanceCm, float AgeSeconds, const FCombatPerceptionSettings& P, float SoundMemorySeconds)
	{
		const FVector D = DirectionView.GetSafeNormal();
		return { static_cast<float>(D.X), static_cast<float>(D.Y), static_cast<float>(D.Z),
			FMath::Clamp(Safe(DistanceCm, FMath::Max(1.f, P.PositionScaleCm)), 0.f, 1.f),
			FMath::Clamp(Safe(AgeSeconds, SoundMemorySeconds), 0.f, 1.f) };
	}

	FObservationData FObservationData::Zero(int32 RayCount)
	{
		FObservationData D;
		D.Self.Init(0.f, SelfSize);
		D.Rays.Init(0.f, FMath::Max(0, RayCount));
		return D;
	}

	FAgentAction Sanitize(const FAgentAction& A)
	{
		// GetClampedToMaxSize passes NaN through (its size comparison is false), so zero non-finite components first.
		auto Finite = [](double V) { return FMath::IsFinite(V) ? V : 0.0; };
		FAgentAction C = A;
		C.Move = FVector2D(Finite(A.Move.X), Finite(A.Move.Y));
		C.Aim = FVector2D(Finite(A.Aim.X), Finite(A.Aim.Y));
		C.Aim = FVector2D(FMath::Clamp(C.Aim.X, -1.0, 1.0), FMath::Clamp(C.Aim.Y, -1.0, 1.0));
		C.Move = C.Move.GetClampedToMaxSize(1.0);
		return C;
	}

	FAgentAction ApplyAbilities(const FAgentAction& A, const FCombatAbilitySettings& Abilities)
	{
		FAgentAction C = A;
		C.bCrouch = A.bCrouch && Abilities.bCrouch;
		C.bJump = A.bJump && Abilities.bJump;
		return C;
	}

	FVector2D AimPerTickDeg(const FVector2D& Aim, const FCombatPerceptionSettings& P, float TickSeconds)
	{
		return Aim * (P.MaxTurnRateDegPerSec * TickSeconds);
	}

	FVector MoveWorld(const FVector2D& Move, float ViewYawDeg)
	{
		const FRotator Yaw(0.0, ViewYawDeg, 0.0);
		return Yaw.RotateVector(FVector(Move.X, Move.Y, 0.0));
	}

	FAgentAction ScriptedAction(const FScriptedTarget& Target, int32 Ammo, int32 MagazineSize, bool bReloading,
		float HeadRadiusCm, const FCombatPerceptionSettings& P, float DecisionSeconds)
	{
		FAgentAction A;
		const float MaxPerDecision = FMath::Max(1e-3f, P.MaxTurnRateDegPerSec * DecisionSeconds);
		if (!Target.bHasTarget)
		{
			A.Aim = FVector2D(1.0, 0.0);
			A.bReload = !bReloading && Ammo < MagazineSize / 2;
			return A;
		}
		A.Aim = FVector2D(FMath::Clamp(Target.AimErrorDeg.X / MaxPerDecision, -1.0, 1.0), FMath::Clamp(Target.AimErrorDeg.Y / MaxPerDecision, -1.0, 1.0));
		const float ToleranceDeg = FMath::Max(MinFireToleranceDeg, FMath::RadiansToDegrees(FMath::Atan2(HeadRadiusCm, FMath::Max(1.f, Target.DistanceCm))));
		const bool bOnTarget = FMath::Abs(Target.AimErrorDeg.X) <= ToleranceDeg && FMath::Abs(Target.AimErrorDeg.Y) <= ToleranceDeg;
		A.bReload = !bReloading && Ammo <= 0;
		A.bFire = bOnTarget && !bReloading && Ammo > 0;
		return A;
	}
}
