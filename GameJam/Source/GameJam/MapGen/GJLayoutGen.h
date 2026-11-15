// Procedural single-story layouts on FPS-Agent's 4 m tile grid (map format v1). See MAPGEN.md at the repo root.
//
// Plain C++17 with no Unreal types, so the same code runs in the game and in the standalone test harness (Tools/MapGen).
// A layout is fully determined by (family, level, seed). All randomness goes through FRng (splitmix64), never std::
// distributions, whose output differs between compilers.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace GJMap
{
	// Map format v1 constants (contract §11.1, PLAN §8.1).
	constexpr int TileCm = 400;
	constexpr int LevelCm = 100;
	constexpr int WallCm = 600;
	constexpr int LowCoverCm = 120;
	constexpr int HighCoverCm = 250;

	// `tiles` characters used by the generators. (Ramps `^ v > <` and void `x` are part of the format but not generated yet.)
	constexpr char Floor = '.';
	constexpr char Wall = '#';
	constexpr char LowCover = 'l';
	constexpr char HighCover = 'h';

	// Contract: S1 trained with cover blocks on at most 12% of the tiles.
	constexpr double MaxCoverFraction = 0.12;

	enum class EFamily : uint8_t { Graph, Wfc, Tunnels };
	const char* FamilyName(EFamily Family);
	bool ParseFamily(const std::string& Name, EFamily& Out);

	/** Deterministic PRNG (splitmix64). */
	struct FRng
	{
		uint64_t State;
		explicit FRng(uint64_t Seed) : State(Seed) {}
		uint64_t Next();
		/** Uniform in [Lo, Hi], inclusive. */
		int Range(int Lo, int Hi);
		/** Uniform in [0, 1). */
		double Unit();
	};

	/** Graph-first rooms: rooms placed without overlap, joined by a spanning tree plus the shortest extra edges (loops). */
	struct FGraphParams
	{
		int Cols = 16;           // map size in tiles (x 4 m)
		int Rows = 16;
		int NumRooms = 4;        // attempted; the placed count can be lower
		int RoomMin = 4;         // room side, tiles (3 tiles = 12 m is the smallest that meets the 10 m open-area rule)
		int RoomMax = 7;
		int Corridor = 2;        // corridor width, tiles (2 = 8 m preferred, 1 = 4 m minimum)
		double LoopFrac = 0.0;   // extra edges beyond the tree, as a fraction of NumRooms
		double Cover = 0.03;     // fraction of room tiles that become cover
		double HighFrac = 0.5;   // share of cover that is high (250 cm); the rest is low (120 cm)
	};

	/**
	 * Wave function collapse over 5 x 5-tile modules (20 m). Each module owns its north row and west column as its border.
	 * Sockets per side: 0 wall, 1 door (2-tile opening, 8 m), 2 open room-to-room edge (only the corner pillar stays).
	 */
	struct FWfcParams
	{
		int ModulesX = 3;        // map size in modules (x 20 m)
		int ModulesY = 3;
		double WRoom = 1.0;      // how common room modules are
		double WCorr = 0.2;      // how common corridor modules are
		double WOpen = 2.0;      // weight multiplier per open (2) edge of a room module
		double WEmpty = 0.05;    // how common solid modules are
		double MinKeep = 0.85;   // reject an attempt if the largest region keeps less of the open space
		int MaxTries = 30;
		bool bClearPillars = false; // clear the corner pillar where four open rooms meet
		double Cover = 0.03;
		double HighFrac = 0.5;
	};

	/**
	 * Storm-drain tunnels: the graph family with 4 m drains between small chambers, and one MOUTH, an opening in the
	 * south wall (the last row) at column EntryC, where the network joins a tunnel outside the map (a club's location socket).
	 * The start is the tile inside the mouth.
	 */
	FGraphParams TunnelPreset(double Level);

	struct FLayout
	{
		EFamily Family = EFamily::Graph;
		double Level = 1.0;
		uint32_t Seed = 0;

		int Rows = 0;
		int Cols = 0;
		std::string Tiles;   // Rows * Cols, row-major; row 0 is north (+X), column 0 is west (+Y points east)
		std::string Heights; // floor level '0'-'9' per tile (all '0' for now: single floor level)
		std::string Zones;   // 'a' player-start zone, 'd' far zone, '.' none

		int StartTile = -1;  // index into Tiles
		int MouthTile = -1;  // tunnels: the floor tile in the outer wall where the map opens to the outside, or -1
		float StartYawDeg = 0.f; // facing at the start, world yaw

		// Generator bookkeeping (metrics).
		int RoomsPlaced = 0;
		int PlannedEdges = 0;
		int Attempts = 1;

		int Index(int R, int C) const { return R * Cols + C; }
		char At(int R, int C) const { return Tiles[static_cast<size_t>(Index(R, C))]; }
		bool InBounds(int R, int C) const { return R >= 0 && C >= 0 && R < Rows && C < Cols; }
		bool IsWalkable(int R, int C) const { return InBounds(R, C) && At(R, C) == Floor; }

		/** World position of a tile centre relative to the map origin (cm), map format v1 convention. */
		void TileCenterCm(int R, int C, double& OutX, double& OutY) const;
		/** Tile under a position relative to the map origin (cm), or -1 when outside the map. */
		int TileAtCm(double X, double Y) const;
	};

	/** Generators return false (and leave Out partly filled) when they fail; skip that seed. They never throw. */
	bool GenerateGraph(const FGraphParams& P, uint32_t Seed, FLayout& Out);
	bool GenerateWfc(const FWfcParams& P, uint32_t Seed, FLayout& Out);
	/** EntryC: the mouth's column (clamped to [1, Cols - 2]); -1 = the middle column. */
	bool GenerateTunnels(const FGraphParams& P, int EntryC, uint32_t Seed, FLayout& Out);

	/** Presets L1-L5, interpolated for fractional levels (e.g. 2.5 is halfway between L2 and L3). Clamped to [1, 5]. */
	FGraphParams GraphPreset(double Level);
	FWfcParams WfcPreset(double Level);

	/** (family, level, seed) -> layout, using the presets (tunnels: mouth in the middle column). */
	bool MakeLayout(EFamily Family, double Level, uint32_t Seed, FLayout& Out);

	/** Walking distance in metres from one tile to every tile (8-connected, no corner cutting); -1 = unreachable. */
	std::vector<float> WalkDistance(const FLayout& L, int FromTile);

	/** A box covering Rows x Cols tiles of one kind (Wall, LowCover or HighCover). */
	struct FRect { int R0, C0, NumRows, NumCols; char Kind; };
	/** Greedy merge of same-kind tiles into larger boxes (keeps the number of colliders and meshes small). */
	std::vector<FRect> MergeRects(const FLayout& L);

	struct FMetrics
	{
		double SideXM = 0, SideYM = 0;  // map extent, metres
		double FreeAreaM2 = 0;
		double FreeFraction = 0;
		double FarthestWalkM = 0;       // from the start tile
		int CoverTiles = 0;
		int WallBoxes = 0;              // merged boxes of all kinds
		int RoomsPlaced = 0;            // graph family
		int PlannedLoops = 0;           // graph family: edges - rooms + 1
	};
	FMetrics ComputeMetrics(const FLayout& L);

	/** Checks the rules the game relies on; returns the list of problems (empty = valid). */
	std::vector<std::string> Validate(const FLayout& L);

	/** The layout as map format v1 text (header keys, then the tiles / heights / zones grids). */
	std::string ToMapFormatV1(const FLayout& L);
	/** One-line description: family, level, seed, size. */
	std::string Describe(const FLayout& L);
}
