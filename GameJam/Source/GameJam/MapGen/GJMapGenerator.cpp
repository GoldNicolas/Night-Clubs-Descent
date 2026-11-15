#include "GJMapGenerator.h"
#include "GameJam.h"
#include "GJEncounterSubsystem.h"
#include "EnemyBean.h"
#include "Common/FPSAgentTypes.h"
#include "TimerManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Light.h"
#include "Engine/OverlapResult.h"
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
	constexpr float RoofThicknessCm = 20.f;
	const FName LocationSocketTag(TEXT("LocationSocket"));

	GJMap::EFamily ToCore(EGJMapFamily Family)
	{
		switch (Family)
		{
		case EGJMapFamily::Wfc: return GJMap::EFamily::Wfc;
		case EGJMapFamily::Tunnels: return GJMap::EFamily::Tunnels;
		default: return GJMap::EFamily::Graph;
		}
	}

	/** A socket marker's facing, snapped to the 90-degree grid. */
	FRotator SocketFacing(const AActor* Marker)
	{
		return FRotator(0.f, FMath::RoundToFloat(Marker->GetActorRotation().Yaw / 90.f) * 90.f, 0.f);
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
		TEXT("Rebuild every procedural map: gj.Map <graph|wfc|tunnels> <level 1-5> [seed]. Socket maps stay tunnels; the family is for a")
		TEXT(" standalone map. Enemies are removed and, on a standalone map, the player moves to the new start."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const TArray<AGJMapGenerator*> Maps = AGJMapGenerator::FindAll(World);
			if (Maps.IsEmpty())
			{
				UE_LOG(LogGameJam, Warning, TEXT("gj.Map: this level has no GJMapGenerator"));
				return;
			}
			bool bFamilyGiven = false;
			EGJMapFamily Family = EGJMapFamily::Graph;
			if (Args.Num() > 0)
			{
				bFamilyGiven = true;
				if (Args[0].Equals(TEXT("graph"), ESearchCase::IgnoreCase)) { Family = EGJMapFamily::Graph; }
				else if (Args[0].Equals(TEXT("wfc"), ESearchCase::IgnoreCase)) { Family = EGJMapFamily::Wfc; }
				else if (Args[0].Equals(TEXT("tunnels"), ESearchCase::IgnoreCase)) { Family = EGJMapFamily::Tunnels; }
				else { UE_LOG(LogGameJam, Warning, TEXT("gj.Map: family must be graph, wfc or tunnels")); return; }
			}
			const int32 Seed = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : RandomSeed();
			for (int32 I = 0; I < Maps.Num(); ++I)
			{
				AGJMapGenerator* Map = Maps[I];
				const float Level = Args.Num() > 1 ? FCString::Atof(*Args[1]) : Map->Level;
				// Each map gets its own seed, derived from the one given, so the whole level is reproducible from one number.
				Map->Regenerate(bFamilyGiven ? Family : Map->Family, Level, Seed + I * 7919);
			}
		}));

	FAutoConsoleCommandWithWorld CmdMapCheck(TEXT("gj.MapCheck"),
		TEXT("Check the built map against its grid (traces, player, enemies) and log PASS / FAIL."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			for (const AGJMapGenerator* Map : AGJMapGenerator::FindAll(World)) { Map->RunSelfCheck(false); }
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
			for (const AGJMapGenerator* Map : AGJMapGenerator::FindAll(World))
			{
				UE_LOG(LogGameJam, Display, TEXT("%s"), *Map->GetDescription());
			}
		}));
}

AGJMapGenerator::AGJMapGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// Movable: a socket map moves itself onto its socket when play starts.
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	// Visible meshes only: all collision is the merged boxes built in BuildGeometry.
	auto MakeVisual = [this](const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Visual = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Visual->SetupAttachment(Root);
		Visual->SetMobility(EComponentMobility::Movable);
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetCanEverAffectNavigation(false);
		return Visual;
	};
	FloorMeshes = MakeVisual(TEXT("FloorMeshes"));
	WallMeshes = MakeVisual(TEXT("WallMeshes"));
	LowCoverMeshes = MakeVisual(TEXT("LowCoverMeshes"));
	HighCoverMeshes = MakeVisual(TEXT("HighCoverMeshes"));
	RoofMeshes = MakeVisual(TEXT("RoofMeshes"));
	LampMeshes = MakeVisual(TEXT("LampMeshes"));
	LampMeshes->SetCastShadow(false);

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

