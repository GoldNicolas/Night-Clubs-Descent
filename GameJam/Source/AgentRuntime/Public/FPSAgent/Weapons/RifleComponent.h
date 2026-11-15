#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Common/FPSAgentTypes.h"
#include "Weapons/RifleFireState.h"
#include "RifleComponent.generated.h"

/** One fired bullet: for HUD hit markers, metrics and (later) RL rewards. */
struct FShotResult
{
	int32 ShotIndex = 0;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float SpreadDegrees = 0.f;
	bool bHitWorld = false;
	EHitZone Zone = EHitZone::None;
	float Damage = 0.f;
	bool bKill = false;
	TWeakObjectPtr<AActor> HitActor;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnShotFired, const FShotResult&);

/** Hitscan automatic rifle. Stats: UFPSAgentSettings::Rifle. Fires along the owner's eyes view point. */
UCLASS(ClassGroup=(FPSAgent), meta=(BlueprintSpawnableComponent))
class AGENTRUNTIME_API URifleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URifleComponent();

	void SetTriggerHeld(bool bHeld) { bTriggerHeld = bHeld; }
	bool IsTriggerHeld() const { return bTriggerHeld; }
	void RequestReload();
	void ResetWeapon();
	void SeedRandom(int32 Seed) { Rng.Initialize(Seed); }
	/** GameJam (vendored change, contract §8): allied agents this rifle's trace passes through (no friendly fire). */
	void SetIgnoredAllies(const TArray<TWeakObjectPtr<AActor>>& Allies) { IgnoredAllies = Allies; }

	/** Traces one bullet using ShotIndex for recoil and bloom. Bypasses cadence/ammo (used by Tick and tests). */
	FShotResult FireShot(int32 ShotIndex);
	/** Spread the next shot would have right now. */
	float GetCurrentSpreadDegrees() const { return SpreadFor(State.ShotIndex); }
	const FRifleFireState& GetFireState() const { return State; }

	FOnShotFired OnShotFired;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	float SpreadFor(int32 ShotIndex) const;

	FRifleFireState State;
	FRandomStream Rng;
	bool bTriggerHeld = false;
	TArray<int32> PendingShots;
	TArray<TWeakObjectPtr<AActor>> IgnoredAllies;
};
