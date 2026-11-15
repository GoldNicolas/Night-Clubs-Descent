#pragma once

#include "CoreMinimal.h"
#include "Common/FPSAgentTypes.h"

/** Fair bot senses and hearing (spec §4, D-024): the same rules as the agent's. Pure: the caller decides view-cone + line-of-sight visibility. */
namespace FPSBots
{
	enum class ESoundKind : uint8 { Footstep, Gunshot };

	/** An audible event in one arena. Each arena keeps its own buffer, so arenas never hear each other (hard rule 3). */
	struct FSoundEvent
	{
		FVector Position = FVector::ZeroVector;
		double Time = 0.0;
		ESoundKind Kind = ESoundKind::Footstep;
		/** INDEX_NONE = the agent, else the bot index. */
		int32 Source = INDEX_NONE;
		/** Seeded noise drawn once when the sound is made (a disc of radius HearingNoiseCm, Z = 0). The agent hears Position + HeardOffset:
		 *  a direction and a rough distance, like the bots (spec §2). Audibility (CanHear) still uses the true Position. */
		FVector HeardOffset = FVector::ZeroVector;
	};

	/** Running makes footstep sounds; walking, crouching and standing still are silent (spec §4). */
	AGENTRUNTIME_API bool IsAudibleMovement(float HorizontalSpeed, bool bCrouched, bool bWalking, float WalkSpeed, const FBotBehaviourSettings& B);
	AGENTRUNTIME_API float HearingRadiusCm(ESoundKind Kind, const FBotBehaviourSettings& B);
	/** Within the hearing radius of its kind, and between 0 and MaxAgeSeconds old. */
	AGENTRUNTIME_API bool CanHear(const FSoundEvent& E, const FVector& Listener, double Now, float MaxAgeSeconds, const FBotBehaviourSettings& B);

	/** What one bot knows about the agent: sightings wait out the reaction delay; heard sounds give a noisy position. */
	struct AGENTRUNTIME_API FBotSenses
	{
		/** When the current continuous sighting started (< 0: not seeing). */
		double SightSince = -1.0;
		FVector LastKnown = FVector::ZeroVector;
		double LastKnownTime = -1.0e9;
		/** Time of the newest sound already reacted to. */
		double LastSoundTime = -1.0e9;

		void Reset() { *this = FBotSenses(); }
		/**
		 * Returns true (OutTarget = where to look or go) when a stimulus becomes actionable:
		 * - seeing the agent continuously for at least ReactionSeconds (every update while it lasts);
		 * - otherwise the newest audible agent sound (Source == INDEX_NONE) that is at least ReactionSeconds old and new to this bot.
		 * bSeesAgent must already include the view cone and the geometry line-of-sight check.
		 */
		bool Update(bool bSeesAgent, const FVector& AgentPos, const TArray<FSoundEvent>& Sounds, const FVector& BotPos, double Now,
			const FBotBehaviourSettings& B, FRandomStream& Rng, FVector& OutTarget);
		bool Remembers(double Now, const FBotBehaviourSettings& B) const { return Now - LastKnownTime <= B.SightMemorySeconds; }
	};
}
