#pragma once

#include "CoreMinimal.h"
#include "Character/BeanCharacter.h"
#include "EnemyBean.generated.h"

class UHealthComponent;
class USkeletalMeshComponent;
struct FShotResult;

/**
 * The game's enemy: FPS-Agent's bean with a skin. Gameplay is the vendored ABeanCharacter, untouched (contract §6): capsule 34/88,
 * eye 158, primitive hitboxes, run 540 / walk 300, the rifle and 100 + 50 health. What this class adds is all outside the contract:
 * - a skin on the bean's unused skeletal mesh slot (mannequin + rifle animation, about 176 cm), NoCollision, never moving a hitbox;
 * - the capsule on the AgentPawn object channel, so enemies never block each other (§8);
 * - possession by the bean AI controller, movement sub-stepped at 1/60 s (§5.4);
 * - death: decisions stop, hitboxes and capsule turn off, the skin ragdolls as a PhysicsBody that bullets and agents ignore (PLAN §9).
 * The brain is the Agent Director: this pawn registers itself when it starts.
 */
UCLASS()
class AGENTRUNTIME_API AEnemyBean : public ABeanCharacter
{
	GENERATED_BODY()

public:
	AEnemyBean(const FObjectInitializer& ObjectInitializer);

	bool IsAlive() const;
	/** agent.ShowBean / agent.ShowHitboxes. */
	void ApplyDebugVisuals();

	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Cosmetic rifle in the skin's right hand. */
	UPROPERTY(VisibleAnywhere, Category="Enemy") TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** Seconds a ragdoll stays before it is removed (keeps the physics budget bounded). */
	UPROPERTY(EditAnywhere, Category="Enemy") float CorpseLifeSeconds = 12.f;
	/** Push given to the ragdoll along the killing shot (cm/s, velocity change). */
	UPROPERTY(EditAnywhere, Category="Enemy") float DeathImpulse = 450.f;

private:
	void HandleDied(UHealthComponent* Victim, AActor* Killer);
	void HandleShot(const FShotResult& Shot);
	FDelegateHandle DiedHandle;
	FDelegateHandle ShotHandle;
	bool bRegistered = false;
};
