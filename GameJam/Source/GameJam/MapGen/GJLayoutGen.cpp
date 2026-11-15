// Procedural single-story layouts on FPS-Agent's 4 m tile grid. See GJLayoutGen.h and MAPGEN.md.
// Plain C++17 (no Unreal types, no exceptions) so Tools/MapGen can build and test it outside the engine.
#include "GJLayoutGen.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <deque>
#include <functional>
#include <queue>
#include <tuple>

namespace GJMap
{
namespace
{
	constexpr int Dr[4] = { -1, 0, 1, 0 }; // N E S W
	constexpr int Dc[4] = { 0, 1, 0, -1 };
	constexpr double SqrtTwo = 1.41421356237309504880;
	constexpr double RadToDeg = 57.29577951308232;

	double Lerp(double A, double B, double T) { return A + (B - A) * T; }
	int LerpInt(int A, int B, double T) { return static_cast<int>(std::lround(Lerp(A, B, T))); }

	/** Splits a level into the two neighbouring presets and the blend between them. */
	void SplitLevel(double Level, int& Lo, int& Hi, double& T)
	{
		const double L = std::min(5.0, std::max(1.0, Level));
		Lo = static_cast<int>(std::floor(L));
		Hi = std::min(5, Lo + 1);
		T = L - Lo;
	}

	void InitGrid(FLayout& L, int Rows, int Cols)
	{
		L.Rows = Rows;
		L.Cols = Cols;
		const size_t N = static_cast<size_t>(Rows) * static_cast<size_t>(Cols);
		L.Tiles.assign(N, Wall);
		L.Heights.assign(N, '0');
		L.Zones.assign(N, '.');
		L.StartTile = -1;
	}

	void SetTile(FLayout& L, int R, int C, char V)
	{
		if (L.InBounds(R, C)) { L.Tiles[static_cast<size_t>(L.Index(R, C))] = V; }
	}

	void FillRect(FLayout& L, int R0, int C0, int NumRows, int NumCols, char V)
	{
		for (int R = R0; R < R0 + NumRows; ++R)
		{
			for (int C = C0; C < C0 + NumCols; ++C) { SetTile(L, R, C, V); }
		}
	}

	void EnsureOuterWall(FLayout& L)
	{
		for (int C = 0; C < L.Cols; ++C) { SetTile(L, 0, C, Wall); SetTile(L, L.Rows - 1, C, Wall); }
		for (int R = 0; R < L.Rows; ++R) { SetTile(L, R, 0, Wall); SetTile(L, R, L.Cols - 1, Wall); }
	}

	int CountFloor(const FLayout& L)
	{
		return static_cast<int>(std::count(L.Tiles.begin(), L.Tiles.end(), Floor));
	}

	/** 4-connected flood fill over floor tiles. Returns the number of tiles reached; Mark gets 1 for each. */
	int Flood(const FLayout& L, int From, std::vector<uint8_t>& Mark)
	{
		Mark.assign(L.Tiles.size(), 0);
		if (From < 0 || L.Tiles[static_cast<size_t>(From)] != Floor) { return 0; }
		std::vector<int> Stack{ From };
		Mark[static_cast<size_t>(From)] = 1;
		int Count = 0;
		while (!Stack.empty())
		{
			const int I = Stack.back();
			Stack.pop_back();
			++Count;
			const int R = I / L.Cols;
			const int C = I % L.Cols;
			for (int D = 0; D < 4; ++D)
			{
				const int NR = R + Dr[D];
				const int NC = C + Dc[D];
				if (!L.IsWalkable(NR, NC)) { continue; }
				const size_t NI = static_cast<size_t>(L.Index(NR, NC));
				if (!Mark[NI]) { Mark[NI] = 1; Stack.push_back(static_cast<int>(NI)); }
			}
		}
		return Count;
	}

	/** Keeps only the largest 4-connected floor region; the rest becomes wall. Returns the kept fraction of the floor. */
	double KeepLargestRegion(FLayout& L)
	{
		const int Total = CountFloor(L);
		if (Total == 0) { return 0.0; }
		std::vector<uint8_t> Seen(L.Tiles.size(), 0);
		std::vector<uint8_t> Mark;
		std::vector<uint8_t> Best;
		int BestCount = 0;
		for (size_t I = 0; I < L.Tiles.size(); ++I)
		{
			if (L.Tiles[I] != Floor || Seen[I]) { continue; }
			const int Count = Flood(L, static_cast<int>(I), Mark);
			for (size_t J = 0; J < Mark.size(); ++J) { Seen[J] |= Mark[J]; }
			if (Count > BestCount) { BestCount = Count; Best = Mark; }
		}
		for (size_t I = 0; I < L.Tiles.size(); ++I)
		{
			if (L.Tiles[I] == Floor && !Best[I]) { L.Tiles[I] = Wall; }
		}
		return static_cast<double>(BestCount) / Total;
	}

	template <typename T>
	void Shuffle(std::vector<T>& V, FRng& Rng)
	{
		for (int I = static_cast<int>(V.size()) - 1; I > 0; --I)
		{
			const int J = Rng.Range(0, I);
			std::swap(V[static_cast<size_t>(I)], V[static_cast<size_t>(J)]);
		}
	}

