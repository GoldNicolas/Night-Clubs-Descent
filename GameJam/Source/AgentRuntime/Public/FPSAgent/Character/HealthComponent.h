#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapons/CombatMath.h"
#include "HealthComponent.generated.h"

class UHealthComponent;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnBeanDamaged, UHealthComponent* /*Victim*/, const FPSCombat::FDamageResult& /*Result*/, AActor* /*DamageInstigator*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBeanDied, UHealthComponent* /*Victim*/, AActor* /*Killer*/);

/** Health + armor pool. Tuning: UFPSAgentSettings::Health. */
UCLASS(ClassGroup=(FPSAgent), meta=(BlueprintSpawnableComponent))
class AGENTRUNTIME_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	void ResetToFull();
	/** Applies a hit; ignored (no events) when already dead. */
	FPSCombat::FDamageResult ApplyHit(float Damage, AActor* DamageInstigator);

	float GetHealth() const { return Health; }
	float GetArmor() const { return Armor; }
	bool IsDead() const { return Health <= 0.f; }

	FOnBeanDamaged OnDamaged;
	FOnBeanDied OnDied;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleInstanceOnly, Category="Health") float Health = 0.f;
	UPROPERTY(VisibleInstanceOnly, Category="Health") float Armor = 0.f;
};
