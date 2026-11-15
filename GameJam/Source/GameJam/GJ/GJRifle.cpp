#include "GJRifle.h"
#include "AgentTargetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GJCombatFX.h"
#include "GJCrowdSubsystem.h"
#include "GJGameSettings.h"
#include "GJPlayerCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
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
	ApplyGunModel();
	PushHud();
}

void AGJRifle::ApplyGunModel()
{
	// GameJam settings: PlayerWeapon. Looks only: the stats are the mirror rifle's whatever the model.
	const UGJGameSettings& S = UGJGameSettings::Get();
	UObject* Gun = S.PlayerWeapon.IsValid() ? S.PlayerWeapon.TryLoad() : nullptr;
	if (USkeletalMesh* SkeletalGun = Cast<USkeletalMesh>(Gun))
	{
		for (USkeletalMeshComponent* Part : { GetFirstPersonMesh(), GetThirdPersonMesh() })
		{
			Part->SetSkeletalMeshAsset(SkeletalGun);
			Part->SetRelativeTransform(S.PlayerWeaponOffset);
		}
		// The gun's own animations, only when they are made for this gun's skeleton.
		auto LoadGunAnim = [SkeletalGun](const FSoftObjectPath& Path) -> UAnimSequence*
		{
			UAnimSequence* Anim = Path.IsValid() ? Cast<UAnimSequence>(Path.TryLoad()) : nullptr;
			return (Anim && Anim->GetSkeleton() == SkeletalGun->GetSkeleton()) ? Anim : nullptr;
		};
		GunFireAnim = LoadGunAnim(S.PlayerWeaponFireAnim);
		GunReloadAnim = LoadGunAnim(S.PlayerWeaponReloadAnim);
	}
	else if (UStaticMesh* StaticGun = Cast<UStaticMesh>(Gun))
	{
		// A static model rides on the template rifle's components (they stay the hand-socket parents), which are hidden.
		auto Add = [&](USkeletalMeshComponent* Parent, bool bFirstPerson)
		{
			UStaticMeshComponent* Model = NewObject<UStaticMeshComponent>(this);
			Model->SetStaticMesh(StaticGun);
			Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Model->SetGenerateOverlapEvents(false);
			Model->SetupAttachment(Parent);
			Model->SetRelativeTransform(S.PlayerWeaponOffset);
			Model->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::WorldSpaceRepresentation);
			Model->SetOnlyOwnerSee(bFirstPerson);
			Model->SetOwnerNoSee(!bFirstPerson);
			Model->RegisterComponent();
			Parent->SetVisibility(false, /*bPropagateToChildren=*/false);
			return Model;
		};
		Add(GetFirstPersonMesh(), true);
		Add(GetThirdPersonMesh(), false);
	}
}

void AGJRifle::PlayGunAnim(UAnimSequence* Anim, float Seconds)
{
	if (!Anim)
	{
		return;
	}
	for (USkeletalMeshComponent* Part : { GetFirstPersonMesh(), GetThirdPersonMesh() })
	{
		Part->PlayAnimation(Anim, /*bLooping=*/false);
		Part->SetPlayRate(Anim->GetPlayLength() / FMath::Max(Seconds, 0.01f)); // fit the clip to the shot interval / reload time
	}
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
	const FRifleStats& Stats = UFPSAgentSettings::Get().Rifle;
	State.Update(Stats, World->GetTimeSeconds(), bTrigger, PendingShots);
	for (const int32 ShotIndex : PendingShots)
	{
		FireShot(ShotIndex);
	}
	const bool bReloading = State.IsReloading();
	if (bReloading && !bWasReloading)
	{
		PlayGunAnim(GunReloadAnim, Stats.ReloadTime); // the magazine change spans the real 2.5 s reload
		PlayArmsReload(Stats.ReloadTime);
	}
	bWasReloading = bReloading;
	PushHud();
}

void AGJRifle::PlayArmsReload(float Seconds)
{
	// The first-person arms reload, through the arms slot of the template's first-person AnimBP, stretched to the reload time.
	const UGJGameSettings& S = UGJGameSettings::Get();
	UAnimSequence* Arms = S.PlayerReloadAnim.IsValid() ? Cast<UAnimSequence>(S.PlayerReloadAnim.TryLoad()) : nullptr;
	const AGameJamCharacter* Holder = Cast<AGameJamCharacter>(PawnOwner);
	USkeletalMeshComponent* ArmsMesh = Holder ? Holder->GetFirstPersonMesh() : nullptr;
	UAnimInstance* Anim = ArmsMesh ? ArmsMesh->GetAnimInstance() : nullptr;
	if (Arms && Anim)
	{
		Anim->PlaySlotAnimationAsDynamicMontage(Arms, S.PlayerArmsSlot, 0.15f, 0.25f, Arms->GetPlayLength() / FMath::Max(Seconds, 0.1f));
	}
}

