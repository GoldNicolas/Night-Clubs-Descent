#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJLayoutGen.h"
#include "GJMapGenerator.generated.h"

class APlayerStart;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;

UENUM(BlueprintType)
enum class EGJMapFamily : uint8
{
	/** Graph-first rooms: separate rooms joined by corridors, loops at higher levels. */
	Graph,
	/** Wave function collapse: 20 m modules of halls, rooms and corridor networks. */
	Wfc,
	/** Storm-drain tunnels: 4 m drains between small chambers, with a mouth in the south wall (always used with a Socket). */
	Tunnels,
};

/**
 * Builds a procedural single-story map (MAPGEN.md) from (family, level, seed) when play starts:
 * - the layout comes from GJMap (GJLayoutGen), on FPS-Agent's 4 m tile grid (contract §11): 6 m walls, 120 / 250 cm cover, open sky;
 * - collision is merged boxes (BlockAll: WorldStatic, blocks pawns, AgentPawn and the Weapon channel, so sight and bullets agree);
 *   the visible meshes are instanced and have no collision;
 * - it owns the player start: the level's PlayerStarts are replaced by one at the layout's start tile, and pawns already
 *   spawned are moved there;
 * - the encounter subsystem asks it for spawn points by walking distance (GetSpawnCandidates).
 * The actor's location is the map's south-west corner at floor level. Drop one into an empty level with a light and sky.
 *
 * SOCKET MODE (Socket set): the map is a tunnel network attached to a hand-built level, such as the club (MAPGEN.md §5b). On play it
 * finds the TargetPoint tagged LocationSocket + <Socket>, hides the gate and the fake tunnel end past it, and moves itself so the
 * network's mouth continues that tunnel. It picks a size and mouth column that keep clear of the level's geometry and of other
 * generators. It doesn't touch the player start. Several socket generators can share a level, one per socket.
 *
 * Console: gj.Map <graph|wfc|tunnels> <level> [seed] rebuilds every generator's map while playing; gj.MapInfo prints them.
 */
UCLASS()
class GAMEJAM_API AGJMapGenerator : public AActor
{
	GENERATED_BODY()

public:
	AGJMapGenerator();

	/** The world's first map generator, or null. */
	static AGJMapGenerator* Find(const UObject* WorldContext);
	/** Every map generator in the world. */
	static TArray<AGJMapGenerator*> FindAll(const UObject* WorldContext);

	/** Attached to a location socket of a hand-built level (Socket set), rather than a whole map of its own. */
	bool IsSocketMap() const { return !Socket.IsNone(); }

	/** Builds the layout for the current Family / Level / Seed (editor preview button, and on play). */
	UFUNCTION(CallInEditor, Category="Map")
	void Generate();

	/** Picks a new random seed and builds it. */
	UFUNCTION(CallInEditor, Category="Map")
	void GenerateNewSeed();

	/** Rebuilds with new settings while playing: enemies are removed and the player goes to the new start. */
	bool Regenerate(EGJMapFamily InFamily, float InLevel, int32 InSeed);

	bool HasLayout() const { return bHasLayout; }
	const GJMap::FLayout& GetLayout() const { return Layout; }
	FString GetDescription() const;

	/** Centre of a tile, at floor level, in world space. */
	FVector TileToWorld(int32 Tile) const;
	/** The tile under a world position, or INDEX_NONE. */
	int32 WorldToTile(const FVector& World) const;
	/** The start tile's floor point and facing. */
	FTransform GetStartTransform() const;

	/** Floor tile centres (world, floor level) whose walking distance from From is within [MinWalkM, MaxWalkM]. */
	void GetSpawnCandidates(const FVector& From, float MinWalkM, float MaxWalkM, TArray<FVector>& Out) const;

	/**
	 * gj.MapCheck: checks the built map against its grid and logs PASS / FAIL. Returns the number of problems.
	 * - Weapon-channel traces and WorldStatic/WorldDynamic object traces (what agents perceive) stop exactly where the grid has
	 *   a wall or cover, low (60 cm: everything blocks) and high (200 cm: low cover doesn't);
	 * - the player stands on a floor tile (bCheckStart: the start tile);
	 * - every living enemy is on a floor tile the player can walk to.
	 */
	int32 RunSelfCheck(bool bCheckStart) const;

