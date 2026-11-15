#include "AgentDirector.h"
#include "AgentRuntime.h"
#include "AgentTargetComponent.h"
#include "Character/BeanCharacter.h"
#include "Character/HealthComponent.h"
#include "Common/FPSAgentSettings.h"
#include "EnemyBean.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "Learning/CombatLearningClasses.h"
#include "LearningAgentsInteractor.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsNeuralNetwork.h"
#include "LearningAgentsPolicy.h"
#include "LearningNeuralNetwork.h"
#include "Misc/Paths.h"
#include "Weapons/RifleComponent.h"

namespace
{
	TAutoConsoleVariable<FString> CVarModel(TEXT("agent.Model"), TEXT(""),
		TEXT("Run id of the snapshot enemies load (Project Settings > Game > Agent Runtime registry). Empty = the settings' ActiveRunId. Read when the director starts: set it before Play."));
	TAutoConsoleVariable<FString> CVarMobility(TEXT("agent.Mobility"), TEXT(""),
		TEXT("Which of Crouch/Jump agents may use, live: walk (none: the game's line), full (both), walk+crouch, walk+jump. Empty = the settings' EnemyMobility. A walk/static snapshot never crouches or jumps whatever this says (contract v1.2)."));
	TAutoConsoleVariable<int32> CVarPaused(TEXT("agent.Paused"), 0,
		TEXT("1 = agents stop deciding and acting (memory kept, triggers released)."));
	TAutoConsoleVariable<int32> CVarDebug(TEXT("agent.Debug"), 1,
		TEXT("1 = show the Agent Director status line on screen."));
	TAutoConsoleVariable<int32> CVarShowBean(TEXT("agent.ShowBean"), 0,
		TEXT("1 = show enemies as the raw training bean instead of their skin (gameplay is identical either way)."));
	TAutoConsoleVariable<int32> CVarShowHitboxes(TEXT("agent.ShowHitboxes"), 0,
		TEXT("1 = draw enemy hitboxes."));

	const TCHAR* StageName(EAgentStage S)
	{
		switch (S)
		{
		case EAgentStage::S0: return TEXT("S0");
		case EAgentStage::S1: return TEXT("S1");
		case EAgentStage::S2: return TEXT("S2");
		default:              return TEXT("S3");
		}
	}

	const TCHAR* MobilityName(EAgentMobility M)
	{
		switch (M)
		{
		case EAgentMobility::Static: return TEXT("static");
		case EAgentMobility::Walk:   return TEXT("walk");
		default:                     return TEXT("full");
		}
	}
}

AAgentDirector::AAgentDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics; // Move input must reach CharacterMovement in the same frame
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Manager = CreateDefaultSubobject<ULearningAgentsManager>(TEXT("LearningAgentsManager"));
	SetCanBeDamaged(false);
}

AAgentDirector* AAgentDirector::Find(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AAgentDirector> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

AAgentDirector* AAgentDirector::Get(const UObject* WorldContext)
{
	if (AAgentDirector* Existing = Find(WorldContext))
	{
		return Existing;
	}
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || !World->IsGameWorld() || World->bIsTearingDown)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	return World->SpawnActor<AAgentDirector>(Params);
}

double AAgentDirector::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0; // game time: time dilation and pause apply to agents like to everything else (§5)
}

void AAgentDirector::BeginPlay()
{
	Super::BeginPlay();
	SoundRng.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
	bReady = InitPolicy();
	if (!bReady)
	{
		UE_LOG(LogAgentRuntime, Error, TEXT("Agent Director: enemies stay idle: %s"), *LoadError);
	}
}

void AAgentDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TArray<int32> Ids;
	Slots.GetKeys(Ids);
	for (const int32 Id : Ids)
	{
		RemoveSlot(Id);
	}
	Super::EndPlay(EndPlayReason);
}

