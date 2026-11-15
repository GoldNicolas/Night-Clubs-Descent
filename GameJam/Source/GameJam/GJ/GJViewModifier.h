#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "GJViewModifier.generated.h"

/**
 * First-person view feel for AGJPlayerCharacter (presentation only): camera bob, landing dip, kick-back and strafe roll.
 * The camera rides on the first-person mesh's head; the character shifts that mesh to sway the gun, and this modifier takes
 * the shift back out of the view, so the gun moves against the camera instead of the camera moving with it.
 */
UCLASS()
class GAMEJAM_API UGJViewModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
};
