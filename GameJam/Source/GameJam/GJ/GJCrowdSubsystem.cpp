#include "GJCrowdSubsystem.h"
#include "EnemyBean.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GJDancer.h"
#include "GJGameSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Weapons/RifleComponent.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDancers(TEXT("gj.Dancers"), 10, TEXT("Dancers in the club crowd (0 = none). Changing it respawns the crowd."));
	TAutoConsoleVariable<float> CVarDanceRadius(TEXT("gj.DanceRadius"), 1000.f, TEXT("Dancers spawn within this radius (cm) of the dance-floor anchor (the club's mirror ball) or the PlayerStart."));

	FAutoConsoleCommandWithWorld CmdClearDancers(TEXT("gj.ClearDancers"), TEXT("Remove every dancer (set gj.Dancers to spawn a new crowd)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UGJCrowdSubsystem* S = World ? World->GetSubsystem<UGJCrowdSubsystem>() : nullptr) { S->ClearDancers(); }
		}));
}

bool UGJCrowdSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UGJCrowdSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	EnemyShotHandle = AEnemyBean::OnAnyEnemyShot.AddUObject(this, &UGJCrowdSubsystem::OnEnemyShot);
}

void UGJCrowdSubsystem::Deinitialize()
{
	AEnemyBean::OnAnyEnemyShot.Remove(EnemyShotHandle);
	Super::Deinitialize();
}

TStatId UGJCrowdSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGJCrowdSubsystem, STATGROUP_Tickables);
}

bool UGJCrowdSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->HasBegunPlay() && !World->bIsTearingDown;
}

void UGJCrowdSubsystem::Tick(float DeltaTime)
{
	const int32 Wanted = FMath::Max(0, CVarDancers.GetValueOnGameThread());
	if (Wanted != SpawnedFor && UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		ClearDancers();
		SpawnDancers(Wanted);
		SpawnedFor = Wanted;
	}
	SpreadPanic();
}

void UGJCrowdSubsystem::SpreadPanic()
{
	// Panic is contagious: a calm dancer near a fleeing one runs from the same shooting.
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextSpreadTime)
	{
		return;
	}
	NextSpreadTime = Now + 0.2;
	const float Radius = UGJGameSettings::Get().PanicSpreadRadiusCm;
	TArray<AGJDancer*> Fleeing;
	TArray<AGJDancer*> Calm;
	for (TActorIterator<AGJDancer> It(GetWorld()); It; ++It)
	{
		(It->IsFleeing() ? Fleeing : Calm).Add(*It);
	}
	for (AGJDancer* Dancer : Calm)
	{
		for (const AGJDancer* Runner : Fleeing)
		{
			if (FVector::Dist(Dancer->GetActorLocation(), Runner->GetActorLocation()) <= Radius)
			{
				Dancer->Panic(Runner->GetPanicOrigin());
				break;
			}
		}
	}
}

void UGJCrowdSubsystem::ClearDancers()
{
	for (TActorIterator<AGJDancer> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	SpawnedFor = -1;
}

void UGJCrowdSubsystem::ReportGunshot(const FVector& Origin)
{
	const float Radius = UGJGameSettings::Get().PanicRadiusCm;
	for (TActorIterator<AGJDancer> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), Origin) <= Radius)
		{
			It->Panic(Origin);
		}
	}
}

void UGJCrowdSubsystem::ReportGunshot(const UObject* WorldContext, const FVector& Origin)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (UGJCrowdSubsystem* Crowd = World ? World->GetSubsystem<UGJCrowdSubsystem>() : nullptr)
	{
		Crowd->ReportGunshot(Origin);
	}
}

void UGJCrowdSubsystem::OnEnemyShot(AEnemyBean* Shooter, const FShotResult& Shot)
{
	if (Shooter && Shooter->GetWorld() == GetWorld())
	{
		ReportGunshot(Shooter->GetActorLocation());
	}
}

int32 UGJCrowdSubsystem::SpawnDancers(int32 Count)
{
	UWorld* World = GetWorld();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player || Count <= 0)
	{
		return 0;
	}
	// The crowd gathers around the level's dance-floor anchor (settings CrowdAnchorClass: the club's mirror ball), or the
	// PlayerStart on a level without one.
	FVector Center = Player->GetActorLocation();
	bool bAnchored = false;
	if (UClass* AnchorClass = UGJGameSettings::Get().CrowdAnchorClass.TryLoadClass<AActor>())
	{
		for (TActorIterator<AActor> It(World, AnchorClass); It; ++It)
		{
			Center = It->GetActorLocation();
			bAnchored = true;
			break;
		}
	}
	for (TActorIterator<APlayerStart> It(World); It && !bAnchored; ++It)
	{
		Center = It->GetActorLocation();
		break;
	}
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(32.f, 86.f);
	const float Radius = FMath::Max(300.f, CVarDanceRadius.GetValueOnGameThread());
	FRandomStream Rng(static_cast<int32>(FPlatformTime::Cycles()));
	TArray<FVector> Placed;
	int32 Spawned = 0;
	for (int32 Attempt = 0; Attempt < Count * 30 && Spawned < Count; ++Attempt)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float R = Radius * FMath::Sqrt(Rng.FRand());
		const FVector Probe(Center.X + R * FMath::Cos(A), Center.Y + R * FMath::Sin(A), Center.Z);
		// Level geometry only (WorldStatic / WorldDynamic): ceilings on the Ceiling object channel are skipped, never the roof.
		FHitResult Floor;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(GJDancerFloor), false);
		Q.AddIgnoredActor(Player);
		FCollisionObjectQueryParams LevelGeometry;
		LevelGeometry.AddObjectTypesToQuery(ECC_WorldStatic);
		LevelGeometry.AddObjectTypesToQuery(ECC_WorldDynamic);
		if (!World->LineTraceSingleByObjectType(Floor, Probe + FVector(0, 0, 600.f), Probe - FVector(0, 0, 1500.f), LevelGeometry, Q)
			|| Floor.ImpactNormal.Z < 0.75f)
		{
			continue;
		}
		const FVector Spot = Floor.ImpactPoint + FVector(0, 0, 88.f);
		if (FVector::Dist2D(Spot, Player->GetActorLocation()) < 250.f) { continue; }
		if (FMath::Abs(Spot.Z - Player->GetActorLocation().Z) > 400.f) { continue; } // stay off wall tops
		if (Placed.ContainsByPredicate([&Spot](const FVector& P) { return FVector::Dist2D(P, Spot) < 110.f; })) { continue; }
		if (World->OverlapBlockingTestByProfile(Spot, FQuat::Identity, TEXT("Pawn"), Capsule)) { continue; }
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
		if (World->SpawnActor<AGJDancer>(AGJDancer::StaticClass(), Spot, FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), Params))
		{
			Placed.Add(Spot);
			++Spawned;
		}
	}
	return Spawned;
}
