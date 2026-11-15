#pragma once

#include "CoreMinimal.h"

AGENTRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogAgentRuntime, Log, All);

/** Collision object channel for agent capsules: ignores itself (allies never block each other, contract §8) and blocks everything else. */
inline constexpr ECollisionChannel ECC_AgentPawn = ECC_GameTraceChannel3;