bool AAgentDirector::InitPolicy()
{
	const UAgentRuntimeSettings& RS = UAgentRuntimeSettings::Get();
	FString RunId = CVarModel.GetValueOnGameThread();
	if (RunId.IsEmpty())
	{
		RunId = RS.ActiveRunId;
	}
	const FAgentModelEntry* Entry = RS.FindModel(RunId);
	if (!Entry)
	{
		LoadError = FString::Printf(TEXT("model '%s' is not in the registry (Project Settings > Game > Agent Runtime)"), *RunId);
		return false;
	}
	Model = *Entry;
	if (!Model.SetString.IsEmpty())
	{
		UE_LOG(LogAgentRuntime, Warning, TEXT("Agent Director: %s was trained with -Set=\"%s\". Mechanics overrides in it must be replicated in the game (contract §3)."),
			*Model.RunId, *Model.SetString);
	}

	Manager->SetMaxAgentNum(FMath::Max(1, RS.MaxAgents));
	ULearningAgentsManager* Mgr = Manager.Get();
	ULearningAgentsInteractor* I = ULearningAgentsInteractor::MakeInteractor(Mgr, UCombatInteractor::StaticClass(), TEXT("Interactor"));

	// Contract §4: exactly 2 x 256, GRU memory 128, ELU, the other settings at their defaults.
	const FCombatPpoSettings& Ppo = UFPSAgentSettings::Get().Combat.Ppo;
	FLearningAgentsPolicySettings PolicySettings;
	PolicySettings.HiddenLayerNum = Ppo.HiddenLayers;
	PolicySettings.HiddenLayerSize = Ppo.HiddenSize;
	PolicySettings.MemoryStateSize = Ppo.MemorySize;
	ULearningAgentsPolicy* P = I ? ULearningAgentsPolicy::MakePolicy(Mgr, I, ULearningAgentsPolicy::StaticClass(), TEXT("Policy"),
		nullptr, nullptr, nullptr, true, true, true, PolicySettings, RS.PolicySeed) : nullptr;
	Interactor = I;
	Policy = P;
	if (!I || !P)
	{
		LoadError = TEXT("Learning Agents interactor/policy setup failed");
		return false;
	}

	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), Model.SnapshotDir));
	if (!LoadSnapshots(Dir))
	{
		return false;
	}
	UE_LOG(LogAgentRuntime, Display, TEXT("Agent Director: ready. model=%s stage=%s trained=%s T=%.0fs maxAgents=%d contract=%s dir=%s"),
		*Model.RunId, StageName(Model.Stage), MobilityName(Model.Mobility), Model.TimeLimitSeconds, RS.MaxAgents, *Model.ContractVersion, *Dir);
	return true;
}

// Copied from FPS-Agent UTrainingSession::LoadSnapshots (contract §4 load check), critic left out (training only).
bool AAgentDirector::LoadSnapshots(const FString& Dir)
{
	TArray<TPair<FString, ULearningAgentsNeuralNetwork*>> Items;
	Items.Emplace(TEXT("encoder.bin"), Policy->GetEncoderNetworkAsset());
	Items.Emplace(TEXT("policy.bin"), Policy->GetPolicyNetworkAsset());
	Items.Emplace(TEXT("decoder.bin"), Policy->GetDecoderNetworkAsset());
	for (const TPair<FString, ULearningAgentsNeuralNetwork*>& Item : Items)
	{
		const FString Path = FPaths::Combine(Dir, Item.Key);
		if (!Item.Value || !FPaths::FileExists(Path))
		{
			LoadError = FString::Printf(TEXT("snapshot file not found: %s"), *Path);
			return false;
		}
	}
	// Learning Agents' LoadNetworkFromSnapshot overwrites the network's sizes and compatibility hash without comparing them, so a snapshot
	// from another network would "load" and then play badly. Compare before and after, and refuse a mismatch.
	struct FShape { int32 In = 0; int32 Out = 0; int32 Hash = 0; bool bKnown = false; };
	auto ShapeOf = [](const ULearningAgentsNeuralNetwork* Network)
	{
		FShape S;
		if (Network && Network->NeuralNetworkData)
		{
			S.In = Network->NeuralNetworkData->GetInputSize();
			S.Out = Network->NeuralNetworkData->GetOutputSize();
			S.Hash = Network->NeuralNetworkData->GetCompatibilityHash();
			S.bKnown = S.In > 0 || S.Out > 0;
		}
		return S;
	};
	TArray<FShape> Before;
	for (const TPair<FString, ULearningAgentsNeuralNetwork*>& Item : Items)
	{
		Before.Add(ShapeOf(Item.Value));
	}
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		FFilePath File;
		File.FilePath = FPaths::Combine(Dir, Items[i].Key);
		Items[i].Value->LoadNetworkFromSnapshot(File);
		const FShape After = ShapeOf(Items[i].Value);
		UE_LOG(LogAgentRuntime, Display, TEXT("Agent Director: %s inputs %d->%d outputs %d->%d hash %d->%d"),
			*Items[i].Key, Before[i].In, After.In, Before[i].Out, After.Out, Before[i].Hash, After.Hash);
		if (!Before[i].bKnown)
		{
			LoadError = FString::Printf(TEXT("%s: the network shape wasn't known before loading, so the compatibility check can't run"), *File.FilePath);
			return false;
		}
		if (After.In != Before[i].In || After.Out != Before[i].Out || After.Hash != Before[i].Hash)
		{
			LoadError = FString::Printf(TEXT("snapshot %s doesn't match the game's network (snapshot: inputs %d, outputs %d, hash %d; game: inputs %d, outputs %d, hash %d). Refused."),
				*File.FilePath, After.In, After.Out, After.Hash, Before[i].In, Before[i].Out, Before[i].Hash);
			return false;
		}
	}
	return true;
}

