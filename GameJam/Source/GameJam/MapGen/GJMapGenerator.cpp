#include "GJMapGenerator.h"
#include "GameJam.h"
#include "GJEncounterSubsystem.h"
#include "EnemyBean.h"
#include "Common/FPSAgentTypes.h"
#include "TimerManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float FloorThicknessCm = 100.f;
	constexpr float PlayerStartHalfHeight = 92.f; // APlayerStart's capsule

	GJMap::EFamily ToCore(EGJMapFamily Family)
	{
		return Family == EGJMapFamily::Graph ? GJMap::EFamily::Graph : GJMap::EFamily::Wfc;
	}

	FString ToFString(const std::string& S)
	{
		return FString(UTF8_TO_TCHAR(S.c_str()));
	}

	int32 RandomSeed()
	{
		return static_cast<int32>((FPlatformTime::Cycles64() * 0x9E3779B97F4A7C15ull >> 33) % 1000000ull);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdMap(TEXT("gj.Map"),
		TEXT("Rebuild the procedural map: gj.Map <graph|wfc> <level 1-5> [seed]. Enemies are removed and the player moves to the new start."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AGJMapGenerator* Map = AGJMapGenerator::Find(World);
			if (!Map)
			{
				UE_LOG(LogGameJam, Warning, TEXT("gj.Map: this level has no GJMapGenerator"));
				return;
			}
			EGJMapFamily Family = Map->Family;
			if (Args.Num() > 0)
			{
				if (Args[0].Equals(TEXT("graph"), ESearchCase::IgnoreCase)) { Family = EGJMapFamily::Graph; }
				else if (Args[0].Equals(TEXT("wfc"), ESearchCase::IgnoreCase)) { Family = EGJMapFamily::Wfc; }
				else { UE_LOG(LogGameJam, Warning, TEXT("gj.Map: family must be graph or wfc")); return; }
			}
			const float Level = Args.Num() > 1 ? FCString::Atof(*Args[1]) : Map->Level;
			const int32 Seed = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : RandomSeed();
			Map->Regenerate(Family, Level, Seed);
		}));

	FAutoConsoleCommandWithWorld CmdMapCheck(TEXT("gj.MapCheck"),
		TEXT("Check the built map against its grid (traces, player, enemies) and log PASS / FAIL."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const AGJMapGenerator* Map = AGJMapGenerator::Find(World)) { Map->RunSelfCheck(false); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdMapSelfTest(TEXT("gj.MapSelfTest"),
		TEXT("gj.MapSelfTest [delay s] [quit]: check this map after the delay, rebuild it as the other family, check again; 'quit' exits (code 1 on failure)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AGJMapGenerator* Map = AGJMapGenerator::Find(World))
			{
				Map->StartSelfTest(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 10.f, Args.Contains(TEXT("quit")));
			}
		}));

	FAutoConsoleCommandWithWorld CmdMapInfo(TEXT("gj.MapInfo"), TEXT("Print the current procedural map's family, level, seed and metrics."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const AGJMapGenerator* Map = AGJMapGenerator::Find(World))
			{
				UE_LOG(LogGameJam, Display, TEXT("%s"), *Map->GetDescription());
			}
		}));
}

AGJMapGenerator::AGJMapGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Visible meshes only: all collision is the merged boxes built in BuildGeometry.
	auto MakeVisual = [this](const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Visual = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Visual->SetupAttachment(Root);
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetCanEverAffectNavigation(false);
		return Visual;
	};
	FloorMeshes = MakeVisual(TEXT("FloorMeshes"));
	WallMeshes = MakeVisual(TEXT("WallMeshes"));
	LowCoverMeshes = MakeVisual(TEXT("LowCoverMeshes"));
	HighCoverMeshes = MakeVisual(TEXT("HighCoverMeshes"));

	// Template blockout assets (LevelPrototyping); swap them for the tunnel/club art kit later.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FloorMat(TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WallMat(TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark.MI_PrototypeGrid_TopDark"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CoverMat(TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_02.MI_PrototypeGrid_Gray_02"));
	BoxMesh = Cube.Object;
	FloorMaterial = FloorMat.Object;
	WallMaterial = WallMat.Object;
	LowCoverMaterial = CoverMat.Object;
	HighCoverMaterial = CoverMat.Object;
}

