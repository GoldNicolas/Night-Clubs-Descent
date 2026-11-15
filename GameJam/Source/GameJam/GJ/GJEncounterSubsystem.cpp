#include "GJEncounterSubsystem.h"
#include "AgentRuntime.h"
#include "Common/FPSAgentSettings.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnemyBean.h"
#include "GJMapGenerator.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"

namespace
{
	TAutoConsoleVariable<int32> CVarEnemies(TEXT("gj.Enemies"), 2, TEXT("Enemies the testing ground keeps alive (0 = none)."));
	TAutoConsoleVariable<float> CVarSpawnRadius(TEXT("gj.SpawnRadius"), 1700.f, TEXT("Enemies spawn within this radius (cm) of the PlayerStart."));
	TAutoConsoleVariable<float> CVarSpawnInterval(TEXT("gj.SpawnInterval"), 2.5f, TEXT("Seconds between enemy arrivals."));

	FAutoConsoleCommandWithWorld CmdKillEnemies(TEXT("gj.KillEnemies"), TEXT("Destroy every enemy (new ones arrive per gj.Enemies)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UGJEncounterSubsystem* S = World ? World->GetSubsystem<UGJEncounterSubsystem>() : nullptr) { S->KillAll(); }
		}));

	constexpr float MinPlayerDistance = 500.f;  // contract §12.5
	constexpr float MinAgentDistance = 400.f;   // contract §12.5 / §8.5
	constexpr float MaxPlayerDistance = 4000.f; // engagements within ~40 m (§7.5)
	constexpr float MaxWalkDistanceM = 60.f;    // procedural maps: reachable within this walk (the 40 m straight-line rule still applies)
}

bool UGJEncounterSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId UGJEncounterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGJEncounterSubsystem, STATGROUP_Tickables);
}

bool UGJEncounterSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->HasBegunPlay() && !World->bIsTearingDown;
}

int32 UGJEncounterSubsystem::CountAliveEnemies() const
{
	int32 N = 0;
	for (TActorIterator<AEnemyBean> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive()) { ++N; }
	}
	return N;
}

void UGJEncounterSubsystem::KillAll()
{
	for (TActorIterator<AEnemyBean> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	NextSpawnTime = GetWorld()->GetTimeSeconds() + CVarSpawnInterval.GetValueOnGameThread();
}

void UGJEncounterSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Now < NextSpawnTime || CountAliveEnemies() >= CVarEnemies.GetValueOnGameThread())
	{
		return;
	}
	NextSpawnTime = Now + FMath::Max(0.1f, CVarSpawnInterval.GetValueOnGameThread());
	if (!TrySpawnOne())
	{
		UE_LOG(LogAgentRuntime, Verbose, TEXT("Encounter: no valid spawn point this round"));
	}
}