bool AAgentDirector::RegisterAgent(ABeanCharacter* Bean)
{
	if (!bReady || !Bean || !Bean->GetHealth() || !Bean->GetRifle())
	{
		return false;
	}
	for (const TPair<int32, FSlot>& Pair : Slots)
	{
		if (Pair.Value.Bean.Get() == Bean) { return true; }
	}
	if (Slots.Num() >= Manager->GetMaxAgentNum())
	{
		UE_LOG(LogAgentRuntime, Warning, TEXT("Agent Director: all %d agent slots are taken; %s stays idle"), Manager->GetMaxAgentNum(), *Bean->GetName());
		return false;
	}
	const int32 Id = Manager->AddAgent(Bean);
	if (Id == INDEX_NONE)
	{
		return false;
	}
	FSlot& Slot = Slots.Add(Id);
	Slot.Bean = Bean;
	Slot.AgentId = Id;
	Slot.DamagedHandle = Bean->GetHealth()->OnDamaged.AddUObject(this, &AAgentDirector::OnAgentDamaged);
	Slot.DiedHandle = Bean->GetHealth()->OnDied.AddUObject(this, &AAgentDirector::OnAgentDied);
	SpawnReset(Slot, Now());
	Manager->ResetAgents({ Id }); // zero its recurrent memory (§4)
	RefreshAllyLists();
	return true;
}

void AAgentDirector::UnregisterAgent(ABeanCharacter* Bean)
{
	for (const TPair<int32, FSlot>& Pair : Slots)
	{
		if (Pair.Value.Bean.Get() == Bean)
		{
			RemoveSlot(Pair.Key);
			return;
		}
	}
}

void AAgentDirector::RemoveSlot(int32 AgentId)
{
	FSlot* Slot = Slots.Find(AgentId);
	if (!Slot)
	{
		return;
	}
	if (ABeanCharacter* Bean = Slot->Bean.Get())
	{
		if (UHealthComponent* H = Bean->GetHealth())
		{
			H->OnDamaged.Remove(Slot->DamagedHandle);
			H->OnDied.Remove(Slot->DiedHandle);
		}
		if (URifleComponent* R = Bean->GetRifle())
		{
			R->SetTriggerHeld(false);
		}
	}
	if (Manager && Manager->HasAgent(AgentId))
	{
		Manager->RemoveAgent(AgentId);
	}
	Slots.Remove(AgentId);
	RefreshAllyLists();
}