TArray<AGJMapGenerator*> AGJMapGenerator::FindAll(const UObject* WorldContext)
{
	TArray<AGJMapGenerator*> Out;
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) { return Out; }
	for (TActorIterator<AGJMapGenerator> It(World); It; ++It) { Out.Add(*It); }
	return Out;
}

void AGJMapGenerator::BeginPlay()
{
	Super::BeginPlay();
	if (!GetWorld() || !GetWorld()->IsGameWorld()) { return; }
	if (bRandomSeedOnPlay) { Seed = RandomSeed(); }
	Generate();
	if (bHasLayout)
	{
		if (!IsSocketMap()) { PlacePlayers(); } // a socket map is part of a level that has its own start
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
	if (!IsSocketMap()) { PlacePlayers(); }
	if (bWriteMapFile) { WriteMapFile(); }
	return true;
}

bool AGJMapGenerator::BuildLayout()
{
	bHasLayout = false;
	GJMap::FGraphParams SocketParams;
	int32 EntryC = -1;
	if (IsSocketMap())
	{
		Family = EGJMapFamily::Tunnels; // the mouth is what joins the socket's tunnel
		if (!PrepareSocket(SocketParams, EntryC)) { return false; }
	}
	// A generator can fail on a seed (e.g. a WFC contradiction every try); the next seed is used then, and logged.
	const uint32 FirstSeed = static_cast<uint32>(FMath::Max(0, Seed));
	for (uint32 Skip = 0; Skip < 8 && !bHasLayout; ++Skip)
	{
		if (IsSocketMap())
		{
			bHasLayout = GJMap::GenerateTunnels(SocketParams, EntryC, FirstSeed + Skip, Layout);
			Layout.Level = Level;
		}
		else
		{
			bHasLayout = GJMap::MakeLayout(ToCore(Family), Level, FirstSeed + Skip, Layout);
		}
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
	const FString Where = IsSocketMap() ? FString::Printf(TEXT("[socket %s] "), *Socket.ToString()) : FString();
	if (!bHasLayout) { return Where + TEXT("no layout"); }
	const GJMap::FMetrics M = GJMap::ComputeMetrics(Layout);
	return Where + FString::Printf(TEXT("%s | free %.0f m2, farthest walk %.0f m, cover tiles %d, boxes %d, rooms %d, planned loops %d (gj.Map %s %.2f %u)"),
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
	for (UPointLightComponent* Lamp : Lamps)
	{
		if (Lamp) { Lamp->DestroyComponent(); }
	}
	Lamps.Reset();
	for (UInstancedStaticMeshComponent* Visual : { FloorMeshes.Get(), WallMeshes.Get(), LowCoverMeshes.Get(), HighCoverMeshes.Get(),
		RoofMeshes.Get(), LampMeshes.Get() })
	{
		Visual->ClearInstances();
	}
}

void AGJMapGenerator::BuildGeometry()
{
	ClearGeometry();
	const TPair<UInstancedStaticMeshComponent*, UMaterialInterface*> Looks[] = {
		{ FloorMeshes, FloorMaterial }, { WallMeshes, WallMaterial }, { LowCoverMeshes, LowCoverMaterial }, { HighCoverMeshes, HighCoverMaterial },
		{ RoofMeshes, RoofMaterial ? RoofMaterial.Get() : WallMaterial.Get() }, { LampMeshes, LampMaterial } };
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

	// Under a roof the walls stop at the roof's top; otherwise they are the map format's 6 m.
	const float WallHeight = bRoof ? RoofHeightCm + RoofThicknessCm : static_cast<float>(GJMap::WallCm);
	for (const GJMap::FRect& Rect : GJMap::MergeRects(Layout))
	{
		UInstancedStaticMeshComponent* Visual = WallMeshes;
		float Height = WallHeight;
		if (Rect.Kind == GJMap::LowCover) { Visual = LowCoverMeshes; Height = static_cast<float>(GJMap::LowCoverCm); }
		if (Rect.Kind == GJMap::HighCover) { Visual = HighCoverMeshes; Height = static_cast<float>(GJMap::HighCoverCm); }
		// Rows R0..R0+NumRows-1 span X from (Rows - R0 - NumRows) to (Rows - R0) tiles: row 0 is north (+X).
		const float X0 = (Layout.Rows - Rect.R0 - Rect.NumRows) * Tile;
		const float Y0 = Rect.C0 * Tile;
		const FVector Size(Rect.NumRows * Tile, Rect.NumCols * Tile, Height);
		AddBlock(Visual, FVector(X0 + Size.X * 0.5f, Y0 + Size.Y * 0.5f, Height * 0.5f), Size, true);
	}

	if (Layout.MouthTile >= 0 && !IsSocketMap())
	{
		// A standalone tunnel map has nothing past its mouth: close it with a wall one tile beyond the map.
		const float Y0 = (Layout.MouthTile % Layout.Cols) * Tile;
		AddBlock(WallMeshes, FVector(-Tile * 0.5f, Y0 + Tile * 0.5f, WallHeight * 0.5f), FVector(Tile, Tile, WallHeight), true);
	}
	if (bRoof)
	{
		AddBlock(RoofMeshes, FVector(MapSize.X * 0.5f, MapSize.Y * 0.5f, RoofHeightCm + RoofThicknessCm * 0.5f),
			FVector(MapSize.X, MapSize.Y, RoofThicknessCm), true, /*bCeiling*/ true);
	}
	BuildLamps();
}

void AGJMapGenerator::BuildLamps()
{
	if (!bLamps) { return; }
	// Greedy along the walk from the start: a lamp on every floor tile with no lamp within LampSpacingTiles - 1 tiles.
	const std::vector<float> Dist = GJMap::WalkDistance(Layout, Layout.StartTile);
	TArray<int32> Order;
	for (int32 I = 0; I < static_cast<int32>(Dist.size()); ++I)
	{
		if (Dist[static_cast<size_t>(I)] >= 0.f) { Order.Add(I); }
	}
	Order.StableSort([&Dist](int32 A, int32 B) { return Dist[static_cast<size_t>(A)] < Dist[static_cast<size_t>(B)]; });
	const float Ceiling = bRoof ? RoofHeightCm : static_cast<float>(GJMap::WallCm);
	TArray<FIntPoint> Placed;
	for (const int32 I : Order)
	{
		if (Placed.Num() >= MaxLamps) { break; }
		const FIntPoint Tile(I / Layout.Cols, I % Layout.Cols);
		if (Placed.ContainsByPredicate([this, &Tile](const FIntPoint& P)
			{ return FMath::Max(FMath::Abs(P.X - Tile.X), FMath::Abs(P.Y - Tile.Y)) < LampSpacingTiles; }))
		{
			continue;
		}
		Placed.Add(Tile);
		double X = 0, Y = 0;
		Layout.TileCenterCm(Tile.X, Tile.Y, X, Y);
		UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
		Lamp->SetupAttachment(Root);
		Lamp->SetMobility(EComponentMobility::Movable);
		Lamp->SetRelativeLocation(FVector(X, Y, Ceiling - 40.f));
		Lamp->SetIntensityUnits(ELightUnits::Candelas);
		Lamp->SetIntensity(LampIntensity);
		Lamp->SetLightColor(LampColor);
		Lamp->SetAttenuationRadius(LampRadiusCm);
		Lamp->SetCastShadows(false);
		Lamp->RegisterComponent();
		Lamps.Add(Lamp);
		AddBlock(LampMeshes, FVector(X, Y, Ceiling - 6.f), FVector(40.f, 40.f, 12.f), false);
	}
}

bool AGJMapGenerator::PrepareSocket(GJMap::FGraphParams& OutParams, int32& OutEntryC)
{
	UWorld* World = GetWorld();
	ClaimedBox.Init();
	AActor* Marker = nullptr;
	for (TActorIterator<AActor> It(World); It && !Marker; ++It)
	{
		if (It->ActorHasTag(LocationSocketTag) && It->ActorHasTag(Socket)) { Marker = *It; }
	}
	if (!Marker)
	{
		UE_LOG(LogGameJam, Error, TEXT("MapGen: no actor tagged %s + %s in this level; socket map not built"), *LocationSocketTag.ToString(),
			*Socket.ToString());
		return false;
	}
	const FRotator Facing = SocketFacing(Marker);
	const FVector Forward = Facing.Vector();
	const FVector Right = FRotationMatrix(Facing).GetUnitAxis(EAxis::Y);
	const FVector At = Marker->GetActorLocation();

	// The tunnel floor (the marker stands about 1 m above it, in the gate, so probe 1.5 m back toward the level).
	float FloorZ = At.Z - 100.f;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GJSocketFloor), false);
	Query.AddIgnoredActor(Marker);
	Query.AddIgnoredActor(this);
	const FVector Probe = At - Forward * 150.f;
	if (World->LineTraceSingleByChannel(Hit, Probe, Probe - FVector(0, 0, 400.f), ECC_Visibility, Query) && !Hit.bStartPenetrating)
	{
		FloorZ = Hit.ImpactPoint.Z;
	}
	else
	{
		UE_LOG(LogGameJam, Warning, TEXT("MapGen: socket %s: no floor found under the tunnel; assuming 1 m below the marker"), *Socket.ToString());
	}
	SocketMarker = Marker;
	if (World->IsGameWorld()) { HideSocketStub(Marker, FloorZ); }

	// The mouth tile starts where the socket's tunnel ends and is centred on it. Largest preset size first, mouth columns in a
	// seeded order; shrink until the network fits.
	const FVector MouthStart(At.X + Forward.X * SocketMouthCm, At.Y + Forward.Y * SocketMouthCm, FloorZ);
	const GJMap::FGraphParams Preset = GJMap::TunnelPreset(Level);
	const float Tile = static_cast<float>(GJMap::TileCm);
	const float Top = (bRoof ? RoofHeightCm : static_cast<float>(GJMap::WallCm)) + RoofThicknessCm;
	FRandomStream Rng(Seed ^ 0x51F15EED);
	for (int32 Size = Preset.Rows; Size >= 8; Size -= 2)
	{
		TArray<int32> Columns;
		for (int32 C = 1; C <= Size - 2; ++C) { Columns.Add(C); }
		for (int32 I = Columns.Num() - 1; I > 0; --I) { Columns.Swap(I, Rng.RandRange(0, I)); }
		for (const int32 C : Columns)
		{
			const FVector Origin = MouthStart - Right * ((C + 0.5f) * Tile);
			const FTransform Frame(Facing, Origin);
			if (!IsRegionFree(Frame, Size, Size)) { continue; }
			OutParams = Preset;
			OutParams.Rows = OutParams.Cols = Size;
			if (Size < Preset.Rows)
			{
				// Fewer chambers on a map shrunk to fit.
				OutParams.NumRooms = FMath::Max(2, FMath::RoundToInt(Preset.NumRooms * static_cast<float>(Size * Size) / static_cast<float>(Preset.Rows * Preset.Rows)));
			}
			OutEntryC = C;
			SetActorLocationAndRotation(Origin, Facing);
			ClaimedBox += Frame.TransformPosition(FVector::ZeroVector);
			ClaimedBox += Frame.TransformPosition(FVector(Size * Tile, Size * Tile, Top));
			UE_LOG(LogGameJam, Log, TEXT("MapGen: socket %s at %s: %d x %d tiles (preset %d), mouth column %d"), *Socket.ToString(),
				*MouthStart.ToCompactString(), Size, Size, Preset.Rows, C);
			return true;
		}
	}
	UE_LOG(LogGameJam, Error, TEXT("MapGen: no room for a tunnel map past socket %s (the level's geometry or another map is in the way)"),
		*Socket.ToString());
	return false;
}

bool AGJMapGenerator::IsRegionFree(const FTransform& Frame, int32 Rows, int32 Cols) const
{
	const float Tile = static_cast<float>(GJMap::TileCm);
	const float Clear = SocketClearanceCm;
	const float Top = (bRoof ? RoofHeightCm : static_cast<float>(GJMap::WallCm)) + RoofThicknessCm;
	// The map plus clearance on the far side and both flanks; the near side (local X = 0) is the mouth, flush with the socket's
	// tunnel, so it starts just past it.
	FBox Box(ForceInit);
	for (const float X : { 10.f, Rows * Tile + Clear })
	{
		for (const float Y : { -Clear, Cols * Tile + Clear })
		{
			Box += Frame.TransformPosition(FVector(X, Y, 0.f));
		}
	}
	Box.Min.Z = Frame.GetLocation().Z + 10.f; // above the level's floor slabs
	Box.Max.Z = Frame.GetLocation().Z + Top;

	for (const AGJMapGenerator* Other : FindAll(this))
	{
		if (Other != this && Other->ClaimedBox.IsValid && Other->ClaimedBox.Intersect(Box)) { return false; }
	}
	// Anything in the level that a pawn would bump into (walls, floors, ceilings, cover).
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Ceiling);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GJSocketRegion), false);
	Query.AddIgnoredActor(this);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Box.GetCenter(), FQuat::Identity, Objects, FCollisionShape::MakeBox(Box.GetExtent()), Query);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const UPrimitiveComponent* Component = Overlap.GetComponent();
		if (Component && Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block) { return false; }
	}
	return true;
}

