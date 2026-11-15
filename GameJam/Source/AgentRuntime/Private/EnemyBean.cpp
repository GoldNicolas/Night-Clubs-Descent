#include "EnemyBean.h"
#include "AgentDirector.h"
#include "AgentRuntime.h"
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
#include "UObject/ConstructorHelpers.h"
#include "Weapons/RifleComponent.h"

namespace
{
	TAutoConsoleVariable<int32> CVarTracers(TEXT("agent.Tracers"), 1,
		TEXT("1 = draw a short tracer for each enemy shot (placeholder for real muzzle/tracer VFX; development builds only)."));

	int32 CVarInt(const TCHAR* Name)
	{
		const IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name);
		return V ? V->GetInt() : 0;
	}

	// Mannequin skin: SKM_Manny_Simple is ~180.5 cm tall; the bean is 176 cm (contract §6.1: the visual SHOULD match the hitboxes).
	constexpr float SkinScale = 176.f / 180.5f;
}

AEnemyBean::AEnemyBean(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = ABeanAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned; // CharacterMovement only simulates controlled pawns (§6.1)

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
}

void AEnemyBean::BeginPlay()
{
	Super::BeginPlay();
	DiedHandle = GetHealth()->OnDied.AddUObject(this, &AEnemyBean::HandleDied);
	ShotHandle = GetRifle()->OnShotFired.AddUObject(this, &AEnemyBean::HandleShot);
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
	if (GetHealth()) { GetHealth()->OnDied.Remove(DiedHandle); }
	if (GetRifle()) { GetRifle()->OnShotFired.Remove(ShotHandle); }
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

	// Ragdoll the skin only: PhysicsBody, physics collision only (no queries: rays, line of sight and bullets never see it),
	// and it ignores pawns and agents, so a corpse never blocks anyone (PLAN §9, contract §6.4).
	USkeletalMeshComponent* Skin = GetMesh();
	Skin->SetVisibility(true, true);
	Skin->SetCollisionObjectType(ECC_PhysicsBody);
	Skin->SetCollisionResponseToAllChannels(ECR_Block);
	Skin->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Skin->SetCollisionResponseToChannel(ECC_AgentPawn, ECR_Ignore);
	Skin->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
	Skin->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Skin->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	if (Skin->GetPhysicsAsset())
	{
		Skin->SetAllBodiesSimulatePhysics(true);
		Skin->SetSimulatePhysics(true);
		Skin->WakeAllRigidBodies();
		Skin->bBlendPhysics = true;
		const FVector Push = Killer ? (GetActorLocation() - Killer->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
		Skin->AddImpulse(Push * DeathImpulse + FVector(0.f, 0.f, 100.f), NAME_None, /*bVelChange=*/true);
	}
	SetLifeSpan(CorpseLifeSeconds);
}

void AEnemyBean::HandleShot(const FShotResult& Shot)
{
#if ENABLE_DRAW_DEBUG
	if (CVarTracers.GetValueOnGameThread() == 0) { return; }
	const FVector Muzzle = (WeaponMesh && WeaponMesh->DoesSocketExist(TEXT("Muzzle")))
		? WeaponMesh->GetSocketLocation(TEXT("Muzzle")) : Shot.Start;
	DrawDebugLine(GetWorld(), Muzzle, Shot.End, Shot.Zone != EHitZone::None ? FColor(255, 60, 40) : FColor(255, 210, 120), false, 0.06f, 0, 0.8f);
#endif
}