void AAgentDirector::SpawnReset(FSlot& Slot, double InNow)
{
	// Contract §12.1. Uncrouch before anything moves the pawn.
	if (ABeanCharacter* Bean = Slot.Bean.Get())
	{
		Bean->ApplyCrouchNow(false);
		Bean->SetWalking(false);
		Bean->StopJumping();
		Bean->GetCharacterMovement()->StopMovementImmediately();
		Bean->GetHealth()->ResetToFull();
		Bean->GetRifle()->ResetWeapon();
		if (AController* C = Bean->GetController())
		{
			// Pitch 0 and the spawn yaw (the spawner faces it away from the player: closest to training, §12.1).
			C->SetControlRotation(FRotator(0.0, Bean->GetActorRotation().Yaw, 0.0));
		}
	}
	Slot.Tracks.Reset();
	Slot.Held = FPSCombatObs::FAgentAction();
	Slot.LastHitTime = -1.0e9;
	Slot.LastHitDirection = FVector::ZeroVector;
	Slot.EpisodeStart = InNow;
}

void AAgentDirector::RegisterTarget(UAgentTargetComponent* Target)
{
	if (!Target)
	{
		return;
	}
	if (Target->TargetId == INDEX_NONE)
	{
		Target->TargetId = NextTargetId++;
	}
	Targets.AddUnique(Target);
	bRestartAllPending = true; // §12.3 (a): a (re)spawned player starts a new virtual episode for every agent
}

void AAgentDirector::UnregisterTarget(UAgentTargetComponent* Target)
{
	Targets.RemoveAll([Target](const TWeakObjectPtr<UAgentTargetComponent>& W) { return !W.IsValid() || W.Get() == Target; });
	if (Target)
	{
		const int32 Id = Target->TargetId;
		for (TPair<int32, FSlot>& Pair : Slots)
		{
			Pair.Value.Tracks.RemoveAll([Id](const FPSPerception::FTrack& T) { return T.BotIndex == Id; });
		}
	}
}

bool AAgentDirector::IsTargetActor(const AActor* Actor) const
{
	if (!Actor) { return false; }
	for (const TWeakObjectPtr<UAgentTargetComponent>& W : Targets)
	{
		if (const UAgentTargetComponent* T = W.Get(); T && T->GetOwner() == Actor) { return true; }
	}
	return false;
}

void AAgentDirector::AddSound(FPSBots::ESoundKind Kind, const FVector& Position, int32 SourceTargetId)
{
	// Same as the arena's AddSound: a rough heard position, drawn once per event on a disc of HearingNoiseCm (§7.4).
	FPSBots::FSoundEvent E;
	E.Position = Position;
	E.Time = Now();
	E.Kind = Kind;
	E.Source = FMath::Max(0, SourceTargetId); // never INDEX_NONE: that marks the agent's own sounds, which it never hears
	const float Angle = SoundRng.FRandRange(0.f, 2.f * PI);
	const float Radius = UFPSAgentSettings::Get().Bots.HearingNoiseCm * FMath::Sqrt(SoundRng.FRand());
	E.HeardOffset = FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.0);
	Sounds.Add(E);
}

AAgentDirector::FSlot* AAgentDirector::FindSlot(const UHealthComponent* Health)
{
	for (TPair<int32, FSlot>& Pair : Slots)
	{
		const ABeanCharacter* Bean = Pair.Value.Bean.Get();
		if (Bean && Bean->GetHealth() == Health) { return &Pair.Value; }
	}
	return nullptr;
}

void AAgentDirector::OnAgentDamaged(UHealthComponent* Victim, const FPSCombat::FDamageResult& Result, AActor* DamageInstigator)
{
	// §9.4: only enemy (player) damage sets the hit indicator. It reaches the network only for S2+ snapshots (BuildObservation).
	FSlot* Slot = FindSlot(Victim);
	if (!Slot || !IsTargetActor(DamageInstigator))
	{
		return;
	}
	const ABeanCharacter* Bean = Slot->Bean.Get();
	Slot->LastHitTime = Now();
	Slot->LastHitDirection = Bean ? (DamageInstigator->GetActorLocation() - Bean->GetActorLocation()).GetSafeNormal() : FVector::ZeroVector;
}

void AAgentDirector::OnAgentDied(UHealthComponent* Victim, AActor* Killer)
{
	// Stop its decisions at once (PLAN §9); the enemy pawn handles its own ragdoll.
	if (FSlot* Slot = FindSlot(Victim))
	{
		RemoveSlot(Slot->AgentId);
	}
}

