#include "GJDancer.h"
#include "AIController.h"
#include "AgentRuntime.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Common/FPSAgentTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GJGameSettings.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	constexpr float DancerHalfHeight = 86.f; // skins are 166-172 cm with feet at their origin, which sits on the capsule bottom
}

AGJDancer::AGJDancer()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned; // CharacterMovement only simulates controlled pawns

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(32.f, DancerHalfHeight);
	Capsule->SetCollisionObjectType(ECC_Pawn);                          // never WorldStatic / WorldDynamic: agents can't see it
	Capsule->SetCollisionResponseToChannel(ECC_AgentPawn, ECR_Ignore);  // agents walk through crowds (contract §8 spirit, PLAN §9)
	Capsule->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);     // bullets pass through crowds
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Capsule->SetCanEverAffectNavigation(false);

	USkeletalMeshComponent* Skin = GetMesh();
	Skin->SetRelativeLocation(FVector(0.f, 0.f, -DancerHalfHeight));
	Skin->SetRelativeRotation(FRotator(0.f, -90.f, 0.f)); // the mannequin faces +Y
	Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Skin->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 540.f, 0.f);
	Move->MaxWalkSpeed = 480.f;
	Move->MaxAcceleration = 2400.f;
	Move->BrakingDecelerationWalking = 2400.f;
}

void AGJDancer::BeginPlay()
{
	Super::BeginPlay();
	const UGJGameSettings& S = UGJGameSettings::Get();
	if (S.DancerSkins.Num() > 0)
	{
		if (USkeletalMesh* PickedSkin = Cast<USkeletalMesh>(S.DancerSkins[FMath::RandRange(0, S.DancerSkins.Num() - 1)].TryLoad()))
		{
			GetMesh()->SetSkeletalMeshAsset(PickedSkin);
		}
	}
	if (S.DanceAnimations.Num() > 0)
	{
		Dance = Cast<UAnimSequence>(S.DanceAnimations[FMath::RandRange(0, S.DanceAnimations.Num() - 1)].TryLoad());
	}
	LocomotionClass = S.FleeAnimClass.TryLoadClass<UAnimInstance>();
	DancePlayRate = FMath::FRandRange(0.9f, 1.1f);
	StartDancing();
}

void AGJDancer::StartDancing()
{
	State = EDancerState::Dancing;
	GetCharacterMovement()->StopMovementImmediately();
	USkeletalMeshComponent* Skin = GetMesh();
	if (Dance)
	{
		Skin->PlayAnimation(Dance, /*bLooping=*/true);
		Skin->SetPlayRate(DancePlayRate);
		Skin->SetPosition(FMath::FRandRange(0.f, Dance->GetPlayLength()), /*bFireNotifies=*/false); // out of sync, like a real crowd
	}
	else
	{
		UseLocomotion(); // no dance clips configured: stand idle
	}
}

void AGJDancer::UseLocomotion()
{
	USkeletalMeshComponent* Skin = GetMesh();
	if (!LocomotionClass)
	{
		return;
	}
	if (Skin->GetAnimationMode() != EAnimationMode::AnimationBlueprint || Skin->GetAnimClass() != LocomotionClass)
	{
		Skin->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Skin->SetAnimInstanceClass(LocomotionClass);
	}
}

void AGJDancer::Panic(const FVector& Origin)
{
	const UGJGameSettings& S = UGJGameSettings::Get();
	const double Now = GetWorld()->GetTimeSeconds();
	if (State != EDancerState::Fleeing)
	{
		PanicOrigin = Origin;
		FVector Away = (GetActorLocation() - Origin).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			Away = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
		}
		FleeDirection = Away.RotateAngleAxis(FMath::FRandRange(-35.f, 35.f), FVector::UpVector);
		// Panic: a snappy start, each dancer at its own pace, up to the locomotion's full jog (its cycle is 600 cm/s).
		UCharacterMovementComponent* Move = GetCharacterMovement();
		Move->MaxWalkSpeed = FMath::Min(S.FleeSpeed * FMath::FRandRange(0.9f, 1.1f), 600.f);
		Move->MaxAcceleration = 3600.f;
		const bool bWasDancing = State == EDancerState::Dancing;
		UseLocomotion();
		if (bWasDancing)
		{
			// A dance clip without root lock can carry the body off the capsule (the bundled ones are root-locked and stay put):
			// start running from where the body really is.
			const FVector Pelvis = GetMesh()->GetSocketLocation(TEXT("pelvis"));
			SetActorLocation(FVector(Pelvis.X, Pelvis.Y, GetActorLocation().Z), /*bSweep=*/true);
		}
		NextJitterTime = Now + FMath::FRandRange(0.25f, 0.6f);
	}
	State = EDancerState::Fleeing;
	FleeUntil = Now + S.FleeSeconds * FMath::FRandRange(0.8f, 1.2f);
}

FVector AGJDancer::SteerAroundWalls(const FVector& Desired) const
{
	const UWorld* World = GetWorld();
	const FVector Start = GetActorLocation();
	FCollisionObjectQueryParams Geometry;
	Geometry.AddObjectTypesToQuery(ECC_WorldStatic);
	Geometry.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GJDancerSteer), false, this);
	const FCollisionShape Probe = FCollisionShape::MakeSphere(30.f);
	auto IsFree = [&](const FVector& Dir)
	{
		FHitResult Hit;
		return !World->SweepSingleByObjectType(Hit, Start, Start + Dir * 180.f, FQuat::Identity, Geometry, Probe, Params);
	};
	if (IsFree(Desired))
	{
		return Desired;
	}
	for (const float Turn : { 45.f, -45.f, 90.f, -90.f, 135.f, -135.f, 180.f })
	{
		const FVector Dir = Desired.RotateAngleAxis(Turn, FVector::UpVector);
		if (IsFree(Dir))
		{
			return Dir;
		}
	}
	return -Desired;
}

void AGJDancer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();
	switch (State)
	{
	case EDancerState::Fleeing:
		if (Now >= FleeUntil)
		{
			State = EDancerState::Cowering;
			CalmAt = Now + UGJGameSettings::Get().CowerSeconds * FMath::FRandRange(0.8f, 1.2f);
			GetCharacterMovement()->StopMovementImmediately();
			UseLocomotion(); // stand and catch breath (idle) instead of sprinting in place
		}
		else
		{
			// Erratic zig-zag: lurch in a new direction every half second or so, but keep heading away from the shooting.
			if (Now >= NextJitterTime)
			{
				FleeDirection = FleeDirection.RotateAngleAxis(FMath::FRandRange(-40.f, 40.f), FVector::UpVector);
				const FVector Away = (GetActorLocation() - PanicOrigin).GetSafeNormal2D();
				if (!Away.IsNearlyZero() && FVector::DotProduct(FleeDirection, Away) < 0.25f)
				{
					FleeDirection = (FleeDirection + Away * 1.5f).GetSafeNormal2D();
				}
				NextJitterTime = Now + FMath::FRandRange(0.35f, 0.8f);
			}
			FleeDirection = SteerAroundWalls(FleeDirection);
			AddMovementInput(FleeDirection, 1.f);
		}
		break;
	case EDancerState::Cowering:
		if (Now >= CalmAt)
		{
			StartDancing();
		}
		break;
	default:
		break;
	}
}
