#include "GJMusicSubsystem.h"
#include "AgentRuntime.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "EnemyBean.h"
#include "Engine/World.h"
#include "GJPlayerCharacter.h"
#include "GJRifle.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Quartz/AudioMixerClockHandle.h"
#include "Quartz/QuartzSubsystem.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"

namespace
{
	TAutoConsoleVariable<int32> CVarMusic(TEXT("gj.Music"), 1, TEXT("Adaptive club music on (1) or off (0)."));
	TAutoConsoleVariable<float> CVarMusicVolume(TEXT("gj.MusicVolume"), -1.f, TEXT("Music level 0-1 (-1 = the GJ Music settings' Volume)."));
	TAutoConsoleVariable<int32> CVarMusicTier(TEXT("gj.MusicTier"), -1,
		TEXT("Force the music tier: 0 Prowl, 1 Hunt, 2 Fight (-1 = follow the game)."));

	FAutoConsoleCommandWithWorld CmdMusicInfo(TEXT("gj.MusicInfo"), TEXT("Print the adaptive music's state."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const UGJMusicSubsystem* M = World ? World->GetSubsystem<UGJMusicSubsystem>() : nullptr)
			{
				UE_LOG(LogAgentRuntime, Display, TEXT("Music: %s"), *M->GetStatusLine());
			}
		}));

	const FName ClockName(TEXT("GJClubMusic"));
	const TCHAR* TierNames[] = {TEXT("Prowl"), TEXT("Hunt"), TEXT("Fight")};

	bool IsRhythmic(EGJMusicRole Role)
	{
		return Role != EGJMusicRole::Pad && Role != EGJMusicRole::Drone;
	}
}

bool UGJMusicSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId UGJMusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGJMusicSubsystem, STATGROUP_Tickables);
}

bool UGJMusicSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->HasBegunPlay() && !World->bIsTearingDown;
}

void UGJMusicSubsystem::Deinitialize()
{
	Stop();
	Super::Deinitialize();
}