void AAgentDirector::RefreshAllyLists()
{
	// §8: an agent's bullets pass through every other agent (no friendly fire).
	TArray<TWeakObjectPtr<AActor>> Allies;
	for (const TPair<int32, FSlot>& Pair : Slots)
	{
		if (ABeanCharacter* Bean = Pair.Value.Bean.Get()) { Allies.Add(Bean); }
	}
	for (const TPair<int32, FSlot>& Pair : Slots)
	{
		if (ABeanCharacter* Bean = Pair.Value.Bean.Get()) { Bean->GetRifle()->SetIgnoredAllies(Allies); }
	}
}

FCombatAbilitySettings AAgentDirector::GetAppliedAbilities() const
{
	FString Requested = CVarMobility.GetValueOnGameThread().TrimStartAndEnd().ToLower();
	if (Requested.IsEmpty())
	{
		Requested = MobilityName(UAgentRuntimeSettings::Get().EnemyMobility);
	}
	const bool bWantCrouch = Requested == TEXT("full") || Requested.Contains(TEXT("crouch"));
	const bool bWantJump = Requested == TEXT("full") || Requested.Contains(TEXT("jump"));
	// A walk or static snapshot's Crouch/Jump outputs were ignored in training: they mean nothing, so never apply them (addendum §1).
	const bool bSnapshotAllows = Model.Mobility == EAgentMobility::Full && CanMove();
	FCombatAbilitySettings A;
	A.bCrouch = bSnapshotAllows && bWantCrouch;
	A.bJump = bSnapshotAllows && bWantJump;
	return A;
}

FString AAgentDirector::GetStatusLine() const
{
	if (!bReady)
	{
		return FString::Printf(TEXT("Agents OFF: %s"), *LoadError);
	}
	const FCombatAbilitySettings A = GetAppliedAbilities();
	const TCHAR* Senses = Model.Stage == EAgentStage::S0 ? TEXT("no hearing, no hit") : Model.Stage == EAgentStage::S1 ? TEXT("footsteps only, no hit") : TEXT("footsteps+gunshots+hit");
	return FString::Printf(TEXT("Agents: %s [%s, trained %s, applied %s] %d alive | %s | %lld decisions | jump asked %lld applied %lld, crouch asked %lld applied %lld (of %lld actions)%s"),
		*Model.RunId, StageName(Model.Stage), MobilityName(Model.Mobility),
		CanMove() ? MobilityTag(true, A) : TEXT("static"), Slots.Num(), Senses, Decisions,
		RawJump, AppliedJump, RawCrouch, AppliedCrouch, ActionsPerformed,
		CVarPaused.GetValueOnGameThread() != 0 ? TEXT(" | PAUSED") : TEXT(""));
}

void AAgentDirector::ApplyVisualToggles()
{
	const int32 Toggles = (CVarShowBean.GetValueOnGameThread() != 0 ? 1 : 0) | (CVarShowHitboxes.GetValueOnGameThread() != 0 ? 2 : 0);
	if (Toggles == LastShowBean)
	{
		return;
	}
	LastShowBean = Toggles;
	for (TActorIterator<AEnemyBean> It(GetWorld()); It; ++It)
	{
		It->ApplyDebugVisuals();
	}
}

void AAgentDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyVisualToggles();
	const double T = Now();
	{
		const FBotBehaviourSettings& Bs = UFPSAgentSettings::Get().Bots;
		const double Keep = Bs.ReactionSeconds + Bs.SoundMemorySeconds + 1.0;
		Sounds.RemoveAll([T, Keep](const FPSBots::FSoundEvent& E) { return T - E.Time > Keep; });
	}

	if (bReady)
	{
		if (CVarPaused.GetValueOnGameThread() != 0)
		{
			for (TPair<int32, FSlot>& Pair : Slots)
			{
				if (ABeanCharacter* Bean = Pair.Value.Bean.Get()) { Bean->GetRifle()->SetTriggerHeld(false); }
			}
		}
		else
		{
			// §5.1: a decision every TicksPerDecision / 60 s of game time (1/15 s); at most one per frame, backlog dropped.
			const double DecisionSeconds = FMath::Max(1, UFPSAgentSettings::Get().Combat.Ppo.TicksPerDecision) / 60.0;
			Accumulator += DeltaSeconds;
			if (Accumulator >= DecisionSeconds)
			{
				Accumulator -= DecisionSeconds;
				if (Accumulator >= DecisionSeconds) { Accumulator = 0.0; }
				Decide(T);
			}
			TickActions(DeltaSeconds);
		}
	}

	if (bReady && T >= NextStatsLogTime)
	{
		UE_LOG(LogAgentRuntime, Display, TEXT("%s"), *GetStatusLine());
		NextStatsLogTime = T + 10.0;
	}
	if (GEngine && CVarDebug.GetValueOnGameThread() != 0)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0x41474E54), 0.5f, bReady ? FColor::Cyan : FColor::Red, GetStatusLine());
	}
}

