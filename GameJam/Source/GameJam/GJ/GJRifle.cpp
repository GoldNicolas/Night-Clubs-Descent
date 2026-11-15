#include "GJRifle.h"
#include "AgentTargetComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Character/BeanCharacter.h"
#include "Character/HealthComponent.h"
#include "Common/FPSAgentSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameJamCharacter.h"
#include "HAL/IConsoleManager.h"
#include "ShooterCharacter.h"
#include "ShooterWeaponHolder.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/CombatMath.h"

namespace
{
	TAutoConsoleVariable<int32> CVarPlayerTracers(TEXT("gj.Tracers"), 1,
		TEXT("1 = draw a short tracer for each player shot (placeholder for real tracer VFX; development builds only)."));
}

AGJRifle::AGJRifle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // fire after movement, like the bean rifle

	// The template rifle's look (BP_ShooterWeapon_Rifle values).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RifleMesh(TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ShootMontage(TEXT("/Game/Variant_Shooter/Anims/FP_Rifle_Shoot_Montage.FP_Rifle_Shoot_Montage"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> FirstPersonAnim(TEXT("/Game/Variant_Shooter/Anims/ABP_FP_Weapon"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> ThirdPersonAnim(TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Rifle"));
	if (RifleMesh.Succeeded())
	{
		GetFirstPersonMesh()->SetSkeletalMeshAsset(RifleMesh.Object);
		GetThirdPersonMesh()->SetSkeletalMeshAsset(RifleMesh.Object);
	}
	if (ShootMontage.Succeeded()) { FiringMontage = ShootMontage.Object; }
	if (FirstPersonAnim.Succeeded()) { FirstPersonAnimInstanceClass = FirstPersonAnim.Class; }
	if (ThirdPersonAnim.Succeeded()) { ThirdPersonAnimInstanceClass = ThirdPersonAnim.Class; }
	MuzzleSocketName = TEXT("Muzzle");

	const FRifleStats& Stats = UFPSAgentSettings::Get().Rifle;
	MagazineSize = Stats.MagazineSize;
	RefireRate = 1.f / Stats.FireRate;
	bFullAuto = false;   // cadence is FRifleFireState's, not the template timer's
	FiringRecoil = 0.f;
	AimVariance = 0.f;
	Rng.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
}

void AGJRifle::BeginPlay()
{
	const FRifleStats& Stats = UFPSAgentSettings::Get().Rifle;
	MagazineSize = Stats.MagazineSize;
	State.Reset(Stats);
	Super::BeginPlay(); // fills CurrentBullets from MagazineSize and attaches the meshes
	PushHud();
}

void AGJRifle::Fire()
{
	// Intentionally empty: shots come from FRifleFireState in Tick while the trigger (bIsFiring) is held.
}

void AGJRifle::RequestReload()
{
	State.StartReload(UFPSAgentSettings::Get().Rifle, GetWorld()->GetTimeSeconds());
	PushHud();
}

float AGJRifle::GetReloadSecondsLeft() const
{
	return State.IsReloading() ? FMath::Max(0.f, static_cast<float>(State.ReloadEndTime - GetWorld()->GetTimeSeconds())) : 0.f;
}

float AGJRifle::SpreadFor(int32 ShotIndex) const
{
	const UFPSAgentSettings& S = UFPSAgentSettings::Get();
	const ACharacter* C = Cast<ACharacter>(PawnOwner);
	const float Speed = C ? C->GetVelocity().Size2D() : 0.f;
	const bool bAirborne = C && C->GetCharacterMovement() && C->GetCharacterMovement()->IsFalling();
	const bool bCrouched = C && C->bIsCrouched;
	return FPSCombat::SpreadDegrees(S.Rifle, S.Movement, Speed, bAirborne, bCrouched, ShotIndex);
}