bool UGJEncounterSubsystem::TrySpawnOne()
{
	UWorld* World = GetWorld();
	const APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	const APawn* Player = PC ? PC->GetPawn() : nullptr;
	if (!Player)
	{
		return false;
	}
	FVector Center = Player->GetActorLocation();
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Center = It->GetActorLocation();
		break;
	}

	const FBeanBody& Body = UFPSAgentSettings::Get().Body;
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Body.CapsuleRadius, Body.StandingHalfHeight);
	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector ViewDir = ViewRot.Vector().GetSafeNormal2D();

	TArray<FVector> Others;
	for (TActorIterator<AEnemyBean> It(World); It; ++It)
	{
		if (It->IsAlive()) { Others.Add(It->GetActorLocation()); }
	}

	FRandomStream Rng(static_cast<int32>(FPlatformTime::Cycles()));
	const float Radius = FMath::Max(300.f, CVarSpawnRadius.GetValueOnGameThread());

	// On a procedural map, probe floor tiles the player can walk to (MAPGEN.md): enemies never spawn in sealed-off pockets.
	TArray<FVector> MapTiles;
	if (const AGJMapGenerator* Map = AGJMapGenerator::Find(World); Map && Map->HasLayout())
	{
		Map->GetSpawnCandidates(Player->GetActorLocation(), MinPlayerDistance / 100.f, MaxWalkDistanceM, MapTiles);
		if (MapTiles.IsEmpty())
		{
			return false;
		}
	}

	// On a hand-built level, spawn at its enemy spawn markers: TargetPoints tagged EnemySpawn, which the map places in every
	// room at least 4 m apart (docs/MAP_DESIGN.md §3.5). The rules below still pick one >= 5 m from the player and out of view.
	TArray<FVector> Markers;
	if (MapTiles.IsEmpty())
	{
		for (TActorIterator<ATargetPoint> It(World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("EnemySpawn")))
			{
				Markers.Add(It->GetActorLocation());
			}
		}
	}

	bool bFound = false;
	float BestScore = -1.e9f;
	FVector Best = FVector::ZeroVector;
	for (int32 Attempt = 0; Attempt < 48; ++Attempt)
	{
		FVector Probe;
		if (!MapTiles.IsEmpty())
		{
			// A random point on a reachable tile, kept clear of the tile edge (walls and cover are whole tiles).
			const FVector& Tile = MapTiles[Rng.RandRange(0, MapTiles.Num() - 1)];
			Probe = Tile + FVector(Rng.FRandRange(-120.f, 120.f), Rng.FRandRange(-120.f, 120.f), 0.f);
		}
		else if (!Markers.IsEmpty())
		{
			Probe = Markers[Rng.RandRange(0, Markers.Num() - 1)];
		}
		else
		{
			const float A = Rng.FRandRange(0.f, 2.f * PI);
			const float R = Radius * FMath::Sqrt(Rng.FRand());
			Probe = FVector(Center.X + R * FMath::Cos(A), Center.Y + R * FMath::Sin(A), Center.Z);
		}

		// Floor below: the first walkable surface under the probe. Level geometry only (WorldStatic / WorldDynamic, what the
		// agents see): ceilings on the Ceiling object channel are skipped, so the probe lands on the floor under them, not the roof.
		FHitResult Floor;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(GJSpawnFloor), false);
		Q.AddIgnoredActor(Player);
		FCollisionObjectQueryParams LevelGeometry;
		LevelGeometry.AddObjectTypesToQuery(ECC_WorldStatic);
		LevelGeometry.AddObjectTypesToQuery(ECC_WorldDynamic);
		if (!World->LineTraceSingleByObjectType(Floor, Probe + FVector(0, 0, 600.f), Probe - FVector(0, 0, 1500.f), LevelGeometry, Q)
			|| Floor.ImpactNormal.Z < 0.75f)
		{
			continue;
		}
		const FVector Spot = Floor.ImpactPoint + FVector(0, 0, Body.StandingHalfHeight + 4.f);
		const float PlayerDist = FVector::Dist2D(Spot, Player->GetActorLocation());
		if (PlayerDist < MinPlayerDistance || PlayerDist > MaxPlayerDistance) { continue; }
		if (Others.ContainsByPredicate([&Spot](const FVector& O) { return FVector::Dist2D(O, Spot) < MinAgentDistance; })) { continue; }
		if (World->OverlapBlockingTestByProfile(Spot, FQuat::Identity, TEXT("Pawn"), Capsule)) { continue; }

		// Prefer spots out of the player's view (behind them or behind geometry), and on the player's floor level.
		const FVector To = (Spot - ViewLoc).GetSafeNormal2D();
		const bool bInView = FVector::DotProduct(To, ViewDir) > FMath::Cos(FMath::DegreesToRadians(55.f));
		FHitResult Block;
		const bool bOccluded = World->LineTraceSingleByChannel(Block, ViewLoc, Spot + FVector(0, 0, 70.f), ECC_Visibility, Q);
		float Score = Rng.FRand();
		if (!bInView || bOccluded) { Score += 2.f; }
		Score -= FMath::Abs(Spot.Z - Player->GetActorLocation().Z) / 200.f;
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Spot;
			bFound = true;
		}
	}
	if (!bFound)
	{
		return false;
	}
	// Facing away from the player, standing, pitch 0: the closest thing to how training episodes began.
	const FRotator Facing(0.f, (Best - Player->GetActorLocation()).GetSafeNormal2D().Rotation().Yaw, 0.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	const AEnemyBean* Enemy = World->SpawnActor<AEnemyBean>(AEnemyBean::StaticClass(), Best, Facing, Params);
	return Enemy != nullptr;
}