void AAgentDirector::Decide(double InNow)
{
	const FCombatPerceptionSettings& P = UFPSAgentSettings::Get().Combat.Perception;
	TArray<int32> Restarted;
	for (TPair<int32, FSlot>& Pair : Slots)
	{
		FSlot& Slot = Pair.Value;
		if (!Slot.Bean.IsValid()) { continue; }
		// §5.2 step 1: update the enemy tracks.
		TArray<FPSPerception::FSighting> Sightings;
		Sense(Slot, Sightings);
		FPSPerception::UpdateTracks(Slot.Tracks, Sightings, InNow, P.MemorySeconds);
		// §12.3: (a) the player (re)spawned; (b) time left is 0 and the agent tracks nobody.
		if (bRestartAllPending || (TimeLeftFraction(Slot, InNow) <= 0.f && Slot.Tracks.Num() == 0))
		{
			Slot.EpisodeStart = InNow;
			Restarted.Add(Pair.Key);
		}
	}
	bRestartAllPending = false;
	if (Restarted.Num() > 0)
	{
		Manager->ResetAgents(Restarted);
	}
	// §5.2 step 2: one batched inference for every agent, mean actions. It gathers (BuildObservation) and performs (SetAction).
	if (Slots.Num() > 0)
	{
		Policy->RunInference(0.f);
		++Decisions;
	}
}

void AAgentDirector::Sense(const FSlot& Slot, TArray<FPSPerception::FSighting>& Out) const
{
	// The arena's Sense, with the registered targets (the player) as the only enemies: allies are never in the list (§8).
	Out.Reset();
	const ABeanCharacter* Agent = Slot.Bean.Get();
	if (!Agent)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const FCombatPerceptionSettings& P = UFPSAgentSettings::Get().Combat.Perception;
	const bool bAgentAlive = !Agent->GetHealth()->IsDead();
	FVector Eye;
	FRotator View;
	Agent->GetActorEyesViewPoint(Eye, View);
	const double FacingCos = FMath::Cos(FMath::DegreesToRadians(0.5 * P.FieldOfViewDeg));
	for (const TWeakObjectPtr<UAgentTargetComponent>& W : Targets)
	{
		const UAgentTargetComponent* Target = W.Get();
		if (!Target || !Target->GetOwner()) { continue; }
		FPSPerception::FSighting S;
		S.BotIndex = Target->GetTargetId();
		S.bAlive = Target->IsTargetAlive();
		if (S.bAlive)
		{
			S.Head = Target->GetHeadLocation();
			const FVector Chest = Target->GetChestLocation();
			S.bVisible = bAgentAlive && !Target->bHiddenFromAgents
				&& FPSPerception::InFieldOfView(Eye, View, S.Head, P.FieldOfViewDeg, P.VerticalFieldOfViewDeg)
				&& (FPSPerception::HasLineOfSight(World, Eye, S.Head) || FPSPerception::HasLineOfSight(World, Eye, Chest));
			S.Velocity = Target->GetTargetVelocity();
			const FVector ToAgent = (Agent->GetActorLocation() - Target->GetOwner()->GetActorLocation()).GetSafeNormal2D();
			S.bFacingAgent = FVector::DotProduct(Target->GetForward2D(), ToAgent) > FacingCos;
			S.bCrouched = Target->IsTargetCrouched();
		}
		Out.Add(S);
	}
}

