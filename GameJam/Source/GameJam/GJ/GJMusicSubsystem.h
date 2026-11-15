#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Sound/QuartzQuantizationUtilities.h"
#include "GJMusicSettings.h"
#include "GJMusicSubsystem.generated.h"

class UAudioComponent;
class UQuartzClockHandle;

/**
 * Adaptive club music (MUSIC.md). Presentation only: agents never hear it (their hearing is the player's footstep/gunshot
 * events, contract §7.4), and nothing here touches agents.
 *
 * Every stem of UGJMusicSettings starts on one Quartz clock at the same sample and loops forever, so stems of 8, 16 and 32 bars
 * stay locked together. The music is mixed by fading stems in and out on the beat:
 * - three intensity tiers: Prowl (no threat: drone, pad, sub, light hats), Hunt (an enemy near or in sight: kick, bass, arp) and
 *   Fight (the player fired or was hit: claps and percussion, lead). It rises at the next bar and steps down slowly;
 * - variation so the loop doesn't wear out: the two bass variants swap at phrase lines, the lead rests some phrases and switches
 *   variant, short breakdowns drop the kick at the end of some 32-bar cycles, the calm tier sometimes brings in the arp;
 * - low health closes the music in with a low-pass filter.
 * Changes are planned on the last beat of a bar so fades land on the downbeat.
 * Console: gj.Music 0|1, gj.MusicVolume, gj.MusicTier -1|0|1|2 (force a tier), gj.MusicInfo.
 */
UCLASS()
class GAMEJAM_API UGJMusicSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	enum class ETier : uint8 { Prowl, Hunt, Fight };

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	bool IsPlaying() const { return bPlaying; }
	ETier GetTier() const { return Tier; }
	/** One line for gj.MusicInfo and logs. */
	FString GetStatusLine() const;

private:
	struct FLayer
	{
		EGJMusicRole Role = EGJMusicRole::Pad;
		TObjectPtr<UAudioComponent> Audio;
		float StemVolume = 1.f;
		/** The fade level last sent (0-1), or -1 before the first. */
		float Level = -1.f;
	};

	bool Start();
	void Stop();
	void UpdateIntensity(double Now);
	ETier DesiredTier(double Now) const;
	/** Decides what plays in bar Bar and starts the fades so they land on its downbeat. */
	void PlanBar(int32 Bar);
	float TargetLevel(EGJMusicRole Role) const;
	void ApplyVolume(float Master);
	void ApplyLowPass(float CutoffHz);

	UFUNCTION()
	void OnBeat(FName ClockName, EQuartzCommandQuantization QuantizationType, int32 NumBars, int32 Beat, float BeatFraction);

	UPROPERTY() TObjectPtr<UQuartzClockHandle> Clock;
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> AudioComponents;
	TArray<FLayer> Layers;

	bool bPlaying = false;
	bool bStartFailed = false;
	float SecondsPerBeat = 0.48f;
	int32 LastPlannedBar = -1;
	int32 CurrentBar = 0;

	ETier Tier = ETier::Prowl;
	double TierSince = 0.0;
	double LastThreatTime = -1.0e9;
	double LastCombatTime = -1.0e9;
	double NextThreatScan = 0.0;
	bool bPlayerDead = false;

	// Arrangement state, re-rolled at phrase lines.
	bool bBassB = false;
	bool bLeadOn = true;
	bool bLeadB = false;
	bool bArpInProwl = false;
	bool bBreakdown = false;
	int32 TierChangeBar = 0;

	float AppliedMaster = -1.f;
	float CutoffHz = 20000.f;
	float AppliedCutoffHz = -1.f;
	FRandomStream Rng;
};
