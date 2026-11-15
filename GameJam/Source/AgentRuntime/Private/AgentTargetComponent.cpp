#include "AgentTargetComponent.h"
#include "AgentDirector.h"
#include "Bots/BotSenses.h"
#include "Common/FPSAgentSettings.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UAgentTargetComponent::UAgentTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics; // after movement: footsteps use this frame's velocity
}

void UAgentTargetComponent::BeginPlay()
{
	Super::BeginPlay();
	bAlive = true;
	if (AAgentDirector* Director = AAgentDirector::Get(this))
	{
		Director->RegisterTarget(this);
	}
}

void UAgentTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AAgentDirector* Director = AAgentDirector::Find(this))
	{
		Director->UnregisterTarget(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UAgentTargetComponent::SetHitboxes(UPrimitiveComponent* InHead, UPrimitiveComponent* InBody, UPrimitiveComponent* InLegs)
{
	HeadBox = InHead;
	BodyBox = InBody;
	LegsBox = InLegs;
}

EHitZone UAgentTargetComponent::GetHitZone(const UPrimitiveComponent* Component) const
{
	if (!Component) { return EHitZone::None; }
	if (Component == HeadBox.Get()) { return EHitZone::Head; }
	if (Component == BodyBox.Get()) { return EHitZone::Body; }
	if (Component == LegsBox.Get()) { return EHitZone::Legs; }
	return EHitZone::None;
}

bool UAgentTargetComponent::ApplyAgentHit(float Damage, EHitZone Zone, AActor* DamageInstigator)
{
	if (!bAlive || !HitHandler.IsBound())
	{
		return false;
	}
	return HitHandler.Execute(Damage, Zone, DamageInstigator);
}

FVector UAgentTargetComponent::GetHeadLocation() const
{
	if (const UPrimitiveComponent* H = HeadBox.Get()) { return H->GetComponentLocation(); }
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
}

FVector UAgentTargetComponent::GetChestLocation() const
{
	if (const UPrimitiveComponent* B = BodyBox.Get()) { return B->GetComponentLocation(); }
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
}

FVector UAgentTargetComponent::GetTargetVelocity() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetVelocity() : FVector::ZeroVector;
}

FVector UAgentTargetComponent::GetForward2D() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
}

bool UAgentTargetComponent::IsTargetCrouched() const
{
	const ACharacter* C = Cast<ACharacter>(GetOwner());
	return C && C->bIsCrouched;
}

bool UAgentTargetComponent::IsMakingFootsteps() const
{
	const UFPSAgentSettings& S = UFPSAgentSettings::Get();
	return bAlive && !bSilentToAgents
		&& FPSBots::IsAudibleMovement(GetTargetVelocity().Size2D(), IsTargetCrouched(), bWalking, S.Movement.WalkSpeed, S.Bots);
}

void UAgentTargetComponent::ReportGunshot()
{
	if (!bAlive || bSilentToAgents) { return; }
	if (AAgentDirector* Director = AAgentDirector::Find(this))
	{
		Director->AddSound(FPSBots::ESoundKind::Gunshot, GetOwner()->GetActorLocation(), TargetId);
	}
}

void UAgentTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UWorld* World = GetWorld();
	if (!World || !IsMakingFootsteps()) { return; }
	// Same cadence as the training arena's EmitFootsteps: one event per FootstepIntervalSeconds while the rule holds (contract §7.4).
	const double Now = World->GetTimeSeconds();
	if (Now < NextFootstepTime) { return; }
	if (AAgentDirector* Director = AAgentDirector::Find(this))
	{
		Director->AddSound(FPSBots::ESoundKind::Footstep, GetOwner()->GetActorLocation(), TargetId);
	}
	NextFootstepTime = Now + UFPSAgentSettings::Get().Bots.FootstepIntervalSeconds;
}
