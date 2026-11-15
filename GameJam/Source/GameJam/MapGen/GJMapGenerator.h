#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJLayoutGen.h"
#include "GJMapGenerator.generated.h"

class APlayerStart;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

UENUM(BlueprintType)
enum class EGJMapFamily : uint8
{
	/** Graph-first rooms: separate rooms joined by corridors, loops at higher levels. */
	Graph,
	/** Wave function collapse: 20 m modules of halls, rooms and corridor networks. */
	Wfc,
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
 * Console: gj.Map <graph|wfc> <level> [seed] rebuilds the map while playing; gj.MapInfo prints the current one.
 */
UCLASS()
class GAMEJAM_API AGJMapGenerator : public AActor
{
	GENERATED_BODY()

public:
	AGJMapGenerator();

	/** The world's map generator, or null. */
	static AGJMapGenerator* Find(const UObject* WorldContext);

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

	/** gj.MapSelfTest: after DelaySeconds check this map, rebuild it as the other family, check again, then optionally quit (exit code 1 on failure). */
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

protected:
	virtual void BeginPlay() override;

private:
	bool BuildLayout();
	void BuildGeometry();
	void ClearGeometry();
	void AddBlock(UInstancedStaticMeshComponent* Visual, const FVector& LocalCenter, const FVector& Size, bool bCollision);
	void PlacePlayers();
	void WriteMapFile() const;

	UPROPERTY(VisibleAnywhere, Category="Map") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category="Look") TObjectPtr<UInstancedStaticMeshComponent> FloorMeshes;
	UPROPERTY(VisibleAnywhere, Category="Look") TObjectPtr<UInstancedStaticMeshComponent> WallMeshes;
	UPROPERTY(VisibleAnywhere, Category="Look") TObjectPtr<UInstancedStaticMeshComponent> LowCoverMeshes;
	UPROPERTY(VisibleAnywhere, Category="Look") TObjectPtr<UInstancedStaticMeshComponent> HighCoverMeshes;

	UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> Colliders;
	UPROPERTY(Transient) TObjectPtr<APlayerStart> MapStart;

	GJMap::FLayout Layout;
	bool bHasLayout = false;

	FTimerHandle SelfTestTimer;
	int32 SelfTestProblems = 0;
};
