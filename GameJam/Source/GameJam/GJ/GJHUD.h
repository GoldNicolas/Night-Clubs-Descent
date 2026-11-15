#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GJHUD.generated.h"

/**
 * Testing-ground HUD (canvas, no assets): crosshair sized by the rifle's real spread, health / armor / ammo, reload, a hit marker,
 * a damage flash pointing at the shooter, and the stealth readout (LOUD while agents can hear your footsteps).
 * The template's UMG widgets (life bar, bullet counter) keep working alongside it.
 */
UCLASS()
class GAMEJAM_API AGJHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCrosshair(float SpreadDeg, float CenterX, float CenterY);
	void DrawHitMarker(float CenterX, float CenterY);
	void DrawDamageIndicator(float CenterX, float CenterY);
	void DrawText(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = 1.2f);
};
