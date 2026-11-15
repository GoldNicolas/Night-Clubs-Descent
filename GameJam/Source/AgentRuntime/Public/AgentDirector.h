#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AgentRuntimeSettings.h"
#include "Bots/BotSenses.h"
#include "Common/FPSAgentTypes.h"
#include "Common/Perception.h"
#include "Learning/CombatObservation.h"
#include "AgentDirector.generated.h"

class ABeanCharacter;
class UAgentTargetComponent;
class UHealthComponent;
class ULearningAgentsInteractor;
class ULearningAgentsManager;
class ULearningAgentsPolicy;
namespace FPSCombat { struct FDamageResult; }

/**
 * One per game world (spawned on demand). Hosts the FPS-Agent policy for every enemy bean and reproduces the training world
 * from each agent's point of view (contract v1.2, §4-§12):
 * - loads the registry's active snapshot and refuses one whose network doesn't match (§4 load check);
 * - a 15 Hz decision clock on game time: update tracks, then one batched RunInference(0) for all agents (§5);
 * - every frame: the held Aim integrated at 720 deg/s and the held Move applied (§5.3);
 * - perception exactly as the arena: view cone + line of sight to the player's head or chest, 2 s memory (§7.2, §9.3);
 * - hearing: only the player's sounds, gated by stage (S1: footsteps only; S2+: gunshots too) (§1.1, §7.4);
 * - the Hit indicator only for S2+ snapshots, only from player damage (§9.4);
 * - Crouch/Jump dropped unless the snapshot is `full` AND the applied mobility allows them (v1.2 addendum; agent.Mobility);
 * - virtual episodes and time left (§12); allies are invisible to each other (§8).
 */
UCLASS(NotPlaceable, Transient)
class AGENTRUNTIME_API AAgentDirector : public AActor
{
	GENERATED_BODY()

public:
	AAgentDirector();

	/** The world's director; spawns it in game worlds when there is none. */
	static AAgentDirector* Get(const UObject* WorldContext);
	/** The world's director, or null. Never spawns. */
	static AAgentDirector* Find(const UObject* WorldContext);

	/** Adds an enemy bean and gives it the full spawn reset of §12.1. False when the model isn't loaded or every slot is taken. */
	bool RegisterAgent(ABeanCharacter* Bean);
	void UnregisterAgent(ABeanCharacter* Bean);

	void RegisterTarget(UAgentTargetComponent* Target);
	void UnregisterTarget(UAgentTargetComponent* Target);

	/** A sound made by a target (the player). Draws the heard-position noise once, when it is made (§7.4). */
	void AddSound(FPSBots::ESoundKind Kind, const FVector& Position, int32 SourceTargetId);

	/** UCombatInteractor's gather callback. */
	void BuildObservation(int32 AgentId, FPSCombatObs::FObservationData& Out) const;
	/** UCombatInteractor's perform callback: buttons apply now, Aim and Move are held until the next decision. */
	void SetAction(int32 AgentId, const FPSCombatObs::FAgentAction& Action);

	bool IsReady() const { return bReady; }
	const FAgentModelEntry& GetModel() const { return Model; }
	/** Crouch/Jump as applied right now: the snapshot's line capped by agent.Mobility (or the settings' EnemyMobility). */
	FCombatAbilitySettings GetAppliedAbilities() const;
	/** One line for HUDs and logs: model, stage, applied mobility, agents, decisions. */
	FString GetStatusLine() const;
	int32 GetAgentCount() const { return Slots.Num(); }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FSlot
	{
		TWeakObjectPtr<ABeanCharacter> Bean;
		int32 AgentId = INDEX_NONE;
		TArray<FPSPerception::FTrack> Tracks;
		FPSCombatObs::FAgentAction Held;
		/** Virtual episode start E (§12). */
		double EpisodeStart = 0.0;
		double LastHitTime = -1.0e9;
		FVector LastHitDirection = FVector::ZeroVector;
		FDelegateHandle DamagedHandle;
		FDelegateHandle DiedHandle;
	};

	bool InitPolicy();
	bool LoadSnapshots(const FString& Dir);
	void Decide(double Now);
	void TickActions(float DeltaSeconds);
	void Sense(const FSlot& Slot, TArray<FPSPerception::FSighting>& Out) const;
	/** Spawn reset (§12.1); the caller batches Manager->ResetAgents. */
	void SpawnReset(FSlot& Slot, double Now);
	float TimeLeftFraction(const FSlot& Slot, double Now) const;
	/** Stage gating: an S0 snapshot never learned to move (§1.1). */
	bool CanMove() const { return Model.Stage != EAgentStage::S0; }
	void RefreshAllyLists();
	void RemoveSlot(int32 AgentId);
	FSlot* FindSlot(const UHealthComponent* Health);
	bool IsTargetActor(const AActor* Actor) const;
	void OnAgentDamaged(UHealthComponent* Victim, const FPSCombat::FDamageResult& Result, AActor* DamageInstigator);
	void OnAgentDied(UHealthComponent* Victim, AActor* Killer);
	void ApplyVisualToggles();
	double Now() const;

	UPROPERTY(VisibleAnywhere, Category="Agents") TObjectPtr<ULearningAgentsManager> Manager;
	UPROPERTY() TObjectPtr<ULearningAgentsInteractor> Interactor;
	UPROPERTY() TObjectPtr<ULearningAgentsPolicy> Policy;

	TMap<int32, FSlot> Slots;
	TArray<TWeakObjectPtr<UAgentTargetComponent>> Targets;
	TArray<FPSBots::FSoundEvent> Sounds;
	FRandomStream SoundRng;
	FAgentModelEntry Model;
	bool bReady = false;
	/** §12.3 (a): the player (re)spawned, so every agent starts a new virtual episode at the next decision. */
	bool bRestartAllPending = false;
	double Accumulator = 0.0;
	int32 NextTargetId = 0;
	int64 Decisions = 0;
	/** Per-agent actions performed, and how many asked for Crouch / Jump (policy output) vs reached the bean (after the mobility mask). */
	int64 ActionsPerformed = 0;
	int64 RawCrouch = 0;
	int64 AppliedCrouch = 0;
	int64 RawJump = 0;
	int64 AppliedJump = 0;
	double NextStatsLogTime = 10.0;
	int32 LastShowBean = -1;
	FString LoadError;
};