	/**
	 * gj.MapSelfTest: after DelaySeconds check every map in the world, rebuild them (a standalone map as the other family, a socket
	 * map with a new seed), check again, then optionally quit (exit code 1 on failure).
	 */
	void StartSelfTest(float DelaySeconds, bool bQuit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map")
	EGJMapFamily Family = EGJMapFamily::Graph;

	/** Difficulty preset 1-5; fractions blend neighbouring presets (2.5 = halfway between L2 and L3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map", meta=(ClampMin=1, ClampMax=5, UIMin=1, UIMax=5))
	float Level = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map")
	int32 Seed = 1;

	/** Draw a new seed every time play starts (logged, so a bad map can be reproduced with gj.Map). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map")
	bool bRandomSeedOnPlay = true;

	/** Also write the layout as an FPS-Agent map format v1 text file to Saved/MapGen/ (for FPS-Agent S3 training via the user). */
	UPROPERTY(EditAnywhere, Category="Map")
	bool bWriteMapFile = true;

	/** Any box-shaped mesh; it is scaled to each wall, cover block and the floor using its bounds. */
	UPROPERTY(EditAnywhere, Category="Look")
	TObjectPtr<UStaticMesh> BoxMesh;

	UPROPERTY(EditAnywhere, Category="Look") TObjectPtr<UMaterialInterface> FloorMaterial;
	UPROPERTY(EditAnywhere, Category="Look") TObjectPtr<UMaterialInterface> WallMaterial;
	UPROPERTY(EditAnywhere, Category="Look") TObjectPtr<UMaterialInterface> LowCoverMaterial;
	UPROPERTY(EditAnywhere, Category="Look") TObjectPtr<UMaterialInterface> HighCoverMaterial;

	/**
	 * Direction tag of the level's LocationSocket TargetPoint to attach to (West, East, South, North); None = a standalone map.
	 * The socket marker faces out of the level, through a gate, SocketMouthCm before the end of its tunnel.
	 */
	UPROPERTY(EditAnywhere, Category="Socket")
	FName Socket;

	/** From the socket marker to the end of its tunnel, where the network's mouth starts (cm, along the marker's facing). */
	UPROPERTY(EditAnywhere, Category="Socket", meta=(EditCondition="Socket != None"))
	float SocketMouthCm = 400.f;

	/** The network keeps this far (cm) from the level's geometry and from other generators' maps; it shrinks to fit. */
	UPROPERTY(EditAnywhere, Category="Socket", meta=(EditCondition="Socket != None"))
	float SocketClearanceCm = 100.f;

	/**
	 * A ceiling over the whole map at RoofHeightCm (underground tunnels). It uses the Ceiling object channel, which blocks pawns
	 * and bullets but is not level geometry for agent perception (GameJam.h, ECC_Ceiling). Walls stop at the roof.
	 */
	UPROPERTY(EditAnywhere, Category="Look")
	bool bRoof = false;

	UPROPERTY(EditAnywhere, Category="Look", meta=(EditCondition="bRoof"))
	float RoofHeightCm = 400.f;

	UPROPERTY(EditAnywhere, Category="Look", meta=(EditCondition="bRoof"))
	TObjectPtr<UMaterialInterface> RoofMaterial;

	/** Ceiling lamps over the floor, about LampSpacingTiles apart (an underground map has no other light). */
	UPROPERTY(EditAnywhere, Category="Lamps")
	bool bLamps = false;

	UPROPERTY(EditAnywhere, Category="Lamps", meta=(EditCondition="bLamps", ClampMin=1))
	int32 LampSpacingTiles = 3;

	UPROPERTY(EditAnywhere, Category="Lamps", meta=(EditCondition="bLamps", ClampMin=0))
	int32 MaxLamps = 40;

	UPROPERTY(EditAnywhere, Category="Lamps", meta=(EditCondition="bLamps"))
	FLinearColor LampColor = FLinearColor(1.f, 0.48f, 0.14f);

	/** Candelas. */
	UPROPERTY(EditAnywhere, Category="Lamps", meta=(EditCondition="bLamps"))
	float LampIntensity = 22.f;

	UPROPERTY(EditAnywhere, Category="Lamps", meta=(EditCondition="bLamps"))
	float LampRadiusCm = 650.f;

	/** The lamp fitting's look (a small box under the roof), e.g. an emissive material. */
	UPROPERTY(EditAnywhere, Category="Lamps", meta=(EditCondition="bLamps"))
	TObjectPtr<UMaterialInterface> LampMaterial;

protected:
	virtual void BeginPlay() override;

private:
	bool BuildLayout();
	/** Socket mode: finds the socket, hides its gate, picks the size and mouth column, moves the actor. False = nothing to build. */
	bool PrepareSocket(GJMap::FGraphParams& OutParams, int32& OutEntryC);
	/** Socket mode: hides the gate and the fake tunnel end between the socket marker and the end of its tunnel. */
	void HideSocketStub(const AActor* Marker, float FloorZ);
	/** Socket mode: true if a map with this placement would stay clear of the level and of other generators. */
	bool IsRegionFree(const FTransform& Frame, int32 Rows, int32 Cols) const;
	void BuildLamps();
	void BuildGeometry();
	void ClearGeometry();
	/** A box: an instance of BoxMesh in Visual and, with bCollision, a BlockAll collider (bCeiling: the Ceiling object type). */
	void AddBlock(UInstancedStaticMeshComponent* Visual, const FVector& LocalCenter, const FVector& Size, bool bCollision, bool bCeiling = false);
	void PlacePlayers();
	void WriteMapFile() const;

	UPROPERTY(VisibleAnywhere, Category="Map") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category="Look") TObjectPtr<UInstancedStaticMeshComponent> FloorMeshes;
	UPROPERTY(VisibleAnywhere, Category="Look") TObjectPtr<UInstancedStaticMeshComponent> WallMeshes;
	UPROPERTY(VisibleAnywhere, Category="Look") TObjectPtr<UInstancedStaticMeshComponent> LowCoverMeshes;
	UPROPERTY(VisibleAnywhere, Category="Look") TObjectPtr<UInstancedStaticMeshComponent> HighCoverMeshes;
	UPROPERTY(VisibleAnywhere, Category="Look") TObjectPtr<UInstancedStaticMeshComponent> RoofMeshes;
	UPROPERTY(VisibleAnywhere, Category="Lamps") TObjectPtr<UInstancedStaticMeshComponent> LampMeshes;

	UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> Colliders;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lamps;
	/** Socket mode: the marker this map is attached to. */
	TWeakObjectPtr<AActor> SocketMarker;
	/** Socket mode: the world box this map occupies (other socket generators keep clear of it). */
	FBox ClaimedBox = FBox(ForceInit);
	UPROPERTY(Transient) TObjectPtr<APlayerStart> MapStart;

	GJMap::FLayout Layout;
	bool bHasLayout = false;

	FTimerHandle SelfTestTimer;
	int32 SelfTestProblems = 0;
};
