#include "Character/HealthComponent.h"
#include "Common/FPSAgentSettings.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetToFull();
}

void UHealthComponent::ResetToFull()
{
	const FHealthTuning& T = UFPSAgentSettings::Get().Health;
	Health = T.MaxHealth;
	Armor = T.MaxArmor;
}

FPSCombat::FDamageResult UHealthComponent::ApplyHit(float Damage, AActor* DamageInstigator)
{
	const bool bWasAlive = !IsDead();
	const FPSCombat::FDamageResult R = FPSCombat::ApplyDamage(Health, Armor, Damage, UFPSAgentSettings::Get().Health.ArmorAbsorbFraction);
	Health = R.Health;
	Armor = R.Armor;
	if (bWasAlive && (R.DamageToHealth > 0.f || R.DamageToArmor > 0.f))
	{
		OnDamaged.Broadcast(this, R, DamageInstigator);
	}
	if (bWasAlive && R.bKilled)
	{
		OnDied.Broadcast(this, DamageInstigator);
	}
	return R;
}
