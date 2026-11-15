#pragma once

#include "CoreMinimal.h"
#include "ShooterWeapon.h"
#include "Common/FPSAgentTypes.h"
#include "Weapons/RifleFireState.h"
#include "GJRifle.generated.h"

/**
 * The player's first weapon: a mirror of the enemy rifle (user decision 2026-11-14). Same numbers, same code paths:
 * - damage 160 / 40 / 34 by hit zone, 9.75 rounds/s automatic, 25 rounds, 2.5 s reload, infinite reserve;
 * - spread from FPS-Agent's CombatMath: 0.25 deg standing, 2.5-5 deg moving (running ruins aim), 8 deg airborne, x0.85 crouched, plus bloom;
 * - the same bullet recoil pattern (it moves the bullet, not the camera);
 * - cadence, magazine and reload from the vendored FRifleFireState.
 * Hitscan from the camera along the control rotation on the Weapon channel, so it hits enemy hitboxes exactly like an agent's shot
 * would hit the player. Looks and animations are the template rifle's.
 */
UCLASS()
class GAMEJAM_API AGJRifle : public AShooterWeapon
{
	GENERATED_BODY()

public:
	AGJRifle();

	void RequestReload();
	bool IsReloading() const { return State.IsReloading(); }
	float GetReloadSecondsLeft() const;
	int32 GetAmmo() const { return State.Ammo; }
	/** Game time of the last shot (music intensity). */
	double GetLastShotTime() const { return State.LastShotTime; }
	/** Cone half-angle the next shot would have right now (for the crosshair). */
	float GetCurrentSpreadDegrees() const { return SpreadFor(State.ShotIndex); }

	/** The last shot that hit an enemy hitbox (HUD hit marker). */
	double GetLastHitTime() const { return LastHitTime; }
	EHitZone GetLastHitZone() const { return LastHitZone; }
	bool WasLastHitKill() const { return bLastHitKill; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	/** Cadence comes from FRifleFireState in Tick; the template's timer-driven Fire is not used. */
	virtual void Fire() override;

	/** Player-side damage knob (1 = exact mirror). */
	UPROPERTY(EditAnywhere, Category="Mirror Rifle") float OutgoingDamageScale = 1.f;
	/** Cosmetic camera kick per shot (pitch input). 0 keeps the mirror exact: recoil already moves the bullet. */
	UPROPERTY(EditAnywhere, Category="Mirror Rifle") float CameraKickPitch = 0.f;

private:
	float SpreadFor(int32 ShotIndex) const;
	void FireShot(int32 ShotIndex);
	void PushHud();

	FRifleFireState State;
	FRandomStream Rng;
	TArray<int32> PendingShots;
	double LastHitTime = -1.0e9;
	EHitZone LastHitZone = EHitZone::None;
	bool bLastHitKill = false;
	int32 LastHudAmmo = -1;
};