	/**
	 * Cover blocks (120 / 250 cm) on room tiles, never next to a doorway or corridor tile, never disconnecting the floor.
	 * Capped at MaxCoverFraction of the walkable area (the S1 training density).
	 */
	void AddCover(FLayout& L, const std::vector<uint8_t>& IsRoom, double Cover, double HighFrac, FRng& Rng)
	{
		std::vector<int> Candidates;
		int RoomTiles = 0;
		for (int R = 1; R < L.Rows - 1; ++R)
		{
			for (int C = 1; C < L.Cols - 1; ++C)
			{
				const size_t I = static_cast<size_t>(L.Index(R, C));
				if (L.Tiles[I] != Floor || !IsRoom[I]) { continue; }
				++RoomTiles;
				bool bNearOpening = false;
				for (int D = 0; D < 4; ++D)
				{
					const int NR = R + Dr[D];
					const int NC = C + Dc[D];
					if (L.IsWalkable(NR, NC) && !IsRoom[static_cast<size_t>(L.Index(NR, NC))]) { bNearOpening = true; }
				}
				if (!bNearOpening) { Candidates.push_back(static_cast<int>(I)); }
			}
		}
		const int FloorTiles = CountFloor(L);
		const int Cap = static_cast<int>(std::floor(MaxCoverFraction * FloorTiles));
		const int Target = std::min(Cap, static_cast<int>(std::lround(Cover * RoomTiles)));
		Shuffle(Candidates, Rng);

		int Placed = 0;
		int Free = FloorTiles;
		std::vector<uint8_t> Mark;
		for (const int I : Candidates)
		{
			if (Placed >= Target) { break; }
			const char Kind = Rng.Unit() < HighFrac ? HighCover : LowCover;
			L.Tiles[static_cast<size_t>(I)] = Kind;
			// Any floor tile works as the flood seed: the floor must stay one region.
			int Seed = -1;
			const int R = I / L.Cols;
			const int C = I % L.Cols;
			for (int D = 0; D < 4 && Seed < 0; ++D)
			{
				if (L.IsWalkable(R + Dr[D], C + Dc[D])) { Seed = L.Index(R + Dr[D], C + Dc[D]); }
			}
			if (Seed >= 0 && Flood(L, Seed, Mark) == Free - 1)
			{
				++Placed;
				--Free;
			}
			else
			{
				L.Tiles[static_cast<size_t>(I)] = Floor;
			}
		}
	}

	float YawToward(const FLayout& L, int FromTile, int ToTile)
	{
		const int R1 = FromTile / L.Cols, C1 = FromTile % L.Cols;
		const int R2 = ToTile / L.Cols, C2 = ToTile % L.Cols;
		const double DX = -(R2 - R1); // row 0 is north (+X)
		const double DY = C2 - C1;
		if (DX == 0 && DY == 0) { return 0.f; }
		return static_cast<float>(std::atan2(DY, DX) * RadToDeg);
	}

	constexpr float ZoneRadiusM = 12.f;

	/** Marks floor tiles within RadiusM (walking) of a centre with Zone; when Room >= 0, only tiles of that room. */
	void PaintZone(FLayout& L, const std::vector<float>& DistFromCenter, float RadiusM, int Room, const std::vector<int>& RoomOf, char Zone)
	{
		for (size_t I = 0; I < L.Tiles.size(); ++I)
		{
			if (L.Tiles[I] != Floor || DistFromCenter[I] < 0.f || DistFromCenter[I] > RadiusM) { continue; }
			if (Room < 0 || RoomOf[I] == Room) { L.Zones[I] = Zone; }
		}
	}

	/** Start, facing and zones for a finished grid. Start is a random room tile (any floor tile if there are no rooms). */
	void PlaceStartAndZones(FLayout& L, const std::vector<int>& RoomOf, FRng& Rng)
	{
		std::vector<int> Floors;
		std::vector<int> RoomFloors;
		for (size_t I = 0; I < L.Tiles.size(); ++I)
		{
			if (L.Tiles[I] != Floor) { continue; }
			Floors.push_back(static_cast<int>(I));
			if (RoomOf[I] >= 0) { RoomFloors.push_back(static_cast<int>(I)); }
		}
		if (Floors.empty()) { return; }
		const std::vector<int>& Pool = RoomFloors.empty() ? Floors : RoomFloors;
		L.StartTile = Pool[static_cast<size_t>(Rng.Range(0, static_cast<int>(Pool.size()) - 1))];

		const std::vector<float> FromStart = WalkDistance(L, L.StartTile);
		int Far = L.StartTile;
		int Exit = -1;
		const int StartRoom = RoomOf[static_cast<size_t>(L.StartTile)];
		for (const int I : Floors)
		{
			const size_t U = static_cast<size_t>(I);
			if (FromStart[U] > FromStart[static_cast<size_t>(Far)]) { Far = I; }
			// The nearest floor tile outside the start room: face it, like a spawn facing the way out.
			if (StartRoom >= 0 && RoomOf[U] != StartRoom && (Exit < 0 || FromStart[U] < FromStart[static_cast<size_t>(Exit)])) { Exit = I; }
		}
		L.StartYawDeg = YawToward(L, L.StartTile, Exit >= 0 ? Exit : Far);

		// 'd' = up to 12 m (walking) around the farthest tile, inside its room; 'a' = the same around the start (wins on overlap).
		PaintZone(L, WalkDistance(L, Far), ZoneRadiusM, RoomOf[static_cast<size_t>(Far)], RoomOf, 'd');
		PaintZone(L, FromStart, ZoneRadiusM, StartRoom, RoomOf, 'a');
	}

	// ---------------------------------------------------------------- graph family

	struct FRoom { int R0, C0, NumRows, NumCols; };

