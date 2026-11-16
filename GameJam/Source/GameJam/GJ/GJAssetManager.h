#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "GJAssetManager.generated.h"

/**
 * The project's asset manager (DefaultEngine.ini [/Script/Engine.Engine] AssetManagerClassName).
 *
 * Unreal packages what the cooked levels reference. The game also loads many assets by path: Project Settings (GameJam,
 * Agent Runtime, GJ Music: skins, animations, gun models, effects, music stems) and its C++ class defaults. Nothing in a level
 * references those, so a packaged build used to miss them while the editor showed them. ModifyCook adds every /Game asset
 * those settings and class defaults point at (the cook then follows their own references) and writes the package list to
 * Saved/GJCookManifest.txt, which Tools/Build/BuildGame.ps1 checks against the cooked output.
 *
 * Console: gj.VerifyContent [quit] loads the same list in a running game and reports anything missing ('quit' exits with
 * code 1 on failure), which the build script runs on Development builds.
 */
UCLASS()
class GAMEJAM_API UGJAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	/** Every /Game asset the project's native classes (settings included) reference in their defaults, soft or hard. */
	static void CollectCodeReferencedAssets(TArray<FSoftObjectPath>& OutAssets);

#if WITH_EDITOR
	virtual void ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook,
		TArray<FName>& PackagesToNeverCook) override;
#endif
};
