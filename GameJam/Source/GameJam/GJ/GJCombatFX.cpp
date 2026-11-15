#include "GJCombatFX.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GJGameSettings.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	TAutoConsoleVariable<int32> CVarTracers(TEXT("gj.Tracers"), 1, TEXT("1 = bullet tracers for every shot (player and enemies)."));
	TAutoConsoleVariable<int32> CVarImpactFX(TEXT("gj.ImpactFX"), 1, TEXT("1 = bullet holes and sparks where shots hit the level."));

	constexpr float TracerSpeed = 30000.f;   // cm/s: a visible streak, not the (instant) hitscan
	constexpr float StreakLength = 650.f;
	constexpr float StreakThickness = 2.4f;  // thinner vanishes under anti-aliasing past ~8 m
	constexpr float TracerIntensity = 60.f;  // emissive: blooms in daylight
	constexpr float BulletHoleSize = 6.f;
	constexpr float BulletHoleLife = 20.f;

	template <typename T>
	T* LoadFX(const FSoftObjectPath& Path)
	{
		return Path.IsValid() ? Cast<T>(Path.TryLoad()) : nullptr;
	}

	/** Level geometry: static and dynamic (this map's floors are movable BlockAllDynamic meshes). */
	FCollisionObjectQueryParams LevelSurfaces()
	{
		FCollisionObjectQueryParams Surfaces;
		Surfaces.AddObjectTypesToQuery(ECC_WorldStatic);
		Surfaces.AddObjectTypesToQuery(ECC_WorldDynamic);
		return Surfaces;
	}

	UNiagaraComponent* AttachFX(UNiagaraSystem* System, USceneComponent* Gun, const FTransform& Where, bool bFirstPerson)
	{
		if (!System || !Gun)
		{
			return nullptr;
		}
		UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Gun, NAME_None, Where.GetLocation(), Where.Rotator(),
			EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/true);
		if (FX && bFirstPerson)
		{
			FX->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson); // same space as the first-person gun
			FX->SetOnlyOwnerSee(true);
		}
		return FX;
	}
}

void GJCombatFX::PlayShot(UWorld* World, const FGJShotFX& Shot)
{
	if (!World || World->IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	const UGJGameSettings& S = UGJGameSettings::Get();

	// Muzzle flash and shell casing ride on the gun model.
	AttachFX(LoadFX<UNiagaraSystem>(S.MuzzleFlash), Shot.Gun, S.GunMuzzle, Shot.bFirstPerson);
	if (Shot.bEjectShell)
	{
		AttachFX(LoadFX<UNiagaraSystem>(S.ShellEject), Shot.Gun, S.GunEjectionPort, Shot.bFirstPerson);
	}

	if (CVarTracers.GetValueOnGameThread() != 0)
	{
		AGJTracer::Spawn(World, Shot.TracerStart, Shot.TracerEnd, LoadFX<UMaterialInterface>(S.TracerMaterial), Shot.TracerSkipCm);
	}

	if (Shot.bHitWorld && CVarImpactFX.GetValueOnGameThread() != 0)
	{
		const FVector N = Shot.ImpactNormal.GetSafeNormal();
		if (UMaterialInterface* Hole = LoadFX<UMaterialInterface>(S.BulletHoleDecal))
		{
			FRotator Facing = (-N).Rotation();
			Facing.Roll = FMath::FRandRange(0.f, 360.f);
			const float Size = BulletHoleSize * FMath::FRandRange(0.8f, 1.2f);
			if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(World, Hole, FVector(3.f, Size, Size), Shot.ImpactPoint, Facing, BulletHoleLife))
			{
				Decal->SetFadeOut(BulletHoleLife - 5.f, 5.f, /*DestroyOwnerAfterFade=*/false);
				Decal->SetFadeScreenSize(0.002f);
			}
		}
		if (UNiagaraSystem* Sparks = LoadFX<UNiagaraSystem>(S.ImpactSparks))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sparks, Shot.ImpactPoint + N * 2.f, N.Rotation());
		}
	}

	if (Shot.bHitCharacter && CVarImpactFX.GetValueOnGameThread() != 0)
	{
		// The exit spray carries on with the bullet: a wall within reach behind the target gets a splatter decal, and the floor
		// just behind it a spray of drops along the bullet (the decal again when no spray effect is set).
		const FVector Dir = Shot.ShotDirection.GetSafeNormal();
		const FCollisionObjectQueryParams Static = LevelSurfaces();
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(GJBloodSpray), false, Shot.HitActor);
		UMaterialInterface* Splatter = LoadFX<UMaterialInterface>(S.CharacterHitSplatter);
		auto SplatterDecal = [World, Splatter](const FHitResult& Surface)
		{
			FRotator Facing = (-Surface.ImpactNormal).Rotation();
			Facing.Roll = FMath::FRandRange(0.f, 360.f);
			const float Size = FMath::FRandRange(14.f, 24.f);
			if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(World, Splatter, FVector(4.f, Size, Size), Surface.ImpactPoint, Facing, 30.f))
			{
				Decal->SetFadeOut(25.f, 5.f, /*DestroyOwnerAfterFade=*/false);
				Decal->SetFadeScreenSize(0.002f);
			}
		};
		FHitResult Wall;
		const bool bWall = World->LineTraceSingleByObjectType(Wall, Shot.ImpactPoint, Shot.ImpactPoint + Dir * 180.f, Static, Params)
			&& FMath::Abs(Wall.ImpactNormal.Z) < 0.6f;
		if (bWall && Splatter)
		{
			SplatterDecal(Wall);
		}
		FHitResult Floor;
		const FVector Behind = Shot.ImpactPoint + FVector(Dir.X, Dir.Y, 0.f).GetSafeNormal() * 50.f;
		if (World->LineTraceSingleByObjectType(Floor, Behind, Behind - FVector(0.f, 0.f, 300.f), Static, Params) && Floor.ImpactNormal.Z > 0.7f)
		{
			if (UNiagaraSystem* Spray = LoadFX<UNiagaraSystem>(S.CharacterHitFX))
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Spray, Floor.ImpactPoint + Floor.ImpactNormal, FRotator(0.f, Dir.Rotation().Yaw, 0.f),
					FVector(S.CharacterHitFXScale));
			}
			else if (Splatter && !bWall)
			{
				SplatterDecal(Floor);
			}
		}
	}

	if (USoundBase* Sound = LoadFX<USoundBase>(S.GunshotSound))
	{
		const float Pitch = FMath::FRandRange(0.94f, 1.06f);
		if (Shot.bSound2D)
		{
			UGameplayStatics::PlaySound2D(World, Sound, Shot.SoundVolume, Pitch);
		}
		else
		{
			const FVector At = Shot.Gun ? Shot.Gun->GetComponentLocation() : Shot.TracerStart;
			UGameplayStatics::PlaySoundAtLocation(World, Sound, At, Shot.SoundVolume, Pitch, 0.f, LoadFX<USoundAttenuation>(S.GunshotAttenuation));
		}
	}
}

