#include "EnemyBean.h"
#include "AgentDirector.h"
#include "AgentRuntime.h"
#include "AgentRuntimeSettings.h"
#include "Common/FPSAgentSettings.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Character/BeanAIController.h"
#include "Character/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/RifleComponent.h"

namespace
{
	TAutoConsoleVariable<int32> CVarTracers(TEXT("agent.Tracers"), 0,
		TEXT("1 = also draw a debug line for each enemy shot (the game's real tracers are gj.Tracers; development builds only)."));

	int32 CVarInt(const TCHAR* Name)
	{
		const IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name);
		return V ? V->GetInt() : 0;
	}

	// Mannequin skin: SKM_Manny_Simple is ~180.5 cm tall; the bean is 176 cm (contract §6.1: the visual SHOULD match the hitboxes).
	constexpr float SkinScale = 176.f / 180.5f;
}

FOnAnyEnemyShot AEnemyBean::OnAnyEnemyShot;
FOnAnyEnemyDied AEnemyBean::OnAnyEnemyDied;

AEnemyBean::AEnemyBean(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = ABeanAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned; // CharacterMovement only simulates controlled pawns (§6.1)
	PrimaryActorTick.bCanEverTick = true;  // presentation only (gun kick and reload animation); gameplay ticks stay the bean's

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCollisionObjectType(ECC_AgentPawn);
	Capsule->SetCollisionResponseToChannel(ECC_AgentPawn, ECR_Ignore); // allies never block each other (§8.3)

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxSimulationTimeStep = 1.f / 60.f; // §5.4: integrate in steps no longer than training's
	Move->MaxSimulationIterations = 8;

	// Skin on the bean's (unused) skeletal mesh slot. NoCollision: it never blocks perception, bullets or movement (§6.1).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> SkinMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> SkinAnim(TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Rifle"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RifleMesh(TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle"));
	USkeletalMeshComponent* Skin = GetMesh();
	if (SkinMesh.Succeeded()) { Skin->SetSkeletalMeshAsset(SkinMesh.Object); }
	if (SkinAnim.Succeeded()) { Skin->SetAnimInstanceClass(SkinAnim.Class); }
	Skin->SetRelativeLocation(FVector(0.f, 0.f, -Capsule->GetUnscaledCapsuleHalfHeight()));
	Skin->SetRelativeRotation(FRotator(0.f, -90.f, 0.f)); // the mannequin faces +Y
	Skin->SetRelativeScale3D(FVector(SkinScale));
	Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Skin->PrimaryComponentTick.bStartWithTickEnabled = true;
	Skin->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(Skin, TEXT("HandGrip_R"));
	if (RifleMesh.Succeeded()) { WeaponMesh->SetSkeletalMeshAsset(RifleMesh.Object); }
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // §6.1: weapon meshes never block the agent's own rays
	WeaponMesh->SetGenerateOverlapEvents(false);

	WeaponStatic = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponStatic"));
	WeaponStatic->SetupAttachment(Skin, TEXT("HandGrip_R"));
	WeaponStatic->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponStatic->SetGenerateOverlapEvents(false);
	WeaponStatic->SetCanEverAffectNavigation(false);
	WeaponStatic->SetVisibility(false);
}

