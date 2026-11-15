#include "Weapons/RifleFireState.h"
#include "Weapons/CombatMath.h"

void FRifleFireState::Reset(const FRifleStats& Stats)
{
	*this = FRifleFireState();
	Ammo = Stats.MagazineSize;
}

bool FRifleFireState::StartReload(const FRifleStats& Stats, double Now)
{
	if (IsReloading() || Ammo >= Stats.MagazineSize)
	{
		return false;
	}
	ReloadEndTime = Now + Stats.ReloadTime;
	return true;
}

int32 FRifleFireState::Update(const FRifleStats& Stats, double Now, bool bTriggerHeld, TArray<int32>& OutShotIndices)
{
	OutShotIndices.Reset();
	if (IsReloading() && Now >= ReloadEndTime)
	{
		Ammo = Stats.MagazineSize;
		ReloadEndTime = -1.0;
		ShotIndex = 0;
	}
	if (!bTriggerHeld || IsReloading())
	{
		bWasTriggerHeld = false; // treat the next held frame as a fresh press (no catch-up burst)
		return 0;
	}
	if (Ammo <= 0)
	{
		StartReload(Stats, Now);
		bWasTriggerHeld = false;
		return 0;
	}
	const double Interval = 1.0 / FMath::Max(0.01f, Stats.FireRate);
	if (!bWasTriggerHeld)
	{
		NextShotTime = FMath::Max(NextShotTime, Now);
	}
	bWasTriggerHeld = true;
	while (Ammo > 0 && Now >= NextShotTime)
	{
		const double Idle = (NextShotTime - LastShotTime) - Interval;
		ShotIndex = FPSCombat::RecoverShotIndex(ShotIndex, Idle, Stats.RecoveryTimePerShot);
		OutShotIndices.Add(ShotIndex);
		++ShotIndex;
		--Ammo;
		LastShotTime = NextShotTime;
		NextShotTime += Interval;
	}
	if (Ammo <= 0)
	{
		StartReload(Stats, Now);
	}
	return OutShotIndices.Num();
}
