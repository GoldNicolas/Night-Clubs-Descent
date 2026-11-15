#include "GJCombatFXSubsystem.h"
#include "Common/FPSAgentTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnemyBean.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GJCombatFX.h"
#include "GJGameSettings.h"
#include "Weapons/RifleComponent.h"

bool UGJCombatFXSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UGJCombatFXSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	EnemyShotHandle = AEnemyBean::OnAnyEnemyShot.AddUObject(this, &UGJCombatFXSubsystem::OnEnemyShot);
	EnemyDiedHandle = AEnemyBean::OnAnyEnemyDied.AddUObject(this, &UGJCombatFXSubsystem::OnEnemyDied);
}

void UGJCombatFXSubsystem::Deinitialize()
{
	AEnemyBean::OnAnyEnemyShot.Remove(EnemyShotHandle);
	AEnemyBean::OnAnyEnemyDied.Remove(EnemyDiedHandle);
	Super::Deinitialize();
}

void UGJCombatFXSubsystem::OnEnemyDied(AEnemyBean* Enemy)
{
	UWorld* World = GetWorld();
	if (!Enemy || Enemy->GetWorld() != World)
	{
		return;
	}
	// Once the death reaction and the ragdoll have put the body down, a pool spreads under it.
	FTimerHandle Timer;
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, WeakEnemy = TWeakObjectPtr<AEnemyBean>(Enemy)]()
	{
		if (const AEnemyBean* Body = WeakEnemy.Get())
		{
			const USkeletalMeshComponent* Skin = Body->GetMesh();
			GJCombatFX::PlayCorpseBlood(GetWorld(), Skin ? Skin->GetSocketLocation(TEXT("pelvis")) : Body->GetActorLocation(), Body);
		}
	}), 1.6f, false);
}

void UGJCombatFXSubsystem::OnEnemyShot(AEnemyBean* Shooter, const FShotResult& Shot)
{
	UWorld* World = GetWorld();
	if (!Shooter || Shooter->GetWorld() != World)
	{
		return;
	}
	const UGJGameSettings& S = UGJGameSettings::Get();
	FGJShotFX FX;
	FX.Gun = Shooter->GetGunComponent();
	FX.bEjectShell = true;
	FX.TracerStart = FX.Gun ? FX.Gun->GetComponentTransform().TransformPosition(S.GunMuzzle.GetLocation()) : Shot.Start;
	FX.TracerEnd = Shot.End;
	if (!Shot.bHitWorld)
	{
		// A hit on a pawn ends at its hitbox, often the player's own head: stop the streak a metre short of the face.
		FX.TracerEnd -= (Shot.End - FX.TracerStart).GetSafeNormal() * 100.f;
	}
	FX.bHitWorld = Shot.bHitWorld;
	FX.ImpactPoint = Shot.End;
	FX.ImpactNormal = (Shot.Start - Shot.End).GetSafeNormal();
	if (Shot.bHitWorld)
	{
		// The shot result carries no surface normal: a short re-trace at the impact gets it (presentation only, never gameplay).
		const FVector Dir = (Shot.End - Shot.Start).GetSafeNormal();
		FHitResult Surface;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GJImpactNormal), false, Shooter);
		if (World->LineTraceSingleByChannel(Surface, Shot.End - Dir * 10.f, Shot.End + Dir * 10.f, ECC_Weapon, Params))
		{
			FX.ImpactNormal = Surface.ImpactNormal;
		}
	}
	GJCombatFX::PlayShot(World, FX);
}