void AEnemyBean::ApplyLook()
{
	const UAgentRuntimeSettings& RS = UAgentRuntimeSettings::Get();
	USkeletalMeshComponent* Skin = GetMesh();
	if (RS.EnemySkins.Num() > 0)
	{
		const FSoftObjectPath& Pick = RS.EnemySkins[FMath::RandRange(0, RS.EnemySkins.Num() - 1)];
		if (USkeletalMesh* PickedSkin = Cast<USkeletalMesh>(Pick.TryLoad()))
		{
			Skin->SetSkeletalMeshAsset(PickedSkin);
		}
		else
		{
			UE_LOG(LogAgentRuntime, Warning, TEXT("Enemy skin %s did not load; keeping the mannequin"), *Pick.ToString());
		}
	}
	// Fit the skin to the bean's height (feet at the skin's origin, which sits on the capsule bottom). Visual only.
	if (const USkeletalMesh* SkinAsset = Skin->GetSkeletalMeshAsset())
	{
		BodyReloadAnim = PickClip({ RS.EnemyBodyReloadAnim });
		BodyFireAnim = PickClip({ RS.EnemyBodyFireAnim });
		const float Height = 2.f * SkinAsset->GetBounds().BoxExtent.Z;
		const float BeanHeight = 2.f * UFPSAgentSettings::Get().Body.StandingHalfHeight;
		if (Height > 1.f)
		{
			Skin->SetRelativeScale3D(FVector(BeanHeight / Height));
		}
	}

	// Gun model: a static or a skeletal mesh, in the skin's hand. NoCollision either way (§6.1).
	UObject* Gun = RS.EnemyWeapon.IsValid() ? RS.EnemyWeapon.TryLoad() : nullptr;
	const FAttachmentTransformRules Keep(EAttachmentRule::KeepRelative, true);
	if (UStaticMesh* StaticGun = Cast<UStaticMesh>(Gun))
	{
		WeaponStatic->SetStaticMesh(StaticGun);
		WeaponStatic->AttachToComponent(Skin, Keep, RS.EnemyWeaponSocket);
		WeaponStatic->SetRelativeTransform(RS.EnemyWeaponOffset);
		WeaponStatic->SetVisibility(true);
		WeaponMesh->SetVisibility(false);
	}
	else if (USkeletalMesh* SkeletalGun = Cast<USkeletalMesh>(Gun))
	{
		WeaponMesh->SetSkeletalMeshAsset(SkeletalGun);
		WeaponMesh->AttachToComponent(Skin, Keep, RS.EnemyWeaponSocket);
		WeaponMesh->SetRelativeTransform(RS.EnemyWeaponOffset);
		auto LoadGunAnim = [SkeletalGun](const FSoftObjectPath& Path) -> UAnimSequence*
		{
			UAnimSequence* Anim = Path.IsValid() ? Cast<UAnimSequence>(Path.TryLoad()) : nullptr;
			return (Anim && Anim->GetSkeleton() == SkeletalGun->GetSkeleton()) ? Anim : nullptr;
		};
		GunFireAnim = LoadGunAnim(RS.EnemyWeaponFireAnim);
		GunReloadAnim = LoadGunAnim(RS.EnemyWeaponReloadAnim);
	}
}

void AEnemyBean::PlayGunAnim(UAnimSequence* Anim, float Seconds)
{
	if (Anim && WeaponMesh && IsAlive())
	{
		WeaponMesh->PlayAnimation(Anim, /*bLooping=*/false);
		WeaponMesh->SetPlayRate(Anim->GetPlayLength() / FMath::Max(Seconds, 0.01f));
	}
}

UAnimSequence* AEnemyBean::PickClip(const TArray<FSoftObjectPath>& Clips) const
{
	const USkeletalMesh* SkinAsset = GetMesh() ? GetMesh()->GetSkeletalMeshAsset() : nullptr;
	if (!SkinAsset || Clips.Num() == 0)
	{
		return nullptr;
	}
	const FSoftObjectPath& Path = Clips[FMath::RandRange(0, Clips.Num() - 1)];
	UAnimSequence* Anim = Path.IsValid() ? Cast<UAnimSequence>(Path.TryLoad()) : nullptr;
	return (Anim && Anim->GetSkeleton() == SkinAsset->GetSkeleton()) ? Anim : nullptr;
}

UAnimSequence* AEnemyBean::PickDeathAnim(const AActor* Killer) const
{
	const UAgentRuntimeSettings& RS = UAgentRuntimeSettings::Get();
	const TArray<FSoftObjectPath>* Clips = &RS.EnemyDeathShotFromFront;
	if (Killer)
	{
		const FVector ToKiller = (Killer->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		const float Front = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToKiller);
		const float Right = FVector::DotProduct(GetActorRightVector().GetSafeNormal2D(), ToKiller);
		if (Front <= -0.707f) { Clips = &RS.EnemyDeathShotFromBack; }
		else if (Front < 0.707f) { Clips = Right >= 0.f ? &RS.EnemyDeathShotFromRight : &RS.EnemyDeathShotFromLeft; }
	}
	return PickClip(Clips->Num() > 0 ? *Clips : RS.EnemyDeathShotFromFront);
}

void AEnemyBean::PlayBodyAction(UAnimSequence* Anim, float PlayRate, float BusySeconds)
{
	UAnimInstance* BodyAnim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || !BodyAnim || !IsAlive())
	{
		return;
	}
	BodyAnim->PlaySlotAnimationAsDynamicMontage(Anim, UAgentRuntimeSettings::Get().EnemyBodySlot, 0.12f, 0.25f, PlayRate);
	BodyBusyUntil = GetWorld()->GetTimeSeconds() + BusySeconds;
}