bool UGJMusicSubsystem::Start()
{
	UWorld* World = GetWorld();
	const UGJMusicSettings& Settings = UGJMusicSettings::Get();
	if (!World->GetAudioDevice().IsValid())
	{
		UE_LOG(LogAgentRuntime, Log, TEXT("Music: no audio device (-nosound?), music off"));
		return false;
	}
	UQuartzSubsystem* Quartz = UQuartzSubsystem::Get(World);
	if (!Quartz)
	{
		UE_LOG(LogAgentRuntime, Warning, TEXT("Music: Quartz unavailable, music off"));
		return false;
	}

	for (const FGJMusicStem& Stem : Settings.Stems)
	{
		USoundBase* Sound = Stem.Sound.LoadSynchronous();
		if (!Sound)
		{
			UE_LOG(LogAgentRuntime, Warning, TEXT("Music: stem %s not found (%s), skipped"), *UEnum::GetValueAsString(Stem.Role),
				*Stem.Sound.ToString());
			continue;
		}
		if (const USoundWave* Wave = Cast<USoundWave>(Sound); Wave && (!Wave->bLooping || Wave->VirtualizationMode != EVirtualizationMode::PlayWhenSilent))
		{
			UE_LOG(LogAgentRuntime, Warning, TEXT("Music: %s should be Looping with Virtualization Mode = Play When Silent, or it drifts out of sync"),
				*Wave->GetName());
		}
		UAudioComponent* Audio = UGameplayStatics::CreateSound2D(World, Sound, 1.f, 1.f, 0.f, nullptr, false, false);
		if (!Audio)
		{
			continue;
		}
		Audio->bIsMusic = true;
		// A UI sound keeps playing while the game is paused: the stems stay on the beat clock (which never pauses), and the pause
		// menu has music, muffled.
		Audio->bIsUISound = true;
		Audio->bAllowSpatialization = false;
		Audio->SetLowPassFilterEnabled(true);
		AudioComponents.Add(Audio);
		FLayer& Layer = Layers.AddDefaulted_GetRef();
		Layer.Role = Stem.Role;
		Layer.Audio = Audio;
		Layer.StemVolume = Stem.Volume;
	}
	if (Layers.IsEmpty())
	{
		UE_LOG(LogAgentRuntime, Warning, TEXT("Music: no stems loaded (Project Settings > Game > GJ Music), music off"));
		return false;
	}

	// One clock for everything. It is created stopped, every stem is queued to start on it, then it starts: all stems begin on
	// the same sample.
	SecondsPerBeat = 60.f / FMath::Max(1.f, Settings.Bpm);
	FQuartzClockSettings ClockSettings;
	Clock = Quartz->CreateNewClock(World, ClockName, ClockSettings, true);
	if (!Clock)
	{
		UE_LOG(LogAgentRuntime, Warning, TEXT("Music: could not create the Quartz clock, music off"));
		Stop();
		return false;
	}
	UQuartzClockHandle* Handle = Clock; // the Quartz API takes the handle by reference
	FQuartzQuantizationBoundary Immediately;
	Clock->SetBeatsPerMinute(World, Immediately, FOnQuartzCommandEventBP(), Handle, Settings.Bpm);
	FOnQuartzMetronomeEventBP OnBeatEvent;
	OnBeatEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UGJMusicSubsystem, OnBeat));
	Clock->SubscribeToQuantizationEvent(World, EQuartzCommandQuantization::Beat, OnBeatEvent, Handle);

	Rng.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
	Tier = ETier::Prowl;
	TierSince = World->GetTimeSeconds();
	LastPlannedBar = -1;
	CurrentBar = 0;
	bBreakdown = false;
	bArpInProwl = false;
	bLeadOn = true;
	bBassB = Rng.FRand() < 0.5f;
	bLeadB = Rng.FRand() < 0.5f;
	FQuartzQuantizationBoundary OnClockStart(EQuartzCommandQuantization::Bar, 1.f, EQuarztQuantizationReference::TransportRelative, true);
	for (FLayer& Layer : Layers)
	{
		Layer.Level = TargetLevel(Layer.Role);
		Layer.Audio->PlayQuantized(World, Handle, OnClockStart, FOnQuartzCommandEventBP(), 0.f, 0.05f, FMath::Max(Layer.Level, 0.0001f));
	}
	Clock->StartClock(World, Handle);
	AppliedMaster = -1.f;
	AppliedCutoffHz = -1.f;
	UE_LOG(LogAgentRuntime, Display, TEXT("Music: %d stems at %.0f BPM"), Layers.Num(), Settings.Bpm);
	return true;
}

void UGJMusicSubsystem::Stop()
{
	for (UAudioComponent* Audio : AudioComponents)
	{
		if (IsValid(Audio))
		{
			Audio->Stop();
			Audio->DestroyComponent();
		}
	}
	AudioComponents.Reset();
	Layers.Reset();
	if (Clock)
	{
		if (UWorld* World = GetWorld())
		{
			if (UQuartzSubsystem* Quartz = UQuartzSubsystem::Get(World))
			{
				UQuartzClockHandle* Handle = Clock;
				Quartz->DeleteClockByHandle(World, Handle);
			}
		}
		Clock = nullptr;
	}
	bPlaying = false;
}