float AAgentDirector::TimeLeftFraction(const FSlot& Slot, double InNow) const
{
	return FMath::Clamp(1.f - static_cast<float>((InNow - Slot.EpisodeStart) / FMath::Max(1e-3f, Model.TimeLimitSeconds)), 0.f, 1.f);
}

void AAgentDirector::BuildObservation(int32 AgentId, FPSCombatObs::FObservationData& Out) const
{
	// The arena's BuildObservation, line for line, plus the stage gating of §1.1 for Hit and Sounds.
	const UFPSAgentSettings& S = UFPSAgentSettings::Get();
	const FCombatPerceptionSettings& P = S.Combat.Perception;
	const FSlot* Slot = Slots.Find(AgentId);
	const ABeanCharacter* Agent = Slot ? Slot->Bean.Get() : nullptr;
	if (!Agent)
	{
		Out = FPSCombatObs::FObservationData::Zero(P.RayYawCount * P.RayPitchesDeg.Num());
		return;
	}
	const double T = Now();
	FVector Eye;
	FRotator View;
	Agent->GetActorEyesViewPoint(Eye, View);
	const float Yaw = View.Yaw;
	const FRifleFireState& Fire = Agent->GetRifle()->GetFireState();

	FPSCombatObs::FSelfState Self;
	Self.VelocityView = FPSPerception::ToViewFrame(Agent->GetVelocity(), Yaw);
	Self.bGrounded = !Agent->IsAirborne();
	Self.bCrouched = Agent->bIsCrouched;
	Self.bWalking = Agent->IsWalking();
	Self.Health = Agent->GetHealth()->GetHealth();
	Self.MaxHealth = S.Health.MaxHealth;
	Self.Armor = Agent->GetHealth()->GetArmor();
	Self.MaxArmor = S.Health.MaxArmor;
	Self.Ammo = Fire.Ammo;
	Self.MagazineSize = S.Rifle.MagazineSize;
	Self.bReloading = Fire.IsReloading();
	Self.SpreadDeg = Agent->GetRifle()->GetCurrentSpreadDegrees();
	Self.SprayShots = Fire.ShotIndex;
	Self.SecondsSinceShot = static_cast<float>(T - Fire.LastShotTime);
	Self.AimPitchDeg = FRotator::NormalizeAxis(View.Pitch);
	Self.TimeLeftFraction = TimeLeftFraction(*Slot, T);
	Out.Self = FPSCombatObs::PackSelf(Self, P);

	Out.Rays.Reset();
	for (const FVector& Dir : FPSPerception::RayDirections(Yaw, P.RayYawCount, P.RayPitchesDeg))
	{
		Out.Rays.Add(FPSPerception::RayFraction(GetWorld(), Eye, Dir, P.RayRangeCm));
	}

	Out.Enemies.Reset();
	for (const FPSPerception::FTrack& Track : FPSPerception::SelectTracks(Slot->Tracks, P.MaxEnemies))
	{
		FPSCombatObs::FEnemyState E;
		E.HeadView = FPSPerception::ToViewFrame(Track.Head - Eye, Yaw);
		E.AimErrorDeg = FPSPerception::AimErrorDeg(Eye, View, Track.Head);
		E.VelocityView = FPSPerception::ToViewFrame(Track.Velocity, Yaw);
		E.bFacingAgent = Track.bFacingAgent;
		E.bCrouched = Track.bCrouched;
		E.bVisible = Track.bVisible;
		E.SecondsSinceSeen = static_cast<float>(T - Track.LastSeenTime);
		Out.Enemies.Add(FPSCombatObs::PackEnemy(E, P));
	}

	// §1.1: S0/S1 bots never shot, so the Hit input is always absent before S2.
	const bool bFeedHit = Model.Stage >= EAgentStage::S2;
	const float HitAge = static_cast<float>(T - Slot->LastHitTime);
	Out.bHit = bFeedHit && HitAge >= 0.f && HitAge <= P.HitIndicatorSeconds;
	Out.Hit = Out.bHit ? FPSCombatObs::PackHit(FPSPerception::ToViewFrame(Slot->LastHitDirection, Yaw), HitAge, P) : TArray<float>();

	// §1.1: S0 hears nothing; S1 hears enemy footsteps only; S2+ footsteps and gunshots.
	Out.Sounds.Reset();
	if (Model.Stage == EAgentStage::S0)
	{
		return;
	}
	const bool bFeedGunshots = Model.Stage >= EAgentStage::S2;
	const FBotBehaviourSettings& Bs = S.Bots;
	TArray<const FPSBots::FSoundEvent*> Heard;
	for (const FPSBots::FSoundEvent& E : Sounds)
	{
		if (E.Kind == FPSBots::ESoundKind::Gunshot && !bFeedGunshots) { continue; }
		if (E.Source != INDEX_NONE && FPSBots::CanHear(E, Agent->GetActorLocation(), T, Bs.SoundMemorySeconds, Bs))
		{
			Heard.Add(&E);
		}
	}
	Heard.StableSort([](const FPSBots::FSoundEvent& A, const FPSBots::FSoundEvent& B) { return A.Time > B.Time; }); // newest first
	for (int32 i = 0; i < Heard.Num() && i < P.MaxSounds; ++i)
	{
		const FVector Delta = Heard[i]->Position + Heard[i]->HeardOffset - Eye;
		Out.Sounds.Add(FPSCombatObs::PackSound(FPSPerception::ToViewFrame(Delta, Yaw), static_cast<float>(Delta.Size()),
			static_cast<float>(T - Heard[i]->Time), P, Bs.SoundMemorySeconds));
	}
}