USceneComponent* AEnemyBean::GetGunComponent() const
{
	if (WeaponStatic && WeaponStatic->IsVisible()) { return WeaponStatic; }
	return WeaponMesh;
}

void AEnemyBean::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Visual kick: the gun model jolts back along its barrel and settles (the offset from the settings is the rest pose).
	if (GunKick > 0.f)
	{
		GunKick = FMath::FInterpTo(GunKick, 0.f, DeltaSeconds, 14.f);
		if (GunKick < 0.01f) { GunKick = 0.f; } // at rest
		if (USceneComponent* Gun = GetGunComponent())
		{
			const FTransform& Rest = UAgentRuntimeSettings::Get().EnemyWeaponOffset;
			Gun->SetRelativeLocation(Rest.GetLocation() + Rest.GetRotation().RotateVector(FVector(0.f, 2.5f * GunKick, 0.4f * GunKick)));
		}
	}
	const bool bReloading = GetRifle() && GetRifle()->GetFireState().IsReloading();
	if (bReloading && !bGunWasReloading)
	{
		const float ReloadTime = FMath::Max(UFPSAgentSettings::Get().Rifle.ReloadTime, 0.1f);
		PlayGunAnim(GunReloadAnim, ReloadTime);
		if (BodyReloadAnim)
		{
			PlayBodyAction(BodyReloadAnim, BodyReloadAnim->GetPlayLength() / ReloadTime, ReloadTime);
		}
	}
	bGunWasReloading = bReloading;
}

void AEnemyBean::BeginPlay()
{
	Super::BeginPlay();
	DiedHandle = GetHealth()->OnDied.AddUObject(this, &AEnemyBean::HandleDied);
	DamagedHandle = GetHealth()->OnDamaged.AddUObject(this, &AEnemyBean::HandleDamaged);
	ShotHandle = GetRifle()->OnShotFired.AddUObject(this, &AEnemyBean::HandleShot);
	ApplyLook();
	ApplyDebugVisuals();
	if (AAgentDirector* Director = AAgentDirector::Get(this))
	{
		bRegistered = Director->RegisterAgent(this);
	}
	if (!bRegistered)
	{
		UE_LOG(LogAgentRuntime, Warning, TEXT("%s: no agent brain (model not loaded or no free slot); it stands idle."), *GetName());
	}
}

void AEnemyBean::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AAgentDirector* Director = AAgentDirector::Find(this))
	{
		Director->UnregisterAgent(this);
	}
	if (GetHealth())
	{
		GetHealth()->OnDied.Remove(DiedHandle);
		GetHealth()->OnDamaged.Remove(DamagedHandle);
	}
	if (GetRifle()) { GetRifle()->OnShotFired.Remove(ShotHandle); }
	GetWorldTimerManager().ClearTimer(RagdollTimer);
	Super::EndPlay(EndPlayReason);
}

bool AEnemyBean::IsAlive() const
{
	return GetHealth() && !GetHealth()->IsDead();
}

void AEnemyBean::ApplyDebugVisuals()
{
	const bool bShowBean = CVarInt(TEXT("agent.ShowBean")) != 0;
	BodyMesh->SetVisibility(bShowBean && IsAlive());
	TopMesh->SetVisibility(bShowBean && IsAlive());
	BottomMesh->SetVisibility(bShowBean && IsAlive());
	if (IsAlive())
	{
		GetMesh()->SetVisibility(!bShowBean, true);
	}
	SetHitboxesVisible(CVarInt(TEXT("agent.ShowHitboxes")) != 0 && IsAlive());
}