void AGJMapGenerator::HideSocketStub(const AActor* Marker, float FloorZ)
{
	// The gate, its warning lamp and the fake continuation past it (fog veils, black end wall, far lamp), plus the tunnel's own end
	// wall and coves: every mesh or light lying wholly between just behind the marker and the end of the tunnel, within its 4 m
	// width. The tunnel's floor, side walls and ceiling run past the marker, so they stay. Labels and outliner folders don't exist
	// in a packaged game, so this goes by geometry.
	const FRotator Facing = SocketFacing(Marker);
	const FVector Forward = Facing.Vector();
	const FVector Right = FRotationMatrix(Facing).GetUnitAxis(EAxis::Y);
	const FVector At = Marker->GetActorLocation();
	const float Top = FloorZ + (bRoof ? RoofHeightCm : static_cast<float>(GJMap::WallCm)) + 40.f;
	const float HalfWidth = GJMap::TileCm * 0.5f + 15.f;
	int32 Hidden = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == Marker || Actor == this || Actor->IsHidden() || Actor->IsA<APawn>() || Actor->IsA<AGJMapGenerator>()) { continue; }
		const bool bLight = Actor->IsA<ALight>();
		if (!bLight && !Actor->FindComponentByClass<UStaticMeshComponent>()) { continue; }
		FVector Center = Actor->GetActorLocation();
		FVector Extent = FVector::ZeroVector;
		if (!bLight) { Actor->GetActorBounds(/*bOnlyCollidingComponents*/ false, Center, Extent); }
		const FVector D = Center - At;
		const double Along = FVector::DotProduct(D, Forward);
		const double Across = FVector::DotProduct(D, Right);
		const double AlongExtent = FMath::Abs(Forward.X) * Extent.X + FMath::Abs(Forward.Y) * Extent.Y;
		const double AcrossExtent = FMath::Abs(Right.X) * Extent.X + FMath::Abs(Right.Y) * Extent.Y;
		if (Along - AlongExtent < -75.0 || Along + AlongExtent > SocketMouthCm + 30.0 || FMath::Abs(Across) + AcrossExtent > HalfWidth
			|| Center.Z - Extent.Z < FloorZ - 30.0 || Center.Z + Extent.Z > Top)
		{
			continue;
		}
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);
		TInlineComponentArray<ULightComponent*> LightComponents(Actor);
		for (ULightComponent* Light : LightComponents) { Light->SetVisibility(false); }
		++Hidden;
	}
	UE_LOG(LogGameJam, Log, TEXT("MapGen: socket %s: hid %d gate / tunnel-end actors"), *Socket.ToString(), Hidden);
}