AGJMapGenerator* AGJMapGenerator::Find(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) { return nullptr; }
	for (TActorIterator<AGJMapGenerator> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AGJMapGenerator::BeginPlay()
{
	Super::BeginPlay();
	if (!GetWorld() || !GetWorld()->IsGameWorld()) { return; }
	if (bRandomSeedOnPlay) { Seed = RandomSeed(); }
	Generate();
	if (bHasLayout)
	{
		PlacePlayers();
		if (bWriteMapFile) { WriteMapFile(); }
	}
}

void AGJMapGenerator::Generate()
{
	if (BuildLayout())
	{
		BuildGeometry();
	}
}

void AGJMapGenerator::GenerateNewSeed()
{
	Seed = RandomSeed();
	Generate();
}

bool AGJMapGenerator::Regenerate(EGJMapFamily InFamily, float InLevel, int32 InSeed)
{
	Family = InFamily;
	Level = FMath::Clamp(InLevel, 1.f, 5.f);
	Seed = InSeed;
	if (UWorld* World = GetWorld())
	{
		if (UGJEncounterSubsystem* Encounter = World->GetSubsystem<UGJEncounterSubsystem>()) { Encounter->KillAll(); }
	}
	Generate();
	if (!bHasLayout) { return false; }
	PlacePlayers();
	if (bWriteMapFile) { WriteMapFile(); }
	return true;
}

bool AGJMapGenerator::BuildLayout()
{
	bHasLayout = false;
	// A generator can fail on a seed (e.g. a WFC contradiction every try); the next seed is used then, and logged.
	const uint32 FirstSeed = static_cast<uint32>(FMath::Max(0, Seed));
	for (uint32 Skip = 0; Skip < 8 && !bHasLayout; ++Skip)
	{
		bHasLayout = GJMap::MakeLayout(ToCore(Family), Level, FirstSeed + Skip, Layout);
	}
	if (!bHasLayout)
	{
		UE_LOG(LogGameJam, Error, TEXT("MapGen: no layout for %s L%.2f from seed %u"), UTF8_TO_TCHAR(GJMap::FamilyName(ToCore(Family))), Level, FirstSeed);
		return false;
	}
	if (Layout.Seed != FirstSeed)
	{
		UE_LOG(LogGameJam, Warning, TEXT("MapGen: seed %u failed, using %u"), FirstSeed, Layout.Seed);
		Seed = static_cast<int32>(Layout.Seed);
	}
	for (const std::string& Problem : GJMap::Validate(Layout))
	{
		UE_LOG(LogGameJam, Warning, TEXT("MapGen: invalid layout: %s"), *ToFString(Problem));
	}
	UE_LOG(LogGameJam, Log, TEXT("MapGen: %s"), *GetDescription());
	return true;
}

FString AGJMapGenerator::GetDescription() const
{
	if (!bHasLayout) { return TEXT("MapGen: no layout"); }
	const GJMap::FMetrics M = GJMap::ComputeMetrics(Layout);
	return FString::Printf(TEXT("%s | free %.0f m2, farthest walk %.0f m, cover tiles %d, boxes %d, rooms %d, planned loops %d (gj.Map %s %.2f %u)"),
		*ToFString(GJMap::Describe(Layout)), M.FreeAreaM2, M.FarthestWalkM, M.CoverTiles, M.WallBoxes, M.RoomsPlaced, M.PlannedLoops,
		UTF8_TO_TCHAR(GJMap::FamilyName(Layout.Family)), Layout.Level, Layout.Seed);
}

void AGJMapGenerator::ClearGeometry()
{
	for (UBoxComponent* Box : Colliders)
	{
		if (Box) { Box->DestroyComponent(); }
	}
	Colliders.Reset();
	for (UInstancedStaticMeshComponent* Visual : { FloorMeshes.Get(), WallMeshes.Get(), LowCoverMeshes.Get(), HighCoverMeshes.Get() })
	{
		Visual->ClearInstances();
	}
}

void AGJMapGenerator::BuildGeometry()
{
	ClearGeometry();
	const TPair<UInstancedStaticMeshComponent*, UMaterialInterface*> Looks[] = {
		{ FloorMeshes, FloorMaterial }, { WallMeshes, WallMaterial }, { LowCoverMeshes, LowCoverMaterial }, { HighCoverMeshes, HighCoverMaterial } };
	for (const TPair<UInstancedStaticMeshComponent*, UMaterialInterface*>& Look : Looks)
	{
		Look.Key->SetStaticMesh(BoxMesh);
		if (Look.Value) { Look.Key->SetMaterial(0, Look.Value); }
	}
	if (!BoxMesh)
	{
		UE_LOG(LogGameJam, Warning, TEXT("MapGen: no BoxMesh set; building collision only"));
	}

	const float Tile = static_cast<float>(GJMap::TileCm);
	// One floor slab under the whole map, its top at the actor's Z.
	const FVector MapSize(Layout.Rows * Tile, Layout.Cols * Tile, FloorThicknessCm);
	AddBlock(FloorMeshes, FVector(MapSize.X * 0.5f, MapSize.Y * 0.5f, -FloorThicknessCm * 0.5f), MapSize, true);

	for (const GJMap::FRect& Rect : GJMap::MergeRects(Layout))
	{
		UInstancedStaticMeshComponent* Visual = WallMeshes;
		float Height = static_cast<float>(GJMap::WallCm);
		if (Rect.Kind == GJMap::LowCover) { Visual = LowCoverMeshes; Height = static_cast<float>(GJMap::LowCoverCm); }
		if (Rect.Kind == GJMap::HighCover) { Visual = HighCoverMeshes; Height = static_cast<float>(GJMap::HighCoverCm); }
		// Rows R0..R0+NumRows-1 span X from (Rows - R0 - NumRows) to (Rows - R0) tiles: row 0 is north (+X).
		const float X0 = (Layout.Rows - Rect.R0 - Rect.NumRows) * Tile;
		const float Y0 = Rect.C0 * Tile;
		const FVector Size(Rect.NumRows * Tile, Rect.NumCols * Tile, Height);
		AddBlock(Visual, FVector(X0 + Size.X * 0.5f, Y0 + Size.Y * 0.5f, Height * 0.5f), Size, true);
	}
}

void AGJMapGenerator::AddBlock(UInstancedStaticMeshComponent* Visual, const FVector& LocalCenter, const FVector& Size, bool bCollision)
{
	if (Visual && BoxMesh)
	{
		const FBox Bounds = BoxMesh->GetBoundingBox();
		const FVector MeshSize = Bounds.GetSize();
		if (MeshSize.X > KINDA_SMALL_NUMBER && MeshSize.Y > KINDA_SMALL_NUMBER && MeshSize.Z > KINDA_SMALL_NUMBER)
		{
			const FVector Scale = Size / MeshSize;
			Visual->AddInstance(FTransform(FQuat::Identity, LocalCenter - Bounds.GetCenter() * Scale, Scale), /*bWorldSpace*/ false);
		}
	}
	if (bCollision)
	{
		// BlockAll: WorldStatic, blocking every channel, including Weapon and AgentPawn (their default response is Block).
		// Agents perceive WorldStatic and bullets use Weapon, so line of sight and bullets agree (contract §11.2).
		UBoxComponent* Box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
		Box->SetupAttachment(Root);
		Box->SetRelativeLocation(LocalCenter);
		Box->SetBoxExtent(Size * 0.5f, /*bUpdateOverlaps*/ false);
		Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Box->SetGenerateOverlapEvents(false);
		Box->SetCanEverAffectNavigation(false);
		Box->RegisterComponent();
		Colliders.Add(Box);
	}
}

FVector AGJMapGenerator::TileToWorld(int32 Tile) const
{
	double X = 0, Y = 0;
	Layout.TileCenterCm(Tile / Layout.Cols, Tile % Layout.Cols, X, Y);
	return FTransform(GetActorRotation(), GetActorLocation()).TransformPosition(FVector(X, Y, 0));
}

int32 AGJMapGenerator::WorldToTile(const FVector& World) const
{
	if (!bHasLayout) { return INDEX_NONE; }
	const FVector Local = FTransform(GetActorRotation(), GetActorLocation()).InverseTransformPosition(World);
	const int32 Tile = Layout.TileAtCm(Local.X, Local.Y);
	return Tile >= 0 ? Tile : INDEX_NONE;
}

FTransform AGJMapGenerator::GetStartTransform() const
{
	const FRotator Facing(0.f, GetActorRotation().Yaw + Layout.StartYawDeg, 0.f);
	return FTransform(Facing, TileToWorld(Layout.StartTile));
}

void AGJMapGenerator::GetSpawnCandidates(const FVector& From, float MinWalkM, float MaxWalkM, TArray<FVector>& Out) const
{
	Out.Reset();
	if (!bHasLayout) { return; }
	int32 FromTile = WorldToTile(From);
	if (FromTile == INDEX_NONE || Layout.Tiles[static_cast<size_t>(FromTile)] != GJMap::Floor)
	{
		// Standing on a tile edge next to a wall: use the nearest floor tile.
		double Best = TNumericLimits<double>::Max();
		for (int32 I = 0; I < static_cast<int32>(Layout.Tiles.size()); ++I)
		{
			if (Layout.Tiles[static_cast<size_t>(I)] != GJMap::Floor) { continue; }
			const double D = FVector::DistSquared2D(TileToWorld(I), From);
			if (D < Best) { Best = D; FromTile = I; }
		}
	}
	if (FromTile == INDEX_NONE) { return; }
	const std::vector<float> Dist = GJMap::WalkDistance(Layout, FromTile);
	for (int32 I = 0; I < static_cast<int32>(Dist.size()); ++I)
	{
		const float D = Dist[static_cast<size_t>(I)];
		if (D >= MinWalkM && D <= MaxWalkM) { Out.Add(TileToWorld(I)); }
	}
}

void AGJMapGenerator::PlacePlayers()
{
	UWorld* World = GetWorld();
	const FTransform Start = GetStartTransform();

	// The map's start replaces every other PlayerStart, so the game mode's spawns and respawns land on it.
	// PlayerStarts are static and can't be moved while playing: replace them.
	TArray<APlayerStart*> Old;
	for (TActorIterator<APlayerStart> It(World); It; ++It) { Old.Add(*It); }
	for (APlayerStart* P : Old) { P->Destroy(); }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MapStart = World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(),
		Start.GetLocation() + FVector(0, 0, PlayerStartHalfHeight + 2.f), Start.Rotator(), Params);

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn) { continue; }
		float HalfHeight = PlayerStartHalfHeight;
		if (const ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) { Movement->StopMovementImmediately(); }
		Pawn->TeleportTo(Start.GetLocation() + FVector(0, 0, HalfHeight + 2.f), Start.Rotator());
		PC->SetControlRotation(Start.Rotator());
	}
}

