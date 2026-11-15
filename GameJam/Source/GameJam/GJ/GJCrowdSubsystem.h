#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GJCrowdSubsystem.generated.h"

class AEnemyBean;
struct FShotResult;

/**
 * The club crowd in any game world, with no map changes (a stand-in until the map branch adds dance floors):
 * - spawns gj.Dancers dancers on the floor around the PlayerStart (gj.DanceRadius), away from the player;
 * - fans every gunshot out to them: the player's rifle calls ReportGunshot, enemy shots arrive via AEnemyBean::OnAnyEnemyShot.
 * Presentation only: nothing here reaches an agent's senses.
 * Console: gj.Dancers N, gj.DanceRadius cm, gj.ClearDancers.
 */
UCLASS()
class GAMEJAM_API UGJCrowdSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	/** A gunshot at Origin (player or enemy): every dancer within the panic radius runs. */
	void ReportGunshot(const FVector& Origin);
	static void ReportGunshot(const UObject* WorldContext, const FVector& Origin);

	void ClearDancers();

private:
	void OnEnemyShot(AEnemyBean* Shooter, const FShotResult& Shot);
	int32 SpawnDancers(int32 Count);
	void SpreadPanic();

	FDelegateHandle EnemyShotHandle;
	int32 SpawnedFor = -1;
	double NextSpreadTime = 0.0;
};
