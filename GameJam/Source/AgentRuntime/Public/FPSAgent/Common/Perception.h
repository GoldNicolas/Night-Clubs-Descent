#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Fair perception (hard rule 4, D-024), shared by the agent and (from build B) the bots. Pure helpers, plus two traces that
 * only hit level geometry: object-type queries on WorldStatic + WorldDynamic, so pawns (object type Pawn) never block a
 * ray and never show up in one.
 */
namespace FPSPerception
{
	/** How far View must turn to look at Target: (yaw, pitch) in degrees, [-180, 180]; + = right / up. */
	AGENTRUNTIME_API FVector2D AimErrorDeg(const FVector& Eye, const FRotator& View, const FVector& Target);
	/** Target inside the view cone (|yaw error| <= FovDeg / 2 and |pitch error| <= VerticalFovDeg / 2). */
	AGENTRUNTIME_API bool InFieldOfView(const FVector& Eye, const FRotator& View, const FVector& Target, float FovDeg, float VerticalFovDeg = 180.f);
	/** A world vector in the yaw-only view frame: X forward, Y right, Z up. */
	AGENTRUNTIME_API FVector ToViewFrame(const FVector& WorldVector, float ViewYawDeg);
	/** For each pitch, YawCount directions evenly around ViewYawDeg (the first one straight ahead). */
	AGENTRUNTIME_API TArray<FVector> RayDirections(float ViewYawDeg, int32 YawCount, const TArray<float>& PitchesDeg);

	/** One bot as seen on this decision. */
	struct FSighting
	{
		int32 BotIndex = INDEX_NONE;
		bool bAlive = true;
		/** Passed this decision's line-of-sight-in-FOV check. */
		bool bVisible = false;
		FVector Head = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		bool bFacingAgent = false;
		bool bCrouched = false;
	};

	/** What the agent knows about a bot: refreshed while visible, kept for MemorySeconds afterwards. */
	struct FTrack
	{
		int32 BotIndex = INDEX_NONE;
		bool bVisible = false;
		double LastSeenTime = 0.0;
		FVector Head = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		bool bFacingAgent = false;
		bool bCrouched = false;
	};

	/** Visible sightings refresh their track; unseen ones keep the last-seen data until MemorySeconds pass; dead bots drop out. */
	AGENTRUNTIME_API void UpdateTracks(TArray<FTrack>& Tracks, const TArray<FSighting>& Sightings, double Now, float MemorySeconds);
	/** At most Max tracks: visible first, then the most recently seen. */
	AGENTRUNTIME_API TArray<FTrack> SelectTracks(const TArray<FTrack>& Tracks, int32 Max);

	/** True if no level geometry blocks From -> To (pawns are ignored). */
	AGENTRUNTIME_API bool HasLineOfSight(const UWorld* World, const FVector& From, const FVector& To);
	/** Fraction (0..1) of RangeCm a ray travels before it hits level geometry (pawns are ignored). */
	AGENTRUNTIME_API float RayFraction(const UWorld* World, const FVector& Start, const FVector& Direction, float RangeCm);
}
