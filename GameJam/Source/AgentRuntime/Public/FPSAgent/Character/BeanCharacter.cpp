#include "Character/BeanCharacter.h"
#include "Camera/CameraComponent.h"
#include "Character/BeanHitboxLayout.h"
#include "Character/HealthComponent.h"
#include "Common/FPSAgentSettings.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/RifleComponent.h"

namespace
{
	void ConfigureHitbox(UShapeComponent* Shape)
	{
		Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Shape->SetCollisionObjectType(ECC_Pawn);
		Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
		Shape->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);
		Shape->SetGenerateOverlapEvents(false);
		Shape->SetCanEverAffectNavigation(false);
		Shape->SetHiddenInGame(true);
	}

	void ConfigureVisual(UStaticMeshComponent* Mesh, UStaticMesh* Asset)
	{
		Mesh->SetStaticMesh(Asset);
		Mesh->SetOwnerNoSee(true); // first-person camera sits inside the bean; others still see it
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
	}
}

ABeanCharacter::ABeanCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	const UFPSAgentSettings& S = UFPSAgentSettings::Get();

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(S.Body.CapsuleRadius, S.Body.StandingHalfHeight);
	Capsule->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore); // bullets hit hitboxes, not the capsule

	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // unused skeletal mesh slot
	GetMesh()->PrimaryComponentTick.bStartWithTickEnabled = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(S.Body.CrouchedHalfHeight);

	HeadHitbox = CreateDefaultSubobject<USphereComponent>(TEXT("HeadHitbox"));
	BodyHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyHitbox"));
	LegsHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("LegsHitbox"));
	UShapeComponent* Hitboxes[] = { HeadHitbox.Get(), BodyHitbox.Get(), LegsHitbox.Get() };
	for (UShapeComponent* Shape : Hitboxes)
	{
		Shape->SetupAttachment(Capsule);
		ConfigureHitbox(Shape);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	TopMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TopMesh"));
	BottomMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BottomMesh"));
	BodyMesh->SetupAttachment(Capsule);
	TopMesh->SetupAttachment(Capsule);
	BottomMesh->SetupAttachment(Capsule);
	ConfigureVisual(BodyMesh, CylinderMesh.Object);
	ConfigureVisual(TopMesh, SphereMesh.Object);
	ConfigureVisual(BottomMesh, SphereMesh.Object);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Capsule);
	Camera->bUsePawnControlRotation = true;

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Rifle = CreateDefaultSubobject<URifleComponent>(TEXT("Rifle"));

	BaseEyeHeight = ComputeBeanHitboxLayout(S.Body, S.Body.StandingHalfHeight).EyeZ;
	CrouchedEyeHeight = ComputeBeanHitboxLayout(S.Body, S.Body.CrouchedHalfHeight).EyeZ;
	ApplyMovementTuning();
	ApplyBodyLayout(S.Body.StandingHalfHeight);
}

void ABeanCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyMovementTuning(); // pick up settings edited after the CDO was built
	ApplyBodyLayout(GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
}

void ABeanCharacter::ApplyMovementTuning()
{
	const FMovementTuning& M = UFPSAgentSettings::Get().Movement;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = (bWalking ? M.WalkSpeed : M.RunSpeed) * SpeedScale;
	Move->MaxWalkSpeedCrouched = M.CrouchSpeed * SpeedScale;
	Move->MaxAcceleration = M.MaxAcceleration;
	Move->BrakingDecelerationWalking = M.BrakingDeceleration;
	Move->GroundFriction = M.GroundFriction;
	Move->JumpZVelocity = M.JumpZVelocity;
	Move->AirControl = M.AirControl;
}

void ABeanCharacter::ApplyBodyLayout(float CapsuleHalfHeight)
{
	const FBeanBody& B = UFPSAgentSettings::Get().Body;
	const FBeanHitboxLayout L = ComputeBeanHitboxLayout(B, CapsuleHalfHeight);
	HeadHitbox->SetSphereRadius(L.HeadRadius);
	HeadHitbox->SetRelativeLocation(FVector(0.f, 0.f, L.HeadCenterZ));
	BodyHitbox->SetBoxExtent(L.BodyExtent);
	BodyHitbox->SetRelativeLocation(FVector(0.f, 0.f, L.BodyCenterZ));
	LegsHitbox->SetBoxExtent(L.LegsExtent);
	LegsHitbox->SetRelativeLocation(FVector(0.f, 0.f, L.LegsCenterZ));
	Camera->SetRelativeLocation(FVector(0.f, 0.f, L.EyeZ));

	// Visual bean = cylinder between two spheres (engine basic shapes are 100 cm across, centered).
	const float R = B.CapsuleRadius;
	const float XY = 2.f * R / 100.f;
	BodyMesh->SetRelativeScale3D(FVector(XY, XY, FMath::Max(0.f, 2.f * CapsuleHalfHeight - 2.f * R) / 100.f));
	TopMesh->SetRelativeScale3D(FVector(XY));
	TopMesh->SetRelativeLocation(FVector(0.f, 0.f, CapsuleHalfHeight - R));
	BottomMesh->SetRelativeScale3D(FVector(XY));
	BottomMesh->SetRelativeLocation(FVector(0.f, 0.f, -(CapsuleHalfHeight - R)));
}

void ABeanCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	ApplyBodyLayout(GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
}

void ABeanCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	ApplyBodyLayout(GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
}

EHitZone ABeanCharacter::GetHitZone(const UPrimitiveComponent* Component) const
{
	if (Component == HeadHitbox) { return EHitZone::Head; }
	if (Component == BodyHitbox) { return EHitZone::Body; }
	if (Component == LegsHitbox) { return EHitZone::Legs; }
	return EHitZone::None;
}

UPrimitiveComponent* ABeanCharacter::GetHitbox(EHitZone Zone) const
{
	switch (Zone)
	{
	case EHitZone::Head: return HeadHitbox;
	case EHitZone::Body: return BodyHitbox;
	case EHitZone::Legs: return LegsHitbox;
	default:             return nullptr;
	}
}

float ABeanCharacter::GetHorizontalSpeed() const
{
	return GetVelocity().Size2D();
}

bool ABeanCharacter::IsAirborne() const
{
	return GetCharacterMovement()->IsFalling();
}

void ABeanCharacter::SetWalking(bool bWalk)
{
	bWalking = bWalk;
	const FMovementTuning& M = UFPSAgentSettings::Get().Movement;
	GetCharacterMovement()->MaxWalkSpeed = (bWalking ? M.WalkSpeed : M.RunSpeed) * SpeedScale;
}

void ABeanCharacter::SetSpeedScale(float Scale)
{
	SpeedScale = FMath::Max(0.f, Scale);
	ApplyMovementTuning();
}

void ABeanCharacter::ApplyCrouchNow(bool bCrouch)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bWantsToCrouch = bCrouch;
	if (bCrouch)
	{
		Move->Crouch(false);   // resizes the capsule now; the base stays on the floor while walking
	}
	else
	{
		Move->UnCrouch(false);
	}
}

void ABeanCharacter::SetHitboxesVisible(bool bVisible)
{
	HeadHitbox->SetHiddenInGame(!bVisible);
	BodyHitbox->SetHiddenInGame(!bVisible);
	LegsHitbox->SetHiddenInGame(!bVisible);
}

void ABeanCharacter::SetDeadState(bool bDead)
{
	SetActorHiddenInGame(bDead);
	SetActorEnableCollision(!bDead);
	Rifle->SetTriggerHeld(false);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (bDead)
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	else
	{
		Move->SetMovementMode(MOVE_Walking);
	}
}