void AGJMapGenerator::AddBlock(UInstancedStaticMeshComponent* Visual, const FVector& LocalCenter, const FVector& Size, bool bCollision, bool bCeiling)
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
		if (bCeiling)
		{
			// The Ceiling object type: blocks pawns and bullets, but agent perception (WorldStatic) doesn't see it.
			Box->SetCollisionObjectType(ECC_Ceiling);
		}
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
				// Grid answer: the first tile along the direction that blocks at this height. Through a mouth the grid ends before a
				// wall does; nothing to compare there.
				int32 Steps = 1;
				bool bOffGrid = false;
				for (;; ++Steps)
				{
					if (!Layout.InBounds(R + Dr[D] * Steps, C + Dc[D] * Steps)) { bOffGrid = true; break; }
					const char T = Layout.At(R + Dr[D] * Steps, C + Dc[D] * Steps);
					if (T == GJMap::Wall || T == GJMap::HighCover || (T == GJMap::LowCover && Height < GJMap::LowCoverCm)) { break; }
				}
				if (bOffGrid) { continue; }
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

	// A socket map must be walkable from its level: a standing capsule passes from inside the socket's tunnel, through the gate's
	// place and the mouth, to the tile inside the mouth, and both ends stand on the same floor height.
	if (IsSocketMap())
	{
		const AActor* Marker = SocketMarker.Get();
		if (!Marker)
		{
			Fail(TEXT("socket marker missing"));
		}
		else
		{
			const FVector Forward = SocketFacing(Marker).Vector();
			const FVector Inside = TileToWorld(Layout.StartTile);
			const FVector Back = FVector(Marker->GetActorLocation().X, Marker->GetActorLocation().Y, Inside.Z) - Forward * 150.f;
			const FCollisionShape Capsule = FCollisionShape::MakeCapsule(40.f, 88.f);
			const FVector Lift(0, 0, 88.f + 10.f);
			FHitResult Hit;
			if (World->SweepSingleByChannel(Hit, Back + Lift, Inside + Lift, FQuat::Identity, ECC_Pawn, Capsule, Query))
			{
				Fail(FString::Printf(TEXT("the way in through socket %s is blocked by %s at %s"), *Socket.ToString(),
					*GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToCompactString()));
			}
			FHitResult Ground;
			if (!World->LineTraceSingleByChannel(Ground, Back + Lift, Back - FVector(0, 0, 200.f), ECC_Visibility, Query)
				|| FMath::Abs(Ground.ImpactPoint.Z - Inside.Z) > 5.f)
			{
				Fail(FString::Printf(TEXT("socket %s: the level's tunnel floor (%.0f) and the map's floor (%.0f) don't meet"), *Socket.ToString(),
					Ground.bBlockingHit ? Ground.ImpactPoint.Z : -1.f, Inside.Z));
			}
		}
	}

	// The player (a socket map is part of a bigger level, so the player can be anywhere in it: walks are measured from the mouth).
	const APawn* Player = World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr;
	int32 PlayerTile = INDEX_NONE;
	if (IsSocketMap())
	{
		PlayerTile = Layout.StartTile;
	}
	else if (!Player)
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
			const int32 Tile = WorldToTile(It->GetActorLocation());
			if (IsSocketMap() && Tile == INDEX_NONE) { continue; } // elsewhere in the level
			++Enemies;
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
	// Every map in the world: a level holds one standalone map, or socket maps attached to a hand-built level.
	auto CheckAll = [this](bool bCheckStart)
	{
		int32 Problems = 0;
		for (const AGJMapGenerator* Map : FindAll(this)) { Problems += Map->RunSelfCheck(bCheckStart); }
		return Problems;
	};
	Timers.SetTimer(SelfTestTimer, FTimerDelegate::CreateWeakLambda(this, [this, DelaySeconds, bQuit, CheckAll]()
	{
		SelfTestProblems += CheckAll(/*bCheckStart*/ false);
		const int32 NewSeed = RandomSeed();
		const TArray<AGJMapGenerator*> Maps = FindAll(this);
		for (int32 I = 0; I < Maps.Num(); ++I)
		{
			AGJMapGenerator* Map = Maps[I];
			const EGJMapFamily Next = Map->IsSocketMap() ? Map->Family : Map->Family == EGJMapFamily::Graph ? EGJMapFamily::Wfc : EGJMapFamily::Graph;
			Map->Regenerate(Next, Map->Level, NewSeed + I * 7919);
		}
		// Right after a rebuild the player must be on the new start; enemies arrive again over the delay.
		SelfTestProblems += CheckAll(/*bCheckStart*/ true);
		GetWorldTimerManager().SetTimer(SelfTestTimer, FTimerDelegate::CreateWeakLambda(this, [this, bQuit, CheckAll]()
		{
			SelfTestProblems += CheckAll(/*bCheckStart*/ false);
			UE_LOG(LogGameJam, Display, TEXT("MapSelfTest: %s (%d problems)"), SelfTestProblems == 0 ? TEXT("PASS") : TEXT("FAIL"), SelfTestProblems);
			if (bQuit) { FPlatformMisc::RequestExitWithStatus(false, SelfTestProblems == 0 ? 0 : 1); }
		}), FMath::Max(1.f, DelaySeconds), false);
	}), FMath::Max(0.1f, DelaySeconds), false);
}

void AGJMapGenerator::WriteMapFile() const
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("MapGen");
	IFileManager::Get().MakeDirectory(*Dir, /*Tree*/ true);
	const FString Name = FString::Printf(TEXT("%s%s-L%.2f-%u.txt"), IsSocketMap() ? *(Socket.ToString() + TEXT("-")) : TEXT(""),
		UTF8_TO_TCHAR(GJMap::FamilyName(Layout.Family)), Layout.Level, Layout.Seed);
	const FString Path = Dir / Name;
	if (FFileHelper::SaveStringToFile(ToFString(GJMap::ToMapFormatV1(Layout)), *Path))
	{
		UE_LOG(LogGameJam, Log, TEXT("MapGen: wrote %s"), *Path);
	}
}
