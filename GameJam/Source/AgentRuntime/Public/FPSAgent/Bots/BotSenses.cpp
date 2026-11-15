#include "Bots/BotSenses.h"

namespace FPSBots
{
	bool IsAudibleMovement(float Speed, bool bCrouched, bool bWalking, float WalkSpeed, const FBotBehaviourSettings& B)
	{
		return !bCrouched && !bWalking && Speed > WalkSpeed * B.SilentSpeedFraction;
	}

	float HearingRadiusCm(ESoundKind Kind, const FBotBehaviourSettings& B)
	{
		return Kind == ESoundKind::Gunshot ? B.GunshotHearingCm : B.FootstepHearingCm;
	}

	bool CanHear(const FSoundEvent& E, const FVector& Listener, double Now, float MaxAgeSeconds, const FBotBehaviourSettings& B)
	{
		const double Age = Now - E.Time;
		return Age >= 0.0 && Age <= MaxAgeSeconds && FVector::Dist(E.Position, Listener) <= HearingRadiusCm(E.Kind, B);
	}

	bool FBotSenses::Update(bool bSeesAgent, const FVector& AgentPos, const TArray<FSoundEvent>& Sounds, const FVector& BotPos, double Now,
		const FBotBehaviourSettings& B, FRandomStream& Rng, FVector& OutTarget)
	{
		if (bSeesAgent)
		{
			if (SightSince < 0.0)
			{
				SightSince = Now;
			}
			if (Now - SightSince + 1e-6 >= B.ReactionSeconds)
			{
				LastKnown = AgentPos;
				LastKnownTime = Now;
				OutTarget = AgentPos;
				return true;
			}
		}
		else
		{
			SightSince = -1.0;
		}
		const FSoundEvent* Best = nullptr;
		for (const FSoundEvent& E : Sounds)
		{
			if (E.Source != INDEX_NONE || E.Time <= LastSoundTime || Now - E.Time + 1e-6 < B.ReactionSeconds)
			{
				continue;
			}
			if (!CanHear(E, BotPos, Now, B.ReactionSeconds + B.SoundMemorySeconds, B))
			{
				continue;
			}
			if (!Best || E.Time > Best->Time)
			{
				Best = &E;
			}
		}
		if (!Best)
		{
			return false;
		}
		LastSoundTime = Best->Time;
		const float Angle = Rng.FRandRange(0.f, 2.f * PI);
		const float Radius = B.HearingNoiseCm * FMath::Sqrt(Rng.FRand());
		OutTarget = Best->Position + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0);
		LastKnown = OutTarget;
		LastKnownTime = Now;
		return true;
	}
}
