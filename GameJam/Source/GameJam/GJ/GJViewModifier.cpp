#include "GJViewModifier.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "GJPlayerCharacter.h"

bool UGJViewModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
	const AGJPlayerCharacter* Player = Cast<AGJPlayerCharacter>(GetViewTarget());
	if (!Player || !Player->GetFirstPersonCameraComponent() || !Player->GetFirstPersonCameraComponent()->IsActive())
	{
		return false; // another camera (the Blueprint's death camera) is in charge
	}
	FVector ViewOffset;
	FRotator ViewRoll;
	FVector ArmsShift;
	FVector WorldOffset;
	Player->GetViewEffects(ViewOffset, ViewRoll, ArmsShift, WorldOffset);
	InOutPOV.Location -= ArmsShift;                                   // the head (camera) moved with the arms: undo it
	InOutPOV.Location += InOutPOV.Rotation.RotateVector(ViewOffset);  // bob, dip, kick-back, in view space
	InOutPOV.Location += WorldOffset;                                 // the fall to the floor on death
	InOutPOV.Rotation = (FQuat(InOutPOV.Rotation) * FQuat(ViewRoll)).Rotator();
	return false;
}
