#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GJEncounterSubsystem.generated.h"

class AEnemyBean;

/**
 * Testing-ground encounter (a stand-in for PLAN §10's Encounter Director). Keeps gj.Enemies enemies alive in any game world, with no map
 * changes: spawn points are sampled on the floor (a procedural map's reachable tiles; in a hand-built level its EnemySpawn-tagged
 * markers plus any socket maps' tiles; otherwise around the PlayerStart) and follow the contract's spawn rules (§12.5, PLAN §10):
 * >= 5 m from the player (preferably out of view), >= 4 m from other agents, floor below and capsule clearance, within 40 m of the
 * player, standing, pitch 0, facing away from the player. Arrivals are staggered (gj.SpawnInterval) and grouped into waves.
 * Console: gj.Enemies N, gj.WaveSize N, gj.WaveBreak s, gj.SpawnRadius cm, gj.SpawnInterval s, gj.KillEnemies.
 */
UCLASS()
class GAMEJAM_API UGJEncounterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	/** Destroys every enemy (gj.KillEnemies). */
	void KillAll();
	int32 GetCurrentWave() const { return CurrentWave; }

private:
	bool TrySpawnOne();
	int32 CountAliveEnemies() const;

	double NextSpawnTime = 1.0;
	double NextWaveTime = 1.0;
	int32 CurrentWave = 1;
	int32 SpawnedThisWave = 0;
};
