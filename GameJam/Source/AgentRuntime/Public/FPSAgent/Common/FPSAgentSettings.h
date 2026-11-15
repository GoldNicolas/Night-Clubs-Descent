#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Common/FPSAgentTypes.h"
#include "FPSAgentSettings.generated.h"

/** Every tunable mechanic. Project Settings > Game > FPS Agent (saved to Config/DefaultGame.ini). */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="FPS Agent"))
class AGENTRUNTIME_API UFPSAgentSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFPSAgentSettings();

	static const UFPSAgentSettings& Get() { return *GetDefault<UFPSAgentSettings>(); }

	UPROPERTY(Config, EditAnywhere, Category="Mechanics") FMovementTuning Movement;
	UPROPERTY(Config, EditAnywhere, Category="Mechanics") FBeanBody Body;
	UPROPERTY(Config, EditAnywhere, Category="Mechanics") FRifleStats Rifle;
	UPROPERTY(Config, EditAnywhere, Category="Mechanics") FHealthTuning Health;
	UPROPERTY(Config, EditAnywhere, Category="Range") FRangeSettings Range;
	UPROPERTY(Config, EditAnywhere, Category="Map") FMapPlaySettings Map;
	UPROPERTY(Config, EditAnywhere, Category="Learning") FReachTaskSettings SpikeA;
	UPROPERTY(Config, EditAnywhere, Category="Learning") FCombatSettings Combat;
	UPROPERTY(Config, EditAnywhere, Category="Learning") FCurriculumSettings Curriculum;
	UPROPERTY(Config, EditAnywhere, Category="Learning") FBotBehaviourSettings Bots;
	/** Mouse look: degrees per mouse count. */
	UPROPERTY(Config, EditAnywhere, Category="Input") float MouseDegreesPerCount = 0.05f;
};
