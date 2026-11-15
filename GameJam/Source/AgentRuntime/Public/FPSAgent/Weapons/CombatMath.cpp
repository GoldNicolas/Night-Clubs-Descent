#include "Weapons/CombatMath.h"

namespace FPSCombat
{
float DamageForZone(const FRifleStats& Rifle, EHitZone Zone)
{
	switch (Zone)
	{
	case EHitZone::Head: return Rifle.DamageHead;
	case EHitZone::Body: return Rifle.DamageBody;
	case EHitZone::Legs: return Rifle.DamageLegs;
	default:             return 0.f;
	}
}

FDamageResult ApplyDamage(float Health, float Armor, float Damage, float ArmorAbsorbFraction)
{
	FDamageResult R;
	R.Health = Health;
	R.Armor = Armor;
	if (Health <= 0.f || Damage <= 0.f)
	{
		return R;
	}
	const float Absorb = FMath::Clamp(ArmorAbsorbFraction, 0.f, 1.f);
	R.DamageToArmor = FMath::Min(FMath::Max(Armor, 0.f), Damage * Absorb);
	R.DamageToHealth = FMath::Min(Damage - R.DamageToArmor, Health); // only what was actually removed (spec §3)
	R.Armor = Armor - R.DamageToArmor;
	R.Health = FMath::Max(0.f, Health - R.DamageToHealth);
	R.bKilled = R.Health <= 0.f;
	return R;
}

float SpreadDegrees(const FRifleStats& Rifle, const FMovementTuning& Move, float HorizontalSpeed, bool bAirborne, bool bCrouched, int32 ShotIndex)
{
	const float Threshold = Move.AccurateSpeedFraction * Move.RunSpeed;
	float Spread;
	if (bAirborne)
	{
		Spread = Rifle.AirSpreadDeg;
	}
	else if (HorizontalSpeed <= Threshold)
	{
		Spread = Rifle.FirstShotSpreadDeg;
	}
	else
	{
		const float Alpha = FMath::Clamp((HorizontalSpeed - Threshold) / FMath::Max(1.f, Move.RunSpeed - Threshold), 0.f, 1.f);
		Spread = FMath::Lerp(Rifle.WalkSpreadDeg, Rifle.RunSpreadDeg, Alpha);
	}
	if (bCrouched && !bAirborne)
	{
		Spread *= Rifle.CrouchSpreadMultiplier;
	}
	if (ShotIndex >= Rifle.AccurateShots)
	{
		Spread += FMath::Min((ShotIndex - Rifle.AccurateShots + 1) * Rifle.SprayBloomPerShotDeg, Rifle.SprayBloomMaxDeg);
	}
	return Spread;
}

FVector2D RecoilOffsetDegrees(const FRifleStats& Rifle, int32 ShotIndex)
{
	if (ShotIndex <= 0)
	{
		return FVector2D::ZeroVector;
	}
	const int32 Accurate = FMath::Max(1, Rifle.AccurateShots);
	if (ShotIndex < Accurate)
	{
		return FVector2D(0.f, ShotIndex * Rifle.RecoilKickFirstShotsDeg);
	}
	const float Base = (Accurate - 1) * Rifle.RecoilKickFirstShotsDeg;
	const float Uncapped = Base + (ShotIndex - Accurate + 1) * Rifle.RecoilClimbPerShotDeg;
	const float Pitch = FMath::Min(Uncapped, Rifle.RecoilClimbMaxDeg);
	float Yaw = 0.f;
	if (Rifle.RecoilClimbPerShotDeg > 0.f && Rifle.RecoilSwayPeriodShots > 0)
	{
		const int32 CapIndex = Accurate - 1 + FMath::CeilToInt((Rifle.RecoilClimbMaxDeg - Base) / Rifle.RecoilClimbPerShotDeg);
		if (ShotIndex > CapIndex)
		{
			Yaw = Rifle.RecoilSwayAmplitudeDeg * FMath::Sin(2.f * UE_PI * (ShotIndex - CapIndex) / Rifle.RecoilSwayPeriodShots);
		}
	}
	return FVector2D(Yaw, Pitch);
}

int32 RecoverShotIndex(int32 ShotIndex, double IdleSeconds, float RecoveryTimePerShot)
{
	if (IdleSeconds <= 0.0)
	{
		return ShotIndex;
	}
	// Full recovery first: also avoids int overflow for huge idle times (e.g. the very first shot).
	if (RecoveryTimePerShot <= 0.f || IdleSeconds >= static_cast<double>(ShotIndex) * RecoveryTimePerShot)
	{
		return 0;
	}
	const int32 Recovered = FMath::FloorToInt32(IdleSeconds / RecoveryTimePerShot);
	return FMath::Max(0, ShotIndex - Recovered);
}

FVector ApplySpreadCone(const FVector& Direction, float SpreadDegrees, const FRandomStream& Rng)
{
	const FVector Dir = Direction.GetSafeNormal();
	if (SpreadDegrees <= 0.f)
	{
		return Dir;
	}
	return Rng.VRandCone(Dir, FMath::DegreesToRadians(SpreadDegrees));
}
} // namespace FPSCombat
