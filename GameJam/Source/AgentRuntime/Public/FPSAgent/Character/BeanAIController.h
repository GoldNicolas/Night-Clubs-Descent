#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BeanAIController.generated.h"

/**
 * AI controller for agents and scripted bots. AAIController::UpdateControlRotation resets the pitch to 0 every tick
 * (and the yaw to the pawn's) unless it focuses a pawn. Our agents and bots aim by setting the control rotation
 * themselves, so this keeps it and only turns the pawn's yaw to match (the bean uses the controller yaw).
 */
UCLASS()
class AGENTRUNTIME_API ABeanAIController : public AAIController
{
	GENERATED_BODY()

public:
	ABeanAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;
};