void UGJMusicSubsystem::Tick(float DeltaTime)
{
	const bool bWanted = UGJMusicSettings::Get().bEnabled && CVarMusic.GetValueOnGameThread() != 0;
	if (!bWanted)
	{
		if (bPlaying) { Stop(); }
		return;
	}
	if (!bPlaying)
	{
		if (!bStartFailed)
		{
			bPlaying = Start();
			bStartFailed = !bPlaying;
		}
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (!GetWorld()->IsPaused()) { UpdateIntensity(Now); }

	const float SettingsVolume = UGJMusicSettings::Get().Volume;
	const float CVarVolume = CVarMusicVolume.GetValueOnGameThread();
	ApplyVolume(FMath::Clamp(CVarVolume >= 0.f ? CVarVolume : SettingsVolume, 0.f, 1.f));

	// Low health and death close the music in; it opens back up as health regenerates.
	const UGJMusicSettings& Settings = UGJMusicSettings::Get();
	float TargetCutoff = 20000.f;
	if (const AGJPlayerCharacter* Player = Cast<AGJPlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)))
	{
		const float Health = Player->GetMaxHealth() > 0.f ? Player->GetHealth() / Player->GetMaxHealth() : 1.f;
		if (bPlayerDead) { TargetCutoff = 450.f; }
		else if (Health < Settings.LowHealthFraction)
		{
			const float T = FMath::Clamp(Health / FMath::Max(0.01f, Settings.LowHealthFraction), 0.f, 1.f);
			// Exponential in frequency so the closing sounds even.
			TargetCutoff = Settings.LowHealthCutoff * FMath::Pow(16000.f / Settings.LowHealthCutoff, T * T);
		}
	}
	if (MenuCutoffHz > 0.f) { TargetCutoff = FMath::Min(TargetCutoff, MenuCutoffHz); } // the main menu muffles the club
	CutoffHz = FMath::Exp(FMath::FInterpTo(FMath::Loge(CutoffHz), FMath::Loge(TargetCutoff), DeltaTime, 4.f));
	ApplyLowPass(CutoffHz);
}

void UGJMusicSubsystem::UpdateIntensity(double Now)
{
	UWorld* World = GetWorld();
	const APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	const AGJPlayerCharacter* Player = PC ? Cast<AGJPlayerCharacter>(PC->GetPawn()) : nullptr;
	bPlayerDead = !Player || Player->IsDead();
	if (bPlayerDead)
	{
		return;
	}

	// Fight: the player fired or was hit.
	LastCombatTime = FMath::Max(LastCombatTime, Player->GetLastDamageTime());
	if (const AGJRifle* Rifle = Player->GetRifle())
	{
		LastCombatTime = FMath::Max(LastCombatTime, Rifle->GetLastShotTime());
	}

	// Hunt: an enemy close by, or one the player can see. Scanned a few times a second.
	if (Now < NextThreatScan)
	{
		return;
	}
	NextThreatScan = Now + 0.25;
	const UGJMusicSettings& Settings = UGJMusicSettings::Get();
	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector ViewDir = ViewRot.Vector();
	for (TActorIterator<AEnemyBean> It(World); It; ++It)
	{
		if (!It->IsAlive())
		{
			continue;
		}
		const FVector Chest = It->GetActorLocation() + FVector(0.f, 0.f, 40.f);
		const float Dist = FVector::Dist(Chest, ViewLoc);
		bool bThreat = Dist < Settings.ThreatRadius;
		if (!bThreat && Dist < Settings.SightRange && FVector::DotProduct((Chest - ViewLoc).GetSafeNormal(), ViewDir) > 0.5f)
		{
			FCollisionQueryParams Q(SCENE_QUERY_STAT(GJMusicSight), false);
			Q.AddIgnoredActor(Player);
			Q.AddIgnoredActor(*It);
			bThreat = !World->LineTraceTestByChannel(ViewLoc, Chest, ECC_Visibility, Q);
		}
		if (bThreat)
		{
			LastThreatTime = Now;
			break;
		}
	}
}

UGJMusicSubsystem::ETier UGJMusicSubsystem::DesiredTier(double Now) const
{
	const int32 Forced = CVarMusicTier.GetValueOnGameThread();
	if (Forced >= 0)
	{
		return static_cast<ETier>(FMath::Min(Forced, 2));
	}
	if (bPlayerDead)
	{
		return ETier::Prowl;
	}
	const UGJMusicSettings& Settings = UGJMusicSettings::Get();
	if (Now - LastCombatTime < Settings.CombatHoldSeconds) { return ETier::Fight; }
	if (Now - LastThreatTime < Settings.ThreatHoldSeconds) { return ETier::Hunt; }
	return ETier::Prowl;
}

