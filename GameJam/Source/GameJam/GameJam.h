// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Main log category used across the project */
DECLARE_LOG_CATEGORY_EXTERN(LogGameJam, Log, All);

/**
 * Ceilings and roofs (Config/DefaultEngine.ini; docs/MAP_DESIGN.md §3.3 on branch map-design): an object channel that blocks pawns and the Weapon
 * channel but is not WorldStatic, so agent perception (WorldStatic only) ignores it. Valid only because nothing is walkable above.
 */
inline constexpr ECollisionChannel ECC_Ceiling = ECC_GameTraceChannel4;
