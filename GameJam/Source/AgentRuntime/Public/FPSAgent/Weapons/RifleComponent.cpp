#include "Weapons/RifleComponent.h"
#include "Character/BeanCharacter.h"
#include "Character/HealthComponent.h"
#include "Common/FPSAgentSettings.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Weapons/CombatMath.h"
#include "AgentTargetComponent.h" // GameJam (vendored change): target adapter

static TAutoConsoleVariable<int32> CVarShowShots(TEXT("fps.ShowShots"), 0,
	TEXT("1 = draw a short tracer for every rifle shot (red = hit a bean, yellow = miss). Viewing only: nothing reads it, and nothing draws under -nullrhi."), ECVF_Default);

URifleComponent::URifleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics; // fire after movement so the eye point is current
	Rng.Initialize(0);
}

void URifleComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetWeapon();
}

void URifleComponent::ResetWeapon()
{
	State.Reset(UFPSAgentSettings::Get().Rifle);
	bTriggerHeld = false;
}

void URifleComponent::RequestReload()
{
	if (const UWorld* World = GetWorld())
	{
		State.StartReload(UFPSAgentSettings::Get().Rifle, World->GetTimeSeconds());
	}
}

void URifleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	State.Update(UFPSAgentSettings::Get().Rifle, GetWorld()->GetTimeSeconds(), bTriggerHeld, PendingShots);
	for (const int32 Index : PendingShots)
	{
		FireShot(Index);
	}
}

float URifleComponent::SpreadFor(int32 ShotIndex) const
{
	const UFPSAgentSettings& S = UFPSAgentSettings::Get();
	const ABeanCharacter* Bean = Cast<ABeanCharacter>(GetOwner());
	const float Speed = Bean ? Bean->GetHorizontalSpeed() : 0.f;
	const bool bAirborne = Bean && Bean->IsAirborne();
	const bool bCrouched = Bean && Bean->bIsCrouched;
	return FPSCombat::SpreadDegrees(S.Rifle, S.Movement, Speed, bAirborne, bCrouched, ShotIndex);
}

FShotResult URifleComponent::FireShot(int32 ShotIndex)
{
	FShotResult Result;
	Result.ShotIndex = ShotIndex;
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return Result;
	}

	const FRifleStats& Stats = UFPSAgentSettings::Get().Rifle;
	FVector EyeLocation;
	FRotator EyeRotation;
	OwnerActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	const FVector2D Recoil = FPSCombat::RecoilOffsetDegrees(Stats, ShotIndex);
	EyeRotation.Pitch += Recoil.Y;
	EyeRotation.Yaw += Recoil.X;
	Result.SpreadDegrees = SpreadFor(ShotIndex);
	const FVector Direction = FPSCombat::ApplySpreadCone(EyeRotation.Vector(), Result.SpreadDegrees, Rng);
	Result.Start = EyeLocation;
	Result.End = EyeLocation + Direction * Stats.MaxRangeCm;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RifleShot), /*bTraceComplex=*/false, OwnerActor);
	for (const TWeakObjectPtr<AActor>& Ally : IgnoredAllies) // GameJam (vendored change, contract §8): shots pass through allies
	{
		if (AActor* A = Ally.Get()) { Params.AddIgnoredActor(A); }
	}
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Result.Start, Result.End, ECC_Weapon, Params))
	{
		Result.End = Hit.ImpactPoint;
		Result.HitActor = Hit.GetActor();
		ABeanCharacter* Victim = Cast<ABeanCharacter>(Hit.GetActor());
		// GameJam (vendored change): target adapter. A non-bean target (the player) resolves its zone and takes the same zone damage
		// through its UAgentTargetComponent; zones and damage values are unchanged.
		UAgentTargetComponent* Target = (!Victim && Hit.GetActor()) ? Hit.GetActor()->FindComponentByClass<UAgentTargetComponent>() : nullptr;
		Result.Zone = Victim ? Victim->GetHitZone(Hit.GetComponent()) : (Target ? Target->GetHitZone(Hit.GetComponent()) : EHitZone::None);
		if (Result.Zone != EHitZone::None)
		{
			Result.Damage = FPSCombat::DamageForZone(Stats, Result.Zone);
			Result.bKill = Victim ? Victim->GetHealth()->ApplyHit(Result.Damage, OwnerActor).bKilled
				: Target->ApplyAgentHit(Result.Damage, Result.Zone, OwnerActor);
		}
		else
		{
			Result.bHitWorld = true;
		}
	}
#if ENABLE_DRAW_DEBUG
	if (CVarShowShots.GetValueOnGameThread() != 0)
	{
		// Start a little right of and below the eye, like a muzzle, so a first-person view sees the tracer instead of a dot.
		const FRotationMatrix Axes(EyeRotation);
		const FVector Muzzle = Result.Start + Axes.GetUnitAxis(EAxis::X) * 40.0 + Axes.GetUnitAxis(EAxis::Y) * 12.0 - Axes.GetUnitAxis(EAxis::Z) * 10.0;
		const bool bHitBean = Result.Zone != EHitZone::None;
		DrawDebugLine(World, Muzzle, Result.End, bHitBean ? FColor::Red : FColor::Yellow, false, 0.25f, 0, 0.3f); // thickness is in cm: thin near the eye
		if (bHitBean)
		{
			DrawDebugPoint(World, Result.End, 10.f, Result.Zone == EHitZone::Head ? FColor::Red : FColor::Orange, false, 0.4f);
		}
	}
#endif
	OnShotFired.Broadcast(Result);
	return Result;
}
