#pragma once

#include "CoreMinimal.h"
#include "Common/FPSAgentTypes.h"

/** Hitbox placement relative to the capsule center. Zones stack without gaps: legs | body | head. */
struct FBeanHitboxLayout
{
	float HeadCenterZ = 0.f;
	float HeadRadius = 0.f;
	float BodyCenterZ = 0.f;
	FVector BodyExtent = FVector::ZeroVector;
	float LegsCenterZ = 0.f;
	FVector LegsExtent = FVector::ZeroVector;
	/** Eye / camera height (= head center). */
	float EyeZ = 0.f;
};

AGENTRUNTIME_API FBeanHitboxLayout ComputeBeanHitboxLayout(const FBeanBody& Body, float CapsuleHalfHeight);