	/** Corridor routing: cheapest path for a Width x Width band, penalising turns and other rooms (fixes unplanned openings). */
	bool RouteCorridor(const FLayout& L, const std::vector<int>& RoomOf, int RoomA, int RoomB, int StartR, int StartC,
		int GoalR, int GoalC, int Width, std::vector<std::pair<int, int>>& OutPath)
	{
		const int MaxR = L.Rows - 1 - Width; // the band (R..R+Width-1) stays inside the outer wall
		const int MaxC = L.Cols - 1 - Width;
		auto BandCost = [&](int R, int C) -> int
		{
			int Cost = 1;
			for (int BR = R; BR < R + Width; ++BR)
			{
				for (int BC = C; BC < C + Width; ++BC)
				{
					const int Room = RoomOf[static_cast<size_t>(L.Index(BR, BC))];
					if (Room >= 0 && Room != RoomA && Room != RoomB) { Cost += 25; }
				}
			}
			return Cost;
		};
		constexpr int TurnCost = 4;
		const size_t NumStates = static_cast<size_t>(L.Rows) * static_cast<size_t>(L.Cols) * 4;
		std::vector<int> Dist(NumStates, INT32_MAX);
		std::vector<int> Prev(NumStates, -1);
		using FItem = std::pair<int, int>; // cost, state
		std::priority_queue<FItem, std::vector<FItem>, std::greater<FItem>> Open;
		for (int D = 0; D < 4; ++D)
		{
			const int S = L.Index(StartR, StartC) * 4 + D;
			Dist[static_cast<size_t>(S)] = 0;
			Open.push({ 0, S });
		}
		int Goal = -1;
		while (!Open.empty())
		{
			const auto [Cost, S] = Open.top();
			Open.pop();
			if (Cost != Dist[static_cast<size_t>(S)]) { continue; }
			const int Cell = S / 4;
			const int Dir = S % 4;
			const int R = Cell / L.Cols;
			const int C = Cell % L.Cols;
			if (R == GoalR && C == GoalC) { Goal = S; break; }
			for (int D = 0; D < 4; ++D)
			{
				const int NR = R + Dr[D];
				const int NC = C + Dc[D];
				if (NR < 1 || NC < 1 || NR > MaxR || NC > MaxC) { continue; }
				const int NS = L.Index(NR, NC) * 4 + D;
				const int NCost = Cost + BandCost(NR, NC) + (D == Dir ? 0 : TurnCost);
				if (NCost < Dist[static_cast<size_t>(NS)])
				{
					Dist[static_cast<size_t>(NS)] = NCost;
					Prev[static_cast<size_t>(NS)] = S;
					Open.push({ NCost, NS });
				}
			}
		}
		if (Goal < 0) { return false; }
		OutPath.clear();
		for (int S = Goal; S >= 0; S = Prev[static_cast<size_t>(S)])
		{
			OutPath.push_back({ (S / 4) / L.Cols, (S / 4) % L.Cols });
		}
		return true;
	}

	/** A tile in the room where a Width-wide band can start, as close to the room centre as fits. */
	void BandAnchor(const FRoom& Room, int Width, int& OutR, int& OutC)
	{
		OutR = std::min(Room.R0 + (Room.NumRows - Width) / 2, Room.R0 + Room.NumRows - Width);
		OutC = std::min(Room.C0 + (Room.NumCols - Width) / 2, Room.C0 + Room.NumCols - Width);
		OutR = std::max(OutR, Room.R0);
		OutC = std::max(OutC, Room.C0);
	}

	struct FDisjointSet
	{
		std::vector<int> Parent;
		explicit FDisjointSet(int N) : Parent(static_cast<size_t>(N)) { for (int I = 0; I < N; ++I) { Parent[static_cast<size_t>(I)] = I; } }
		int Find(int X) { while (Parent[static_cast<size_t>(X)] != X) { X = Parent[static_cast<size_t>(X)] = Parent[static_cast<size_t>(Parent[static_cast<size_t>(X)])]; } return X; }
		bool Union(int A, int B) { A = Find(A); B = Find(B); if (A == B) { return false; } Parent[static_cast<size_t>(A)] = B; return true; }
	};

	// ---------------------------------------------------------------- WFC family

	constexpr int ModuleTiles = 5;
	enum : uint8_t { KindEmpty = 0, KindCorr = 1, KindRoom = 2 };

	struct FVariant
	{
		uint8_t Kind;
		std::array<uint8_t, 4> Sockets; // N E S W
		double Weight;
	};

	std::vector<FVariant> MakeVariants(const FWfcParams& P)
	{
		std::vector<FVariant> V;
		V.push_back({ KindEmpty, { 0, 0, 0, 0 }, std::max(1e-6, P.WEmpty) });
		std::vector<FVariant> Corr;
		std::vector<FVariant> Room;
		for (int Code = 0; Code < 81; ++Code)
		{
			std::array<uint8_t, 4> S{};
			int X = Code;
			int Ones = 0, Twos = 0;
			for (int D = 0; D < 4; ++D)
			{
				S[static_cast<size_t>(D)] = static_cast<uint8_t>(X % 3);
				X /= 3;
				Ones += S[static_cast<size_t>(D)] == 1;
				Twos += S[static_cast<size_t>(D)] == 2;
			}
			if (Ones + Twos == 0) { continue; }
			if (Twos == 0) { Corr.push_back({ KindCorr, S, Ones == 1 ? 0.5 : 1.0 }); } // dead ends are rarer
			Room.push_back({ KindRoom, S, std::pow(std::max(1e-6, P.WOpen), Twos) });
		}
		for (FVariant& C : Corr) { C.Weight *= std::max(1e-6, P.WCorr) / static_cast<double>(Corr.size()); V.push_back(C); }
		for (FVariant& R : Room) { R.Weight *= std::max(1e-6, P.WRoom) / static_cast<double>(Room.size()); V.push_back(R); }
		return V;
	}