void GJCombatFX::PlayCorpseBlood(UWorld* World, const FVector& Body, const AActor* Corpse)
{
	const UGJGameSettings& S = UGJGameSettings::Get();
	UNiagaraSystem* Pool = World ? LoadFX<UNiagaraSystem>(S.CorpseBloodFX) : nullptr;
	if (!Pool || CVarImpactFX.GetValueOnGameThread() == 0)
	{
		return;
	}
	FHitResult Floor;
	if (World->LineTraceSingleByObjectType(Floor, Body + FVector(0.f, 0.f, 50.f), Body - FVector(0.f, 0.f, 250.f),
		LevelSurfaces(), FCollisionQueryParams(SCENE_QUERY_STAT(GJCorpseBlood), false, Corpse)) && Floor.ImpactNormal.Z > 0.7f)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Pool, Floor.ImpactPoint + Floor.ImpactNormal,
			FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), FVector(S.CorpseBloodFXScale));
	}
}

AGJTracer::AGJTracer()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Streak = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Streak"));
	RootComponent = Streak;
	if (Cylinder.Succeeded()) { Streak->SetStaticMesh(Cylinder.Object); }
	Streak->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Streak->SetGenerateOverlapEvents(false);
	Streak->SetCanEverAffectNavigation(false);
	Streak->SetCastShadow(false);
	Streak->SetMobility(EComponentMobility::Movable);
}

void AGJTracer::Spawn(UWorld* World, const FVector& Start, const FVector& End, UMaterialInterface* InMaterial, float SkipCm)
{
	const float Length = FVector::Dist(Start, End);
	SkipCm = FMath::Max(0.f, SkipCm);
	if (!World || !InMaterial || Length - SkipCm < 50.f)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGJTracer* Tracer = World->SpawnActor<AGJTracer>(AGJTracer::StaticClass(), Start, FRotator::ZeroRotator, Params);
	if (!Tracer)
	{
		return;
	}
	Tracer->From = Start;
	Tracer->Dir = (End - Start) / Length;
	Tracer->Total = Length;
	Tracer->Travelled = SkipCm;
	Tracer->MinTail = SkipCm;
	Tracer->Material = Tracer->Streak->CreateDynamicMaterialInstance(0, InMaterial);
	if (Tracer->Material)
	{
		Tracer->Material->SetScalarParameterValue(TEXT("Intensity"), TracerIntensity);
	}
	Tracer->Place(FMath::Min(SkipCm + StreakLength * 0.5f, Length), SkipCm); // first frame: a short streak leaving the muzzle
}

void AGJTracer::Place(float Head, float Tail)
{
	const float Len = FMath::Max(1.f, Head - Tail);
	const FVector Mid = From + Dir * (0.5f * (Head + Tail));
	// The engine cylinder is 100 cm tall along Z and 100 cm across.
	SetActorLocationAndRotation(Mid, FRotationMatrix::MakeFromZ(Dir).Rotator());
	SetActorScale3D(FVector(StreakThickness / 100.f, StreakThickness / 100.f, Len / 100.f));
	if (Material)
	{
		Material->SetScalarParameterValue(TEXT("Fade"), FMath::Clamp(1.f - 0.6f * (Tail / FMath::Max(Total, 1.f)), 0.f, 1.f));
	}
}

void AGJTracer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Travelled += TracerSpeed * DeltaSeconds;
	const float Head = FMath::Min(Travelled, Total);
	const float Tail = FMath::Max(MinTail, Travelled - StreakLength);
	if (Tail >= Total)
	{
		Destroy();
		return;
	}
	Place(Head, Tail);
}