void UGJMusicSubsystem::OnBeat(FName InClockName, EQuartzCommandQuantization QuantizationType, int32 NumBars, int32 Beat, float BeatFraction)
{
	if (!bPlaying || !Clock)
	{
		return;
	}
	// Which beat this is, counted from the clock start (the stems started on beat 0). The run time is latency-blurred, but
	// rounding to the nearest beat is exact.
	const float RunTime = Clock->GetEstimatedRunTime(GetWorld());
	const int32 BeatIndex = FMath::RoundToInt(RunTime / SecondsPerBeat);
	if (BeatIndex < 8)
	{
		UE_LOG(LogAgentRuntime, Verbose, TEXT("Music: beat %d (clock %.3f s; Quartz bar %d beat %d)"), BeatIndex, RunTime, NumBars, Beat);
	}
	CurrentBar = BeatIndex / 4;
	if (BeatIndex % 4 == 3 && CurrentBar + 1 > LastPlannedBar)
	{
		PlanBar(CurrentBar + 1);
	}
}

void UGJMusicSubsystem::PlanBar(int32 Bar)
{
	LastPlannedBar = Bar;
	const double Now = GetWorld()->GetTimeSeconds();
	const UGJMusicSettings& Settings = UGJMusicSettings::Get();
	const ETier Desired = DesiredTier(Now);
	const ETier Previous = Tier;
	if (Desired > Tier)
	{
		Tier = Desired;
	}
	else if (Desired < Tier && Bar % 4 == 0 && Now - TierSince >= Settings.MinTierSeconds)
	{
		Tier = static_cast<ETier>(static_cast<uint8>(Tier) - 1);
	}
	if (Tier != Previous)
	{
		TierSince = Now;
		TierChangeBar = Bar;
		if (Tier == ETier::Fight) { bLeadOn = true; } // a fight always brings the lead in
	}

	// Phrase lines (16 bars): re-roll the variations so the loop keeps moving.
	if (Bar % 16 == 0 && Bar > 0)
	{
		if (Rng.FRand() < 0.4f) { bBassB = !bBassB; }
		bLeadOn = Rng.FRand() < 0.7f;
		if (bLeadOn && Rng.FRand() < 0.5f) { bLeadB = !bLeadB; }
		bArpInProwl = Rng.FRand() < 0.35f;
	}
	// Breakdowns: the last 4 bars of some 32-bar cycles drop the kick and bass, if the tier has been steady for a while.
	if (Bar % 32 == 28)
	{
		bBreakdown = Tier != ETier::Prowl && Bar - TierChangeBar >= 8 && Rng.FRand() < 0.5f;
	}
	else if (Bar % 32 == 0)
	{
		bBreakdown = false;
	}

	const bool bLog = Tier != Previous || Bar % 16 == 0 || Bar % 32 == 28;

	// Fades start on the last beat and land on the downbeat; rhythmic parts move in a beat, beds in two bars.
	const float Beat = SecondsPerBeat;
	for (FLayer& Layer : Layers)
	{
		const float Target = TargetLevel(Layer.Role);
		if (FMath::IsNearlyEqual(Target, Layer.Level, 0.01f))
		{
			continue;
		}
		const bool bRising = Target > Layer.Level;
		float Fade = IsRhythmic(Layer.Role) ? (bRising ? 0.9f * Beat : 4.f * Beat) : 8.f * Beat;
		if (bBreakdown && Bar % 32 == 28 && !bRising) { Fade = 0.9f * Beat; } // the drop is a cut, not a fade
		Layer.Audio->AdjustVolume(Fade, FMath::Max(Target, 0.0001f));
		Layer.Level = Target;
	}
	if (bLog)
	{
		UE_LOG(LogAgentRuntime, Log, TEXT("Music: bar %d -> %s"), Bar, *GetStatusLine());
	}
}