	/** One collapse attempt. Out gets the chosen variant per module (row-major, ModulesY x ModulesX). */
	bool CollapseOnce(const FWfcParams& P, const std::vector<FVariant>& Variants, FRng& Rng, std::vector<int>& Out)
	{
		const int NX = P.ModulesX;
		const int NY = P.ModulesY;
		const size_t NV = Variants.size();
		std::vector<std::vector<uint8_t>> Alive(static_cast<size_t>(NX * NY), std::vector<uint8_t>(NV, 1));
		auto Cell = [NX](int Y, int X) { return static_cast<size_t>(Y * NX + X); };

		// Sides facing out of the map must be walls.
		for (int Y = 0; Y < NY; ++Y)
		{
			for (int X = 0; X < NX; ++X)
			{
				for (size_t V = 0; V < NV; ++V)
				{
					const auto& S = Variants[V].Sockets;
					if ((Y == 0 && S[0]) || (X == NX - 1 && S[1]) || (Y == NY - 1 && S[2]) || (X == 0 && S[3])) { Alive[Cell(Y, X)][V] = 0; }
				}
			}
		}

		auto Propagate = [&](std::deque<size_t> Queue) -> bool
		{
			while (!Queue.empty())
			{
				const size_t I = Queue.front();
				Queue.pop_front();
				const int Y = static_cast<int>(I) / NX;
				const int X = static_cast<int>(I) % NX;
				for (int D = 0; D < 4; ++D)
				{
					const int NYp = Y + Dr[D];
					const int NXp = X + Dc[D];
					if (NYp < 0 || NXp < 0 || NYp >= NY || NXp >= NX) { continue; }
					uint8_t Offered = 0; // socket values this cell can present on side D
					for (size_t V = 0; V < NV; ++V)
					{
						if (Alive[I][V]) { Offered |= static_cast<uint8_t>(1u << Variants[V].Sockets[static_cast<size_t>(D)]); }
					}
					const size_t N = Cell(NYp, NXp);
					const size_t Opp = static_cast<size_t>((D + 2) % 4);
					bool bChanged = false;
					bool bAny = false;
					for (size_t V = 0; V < NV; ++V)
					{
						if (!Alive[N][V]) { continue; }
						if (!(Offered & (1u << Variants[V].Sockets[Opp]))) { Alive[N][V] = 0; bChanged = true; }
						else { bAny = true; }
					}
					if (!bAny) { return false; }
					if (bChanged) { Queue.push_back(N); }
				}
			}
			return true;
		};

		std::deque<size_t> All;
		for (size_t I = 0; I < Alive.size(); ++I) { All.push_back(I); }
		if (!Propagate(All)) { return false; }

		for (;;)
		{
			// Lowest-entropy undecided module, ties broken by noise.
			size_t Best = SIZE_MAX;
			double BestH = 1e300;
			for (size_t I = 0; I < Alive.size(); ++I)
			{
				double Sum = 0, SumLog = 0;
				int Count = 0;
				for (size_t V = 0; V < NV; ++V)
				{
					if (!Alive[I][V]) { continue; }
					const double W = Variants[V].Weight;
					Sum += W;
					SumLog += W * std::log(W);
					++Count;
				}
				if (Count == 0) { return false; }
				if (Count == 1) { continue; }
				const double H = std::log(Sum) - SumLog / Sum + 1e-6 * Rng.Unit();
				if (H < BestH) { BestH = H; Best = I; }
			}
			if (Best == SIZE_MAX) { break; }
			double Sum = 0;
			for (size_t V = 0; V < NV; ++V) { if (Alive[Best][V]) { Sum += Variants[V].Weight; } }
			double Pick = Rng.Unit() * Sum;
			size_t Chosen = SIZE_MAX;
			for (size_t V = 0; V < NV; ++V)
			{
				if (!Alive[Best][V]) { continue; }
				Chosen = V;
				Pick -= Variants[V].Weight;
				if (Pick <= 0) { break; }
			}
			for (size_t V = 0; V < NV; ++V) { Alive[Best][V] = V == Chosen ? 1 : 0; }
			if (!Propagate({ Best })) { return false; }
		}

		Out.assign(Alive.size(), 0);
		for (size_t I = 0; I < Alive.size(); ++I)
		{
			for (size_t V = 0; V < NV; ++V) { if (Alive[I][V]) { Out[I] = static_cast<int>(V); break; } }
		}
		return true;
	}

