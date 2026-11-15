#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Common/FPSAgentTypes.h"
#include "AgentTargetComponent.generated.h"

/** Applies agent rifle damage to the owner. Returns true when the hit killed it. */
DECLARE_DELEGATE_RetVal_ThreeParams(bool, FAgentHitHandler, float /*Damage*/, EHitZone /*Zone*/, AActor* /*DamageInstigator*/);

/**
 * Makes its owner (the player) an enemy of the agents (contract §7). It is the only thing agents know about the player:
 * - what §7.1 asks for: head and chest points (the hitbox centres), velocity, facing, crouched, alive, a stable id;
 * - the hit zones agent bullets can hit (§7.3): the owner registers its head/body/legs primitives;
 * - the sound events (§7.4): footsteps while running audibly, and one gunshot per bullet (ReportGunshot).
 * Registered with the Agent Director while it exists. Agents never see anything about the player that isn't here.
 */
UCLASS(ClassGroup=(AgentRuntime), meta=(BlueprintSpawnableComponent))
class AGENTRUNTIME_API UAgentTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAgentTargetComponent();

	/** The primitives agent bullets hit (each blocks only the Weapon channel). Head and Body centres are the Head and Chest points. */
	void SetHitboxes(UPrimitiveComponent* InHead, UPrimitiveComponent* InBody, UPrimitiveComponent* InLegs);
	EHitZone GetHitZone(const UPrimitiveComponent* Component) const;

	/** Agent rifle damage (vendored rifle's target adapter). Returns true when it killed. */
	bool ApplyAgentHit(float Damage, EHitZone Zone, AActor* DamageInstigator);
	FAgentHitHandler HitHandler;

	FVector GetHeadLocation() const;
	FVector GetChestLocation() const;
	FVector GetTargetVelocity() const;
	/** The body's facing, yaw only, normalized 2D. */
	FVector GetForward2D() const;
	bool IsTargetCrouched() const;
	bool IsTargetAlive() const { return bAlive; }
	int32 GetTargetId() const { return TargetId; }

	/** Dead targets drop out of every agent's tracks at once. */
	void SetAlive(bool bInAlive) { bAlive = bInAlive; }
	/** The owner's silent-move mode (walk key). Walking, crouching and standing make no footsteps. */
	void SetWalking(bool bInWalking) { bWalking = bInWalking; }
	bool IsWalking() const { return bWalking; }
	/** True while the owner's movement makes footstep sounds agents can hear (the §7.4 rule). For the HUD. */
	bool IsMakingFootsteps() const;

	/** One event per bullet fired, hits and misses alike. The director feeds gunshots to S2+ snapshots only (§1.1). */
	void ReportGunshot();

	/** Hook (cloak perk): agents treat the target as not visible, an explicit extra test of §7.2 step 3. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Agents") bool bHiddenFromAgents = false;
	/** Hook (cloak perk): no footstep or gunshot events reach agents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Agents") bool bSilentToAgents = false;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class AAgentDirector;

	TWeakObjectPtr<UPrimitiveComponent> HeadBox;
	TWeakObjectPtr<UPrimitiveComponent> BodyBox;
	TWeakObjectPtr<UPrimitiveComponent> LegsBox;
	bool bAlive = true;
	bool bWalking = false;
	int32 TargetId = INDEX_NONE;
	double NextFootstepTime = 0.0;
};