void AEnemyBean::HandleDied(UHealthComponent* Victim, AActor* Killer)
{
	// The director has already stopped this agent's decisions (it listens to the same event).
	UE_LOG(LogAgentRuntime, Display, TEXT("Enemy %s killed by %s"), *GetName(), Killer ? *Killer->GetName() : TEXT("?"));
	GetRifle()->SetTriggerHeld(false);
	HeadHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LegsHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetHitboxesVisible(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->DisableMovement();
	BodyMesh->SetVisibility(false);
	TopMesh->SetVisibility(false);
	BottomMesh->SetVisibility(false);
	DetachFromControllerPendingDestroy();

	USkeletalMeshComponent* Skin = GetMesh();
	Skin->SetVisibility(true, true);
	GunKick = 0.f;
	DeathPush = Killer ? (GetActorLocation() - Killer->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
	if (UAnimSequence* Death = PickDeathAnim(Killer))
	{
		// The template's deaths are reactions (the body staggers away from the shot but never reaches the floor): play one,
		// then hand the body to physics partway through so it drops where the stagger took it. NoCollision until then.
		Skin->PlayAnimation(Death, /*bLooping=*/false);
		const float Delay = FMath::Max(0.05f, Death->GetPlayLength() * UAgentRuntimeSettings::Get().EnemyDeathRagdollAt);
		GetWorldTimerManager().SetTimer(RagdollTimer, FTimerDelegate::CreateUObject(this, &AEnemyBean::StartRagdoll, DeathImpulse * 0.35f),
			Delay, false);
	}
	else
	{
		StartRagdoll(DeathImpulse);
	}
	SetLifeSpan(CorpseLifeSeconds);
	OnAnyEnemyDied.Broadcast(this);
}

void AEnemyBean::HandleDamaged(UHealthComponent* Victim, const FPSCombat::FDamageResult& Result, AActor* DamageInstigator)
{
	if (Result.bKilled || !IsAlive())
	{
		return; // the death handles it
	}
	// A hit reaction on the upper body (the legs keep the agent's movement), from the side the shot came from. A reload in
	// progress keeps going: the hand is on the magazine.
	if (GetWorld()->GetTimeSeconds() < BodyBusyUntil && GetRifle() && GetRifle()->GetFireState().IsReloading())
	{
		return;
	}
	const UAgentRuntimeSettings& RS = UAgentRuntimeSettings::Get();
	bool bFromBack = false;
	if (DamageInstigator)
	{
		const FVector ToShooter = (DamageInstigator->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		bFromBack = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToShooter) < -0.5f;
	}
	UAnimSequence* React = PickClip(bFromBack && RS.EnemyHitReactFromBack.Num() > 0 ? RS.EnemyHitReactFromBack : RS.EnemyHitReactFromFront);
	if (React)
	{
		PlayBodyAction(React, 1.f, React->GetPlayLength() * 0.8f);
	}
}

void AEnemyBean::StartRagdoll(float Impulse)
{
	// The skin only: PhysicsBody, physics collision only (no queries: rays, line of sight and bullets never see it), and it
	// ignores pawns and agents, so a corpse never blocks anyone (PLAN §9, contract §6.4).
	USkeletalMeshComponent* Skin = GetMesh();
	if (!Skin || !Skin->GetPhysicsAsset())
	{
		return; // no physics asset: the death clip's last pose stays
	}
	Skin->SetCollisionObjectType(ECC_PhysicsBody);
	Skin->SetCollisionResponseToAllChannels(ECR_Block);
	Skin->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Skin->SetCollisionResponseToChannel(ECC_AgentPawn, ECR_Ignore);
	Skin->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
	Skin->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Skin->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	Skin->SetAllBodiesSimulatePhysics(true);
	Skin->SetSimulatePhysics(true);
	Skin->WakeAllRigidBodies();
	Skin->bBlendPhysics = true;
	Skin->AddImpulse(DeathPush * Impulse + FVector(0.f, 0.f, 0.2f * Impulse), NAME_None, /*bVelChange=*/true);
}

void AEnemyBean::HandleShot(const FShotResult& Shot)
{
	OnAnyEnemyShot.Broadcast(this, Shot);
	PlayGunAnim(GunFireAnim, 1.f / UFPSAgentSettings::Get().Rifle.FireRate); // one bolt cycle per shot
	GunKick = FMath::Min(GunKick + 1.f, 1.5f);
	if (BodyFireAnim && GetWorld()->GetTimeSeconds() >= BodyBusyUntil)
	{
		// The shoulders take each shot's recoil (restarted every shot; a reload or hit reaction keeps the slot meanwhile).
		UAnimInstance* BodyAnim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (BodyAnim && IsAlive())
		{
			BodyAnim->PlaySlotAnimationAsDynamicMontage(BodyFireAnim, UAgentRuntimeSettings::Get().EnemyBodySlot, 0.03f, 0.2f, 1.f);
		}
	}
#if ENABLE_DRAW_DEBUG
	if (CVarTracers.GetValueOnGameThread() == 0) { return; }
	const FVector Muzzle = (WeaponMesh && WeaponMesh->DoesSocketExist(TEXT("Muzzle")))
		? WeaponMesh->GetSocketLocation(TEXT("Muzzle")) : Shot.Start;
	DrawDebugLine(GetWorld(), Muzzle, Shot.End, Shot.Zone != EHitZone::None ? FColor(255, 60, 40) : FColor(255, 210, 120), false, 0.06f, 0, 0.8f);
#endif
}