FVector2D AGJRifle::GetViewRecoilDegrees() const
{
	// The recoil of the last bullet, recovering while idle exactly as the spray index does (FPSCombat::RecoverShotIndex).
	const UWorld* World = GetWorld();
	if (!World || State.ShotIndex <= 0)
	{
		return FVector2D::ZeroVector;
	}
	const FRifleStats& Stats = UFPSAgentSettings::Get().Rifle;
	const double Idle = World->GetTimeSeconds() - State.LastShotTime - 1.0 / Stats.FireRate;
	const int32 Index = Idle > 0.0 ? FPSCombat::RecoverShotIndex(State.ShotIndex, Idle, Stats.RecoveryTimePerShot) : State.ShotIndex;
	return Index > 0 ? FPSCombat::RecoilOffsetDegrees(Stats, Index - 1) : FVector2D::ZeroVector;
}

void AGJRifle::FireShot(int32 ShotIndex)
{
	APawn* Shooter = PawnOwner;
	UWorld* World = GetWorld();
	if (!Shooter || !World) { return; }
	const FRifleStats& Stats = UFPSAgentSettings::Get().Rifle;

	// From what the player sees: the camera, along the control rotation, plus the bean rifle's recoil and spread.
	const AGameJamCharacter* FirstPerson = Cast<AGameJamCharacter>(Shooter);
	FVector Eye = (FirstPerson && FirstPerson->GetFirstPersonCameraComponent())
		? FirstPerson->GetFirstPersonCameraComponent()->GetComponentLocation() : Shooter->GetPawnViewLocation();
	FRotator View = Shooter->GetControlRotation();
	AGJPlayerCharacter* Player = Cast<AGJPlayerCharacter>(Shooter);
	if (Player)
	{
		Eye -= Player->GetArmsWorldShift(); // the camera rides on the arms mesh: shoot from the true eye, not the gun's bob or framing
		View -= Player->GetAimPunch();      // the view punch already shows the recoil: aim from the player's own aim, then add it once
	}
	const FVector2D Recoil = FPSCombat::RecoilOffsetDegrees(Stats, ShotIndex);
	View.Pitch += Recoil.Y;
	View.Yaw += Recoil.X;
	const FVector Direction = FPSCombat::ApplySpreadCone(View.Vector(), SpreadFor(ShotIndex), Rng);
	FVector End = Eye + Direction * Stats.MaxRangeCm;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GJRifleShot), /*bTraceComplex=*/false, Shooter);
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	bool bHitEnemy = false;
	const bool bHitAnything = World->LineTraceSingleByChannel(Hit, Eye, End, ECC_Weapon, Params);
	if (bHitAnything)
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

	// Every bullet is a gunshot event for the agents (fed to S2+ snapshots only)...
	if (UAgentTargetComponent* Target = Shooter->FindComponentByClass<UAgentTargetComponent>())
	{
		Target->ReportGunshot();
	}
	// ...and, separately, for the crowd (presentation: the agents never hear this one).
	UGJCrowdSubsystem::ReportGunshot(this, Eye);

	PlayGunAnim(GunFireAnim, 1.f / Stats.FireRate); // one bolt cycle per shot
	if (WeaponOwner)
	{
		WeaponOwner->PlayFiringMontage(FiringMontage);
		if (CameraKickPitch != 0.f) { WeaponOwner->AddWeaponRecoil(CameraKickPitch); }
	}
	MakeNoise(ShotLoudness, Shooter, Shooter->GetActorLocation(), ShotNoiseRange, ShotNoiseTag);
	if (Player)
	{
		Player->OnRifleShot(); // gun kick-back and view kick
	}

	// Effects: muzzle flash and shell on the first-person gun, a tracer from just below the view toward the hit, impact on the level.
	FGJShotFX FX;
	FX.Gun = GetFirstPersonMesh();
	FX.bFirstPerson = true;
	FX.bEjectShell = true;
	FX.bSound2D = true;
	FX.SoundVolume = 0.7f;
	const FRotationMatrix Axes(Shooter->GetControlRotation());
	FX.TracerStart = Eye + Axes.GetUnitAxis(EAxis::X) * 90.0 + Axes.GetUnitAxis(EAxis::Y) * 14.0 - Axes.GetUnitAxis(EAxis::Z) * 12.0;
	FX.TracerEnd = End;
	FX.TracerSkipCm = 150.f;
	FX.bHitWorld = bHitAnything && !bHitEnemy && !Cast<ABeanCharacter>(Hit.GetActor());
	FX.bHitCharacter = bHitEnemy;
	FX.HitActor = bHitEnemy ? Hit.GetActor() : nullptr;
	FX.ImpactPoint = Hit.ImpactPoint;
	FX.ImpactNormal = Hit.ImpactNormal;
	FX.ShotDirection = Direction;
	GJCombatFX::PlayShot(World, FX);
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
