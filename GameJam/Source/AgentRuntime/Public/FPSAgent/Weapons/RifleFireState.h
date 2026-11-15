#pragma once

#include "CoreMinimal.h"
#include "Common/FPSAgentTypes.h"

/** Pure fire cadence / ammo / reload state machine. Times in seconds (world time). */
struct AGENTRUNTIME_API FRifleFireState
{
	int32 Ammo = 0;
	/** Spray index the next shot will use (0 = first, accurate shot). */
	int32 ShotIndex = 0;
	double NextShotTime = 0.0;
	double LastShotTime = -1.0e9;
	/** < 0 when not reloading. */
	double ReloadEndTime = -1.0;
	bool bWasTriggerHeld = false;

	void Reset(const FRifleStats& Stats);
	bool IsReloading() const { return ReloadEndTime >= 0.0; }
	/** Starts a reload unless the magazine is full or a reload is running. */
	bool StartReload(const FRifleStats& Stats, double Now);
	/** Advances to Now. Fills OutShotIndices with the spray index of each shot fired (oldest first); returns the count. */
	int32 Update(const FRifleStats& Stats, double Now, bool bTriggerHeld, TArray<int32>& OutShotIndices);
};