	/** Draws the modules into tiles. Each module owns its north row and west column (its border). RoomOf = module id for room interiors. */
	void DrawModules(const FWfcParams& P, const std::vector<FVariant>& Variants, const std::vector<int>& Chosen, FLayout& L,
		std::vector<int>& RoomOf)
	{
		const int NX = P.ModulesX;
		InitGrid(L, P.ModulesY * ModuleTiles + 1, NX * ModuleTiles + 1);
		RoomOf.assign(L.Tiles.size(), -1);
		for (int Y = 0; Y < P.ModulesY; ++Y)
		{
			for (int X = 0; X < NX; ++X)
			{
				const int Id = Y * NX + X;
				const FVariant& V = Variants[static_cast<size_t>(Chosen[static_cast<size_t>(Id)])];
				const int R0 = Y * ModuleTiles;
				const int C0 = X * ModuleTiles;
				const uint8_t N = V.Sockets[0], E = V.Sockets[1], S = V.Sockets[2], W = V.Sockets[3];
				if (V.Kind == KindRoom)
				{
					FillRect(L, R0 + 1, C0 + 1, 4, 4, Floor);
					for (int R = R0 + 1; R < R0 + 5; ++R)
					{
						for (int C = C0 + 1; C < C0 + 5; ++C) { RoomOf[static_cast<size_t>(L.Index(R, C))] = Id; }
					}
					if (N == 1) { FillRect(L, R0, C0 + 2, 1, 2, Floor); }
					if (N == 2) { FillRect(L, R0, C0 + 1, 1, 4, Floor); }
					if (W == 1) { FillRect(L, R0 + 2, C0, 2, 1, Floor); }
					if (W == 2) { FillRect(L, R0 + 1, C0, 4, 1, Floor); }
				}
				else if (V.Kind == KindCorr)
				{
					FillRect(L, R0 + 2, C0 + 2, 2, 2, Floor); // hub
					if (N == 1) { FillRect(L, R0, C0 + 2, 2, 2, Floor); }
					if (S == 1) { FillRect(L, R0 + 4, C0 + 2, 1, 2, Floor); }
					if (E == 1) { FillRect(L, R0 + 2, C0 + 4, 2, 1, Floor); }
					if (W == 1) { FillRect(L, R0 + 2, C0, 2, 2, Floor); }
				}
				// The south and east borders belong to the neighbours, which draw the same socket from their side.
			}
		}

		if (P.bClearPillars)
		{
			auto Mod = [&](int Y, int X) -> const FVariant& { return Variants[static_cast<size_t>(Chosen[static_cast<size_t>(Y * NX + X)])]; };
			for (int Y = 1; Y < P.ModulesY; ++Y)
			{
				for (int X = 1; X < NX; ++X)
				{
					const FVariant& A = Mod(Y - 1, X - 1);
					const FVariant& B = Mod(Y - 1, X);
					const FVariant& C = Mod(Y, X - 1);
					if (A.Sockets[1] == 2 && C.Sockets[1] == 2 && A.Sockets[2] == 2 && B.Sockets[2] == 2)
					{
						SetTile(L, Y * ModuleTiles, X * ModuleTiles, Floor);
					}
				}
			}
		}
	}
} // namespace

// ---------------------------------------------------------------- public API

const char* FamilyName(EFamily Family)
{
	return Family == EFamily::Graph ? "graph" : "wfc";
}

bool ParseFamily(const std::string& Name, EFamily& Out)
{
	if (Name == "graph") { Out = EFamily::Graph; return true; }
	if (Name == "wfc") { Out = EFamily::Wfc; return true; }
	return false;
}

uint64_t FRng::Next()
{
	uint64_t Z = (State += 0x9E3779B97F4A7C15ull);
	Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
	Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
	return Z ^ (Z >> 31);
}

int FRng::Range(int Lo, int Hi)
{
	if (Hi <= Lo) { return Lo; }
	const uint64_t Span = static_cast<uint64_t>(Hi - Lo) + 1;
	return Lo + static_cast<int>(Next() % Span);
}

double FRng::Unit()
{
	return static_cast<double>(Next() >> 11) * (1.0 / 9007199254740992.0);
}

void FLayout::TileCenterCm(int R, int C, double& OutX, double& OutY) const
{
	OutX = (Rows - 1 - R + 0.5) * TileCm;
	OutY = (C + 0.5) * TileCm;
}

int FLayout::TileAtCm(double X, double Y) const
{
	const int R = Rows - 1 - static_cast<int>(std::floor(X / TileCm));
	const int C = static_cast<int>(std::floor(Y / TileCm));
	return InBounds(R, C) ? Index(R, C) : -1;
}

bool GenerateGraph(const FGraphParams& P, uint32_t Seed, FLayout& Out)
{
	FRng Rng(0x6A09E667F3BCC908ull ^ Seed);
	Out = FLayout();
	Out.Family = EFamily::Graph;
	Out.Seed = Seed;
	const int Rows = std::max(8, P.Rows);
	const int Cols = std::max(8, P.Cols);
	const int Width = std::max(1, P.Corridor);
	const int RoomMin = std::max(std::max(3, Width), P.RoomMin);
	const int RoomMax = std::max(RoomMin, P.RoomMax);
	InitGrid(Out, Rows, Cols);
	std::vector<int> RoomOf(Out.Tiles.size(), -1);

	// Rooms: no overlap, at least one wall tile between them and inside the outer wall.
	std::vector<FRoom> Rooms;
	for (int Attempt = 0; Attempt < P.NumRooms * 40 && static_cast<int>(Rooms.size()) < P.NumRooms; ++Attempt)
	{
		FRoom Room;
		Room.NumRows = Rng.Range(RoomMin, RoomMax);
		Room.NumCols = Rng.Range(RoomMin, RoomMax);
		if (Room.NumRows > Rows - 2 || Room.NumCols > Cols - 2) { continue; }
		Room.R0 = Rng.Range(1, Rows - 1 - Room.NumRows);
		Room.C0 = Rng.Range(1, Cols - 1 - Room.NumCols);
		const bool bOverlaps = std::any_of(Rooms.begin(), Rooms.end(), [&Room](const FRoom& O)
		{
			return Room.R0 <= O.R0 + O.NumRows && O.R0 <= Room.R0 + Room.NumRows
				&& Room.C0 <= O.C0 + O.NumCols && O.C0 <= Room.C0 + Room.NumCols;
		});
		if (bOverlaps) { continue; }
		const int Id = static_cast<int>(Rooms.size());
		Rooms.push_back(Room);
		FillRect(Out, Room.R0, Room.C0, Room.NumRows, Room.NumCols, Floor);
		for (int R = Room.R0; R < Room.R0 + Room.NumRows; ++R)
		{
			for (int C = Room.C0; C < Room.C0 + Room.NumCols; ++C) { RoomOf[static_cast<size_t>(Out.Index(R, C))] = Id; }
		}
	}
	const int NumRooms = static_cast<int>(Rooms.size());
	Out.RoomsPlaced = NumRooms;
	if (NumRooms < 2) { return false; }

	// Edges: spanning tree over centre distances, then the shortest non-tree edges as loops.
	std::vector<std::tuple<int, int, int>> Edges; // distance, a, b
	for (int A = 0; A < NumRooms; ++A)
	{
		for (int B = A + 1; B < NumRooms; ++B)
		{
			const FRoom& RA = Rooms[static_cast<size_t>(A)];
			const FRoom& RB = Rooms[static_cast<size_t>(B)];
			const int D = std::abs((2 * RA.R0 + RA.NumRows) - (2 * RB.R0 + RB.NumRows)) + std::abs((2 * RA.C0 + RA.NumCols) - (2 * RB.C0 + RB.NumCols));
			Edges.emplace_back(D, A, B);
		}
	}
	std::sort(Edges.begin(), Edges.end());
	FDisjointSet Sets(NumRooms);
	std::vector<std::pair<int, int>> Chosen;
	std::vector<std::pair<int, int>> Spare;
	for (const auto& [D, A, B] : Edges)
	{
		(void)D;
		if (Sets.Union(A, B)) { Chosen.emplace_back(A, B); }
		else { Spare.emplace_back(A, B); }
	}
	const int Loops = std::min(static_cast<int>(Spare.size()), static_cast<int>(std::lround(P.LoopFrac * P.NumRooms)));
	for (int I = 0; I < Loops; ++I) { Chosen.push_back(Spare[static_cast<size_t>(I)]); }
	Out.PlannedEdges = static_cast<int>(Chosen.size());

	// Corridors.
	std::vector<std::pair<int, int>> Path;
	for (const auto& [A, B] : Chosen)
	{
		int SR, SC, GR, GC;
		BandAnchor(Rooms[static_cast<size_t>(A)], Width, SR, SC);
		BandAnchor(Rooms[static_cast<size_t>(B)], Width, GR, GC);
		if (!RouteCorridor(Out, RoomOf, A, B, SR, SC, GR, GC, Width, Path)) { continue; }
		for (const auto& [R, C] : Path) { FillRect(Out, R, C, Width, Width, Floor); }
	}
	EnsureOuterWall(Out);
	if (KeepLargestRegion(Out) < 0.999) { return false; } // the tree connects every room; anything else is a bug

	std::vector<uint8_t> IsRoom(Out.Tiles.size(), 0);
	for (size_t I = 0; I < IsRoom.size(); ++I) { IsRoom[I] = RoomOf[I] >= 0; }
	AddCover(Out, IsRoom, P.Cover, P.HighFrac, Rng);
	PlaceStartAndZones(Out, RoomOf, Rng);
	return Out.StartTile >= 0;
}

bool GenerateWfc(const FWfcParams& P, uint32_t Seed, FLayout& Out)
{
	FRng Rng(0xBB67AE8584CAA73Bull ^ Seed);
	FWfcParams Q = P;
	Q.ModulesX = std::max(2, P.ModulesX);
	Q.ModulesY = std::max(2, P.ModulesY);
	const std::vector<FVariant> Variants = MakeVariants(Q);
	std::vector<int> Chosen;
	std::vector<int> RoomOf;
	for (int Try = 1; Try <= std::max(1, Q.MaxTries); ++Try)
	{
		if (!CollapseOnce(Q, Variants, Rng, Chosen)) { continue; }
		Out = FLayout();
		Out.Family = EFamily::Wfc;
		Out.Seed = Seed;
		Out.Attempts = Try;
		DrawModules(Q, Variants, Chosen, Out, RoomOf);
		EnsureOuterWall(Out);
		const int FloorBefore = CountFloor(Out);
		if (FloorBefore < 16 || KeepLargestRegion(Out) < Q.MinKeep) { continue; }
		for (size_t I = 0; I < RoomOf.size(); ++I) { if (Out.Tiles[I] != Floor) { RoomOf[I] = -1; } }

		std::vector<uint8_t> IsRoom(Out.Tiles.size(), 0);
		for (size_t I = 0; I < IsRoom.size(); ++I) { IsRoom[I] = RoomOf[I] >= 0; }
		AddCover(Out, IsRoom, Q.Cover, Q.HighFrac, Rng);
		// Open (2) edges merge room modules into halls: zones use the merged hall, not the single module.
		std::vector<int> HallOf(RoomOf.size(), -1);
		{
			std::vector<int> Stack;
			for (size_t I = 0; I < RoomOf.size(); ++I)
			{
				if (RoomOf[I] < 0 || HallOf[I] >= 0) { continue; }
				HallOf[I] = static_cast<int>(I);
				Stack.push_back(static_cast<int>(I));
				while (!Stack.empty())
				{
					const int J = Stack.back();
					Stack.pop_back();
					for (int D = 0; D < 4; ++D)
					{
						const int NR = J / Out.Cols + Dr[D];
						const int NC = J % Out.Cols + Dc[D];
						if (!Out.InBounds(NR, NC)) { continue; }
						const size_t K = static_cast<size_t>(Out.Index(NR, NC));
						// Step through an open seam tile (not a room interior, but floor between two room interiors).
						if (RoomOf[K] >= 0 && HallOf[K] < 0) { HallOf[K] = static_cast<int>(I); Stack.push_back(static_cast<int>(K)); }
					}
				}
			}
		}
		// Seams of open edges are floor that belongs to no module interior; merge rooms across them.
		for (int R = 1; R < Out.Rows - 1; ++R)
		{
			for (int C = 1; C < Out.Cols - 1; ++C)
			{
				const size_t I = static_cast<size_t>(Out.Index(R, C));
				if (Out.Tiles[I] != Floor || RoomOf[I] >= 0) { continue; }
				const int NorthH = HallOf[static_cast<size_t>(Out.Index(R - 1, C))];
				const int SouthH = HallOf[static_cast<size_t>(Out.Index(R + 1, C))];
				const int WestH = HallOf[static_cast<size_t>(Out.Index(R, C - 1))];
				const int EastH = HallOf[static_cast<size_t>(Out.Index(R, C + 1))];
				int A = -1, B = -1;
				if (NorthH >= 0 && SouthH >= 0) { A = NorthH; B = SouthH; }
				else if (WestH >= 0 && EastH >= 0) { A = WestH; B = EastH; }
				if (A < 0) { continue; }
				// Only an open edge (2) seam: a door seam (1) has wall tiles beside it along the border.
				const bool bVertical = NorthH >= 0 && SouthH >= 0;
				const int SideR1 = bVertical ? R : R - 1, SideC1 = bVertical ? C - 1 : C;
				const int SideR2 = bVertical ? R : R + 1, SideC2 = bVertical ? C + 1 : C;
				const bool bOpenSeam = Out.At(SideR1, SideC1) == Floor && Out.At(SideR2, SideC2) == Floor;
				if (!bOpenSeam) { continue; }
				const int Keep = std::min(A, B);
				const int Drop = std::max(A, B);
				for (int& H : HallOf) { if (H == Drop) { H = Keep; } }
				HallOf[I] = Keep;
			}
		}
		// The seam's end tiles (next to the corner pillars) sit between two tiles of one hall: they belong to it too.
		for (int R = 1; R < Out.Rows - 1; ++R)
		{
			for (int C = 1; C < Out.Cols - 1; ++C)
			{
				const size_t I = static_cast<size_t>(Out.Index(R, C));
				if (Out.Tiles[I] != Floor || HallOf[I] >= 0) { continue; }
				const int NorthH = HallOf[static_cast<size_t>(Out.Index(R - 1, C))];
				const int WestH = HallOf[static_cast<size_t>(Out.Index(R, C - 1))];
				if (NorthH >= 0 && NorthH == HallOf[static_cast<size_t>(Out.Index(R + 1, C))]) { HallOf[I] = NorthH; }
				else if (WestH >= 0 && WestH == HallOf[static_cast<size_t>(Out.Index(R, C + 1))]) { HallOf[I] = WestH; }
			}
		}
		PlaceStartAndZones(Out, HallOf, Rng);
		if (Out.StartTile >= 0) { return true; }
	}
	return false;
}

FGraphParams GraphPreset(double Level)
{
	//                      size rooms min max corr loops  cover
	static const double T[5][7] = {
		{ 16,  4, 4, 7, 2, 0.00, 0.04 },
		{ 20,  6, 4, 6, 2, 0.15, 0.06 },
		{ 24,  9, 3, 6, 2, 0.25, 0.08 },
		{ 28, 12, 3, 5, 1, 0.35, 0.10 },
		{ 32, 16, 3, 5, 1, 0.50, 0.12 },
	};
	int Lo, Hi;
	double F;
	SplitLevel(Level, Lo, Hi, F);
	const double* A = T[Lo - 1];
	const double* B = T[Hi - 1];
	FGraphParams P;
	P.Rows = P.Cols = LerpInt(static_cast<int>(A[0]), static_cast<int>(B[0]), F);
	P.NumRooms = LerpInt(static_cast<int>(A[1]), static_cast<int>(B[1]), F);
	P.RoomMin = LerpInt(static_cast<int>(A[2]), static_cast<int>(B[2]), F);
	P.RoomMax = LerpInt(static_cast<int>(A[3]), static_cast<int>(B[3]), F);
	P.Corridor = LerpInt(static_cast<int>(A[4]), static_cast<int>(B[4]), F);
	P.LoopFrac = Lerp(A[5], B[5], F);
	P.Cover = Lerp(A[6], B[6], F);
	return P;
}

FWfcParams WfcPreset(double Level)
{
	//                   modules room  corr  open  empty cover
	static const double T[5][6] = {
		{ 3, 1.00, 0.20, 2.0, 0.05, 0.04 },
		{ 4, 0.90, 0.35, 1.5, 0.10, 0.06 },
		{ 5, 0.70, 0.50, 1.0, 0.15, 0.08 },
		{ 6, 0.50, 0.70, 0.7, 0.20, 0.10 },
		{ 7, 0.35, 0.90, 0.5, 0.25, 0.12 },
	};
	int Lo, Hi;
	double F;
	SplitLevel(Level, Lo, Hi, F);
	const double* A = T[Lo - 1];
	const double* B = T[Hi - 1];
	FWfcParams P;
	P.ModulesX = P.ModulesY = LerpInt(static_cast<int>(A[0]), static_cast<int>(B[0]), F);
	P.WRoom = Lerp(A[1], B[1], F);
	P.WCorr = Lerp(A[2], B[2], F);
	P.WOpen = Lerp(A[3], B[3], F);
	P.WEmpty = Lerp(A[4], B[4], F);
	P.Cover = Lerp(A[5], B[5], F);
	return P;
}

bool MakeLayout(EFamily Family, double Level, uint32_t Seed, FLayout& Out)
{
	const bool bOk = Family == EFamily::Graph ? GenerateGraph(GraphPreset(Level), Seed, Out) : GenerateWfc(WfcPreset(Level), Seed, Out);
	Out.Level = Level;
	return bOk;
}

std::vector<float> WalkDistance(const FLayout& L, int FromTile)
{
	std::vector<float> Dist(L.Tiles.size(), -1.f);
	if (FromTile < 0 || static_cast<size_t>(FromTile) >= L.Tiles.size() || L.Tiles[static_cast<size_t>(FromTile)] != Floor) { return Dist; }
	const double Step = TileCm / 100.0;
	std::vector<double> D(L.Tiles.size(), 1e300);
	using FItem = std::pair<double, int>;
	std::priority_queue<FItem, std::vector<FItem>, std::greater<FItem>> Open;
	D[static_cast<size_t>(FromTile)] = 0;
	Open.push({ 0.0, FromTile });
	while (!Open.empty())
	{
		const auto [Cost, I] = Open.top();
		Open.pop();
		if (Cost > D[static_cast<size_t>(I)]) { continue; }
		const int R = I / L.Cols;
		const int C = I % L.Cols;
		for (int DR = -1; DR <= 1; ++DR)
		{
			for (int DC = -1; DC <= 1; ++DC)
			{
				if ((DR == 0 && DC == 0) || !L.IsWalkable(R + DR, C + DC)) { continue; }
				const bool bDiagonal = DR != 0 && DC != 0;
				if (bDiagonal && (!L.IsWalkable(R + DR, C) || !L.IsWalkable(R, C + DC))) { continue; } // no corner cutting
				const int N = L.Index(R + DR, C + DC);
				const double NCost = Cost + (bDiagonal ? Step * SqrtTwo : Step);
				if (NCost < D[static_cast<size_t>(N)]) { D[static_cast<size_t>(N)] = NCost; Open.push({ NCost, N }); }
			}
		}
	}
	for (size_t I = 0; I < D.size(); ++I) { if (D[I] < 1e299) { Dist[I] = static_cast<float>(D[I]); } }
	return Dist;
}

std::vector<FRect> MergeRects(const FLayout& L)
{
	std::vector<FRect> Out;
	std::vector<uint8_t> Used(L.Tiles.size(), 0);
	for (int R = 0; R < L.Rows; ++R)
	{
		for (int C = 0; C < L.Cols; ++C)
		{
			const size_t I = static_cast<size_t>(L.Index(R, C));
			const char Kind = L.Tiles[I];
			if (Used[I] || (Kind != Wall && Kind != LowCover && Kind != HighCover)) { continue; }
			auto Free = [&](int RR, int CC) { return L.InBounds(RR, CC) && L.At(RR, CC) == Kind && !Used[static_cast<size_t>(L.Index(RR, CC))]; };
			int NumCols = 1;
			while (Free(R, C + NumCols)) { ++NumCols; }
			int NumRows = 1;
			for (;;)
			{
				bool bRowOk = true;
				for (int CC = C; CC < C + NumCols && bRowOk; ++CC) { bRowOk = Free(R + NumRows, CC); }
				if (!bRowOk) { break; }
				++NumRows;
			}
			for (int RR = R; RR < R + NumRows; ++RR)
			{
				for (int CC = C; CC < C + NumCols; ++CC) { Used[static_cast<size_t>(L.Index(RR, CC))] = 1; }
			}
			Out.push_back({ R, C, NumRows, NumCols, Kind });
		}
	}
	return Out;
}

FMetrics ComputeMetrics(const FLayout& L)
{
	FMetrics M;
	const double TileM = TileCm / 100.0;
	M.SideXM = L.Rows * TileM;
	M.SideYM = L.Cols * TileM;
	const int Floors = CountFloor(L);
	M.FreeAreaM2 = Floors * TileM * TileM;
	M.FreeFraction = L.Tiles.empty() ? 0.0 : static_cast<double>(Floors) / static_cast<double>(L.Tiles.size());
	for (const float D : WalkDistance(L, L.StartTile)) { M.FarthestWalkM = std::max(M.FarthestWalkM, static_cast<double>(D)); }
	M.CoverTiles = static_cast<int>(std::count(L.Tiles.begin(), L.Tiles.end(), LowCover) + std::count(L.Tiles.begin(), L.Tiles.end(), HighCover));
	M.WallBoxes = static_cast<int>(MergeRects(L).size());
	M.RoomsPlaced = L.RoomsPlaced;
	M.PlannedLoops = L.Family == EFamily::Graph && L.RoomsPlaced > 0 ? L.PlannedEdges - L.RoomsPlaced + 1 : 0;
	return M;
}

std::vector<std::string> Validate(const FLayout& L)
{
	std::vector<std::string> Errors;
	const size_t N = static_cast<size_t>(L.Rows) * static_cast<size_t>(L.Cols);
	if (L.Rows < 3 || L.Cols < 3 || L.Tiles.size() != N || L.Heights.size() != N || L.Zones.size() != N)
	{
		Errors.push_back("grid sizes are inconsistent");
		return Errors;
	}
	for (int R = 0; R < L.Rows; ++R)
	{
		for (int C = 0; C < L.Cols; ++C)
		{
			const bool bEdge = R == 0 || C == 0 || R == L.Rows - 1 || C == L.Cols - 1;
			const char T = L.At(R, C);
			if (bEdge && T != Wall) { Errors.push_back("outer ring is not all wall"); R = L.Rows; break; }
			if (T != Floor && T != Wall && T != LowCover && T != HighCover) { Errors.push_back(std::string("unknown tile '") + T + "'"); }
		}
	}
	if (L.StartTile < 0 || L.Tiles[static_cast<size_t>(L.StartTile)] != Floor) { Errors.push_back("start is not on a floor tile"); return Errors; }
	const std::vector<float> Dist = WalkDistance(L, L.StartTile);
	int Unreachable = 0, ZoneA = 0, ZoneD = 0, Floors = 0, Cover = 0;
	for (size_t I = 0; I < N; ++I)
	{
		const char T = L.Tiles[I];
		Floors += T == Floor;
		Cover += T == LowCover || T == HighCover;
		if (T == Floor && Dist[I] < 0.f) { ++Unreachable; }
		if (L.Heights[I] != '0') { Errors.push_back("heights other than level 0 are not generated yet"); break; }
		if (L.Zones[I] != '.' && T != Floor) { Errors.push_back("a zone tile is not floor"); break; }
		ZoneA += L.Zones[I] == 'a';
		ZoneD += L.Zones[I] == 'd';
	}
	if (Unreachable > 0) { Errors.push_back(std::to_string(Unreachable) + " floor tiles are unreachable from the start"); }
	if (ZoneA == 0) { Errors.push_back("no 'a' zone"); }
	if (ZoneD == 0) { Errors.push_back("no 'd' zone"); }
	if (Floors + Cover > 0 && Cover > MaxCoverFraction * (Floors + Cover) + 1) { Errors.push_back("cover exceeds 12% of the walkable area"); }
	return Errors;
}

std::string ToMapFormatV1(const FLayout& L)
{
	std::string S;
	char Buf[256];
	std::snprintf(Buf, sizeof(Buf), "# GameJam layoutgen: family=%s level=%.2f seed=%u\n", FamilyName(L.Family), L.Level, L.Seed);
	S += Buf;
	std::snprintf(Buf, sizeof(Buf), "tile_cm %d\nlevel_cm %d\nwall_cm %d\nrows %d\ncols %d\n", TileCm, LevelCm, WallCm, L.Rows, L.Cols);
	S += Buf;
	const std::pair<const char*, const std::string*> Grids[3] = { { "tiles", &L.Tiles }, { "heights", &L.Heights }, { "zones", &L.Zones } };
	for (const auto& [Name, Grid] : Grids)
	{
		S += Name;
		S += '\n';
		for (int R = 0; R < L.Rows; ++R)
		{
			S.append(*Grid, static_cast<size_t>(R) * static_cast<size_t>(L.Cols), static_cast<size_t>(L.Cols));
			S += '\n';
		}
	}
	return S;
}

std::string Describe(const FLayout& L)
{
	char Buf[160];
	std::snprintf(Buf, sizeof(Buf), "%s L%.2f seed %u: %d x %d tiles (%d x %d m)", FamilyName(L.Family), L.Level, L.Seed, L.Rows, L.Cols,
		L.Rows * TileCm / 100, L.Cols * TileCm / 100);
	return Buf;
}
} // namespace GJMap
