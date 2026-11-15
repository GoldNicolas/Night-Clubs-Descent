// Standalone harness for GameJam/Source/GameJam/MapGen/GJLayoutGen (no Unreal needed). Build and run with ./build.sh.
//
//   mapgen_test stats [seeds]               per family and level: metrics (mean +- sd), timing, validation failures, determinism
//   mapgen_test show <graph|wfc> <level> <seed>   ASCII map, metrics and validation for one layout
//   mapgen_test dump <dir> [seeds]          writes map format v1 files <family>-L<level>-<seed>.txt (for render.py)
#include "GJLayoutGen.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using namespace GJMap;

namespace
{
	struct FStat
	{
		std::vector<double> V;
		void Add(double X) { V.push_back(X); }
		double Mean() const { double S = 0; for (double X : V) { S += X; } return V.empty() ? 0 : S / static_cast<double>(V.size()); }
		double Sd() const
		{
			if (V.size() < 2) { return 0; }
			const double M = Mean();
			double S = 0;
			for (double X : V) { S += (X - M) * (X - M); }
			return std::sqrt(S / static_cast<double>(V.size() - 1));
		}
	};

	void PrintLayout(const FLayout& L)
	{
		for (int R = 0; R < L.Rows; ++R)
		{
			std::string Line;
			for (int C = 0; C < L.Cols; ++C)
			{
				const size_t I = static_cast<size_t>(L.Index(R, C));
				char T = L.Tiles[I];
				if (static_cast<int>(I) == L.StartTile) { T = '@'; }
				else if (T == Floor && L.Zones[I] != '.') { T = L.Zones[I]; }
				Line += T;
				Line += ' ';
			}
			std::printf("%s\n", Line.c_str());
		}
	}

	int Stats(int Seeds)
	{
		int Failures = 0;
		std::printf("%-6s %-3s %10s %16s %14s %14s %10s %8s %8s %6s %6s\n", "family", "lvl", "side m", "free m2", "farthest m",
			"boxes", "cover", "rooms", "loops", "ms", "skip");
		for (const EFamily F : { EFamily::Graph, EFamily::Wfc })
		{
			for (int Level = 1; Level <= 5; ++Level)
			{
				FStat Side, Free, Far, Boxes, Cover, Rooms, Loops, Ms;
				int Skipped = 0;
				for (int Seed = 0; Seed < Seeds; ++Seed)
				{
					FLayout L;
					const auto T0 = std::chrono::steady_clock::now();
					const bool bOk = MakeLayout(F, Level, static_cast<uint32_t>(Seed), L);
					const auto T1 = std::chrono::steady_clock::now();
					if (!bOk) { ++Skipped; continue; }
					const std::vector<std::string> Errors = Validate(L);
					if (!Errors.empty())
					{
						++Failures;
						std::printf("  INVALID %s: %s\n", Describe(L).c_str(), Errors.front().c_str());
					}
					FLayout Again;
					MakeLayout(F, Level, static_cast<uint32_t>(Seed), Again);
					if (Again.Tiles != L.Tiles || Again.Zones != L.Zones || Again.StartTile != L.StartTile)
					{
						++Failures;
						std::printf("  NOT DETERMINISTIC %s\n", Describe(L).c_str());
					}
					const FMetrics M = ComputeMetrics(L);
					Side.Add(M.SideXM);
					Free.Add(M.FreeAreaM2);
					Far.Add(M.FarthestWalkM);
					Boxes.Add(M.WallBoxes);
					Cover.Add(M.CoverTiles);
					Rooms.Add(M.RoomsPlaced);
					Loops.Add(M.PlannedLoops);
					Ms.Add(std::chrono::duration<double, std::milli>(T1 - T0).count());
				}
				std::printf("%-6s L%-2d %10.0f %9.0f +- %4.0f %7.0f +- %3.0f %7.0f +- %3.0f %10.1f %8.1f %8.1f %6.2f %6d\n", FamilyName(F), Level,
					Side.Mean(), Free.Mean(), Free.Sd(), Far.Mean(), Far.Sd(), Boxes.Mean(), Boxes.Sd(), Cover.Mean(), Rooms.Mean(), Loops.Mean(),
					Ms.Mean(), Skipped);
				if (Skipped * 10 > Seeds) { ++Failures; std::printf("  TOO MANY SKIPPED SEEDS\n"); }
			}
		}
		std::printf(Failures ? "\nFAILED (%d problems)\n" : "\nOK\n", Failures);
		return Failures ? 1 : 0;
	}

	int Show(const std::string& Family, double Level, uint32_t Seed)
	{
		EFamily F;
		if (!ParseFamily(Family, F)) { std::fprintf(stderr, "family must be graph or wfc\n"); return 2; }
		FLayout L;
		if (!MakeLayout(F, Level, Seed, L)) { std::printf("generation failed for this seed (skip it)\n"); return 1; }
		std::printf("%s   (@ start, a/d zones, # wall, l/h low/high cover)\n\n", Describe(L).c_str());
		PrintLayout(L);
		const FMetrics M = ComputeMetrics(L);
		std::printf("\nfree %.0f m2 (%.0f%%), farthest walk %.0f m, cover tiles %d, boxes %d, rooms %d, planned loops %d, wfc attempts %d, start yaw %.0f\n",
			M.FreeAreaM2, 100 * M.FreeFraction, M.FarthestWalkM, M.CoverTiles, M.WallBoxes, M.RoomsPlaced, M.PlannedLoops, L.Attempts, L.StartYawDeg);
		const std::vector<std::string> Errors = Validate(L);
		for (const std::string& E : Errors) { std::printf("INVALID: %s\n", E.c_str()); }
		if (Errors.empty()) { std::printf("valid\n"); }
		return Errors.empty() ? 0 : 1;
	}

	int Dump(const std::string& Dir, int Seeds)
	{
		for (const EFamily F : { EFamily::Graph, EFamily::Wfc })
		{
			for (int Level = 1; Level <= 5; ++Level)
			{
				for (int Seed = 0; Seed < Seeds; ++Seed)
				{
					FLayout L;
					if (!MakeLayout(F, Level, static_cast<uint32_t>(Seed), L)) { continue; }
					const std::string Path = Dir + "/" + FamilyName(F) + "-L" + std::to_string(Level) + "-" + std::to_string(Seed) + ".txt";
					std::ofstream(Path) << ToMapFormatV1(L);
				}
			}
		}
		return 0;
	}
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "stats";
	if (Mode == "stats") { return Stats(Argc > 2 ? std::atoi(Argv[2]) : 30); }
	if (Mode == "show" && Argc > 4) { return Show(Argv[2], std::atof(Argv[3]), static_cast<uint32_t>(std::strtoul(Argv[4], nullptr, 10))); }
	if (Mode == "dump" && Argc > 2) { return Dump(Argv[2], Argc > 3 ? std::atoi(Argv[3]) : 3); }
	std::fprintf(stderr, "usage: mapgen_test stats [seeds] | show <graph|wfc> <level> <seed> | dump <dir> [seeds]\n");
	return 2;
}
