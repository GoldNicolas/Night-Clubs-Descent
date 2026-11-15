#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Common/FPSAgentTypes.h"
#include "BeanCharacter.generated.h"

class UBoxComponent;
class UCameraComponent;
class UHealthComponent;
class URifleComponent;
class USphereComponent;
class UStaticMeshComponent;

/** The bean: capsule movement body + head/body/legs hitboxes + rifle + health. Gameplay is render-free (works under -nullrhi). */
UCLASS()
class AGENTRUNTIME_API ABeanCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABeanCharacter(const FObjectInitializer& ObjectInitializer);

	UHealthComponent* GetHealth() const { return Health; }
	URifleComponent* GetRifle() const { return Rifle; }
	/** Zone of one of this bean's hitbox components, or None. */
	EHitZone GetHitZone(const UPrimitiveComponent* Component) const;
	UPrimitiveComponent* GetHitbox(EHitZone Zone) const;
	float GetHorizontalSpeed() const;
	bool IsAirborne() const;
	void SetWalking(bool bWalk);
	bool IsWalking() const { return bWalking; }
	/** Movement speed multiplier for run, walk and crouch (scripted bots: the stage's BotSpeedScale). */
	void SetSpeedScale(float Scale);
	/** Crouch or stand up now, keeping the feet on the floor (episode starts: Crouch()/UnCrouch() would wait for the next movement tick). */
	void ApplyCrouchNow(bool bCrouch);
	void SetHitboxesVisible(bool bVisible);
	/** Dead beans are hidden, have no collision, and neither move nor shoot. */
	void SetDeadState(bool bDead);

	virtual void BeginPlay() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

protected:
	void ApplyMovementTuning();
	void ApplyBodyLayout(float CapsuleHalfHeight);

	UPROPERTY(VisibleAnywhere, Category="Bean") TObjectPtr<USphereComponent> HeadHitbox;
	UPROPERTY(VisibleAnywhere, Category="Bean") TObjectPtr<UBoxComponent> BodyHitbox;
	UPROPERTY(VisibleAnywhere, Category="Bean") TObjectPtr<UBoxComponent> LegsHitbox;
	UPROPERTY(VisibleAnywhere, Category="Bean") TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY(VisibleAnywhere, Category="Bean") TObjectPtr<UStaticMeshComponent> TopMesh;
	UPROPERTY(VisibleAnywhere, Category="Bean") TObjectPtr<UStaticMeshComponent> BottomMesh;
	UPROPERTY(VisibleAnywhere, Category="Bean") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, Category="Bean") TObjectPtr<UHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, Category="Bean") TObjectPtr<URifleComponent> Rifle;

private:
	bool bWalking = false;
	float SpeedScale = 1.f;
};