int32 AGJMapGenerator::RunSelfCheck(bool bCheckStart) const
{
	UWorld* World = GetWorld();
	if (!World || !bHasLayout)
	{
		UE_LOG(LogGameJam, Error, TEXT("MapCheck: FAIL (no layout)"));
		return 1;
	}
	int32 Problems = 0;
	int32 Traces = 0;
	auto Fail = [&Problems](const FString& What)
	{
		++Problems;
		if (Problems <= 20) { UE_LOG(LogGameJam, Error, TEXT("MapCheck: %s"), *What); }
	};

	// Pawns (player hitboxes block Weapon) are not level geometry: ignore them.
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GJMapCheck), /*bTraceComplex*/ false);
	for (TActorIterator<APawn> It(World); It; ++It) { Query.AddIgnoredActor(*It); }
	// What agents perceive: WorldStatic and WorldDynamic only (contract §11).
	FCollisionObjectQueryParams Perceived;
	Perceived.AddObjectTypesToQuery(ECC_WorldStatic);
	Perceived.AddObjectTypesToQuery(ECC_WorldDynamic);
	const FVector Up = GetActorQuat().GetUpVector();
	const int32 Dr[4] = { -1, 0, 1, 0 };
	const int32 Dc[4] = { 0, 1, 0, -1 };

	// Every 3rd floor tile (and the start), 4 directions, two heights, two query kinds.
	for (int32 Tile = 0; Tile < static_cast<int32>(Layout.Tiles.size()); ++Tile)
	{
		if (Layout.Tiles[static_cast<size_t>(Tile)] != GJMap::Floor || (Tile % 3 != 0 && Tile != Layout.StartTile)) { continue; }
		const int32 R = Tile / Layout.Cols;
		const int32 C = Tile % Layout.Cols;
		for (int32 D = 0; D < 4; ++D)
		{
			for (const float Height : { 60.f, 200.f })
			{
				// Grid answer: the first tile along the direction that blocks at this height.
				int32 Steps = 1;
				for (;; ++Steps)
				{
					const char T = Layout.At(R + Dr[D] * Steps, C + Dc[D] * Steps);
					if (T == GJMap::Wall || T == GJMap::HighCover || (T == GJMap::LowCover && Height < GJMap::LowCoverCm)) { break; }
				}
				const float Expected = (Steps - 0.5f) * GJMap::TileCm;
				const FVector Start = TileToWorld(Tile) + Up * Height;
				const int32 Target = Layout.Index(R + Dr[D] * Steps, C + Dc[D] * Steps);
				const FVector End = Start + (TileToWorld(Target) + Up * Height - Start).GetSafeNormal() * (Expected + 300.f);
				FHitResult Hit;
				const bool bWeapon = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Weapon, Query);
				const float WeaponDist = bWeapon ? Hit.Distance : -1.f;
				const bool bSeen = World->LineTraceSingleByObjectType(Hit, Start, End, Perceived, Query);
				const float SeenDist = bSeen ? Hit.Distance : -1.f;
				Traces += 2;
				if (FMath::Abs(WeaponDist - Expected) > 5.f || FMath::Abs(SeenDist - Expected) > 5.f)
				{
					Fail(FString::Printf(TEXT("tile (%d,%d) dir %d at %.0f cm: grid says blocked at %.0f cm, Weapon trace %.0f, sight trace %.0f"),
						R, C, D, Height, Expected, WeaponDist, SeenDist));
				}
			}
		}
	}

	// The player.
	const APawn* Player = World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr;
	int32 PlayerTile = INDEX_NONE;
	if (!Player)
	{
		Fail(TEXT("no player pawn"));
	}
	else
	{
		PlayerTile = WorldToTile(Player->GetActorLocation());
		const ACharacter* Character = Cast<ACharacter>(Player);
		const float HalfHeight = Character ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : PlayerStartHalfHeight;
		const float AboveFloor = FVector::DotProduct(Player->GetActorLocation() - GetActorLocation(), Up) - HalfHeight;
		if (PlayerTile == INDEX_NONE || Layout.Tiles[static_cast<size_t>(PlayerTile)] != GJMap::Floor)
		{
			Fail(FString::Printf(TEXT("player is not on a floor tile (%s)"), *Player->GetActorLocation().ToString()));
		}
		else if (bCheckStart && PlayerTile != Layout.StartTile)
		{
			Fail(FString::Printf(TEXT("player is on tile %d, not the start tile %d"), PlayerTile, Layout.StartTile));
		}
		if (FMath::Abs(AboveFloor) > 30.f)
		{
			Fail(FString::Printf(TEXT("player's feet are %.0f cm off the floor"), AboveFloor));
		}
	}

	// Enemies.
	int32 Enemies = 0;
	if (PlayerTile != INDEX_NONE)
	{
		const std::vector<float> Dist = GJMap::WalkDistance(Layout, PlayerTile);
		for (TActorIterator<AEnemyBean> It(World); It; ++It)
		{
			if (!It->IsAlive()) { continue; }
			++Enemies;
			const int32 Tile = WorldToTile(It->GetActorLocation());
			if (Tile == INDEX_NONE || Layout.Tiles[static_cast<size_t>(Tile)] != GJMap::Floor || Dist[static_cast<size_t>(Tile)] < 0.f)
			{
				Fail(FString::Printf(TEXT("enemy %s is not on a reachable floor tile (%s)"), *It->GetName(), *It->GetActorLocation().ToString()));
			}
		}
	}

	if (Problems == 0)
	{
		UE_LOG(LogGameJam, Display, TEXT("MapCheck: PASS %s | %d traces, player on tile %d, %d enemies on reachable tiles"), *GetDescription(), Traces,
			PlayerTile, Enemies);
	}
	else
	{
		UE_LOG(LogGameJam, Error, TEXT("MapCheck: FAIL (%d problems) %s"), Problems, *GetDescription());
	}
	return Problems;
}

