#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GJDancer.generated.h"

class UAnimSequence;

/**
 * Ambient club-goer (PLAN.md §9 crowd). Presentation only, so the trained enemies behave exactly as in training:
 * - never in an agent's target list, and its capsule is a Pawn (not WorldStatic/WorldDynamic), so agent rays and line of sight ignore it;
 * - it ignores the AgentPawn channel (agents walk through crowds) and the Weapon channel (bullets pass through crowds);
 * - its sounds and panic never reach an agent.
 * Dances on a loop. A gunshot nearby (UGJCrowdSubsystem) makes it run away from the shot on the mannequin's locomotion, steering
 * around walls; after the shooting stops it stands still for a while, then dances again.
 */
UCLASS()
class GAMEJAM_API AGJDancer : public ACharacter
{
	GENERATED_BODY()

public:
	AGJDancer();

	/** A gunshot at Origin: run away from it. Another shot while running extends the run. */
	void Panic(const FVector& Origin);
	bool IsFleeing() const { return State == EDancerState::Fleeing; }
	/** Where the shooting this dancer is running from came from (panic spreads with it). */
	FVector GetPanicOrigin() const { return PanicOrigin; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	enum class EDancerState : uint8 { Dancing, Fleeing, Cowering };

	void StartDancing();
	void UseLocomotion();
	/** The flee direction, turned away from walls and cover just ahead. */
	FVector SteerAroundWalls(const FVector& Desired) const;

	EDancerState State = EDancerState::Dancing;
	FVector FleeDirection = FVector::ForwardVector;
	FVector PanicOrigin = FVector::ZeroVector;
	double NextJitterTime = 0.0;
	double FleeUntil = 0.0;
	double CalmAt = 0.0;
	float DancePlayRate = 1.f;

	UPROPERTY() TObjectPtr<UAnimSequence> Dance;
	UPROPERTY() TSubclassOf<UAnimInstance> LocomotionClass;
};
