#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GJFrontEndSettings.generated.h"

/**
 * Main menu and starting sequence (Project Settings > Game > GJ Front End). See UGJRunSubsystem and FRONTEND.md.
 * PLAN.md leaves the game's name open: Title is a placeholder until the team picks one.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="GJ Front End"))
class GAMEJAM_API UGJFrontEndSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UGJFrontEndSettings& Get() { return *GetDefault<UGJFrontEndSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Show the main menu and the intro in levels that have a menu camera marker (actor tagged GJMenuOrbit). */
	UPROPERTY(Config, EditAnywhere, Category="Front End")
	bool bEnabled = true;

	/** The game's name on the title screen and the intro card (placeholder: PLAN.md leaves the name open). */
	UPROPERTY(Config, EditAnywhere, Category="Front End")
	FText Title = NSLOCTEXT("GJFrontEnd", "Title", "Night Clubs Descent");

	/** One line under the title (from the brief: no new lore). */
	UPROPERTY(Config, EditAnywhere, Category="Front End")
	FText Tagline = NSLOCTEXT("GJFrontEnd", "Tagline", "Beneath Las Vegas, the party never ends.");

	/** Length of the camera's descent from the drains to the player's eyes. */
	UPROPERTY(Config, EditAnywhere, Category="Intro", meta=(ClampMin=2, UIMin=2, UIMax=30))
	float IntroSeconds = 10.f;

	/** Blend from the intro camera into the player's view at the end. */
	UPROPERTY(Config, EditAnywhere, Category="Intro", meta=(ClampMin=0))
	float IntroBlendSeconds = 0.6f;

	/** Seconds per lap of the menu camera around the GJMenuOrbit marker. */
	UPROPERTY(Config, EditAnywhere, Category="Menu", meta=(ClampMin=10))
	float MenuOrbitSeconds = 90.f;

	UPROPERTY(Config, EditAnywhere, Category="Menu")
	float MenuOrbitRadiusCm = 1000.f;

	/** Camera height above the marker; it looks at a point 1.2 m above the marker. */
	UPROPERTY(Config, EditAnywhere, Category="Menu")
	float MenuOrbitHeightCm = 420.f;

	/** The music's low-pass cutoff while the menu is up (the music opens up when the intro starts). */
	UPROPERTY(Config, EditAnywhere, Category="Menu")
	float MenuMusicCutoffHz = 900.f;
};
