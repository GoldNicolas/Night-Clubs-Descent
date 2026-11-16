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

	/** Weapon feel: a shot was fired (gun kick-back plus a sharp extra view kick). */
	void OnRifleShot();
	/** The recoil punch currently added to the control rotation. The rifle aims bullets from the player's own aim (without it). */
	FRotator GetAimPunch() const { return AimPunchApplied; }
	/** Game time of death, or -1 while alive (the HUD darkens the screen from it). */
	double GetDeathTime() const { return DeathTime; }
	/** For UGJViewModifier: view-space camera offset, view rotation offset, the arms shift to take back out of the camera,
	 *  and a world-space camera offset (the fall on death). */
	void GetViewEffects(FVector& OutViewOffset, FRotator& OutViewRoll, FVector& OutArmsWorldShift, FVector& OutViewWorldOffset) const;
	/** How far the camera component currently rides off the true eye (it sits on the arms mesh, which bobs, kicks and rests
	 *  ArmsRestOffsetCm off the eye). Subtract it from the camera location to get the eye the bullets leave from. */
	FVector GetArmsWorldShift() const { return ArmsWorldShift; }

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	/** Stops the template's respawn after death (the front end ends the run instead: results screen). */
	void CancelRespawn() { GetWorldTimerManager().ClearTimer(RespawnTimer); }

	/** Look input, scaled by the menu's mouse sensitivity (FGJUserSettings). */
	virtual void DoAim(float Yaw, float Pitch) override;

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
	/** Difficulty knob (contract §13): scales damage taken from agents. */
	UPROPERTY(EditAnywhere, Category="Health", meta=(ClampMin=0, ClampMax=1)) float IncomingDamageScale = 0.5f;

	UPROPERTY(EditAnywhere, Category="Weapons") TSubclassOf<AShooterWeapon> DefaultWeaponClass;

	/** Weapon feel (presentation only; bullets keep the mirror rifle's exact spread and recoil). */
	UPROPERTY(EditAnywhere, Category="Weapon Feel") bool bWeaponFeel = true;
	/** Where the arms and gun rest relative to the eye, in view space (X forward, Y right, Z up). Framing only: the eye doesn't
	 *  move. (The near suit sleeve is handled by the first-person material, GJGameSettings::PlayerFirstPersonMaterial.) */
	UPROPERTY(EditAnywhere, Category="Weapon Feel", meta=(Units="cm")) FVector ArmsRestOffsetCm = FVector(0.f, 0.f, -1.5f);
	/** Gun swing while walking/running (figure eight), and the view's own bob. */
	UPROPERTY(EditAnywhere, Category="Weapon Feel", meta=(Units="cm")) float GunBobCm = 1.2f;
	UPROPERTY(EditAnywhere, Category="Weapon Feel", meta=(Units="cm")) float CameraBobCm = 0.45f;
	/** The gun lags behind fast turns: cm per degree/second of turn, up to SwayMaxCm. */
	UPROPERTY(EditAnywhere, Category="Weapon Feel") float SwayCmPerDegPerSec = 0.01f;
	UPROPERTY(EditAnywhere, Category="Weapon Feel", meta=(Units="cm")) float SwayMaxCm = 2.2f;
	/** Per shot: the gun jolts back, and the view kicks up on top of the recoil pattern (then springs back). */
	UPROPERTY(EditAnywhere, Category="Weapon Feel", meta=(Units="cm")) float KickBackCm = 1.8f;
	UPROPERTY(EditAnywhere, Category="Weapon Feel") float KickPitchDeg = 0.35f;
	/** Taking a full rifle hit rolls the view this far away from the shot, then it springs back. Roll only: the crosshair and
	 *  the aim stay true (no aim punch, so the player isn't weaker than the mirror rifle implies). */
	UPROPERTY(EditAnywhere, Category="Weapon Feel") float HitFlinchRollDeg = 2.5f;

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

	void UpdateWeaponFeel(float DeltaSeconds);
	void EnsureViewModifier();

	// Weapon feel state
	FVector ArmsBaseLocation = FVector::ZeroVector;
	bool bArmsBaseCaptured = false;
	float BobPhase = 0.f;
	float BobBlend = 0.f;
	FVector2D Sway = FVector2D::ZeroVector;
	FRotator LastAim = FRotator::ZeroRotator;
	bool bHasLastAim = false;
	float KickBack = 0.f;
	float KickBackVel = 0.f;
	float KickPitch = 0.f;
	float KickPitchVel = 0.f;
	float LandDip = 0.f;
	float LandDipVel = 0.f;
	float FallSpeed = 0.f;
	FVector2D PatternPunch = FVector2D::ZeroVector;
	FRotator AimPunchApplied = FRotator::ZeroRotator;
	FVector ViewOffset = FVector::ZeroVector;
	FRotator ViewRoll = FRotator::ZeroRotator;
	FVector ArmsWorldShift = FVector::ZeroVector;
	FVector ViewWorldOffset = FVector::ZeroVector;
	float HitRoll = 0.f;
	float HitRollVel = 0.f;
	double DeathTime = -1.0;
	float DeathDropCm = 0.f;

	float CurrentArmor = 0.f;
	double LastDamageTime = -1.0e9;
	FVector LastDamageSource = FVector::ZeroVector;
	bool bWalkHeld = false;
};
