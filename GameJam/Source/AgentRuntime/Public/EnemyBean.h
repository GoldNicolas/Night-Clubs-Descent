#pragma once

#include "CoreMinimal.h"
#include "Character/BeanCharacter.h"
#include "Weapons/CombatMath.h"
#include "EnemyBean.generated.h"

class UHealthComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
struct FShotResult;
class AEnemyBean;

/** Every shot any enemy fires (presentation listeners: crowds, sound, VFX). Never feeds an agent's senses. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAnyEnemyShot, AEnemyBean* /*Shooter*/, const FShotResult& /*Shot*/);
/** Every enemy death (presentation listeners: blood, score). Never feeds an agent's senses. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAnyEnemyDied, AEnemyBean* /*Enemy*/);

/**
 * The game's enemy: FPS-Agent's bean with a skin. Gameplay is the vendored ABeanCharacter, untouched (contract §6): capsule 34/88,
 * eye 158, primitive hitboxes, run 540 / walk 300, the rifle and 100 + 50 health. What this class adds is all outside the contract:
 * - a skin on the bean's unused skeletal mesh slot (mannequin + rifle animation, about 176 cm), NoCollision, never moving a hitbox;
 * - the capsule on the AgentPawn object channel, so enemies never block each other (§8);
 * - possession by the bean AI controller, movement sub-stepped at 1/60 s (§5.4);
 * - animation on the skin only: gun bolt / reload clips, and on the body's upper-body slot a recoil per shot, the reload and hit
 *   reactions (settings Enemy Look); none of it touches the aim, the hitboxes or the agent's inputs;
 * - death: decisions stop, hitboxes and capsule turn off, the skin plays a directional death reaction and then ragdolls as a
 *   PhysicsBody that bullets and agents ignore (PLAN §9).
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
	/** The gun model being shown (static or skeletal), for presentation effects such as muzzle flashes. */
	USceneComponent* GetGunComponent() const;

	static FOnAnyEnemyShot OnAnyEnemyShot;
	static FOnAnyEnemyDied OnAnyEnemyDied;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Cosmetic rifle in the skin's right hand (a skeletal gun model). */
	UPROPERTY(VisibleAnywhere, Category="Enemy") TObjectPtr<USkeletalMeshComponent> WeaponMesh;
	/** The same, for a static gun model (Agent Runtime settings: EnemyWeapon). */
	UPROPERTY(VisibleAnywhere, Category="Enemy") TObjectPtr<UStaticMeshComponent> WeaponStatic;

	/** Seconds a ragdoll stays before it is removed (keeps the physics budget bounded). */
	UPROPERTY(EditAnywhere, Category="Enemy") float CorpseLifeSeconds = 12.f;
	/** Push given to the ragdoll along the killing shot (cm/s, velocity change). */
	UPROPERTY(EditAnywhere, Category="Enemy") float DeathImpulse = 450.f;

private:
	/** Skin (random from the settings, fitted to 176 cm) and gun model. Visual only. */
	void ApplyLook();
	void HandleDied(UHealthComponent* Victim, AActor* Killer);
	void HandleDamaged(UHealthComponent* Victim, const FPSCombat::FDamageResult& Result, AActor* DamageInstigator);
	void HandleShot(const FShotResult& Shot);
	/** A skeletal gun model's own animations (Agent Runtime settings), time-scaled to the bean rifle's cadence and reload. */
	void PlayGunAnim(class UAnimSequence* Anim, float Seconds);
	/** An upper-body clip on the body's slot (settings EnemyBodySlot); BusySeconds keeps the per-shot recoil off it meanwhile. */
	void PlayBodyAction(class UAnimSequence* Anim, float PlayRate, float BusySeconds);
	/** A clip from Clips (the skin's skeleton only), or null. */
	class UAnimSequence* PickClip(const TArray<FSoftObjectPath>& Clips) const;
	/** A death clip for a kill from Killer's side (settings EnemyDeathShotFrom*), or null. */
	class UAnimSequence* PickDeathAnim(const AActor* Killer) const;
	/** The skin goes limp: PhysicsBody, physics collision only, pushed along the killing shot. */
	void StartRagdoll(float Impulse);

	FDelegateHandle DiedHandle;
	FDelegateHandle DamagedHandle;
	FDelegateHandle ShotHandle;
	FTimerHandle RagdollTimer;
	FVector DeathPush = FVector::ZeroVector;
	double BodyBusyUntil = 0.0;
	bool bRegistered = false;
	bool bGunWasReloading = false;
	/** Visual gun kick per shot (decays); the gun model only, never the aim or the hitboxes. */
	float GunKick = 0.f;
	UPROPERTY() TObjectPtr<class UAnimSequence> GunFireAnim;
	UPROPERTY() TObjectPtr<class UAnimSequence> GunReloadAnim;
	UPROPERTY() TObjectPtr<class UAnimSequence> BodyReloadAnim;
	UPROPERTY() TObjectPtr<class UAnimSequence> BodyFireAnim;
};
