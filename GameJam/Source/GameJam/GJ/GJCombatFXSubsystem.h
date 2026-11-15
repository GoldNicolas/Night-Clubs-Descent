#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GJCombatFXSubsystem.generated.h"

class AEnemyBean;
struct FShotResult;

/** Enemy gunfire effects (muzzle flash, shell, tracer, impact, sound) from AEnemyBean::OnAnyEnemyShot, and a blood pool under
 *  each dead enemy once its body is down (AEnemyBean::OnAnyEnemyDied). Presentation only. */
UCLASS()
class GAMEJAM_API UGJCombatFXSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void OnEnemyShot(AEnemyBean* Shooter, const FShotResult& Shot);
	void OnEnemyDied(AEnemyBean* Enemy);
	FDelegateHandle EnemyShotHandle;
	FDelegateHandle EnemyDiedHandle;
};