void AAgentDirector::SetAction(int32 AgentId, const FPSCombatObs::FAgentAction& Action)
{
	// The arena's SetAction: a masked Crouch/Jump never reaches the bean (addendum §1, D-033).
	FSlot* Slot = Slots.Find(AgentId);
	if (!Slot)
	{
		return;
	}
	const FPSCombatObs::FAgentAction Raw = FPSCombatObs::Sanitize(Action);
	Slot->Held = FPSCombatObs::ApplyAbilities(Raw, GetAppliedAbilities());
	++ActionsPerformed;
	RawCrouch += Raw.bCrouch ? 1 : 0;
	RawJump += Raw.bJump ? 1 : 0;
	AppliedCrouch += (CanMove() && Slot->Held.bCrouch) ? 1 : 0;
	AppliedJump += (CanMove() && Slot->Held.bJump) ? 1 : 0;
	ABeanCharacter* Agent = Slot->Bean.Get();
	if (!Agent || Agent->GetHealth()->IsDead())
	{
		return;
	}
	Agent->GetRifle()->SetTriggerHeld(Slot->Held.bFire);
	if (Slot->Held.bReload)
	{
		Agent->GetRifle()->RequestReload();
	}
	if (CanMove())
	{
		if (Slot->Held.bCrouch) { Agent->Crouch(); } else { Agent->UnCrouch(); }
		Agent->SetWalking(Slot->Held.bWalk);
		if (Slot->Held.bJump) { Agent->Jump(); } else { Agent->StopJumping(); }
	}
}

void AAgentDirector::TickActions(float DeltaSeconds)
{
	// The arena's TickAction (§5.3): Aim is a rate, integrated every frame on game time; Move uses the updated yaw.
	const FCombatPerceptionSettings& P = UFPSAgentSettings::Get().Combat.Perception;
	for (TPair<int32, FSlot>& Pair : Slots)
	{
		FSlot& Slot = Pair.Value;
		ABeanCharacter* Agent = Slot.Bean.Get();
		if (!Agent || Agent->GetHealth()->IsDead()) { continue; }
		AController* C = Agent->GetController();
		if (!C) { continue; }
		const FVector2D Step = FPSCombatObs::AimPerTickDeg(Slot.Held.Aim, P, DeltaSeconds);
		FRotator R = C->GetControlRotation();
		R.Yaw = FRotator::NormalizeAxis(R.Yaw + Step.X);
		R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch) + Step.Y, -P.MaxPitchDeg, P.MaxPitchDeg);
		R.Roll = 0.0;
		C->SetControlRotation(R);
		if (CanMove())
		{
			Agent->AddMovementInput(FPSCombatObs::MoveWorld(Slot.Held.Move, R.Yaw), 1.f);
		}
	}
}