float UGJMusicSubsystem::TargetLevel(EGJMusicRole Role) const
{
	const bool bProwl = Tier == ETier::Prowl;
	const bool bFight = Tier == ETier::Fight;
	const bool bLeadPlaying = bFight && bLeadOn;
	float Level = 0.f;
	switch (Role)
	{
	case EGJMusicRole::Kick: Level = bProwl ? 0.f : 1.f; break;
	case EGJMusicRole::Hats: Level = bProwl ? 0.45f : (bFight ? 1.f : 0.85f); break;
	case EGJMusicRole::Perc: Level = bFight ? 1.f : 0.f; break;
	case EGJMusicRole::BassA: Level = !bProwl && !bBassB ? 1.f : 0.f; break;
	case EGJMusicRole::BassB: Level = !bProwl && bBassB ? 1.f : 0.f; break;
	case EGJMusicRole::Sub: Level = bProwl ? 0.85f : 0.f; break;
	case EGJMusicRole::Pad: Level = bProwl ? 0.9f : (bFight ? 0.45f : 0.6f); break;
	case EGJMusicRole::Drone: Level = bProwl ? 1.f : (bFight ? 0.3f : 0.55f); break;
	case EGJMusicRole::Arp: Level = bProwl ? (bArpInProwl ? 0.5f : 0.f) : (bLeadPlaying ? 0.45f : (bFight ? 0.7f : 0.8f)); break;
	case EGJMusicRole::LeadA: Level = bLeadPlaying && !bLeadB ? 1.f : 0.f; break;
	case EGJMusicRole::LeadB: Level = bLeadPlaying && bLeadB ? 1.f : 0.f; break;
	}
	if (bBreakdown && !bProwl)
	{
		switch (Role)
		{
		case EGJMusicRole::Kick:
		case EGJMusicRole::BassA:
		case EGJMusicRole::BassB:
		case EGJMusicRole::Perc: Level = 0.f; break;
		case EGJMusicRole::Sub: Level = 0.9f; break;
		case EGJMusicRole::Pad: Level = 1.f; break;
		case EGJMusicRole::Hats: Level *= 0.6f; break;
		case EGJMusicRole::Drone: Level = 0.6f; break;
		default: break;
		}
	}
	return Level;
}

void UGJMusicSubsystem::ApplyVolume(float Master)
{
	if (FMath::IsNearlyEqual(Master, AppliedMaster, 0.001f))
	{
		return;
	}
	AppliedMaster = Master;
	for (FLayer& Layer : Layers)
	{
		Layer.Audio->SetVolumeMultiplier(FMath::Max(0.0001f, Master * Layer.StemVolume));
	}
}

void UGJMusicSubsystem::ApplyLowPass(float Hz)
{
	if (AppliedCutoffHz > 0.f && FMath::Abs(Hz - AppliedCutoffHz) < AppliedCutoffHz * 0.02f)
	{
		return;
	}
	AppliedCutoffHz = Hz;
	for (FLayer& Layer : Layers)
	{
		Layer.Audio->SetLowPassFilterFrequency(Hz);
	}
}

FString UGJMusicSubsystem::GetStatusLine() const
{
	if (!bPlaying)
	{
		return bStartFailed ? TEXT("off (failed to start, see log)") : TEXT("off");
	}
	FString On;
	for (const FLayer& Layer : Layers)
	{
		if (Layer.Level > 0.01f)
		{
			On += FString::Printf(TEXT("%s%s %.2f"), On.IsEmpty() ? TEXT("") : TEXT(", "),
				*StaticEnum<EGJMusicRole>()->GetNameStringByValue(static_cast<int64>(Layer.Role)), Layer.Level);
		}
	}
	return FString::Printf(TEXT("%s%s, bar %d, low-pass %.0f Hz, master %.2f | %s"), TierNames[static_cast<int32>(Tier)],
		bBreakdown ? TEXT(" (breakdown)") : TEXT(""), CurrentBar, CutoffHz, AppliedMaster, *On);
}
