#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJCombatFX.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

/** One shot's presentation: what GJCombatFX::PlayShot draws and plays. */
struct FGJShotFX
{
	/** The gun model the muzzle flash and shell casing attach to (GameJam settings GunMuzzle / GunEjectionPort are in its space). */
	USceneComponent* Gun = nullptr;
	/** The gun is a first-person primitive: attached effects render in the same first-person space and only for the owner. */
	bool bFirstPerson = false;
	bool bEjectShell = false;
	/** Tracer, drawn from the start to the end point. The first TracerSkipCm stay dark: the player's own tracer would otherwise
	 *  start as a thick beam right under the camera. */
	FVector TracerStart = FVector::ZeroVector;
	FVector TracerEnd = FVector::ZeroVector;
	float TracerSkipCm = 0.f;
	/** The bullet stopped on level geometry: bullet hole + sparks. */
	bool bHitWorld = false;
	/** The bullet hit a character (an enemy): hit effect + a splatter on the surface behind it. */
	bool bHitCharacter = false;
	/** The character hit (the blood traces look past it). */
	const AActor* HitActor = nullptr;
	FVector ImpactPoint = FVector::ZeroVector;
	FVector ImpactNormal = FVector::UpVector;
	FVector ShotDirection = FVector::ForwardVector;
	/** Gunshot audio: 2D for the player's own gun, otherwise at the muzzle with distance falloff. */
	bool bSound2D = false;
	float SoundVolume = 1.f;
};

/**
 * Weapon effects for the player's and the enemies' shots (presentation only). Nothing here has collision, so it never blocks
 * bullets or agent perception (contract §11: VFX, decals and debris stay out of WorldStatic / WorldDynamic queries).
 * Console: gj.Tracers 0/1, gj.ImpactFX 0/1.
 */
namespace GJCombatFX
{
	GAMEJAM_API void PlayShot(UWorld* World, const FGJShotFX& Shot);
	/** A blood pool on the floor under a body (settings CorpseBloodFX); Corpse is ignored by the floor trace. */
	GAMEJAM_API void PlayCorpseBlood(UWorld* World, const FVector& Body, const AActor* Corpse);
}

/** A bright bullet streak flying from muzzle to impact (a stretched unlit additive cylinder), then gone. */
UCLASS(NotPlaceable, Transient)
class GAMEJAM_API AGJTracer : public AActor
{
	GENERATED_BODY()

public:
	AGJTracer();
	static void Spawn(UWorld* World, const FVector& Start, const FVector& End, UMaterialInterface* Material, float SkipCm = 0.f);
	virtual void Tick(float DeltaSeconds) override;

private:
	void Place(float Head, float Tail);

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Streak;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
	FVector From = FVector::ZeroVector;
	FVector Dir = FVector::ForwardVector;
	float Total = 0.f;
	float Travelled = 0.f;
	float MinTail = 0.f;
};
