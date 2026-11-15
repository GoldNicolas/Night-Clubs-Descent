#include "Character/BeanAIController.h"
#include "GameFramework/Pawn.h"

ABeanAIController::ABeanAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSetControlRotationFromPawnOrientation = false;
}

void ABeanAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	if (APawn* MyPawn = GetPawn(); MyPawn && bUpdatePawn)
	{
		MyPawn->FaceRotation(GetControlRotation(), DeltaTime);
	}
}
