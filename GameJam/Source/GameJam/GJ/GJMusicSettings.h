#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GJMusicSettings.generated.h"

class USoundBase;

/** What a stem does in the arrangement (MUSIC.md). The music subsystem decides which roles sound at each intensity tier. */
UENUM(BlueprintType)
enum class EGJMusicRole : uint8
{
	Kick,
	Hats,
	Perc,
	/** Bass variants: one of the two plays at a time. */
	BassA,
	BassB,
	/** Clean low end for calm moments and breakdowns. */
	Sub,
	Pad,
	/** Tunnel ambience: the calm tier's bed. */
	Drone,
	Arp,
	/** Lead variants: one of the two plays at a time, and not in every phrase. */
	LeadA,
	LeadB,
};

USTRUCT(BlueprintType)
struct FGJMusicStem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Music") EGJMusicRole Role = EGJMusicRole::Pad;

	/**
	 * A looping sound whose length is a whole number of bars at the settings' BPM (MUS_Club_* are 8, 16 or 32 bars at 125 BPM).
	 * Import with Looping on and Virtualization Mode = Play When Silent, or a muted stem drops out of sync.
	 */
	UPROPERTY(EditAnywhere, Category="Music") TSoftObjectPtr<USoundBase> Sound;

	/** Mix trim for this stem (1 = as rendered). */
	UPROPERTY(EditAnywhere, Category="Music", meta=(ClampMin=0, ClampMax=4)) float Volume = 1.f;
};

/**
 * The adaptive club music (Project Settings > Game > GJ Music): stems, tempo and how quickly the music follows the fight.
 * Swapping in new stems needs no code: give each one a role and keep every stem a whole number of bars at Bpm.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="GJ Music"))
class GAMEJAM_API UGJMusicSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGJMusicSettings();

	static const UGJMusicSettings& Get() { return *GetDefault<UGJMusicSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category="Music") bool bEnabled = true;

	/** The stems' tempo. The beat clock runs at this and every stem must loop on a bar line of it. */
	UPROPERTY(Config, EditAnywhere, Category="Music", meta=(ClampMin=40, ClampMax=240)) float Bpm = 125.f;

	/** Overall music level (gj.MusicVolume overrides it while playing). Keep it under the player's footsteps and gunfire. */
	UPROPERTY(Config, EditAnywhere, Category="Music", meta=(ClampMin=0, ClampMax=1)) float Volume = 0.7f;

	UPROPERTY(Config, EditAnywhere, Category="Music") TArray<FGJMusicStem> Stems;

	/** An enemy this close raises the music to the Hunt tier, seen or not. */
	UPROPERTY(Config, EditAnywhere, Category="Intensity", meta=(Units="cm")) float ThreatRadius = 1500.f;
	/** An enemy the player can see (in front, clear line) within this range raises the music to the Hunt tier. */
	UPROPERTY(Config, EditAnywhere, Category="Intensity", meta=(Units="cm")) float SightRange = 4000.f;
	/** Hunt lasts this long after the last threat. */
	UPROPERTY(Config, EditAnywhere, Category="Intensity", meta=(Units="s")) float ThreatHoldSeconds = 6.f;
	/** Fight lasts this long after the player last fired or was hit. */
	UPROPERTY(Config, EditAnywhere, Category="Intensity", meta=(Units="s")) float CombatHoldSeconds = 7.f;
	/** The music rises at the next bar, but steps down one tier at a time and only after holding a tier this long. */
	UPROPERTY(Config, EditAnywhere, Category="Intensity", meta=(Units="s")) float MinTierSeconds = 12.f;
	/** Below this health fraction the music closes in (low-pass), down to LowHealthCutoff near death. */
	UPROPERTY(Config, EditAnywhere, Category="Intensity", meta=(ClampMin=0, ClampMax=1)) float LowHealthFraction = 0.4f;
	UPROPERTY(Config, EditAnywhere, Category="Intensity", meta=(Units="Hz")) float LowHealthCutoff = 700.f;
};