void AGJMapGenerator::StartSelfTest(float DelaySeconds, bool bQuit)
{
	SelfTestProblems = 0;
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.SetTimer(SelfTestTimer, FTimerDelegate::CreateWeakLambda(this, [this, DelaySeconds, bQuit]()
	{
		SelfTestProblems += RunSelfCheck(/*bCheckStart*/ false);
		Regenerate(Family == EGJMapFamily::Graph ? EGJMapFamily::Wfc : EGJMapFamily::Graph, Level, RandomSeed());
		// Right after a rebuild the player must be on the new start; enemies arrive again over the delay.
		SelfTestProblems += RunSelfCheck(/*bCheckStart*/ true);
		GetWorldTimerManager().SetTimer(SelfTestTimer, FTimerDelegate::CreateWeakLambda(this, [this, bQuit]()
		{
			SelfTestProblems += RunSelfCheck(/*bCheckStart*/ false);
			UE_LOG(LogGameJam, Display, TEXT("MapSelfTest: %s (%d problems)"), SelfTestProblems == 0 ? TEXT("PASS") : TEXT("FAIL"), SelfTestProblems);
			if (bQuit) { FPlatformMisc::RequestExitWithStatus(false, SelfTestProblems == 0 ? 0 : 1); }
		}), FMath::Max(1.f, DelaySeconds), false);
	}), FMath::Max(0.1f, DelaySeconds), false);
}

void AGJMapGenerator::WriteMapFile() const
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("MapGen");
	IFileManager::Get().MakeDirectory(*Dir, /*Tree*/ true);
	const FString Name = FString::Printf(TEXT("%s-L%.2f-%u.txt"), UTF8_TO_TCHAR(GJMap::FamilyName(Layout.Family)), Layout.Level, Layout.Seed);
	const FString Path = Dir / Name;
	if (FFileHelper::SaveStringToFile(ToFString(GJMap::ToMapFormatV1(Layout)), *Path))
	{
		UE_LOG(LogGameJam, Log, TEXT("MapGen: wrote %s"), *Path);
	}
}
