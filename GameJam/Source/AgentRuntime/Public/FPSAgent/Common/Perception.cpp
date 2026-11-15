#include "Common/Perception.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"

namespace
{
	FCollisionObjectQueryParams LevelGeometry()
	{
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		return Objects;
	}
}

namespace FPSPerception
{
	FVector2D AimErrorDeg(const FVector& Eye, const FRotator& View, const FVector& Target)
	{
		const FRotator To = (Target - Eye).Rotation();
		return FVector2D(FRotator::NormalizeAxis(To.Yaw - View.Yaw), FRotator::NormalizeAxis(To.Pitch - FRotator::NormalizeAxis(View.Pitch)));
	}

	bool InFieldOfView(const FVector& Eye, const FRotator& View, const FVector& Target, float FovDeg, float VerticalFovDeg)
	{
		const FVector2D Err = AimErrorDeg(Eye, View, Target);
		return FMath::Abs(Err.X) <= 0.5 * FovDeg && FMath::Abs(Err.Y) <= 0.5 * VerticalFovDeg;
	}

	FVector ToViewFrame(const FVector& WorldVector, float ViewYawDeg)
	{
		return FRotator(0.0, ViewYawDeg, 0.0).UnrotateVector(WorldVector);
	}

	TArray<FVector> RayDirections(float ViewYawDeg, int32 YawCount, const TArray<float>& PitchesDeg)
	{
		TArray<FVector> Out;
		const int32 N = FMath::Max(1, YawCount);
		Out.Reserve(N * PitchesDeg.Num());
		for (const float Pitch : PitchesDeg)
		{
			for (int32 k = 0; k < N; ++k)
			{
				Out.Add(FRotator(Pitch, ViewYawDeg + 360.0 * k / N, 0.0).Vector());
			}
		}
		return Out;
	}

	void UpdateTracks(TArray<FTrack>& Tracks, const TArray<FSighting>& Sightings, double Now, float MemorySeconds)
	{
		for (const FSighting& S : Sightings)
		{
			const int32 Idx = Tracks.IndexOfByPredicate([&S](const FTrack& T) { return T.BotIndex == S.BotIndex; });
			if (!S.bAlive)
			{
				if (Idx != INDEX_NONE) { Tracks.RemoveAt(Idx); }
				continue;
			}
			if (S.bVisible)
			{
				FTrack& T = Idx != INDEX_NONE ? Tracks[Idx] : Tracks.AddDefaulted_GetRef();
				T.BotIndex = S.BotIndex;
				T.bVisible = true;
				T.LastSeenTime = Now;
				T.Head = S.Head;
				T.Velocity = S.Velocity;
				T.bFacingAgent = S.bFacingAgent;
				T.bCrouched = S.bCrouched;
			}
			else if (Idx != INDEX_NONE)
			{
				Tracks[Idx].bVisible = false;
			}
		}
		Tracks.RemoveAll([Now, MemorySeconds](const FTrack& T) { return !T.bVisible && Now - T.LastSeenTime > MemorySeconds; });
	}

	TArray<FTrack> SelectTracks(const TArray<FTrack>& Tracks, int32 Max)
	{
		TArray<FTrack> Sorted = Tracks;
		Sorted.StableSort([](const FTrack& A, const FTrack& B)
		{
			if (A.bVisible != B.bVisible) { return A.bVisible; }
			return A.LastSeenTime > B.LastSeenTime;
		});
		if (Sorted.Num() > Max) { Sorted.SetNum(FMath::Max(0, Max)); }
		return Sorted;
	}

	bool HasLineOfSight(const UWorld* World, const FVector& From, const FVector& To)
	{
		FHitResult Hit;
		return !World->LineTraceSingleByObjectType(Hit, From, To, LevelGeometry(), FCollisionQueryParams(SCENE_QUERY_STAT(FPSLineOfSight), false));
	}

	float RayFraction(const UWorld* World, const FVector& Start, const FVector& Direction, float RangeCm)
	{
		FHitResult Hit;
		const FVector End = Start + Direction.GetSafeNormal() * RangeCm;
		if (World->LineTraceSingleByObjectType(Hit, Start, End, LevelGeometry(), FCollisionQueryParams(SCENE_QUERY_STAT(FPSRay), false)))
		{
			return FMath::Clamp(static_cast<float>(Hit.Distance / RangeCm), 0.f, 1.f);
		}
		return 1.f;
	}
}
