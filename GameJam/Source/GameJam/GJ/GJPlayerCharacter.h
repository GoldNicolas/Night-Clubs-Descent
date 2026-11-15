#pragma once

#include "CoreMinimal.h"
#include "ShooterCharacter.h"
#include "Common/FPSAgentTypes.h"
#include "GJPlayerCharacter.generated.h"

class AGJRifle;
class UAgentTargetComponent;
class UBoxComponent;
class UInputAction;
class UInputMappingContext;
class USphereComponent;

/**
 * The player: the template's first-person ShooterCharacter (arms, camera, weapon pipeline, UI), fitted to the FPS-Agent contract's
 * player envelope (§7) and the differences the user chose:
 * - body: capsule 34 / 88 (crouched 60), so the head sits where agents learned to aim (158 cm standing, 102 crouched);
 * - hitboxes: the bean's exact primitive set (head sphere, body box, legs box) from the same layout function; meshes ignore bullets;
 * - movement: the bean's numbers (run 540, walk 300, crouch 190, accel 4000, jump apex ~90 cm) plus a walk key and crouch:
 *   walking and crouching are silent to agents, running above 315 cm/s makes footsteps they hear within 15 m;
 * - toughness: 100 health + 100 armor (armor absorbs first), health regenerates after a few seconds without damage;
 * - weapon: the mirror rifle (AGJRifle);
 * - UAgentTargetComponent: everything agents may know about the player, nothing more.
 */
UCLASS(abstract)
class GAMEJAM_API AGJPlayerCharacter : public AShooterCharacter
{
	GENERATED_BODY()

public:
	AGJPlayerCharacter();

	UAgentTargetComponent* GetAgentTarget() const { return AgentTarget; }
	AGJRifle* GetRifle() const;
	float GetHealth() const { return CurrentHP; }
	float GetMaxHealth() const { return MaxHP; }
	float GetArmor() const { return CurrentArmor; }
	float GetMaxArmor() const { return MaxArmor; }
	bool IsWalkHeld() const { return bWalkHeld; }
	/** Game time of the last damage taken and where it came from (HUD damage indicator). */
	double GetLastDamageTime() const { return LastDamageTime; }
	FVector GetLastDamageSource() const { return LastDamageSource; }

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	virtual void NotifyControllerChanged() override;

	UPROPERTY(VisibleAnywhere, Category="Agents") TObjectPtr<USphereComponent> HeadHitbox;
	UPROPERTY(VisibleAnywhere, Category="Agents") TObjectPtr<UBoxComponent> BodyHitbox;
	UPROPERTY(VisibleAnywhere, Category="Agents") TObjectPtr<UBoxComponent> LegsHitbox;
	UPROPERTY(VisibleAnywhere, Category="Agents") TObjectPtr<UAgentTargetComponent> AgentTarget;

	UPROPERTY(EditAnywhere, Category="Health") float PlayerMaxHealth = 100.f;
	UPROPERTY(EditAnywhere, Category="Health") float MaxArmor = 100.f;
	/** Fraction of each hit armor takes while it lasts (1 = armor first, like the bean). */
	UPROPERTY(EditAnywhere, Category="Health", meta=(ClampMin=0, ClampMax=1)) float ArmorAbsorbFraction = 1.f;
	UPROPERTY(EditAnywhere, Category="Health", meta=(Units="s")) float HealthRegenDelay = 4.f;
	UPROPERTY(EditAnywhere, Category="Health") float HealthRegenPerSecond = 25.f;
	/** Difficulty knob (contract §13): scales damage taken from agents. 1 = the agent rifle's real damage. */
	UPROPERTY(EditAnywhere, Category="Health") float IncomingDamageScale = 1.f;

	UPROPERTY(EditAnywhere, Category="Weapons") TSubclassOf<AShooterWeapon> DefaultWeaponClass;

private:
	bool HandleAgentHit(float Damage, EHitZone Zone, AActor* DamageInstigator);
	void GiveDefaultWeapon();
	void AddPlayerInputContext();
	void RemovePlayerInputContext();
	void ApplyPlayerDamage(float Damage, AActor* DamageInstigator);
	void ApplyBodyLayout();
	void ApplyMovementTuning();
	void OnWalkStarted();
	void OnWalkEnded();
	void OnCrouchStarted();
	void OnCrouchEnded();
	void OnReload();

	UPROPERTY() TObjectPtr<UInputMappingContext> AgentRulesContext;
	UPROPERTY() TObjectPtr<UInputAction> WalkAction;
	UPROPERTY() TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY() TObjectPtr<UInputAction> ReloadAction;

	float CurrentArmor = 0.f;
	double LastDamageTime = -1.0e9;
	FVector LastDamageSource = FVector::ZeroVector;
	bool bWalkHeld = false;
};