void AGJRifle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	if (!World) { return; }
	const AShooterCharacter* Holder = Cast<AShooterCharacter>(PawnOwner);
	const bool bTrigger = bIsFiring && !IsHidden() && Holder && !Holder->IsDead();
	State.Update(UFPSAgentSettings::Get().Rifle, World->GetTimeSeconds(), bTrigger, PendingShots);
	for (const int32 ShotIndex : PendingShots)
	{
		FireShot(ShotIndex);
	}
	PushHud();
}

void AGJRifle::FireShot(int32 ShotIndex)
{
	APawn* Shooter = PawnOwner;
	UWorld* World = GetWorld();
	if (!Shooter || !World) { return; }
	const FRifleStats& Stats = UFPSAgentSettings::Get().Rifle;

	// From what the player sees: the camera, along the control rotation, plus the bean rifle's recoil and spread.
	const AGameJamCharacter* FirstPerson = Cast<AGameJamCharacter>(Shooter);
	const FVector Eye = (FirstPerson && FirstPerson->GetFirstPersonCameraComponent())
		? FirstPerson->GetFirstPersonCameraComponent()->GetComponentLocation() : Shooter->GetPawnViewLocation();
	FRotator View = Shooter->GetControlRotation();
	const FVector2D Recoil = FPSCombat::RecoilOffsetDegrees(Stats, ShotIndex);
	View.Pitch += Recoil.Y;
	View.Yaw += Recoil.X;
	const FVector Direction = FPSCombat::ApplySpreadCone(View.Vector(), SpreadFor(ShotIndex), Rng);
	FVector End = Eye + Direction * Stats.MaxRangeCm;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GJRifleShot), /*bTraceComplex=*/false, Shooter);
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	bool bHitEnemy = false;
	if (World->LineTraceSingleByChannel(Hit, Eye, End, ECC_Weapon, Params))
	{
		End = Hit.ImpactPoint;
		if (ABeanCharacter* Victim = Cast<ABeanCharacter>(Hit.GetActor()))
		{
			const EHitZone Zone = Victim->GetHitZone(Hit.GetComponent());
			if (Zone != EHitZone::None && !Victim->GetHealth()->IsDead())
			{
				const float Damage = FPSCombat::DamageForZone(Stats, Zone) * OutgoingDamageScale;
				// The player pawn is the instigator: the agent's hit indicator points at it (contract §7.3).
				bLastHitKill = Victim->GetHealth()->ApplyHit(Damage, Shooter).bKilled;
				LastHitZone = Zone;
				LastHitTime = World->GetTimeSeconds();
				bHitEnemy = true;
			}
		}
	}

	// Every bullet is a gunshot event for the agents (fed to S2+ snapshots only).
	if (UAgentTargetComponent* Target = Shooter->FindComponentByClass<UAgentTargetComponent>())
	{
		Target->ReportGunshot();
	}

	if (WeaponOwner)
	{
		WeaponOwner->PlayFiringMontage(FiringMontage);
		if (CameraKickPitch != 0.f) { WeaponOwner->AddWeaponRecoil(CameraKickPitch); }
	}
	MakeNoise(ShotLoudness, Shooter, Shooter->GetActorLocation(), ShotNoiseRange, ShotNoiseTag);

#if ENABLE_DRAW_DEBUG
	if (CVarPlayerTracers.GetValueOnGameThread() != 0)
	{
		const FRotationMatrix Axes(View);
		const FVector Muzzle = Eye + Axes.GetUnitAxis(EAxis::X) * 40.0 + Axes.GetUnitAxis(EAxis::Y) * 12.0 - Axes.GetUnitAxis(EAxis::Z) * 10.0;
		DrawDebugLine(World, Muzzle, End, bHitEnemy ? FColor(255, 60, 40) : FColor(140, 220, 255), false, 0.05f, 0, 0.5f);
	}
#endif
}

void AGJRifle::PushHud()
{
	if (State.Ammo == LastHudAmmo) { return; }
	LastHudAmmo = State.Ammo;
	CurrentBullets = State.Ammo;
	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(State.Ammo, MagazineSize);
	}
}
