#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AgentRuntimeSettings.generated.h"

/** Curriculum stage a snapshot was trained in (contract §1.1). It decides which inputs and outputs the game feeds and applies. */
UENUM(BlueprintType)
enum class EAgentStage : uint8 { S0, S1, S2, S3 };

/** Mobility line (contract v1.2 addendum): static = aim/fire/reload; walk = + move and walk; full = + crouch and jump. */
UENUM(BlueprintType)
enum class EAgentMobility : uint8 { Static, Walk, Full };

/** One entry of the model registry (PLAN.md §4.1). */
USTRUCT(BlueprintType)
struct FAgentModelEntry
{
	GENERATED_BODY()

	/** Run id without the "run-" release prefix, e.g. S1-20261113-L009 or S1-walk-20261114-153001. */
	UPROPERTY(EditAnywhere, Category="Model") FString RunId;
	UPROPERTY(EditAnywhere, Category="Model") EAgentStage Stage = EAgentStage::S0;
	/** The line the snapshot was trained as. It caps what the game may apply: Crouch/Jump only for a full snapshot. */
	UPROPERTY(EditAnywhere, Category="Model") EAgentMobility Mobility = EAgentMobility::Static;
	/** Episode time limit T (contract §12): S0 20 s, S1 30 s, or the run's Curriculum.<stage>.TimeLimitSeconds override. */
	UPROPERTY(EditAnywhere, Category="Model") float TimeLimitSeconds = 20.f;
	/** Folder holding encoder.bin, policy.bin and decoder.bin, relative to the project's Content folder. */
	UPROPERTY(EditAnywhere, Category="Model") FString SnapshotDir;
	/** The run's -Set string from its run report (empty = build defaults). Mechanics overrides in it must be replicated (contract §3). */
	UPROPERTY(EditAnywhere, Category="Model") FString SetString;
	UPROPERTY(EditAnywhere, Category="Model") FString ContractVersion = TEXT("1.2");
	/** SHA-256 of the three files, from the release's SHA256SUMS (verified when the snapshot was copied in). */
	UPROPERTY(EditAnywhere, Category="Model") FString Sha256Encoder;
	UPROPERTY(EditAnywhere, Category="Model") FString Sha256Policy;
	UPROPERTY(EditAnywhere, Category="Model") FString Sha256Decoder;
};

/** Project Settings > Game > Agent Runtime (saved to Config/DefaultGame.ini). */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Agent Runtime"))
class AGENTRUNTIME_API UAgentRuntimeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAgentRuntimeSettings();

	static const UAgentRuntimeSettings& Get() { return *GetDefault<UAgentRuntimeSettings>(); }

	const FAgentModelEntry* FindModel(const FString& RunId) const;

	/** The model registry. */
	UPROPERTY(Config, EditAnywhere, Category="Models") TArray<FAgentModelEntry> Models;

	/** The snapshot enemies run. Console: agent.Model <run id> before Play overrides it. */
	UPROPERTY(Config, EditAnywhere, Category="Models") FString ActiveRunId;

	/** The game's enemy line (contract v1.2: walk = never crouch or jump). Console: agent.Mobility walk|full|... overrides it live. */
	UPROPERTY(Config, EditAnywhere, Category="Enemies") EAgentMobility EnemyMobility = EAgentMobility::Walk;

	/** Learning Agents agent slots (the most enemies alive at once). */
	UPROPERTY(Config, EditAnywhere, Category="Enemies", meta=(ClampMin=1, ClampMax=64)) int32 MaxAgents = 16;

	UPROPERTY(Config, EditAnywhere, Category="Enemies") int32 PolicySeed = 1234;
};
