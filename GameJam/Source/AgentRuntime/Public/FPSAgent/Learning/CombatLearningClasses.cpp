#include "Learning/CombatLearningClasses.h"
#include "Common/FPSAgentSettings.h"
#include "Learning/CombatObservation.h"
#include "AgentDirector.h" // GameJam (vendored change): TaskOf glue -> AAgentDirector
#include "LearningAgentsActions.h"
#include "LearningAgentsObservations.h"

namespace
{
	// GameJam (vendored change): the Make* helpers create the interactor with the manager component as Outer, whose owner is the Agent Director.
	AAgentDirector* TaskOf(const UObject* Listener)
	{
		return Listener ? Listener->GetTypedOuter<AAgentDirector>() : nullptr;
	}

	int32 RayCount()
	{
		const FCombatPerceptionSettings& P = UFPSAgentSettings::Get().Combat.Perception;
		return P.RayYawCount * P.RayPitchesDeg.Num();
	}
}

void UCombatInteractor::SpecifyAgentObservation_Implementation(FLearningAgentsObservationSchemaElement& OutObservationSchemaElement, ULearningAgentsObservationSchema* InObservationSchema)
{
	using O = ULearningAgentsObservations;
	const FCombatPerceptionSettings& P = UFPSAgentSettings::Get().Combat.Perception;
	TMap<FName, FLearningAgentsObservationSchemaElement> E;
	E.Add(TEXT("Self"), O::SpecifyContinuousObservation(InObservationSchema, FPSCombatObs::SelfSize, 1.f, TEXT("Self")));
	E.Add(TEXT("Rays"), O::SpecifyContinuousObservation(InObservationSchema, RayCount(), 1.f, TEXT("Rays")));
	E.Add(TEXT("Enemies"), O::SpecifySetObservation(InObservationSchema,
		O::SpecifyContinuousObservation(InObservationSchema, FPSCombatObs::EnemySize, 1.f, TEXT("Enemy")), P.MaxEnemies, 32, 4, 32, TEXT("Enemies")));
	E.Add(TEXT("Hit"), O::SpecifyOptionalObservation(InObservationSchema,
		O::SpecifyContinuousObservation(InObservationSchema, FPSCombatObs::HitSize, 1.f, TEXT("HitDirection")), 32, TEXT("Hit")));
	E.Add(TEXT("Sounds"), O::SpecifySetObservation(InObservationSchema,
		O::SpecifyContinuousObservation(InObservationSchema, FPSCombatObs::SoundSize, 1.f, TEXT("Sound")), P.MaxSounds, 32, 4, 32, TEXT("Sounds")));
	OutObservationSchemaElement = O::SpecifyStructObservation(InObservationSchema, E, TEXT("Combat"));
}

void UCombatInteractor::GatherAgentObservation_Implementation(FLearningAgentsObservationObjectElement& OutObservationObjectElement, ULearningAgentsObservationObject* InObservationObject, const int32 AgentId)
{
	using O = ULearningAgentsObservations;
	FPSCombatObs::FObservationData D = FPSCombatObs::FObservationData::Zero(RayCount());
	if (const AAgentDirector* Task = TaskOf(this))
	{
		Task->BuildObservation(AgentId, D);
	}
	TMap<FName, FLearningAgentsObservationObjectElement> E;
	E.Add(TEXT("Self"), O::MakeContinuousObservation(InObservationObject, D.Self, TEXT("Self")));
	E.Add(TEXT("Rays"), O::MakeContinuousObservation(InObservationObject, D.Rays, TEXT("Rays")));
	TSet<FLearningAgentsObservationObjectElement> Enemies;
	for (const TArray<float>& V : D.Enemies) { Enemies.Add(O::MakeContinuousObservation(InObservationObject, V, TEXT("Enemy"))); }
	E.Add(TEXT("Enemies"), O::MakeSetObservation(InObservationObject, Enemies, TEXT("Enemies")));
	E.Add(TEXT("Hit"), D.bHit
		? O::MakeOptionalObservation(InObservationObject, O::MakeContinuousObservation(InObservationObject, D.Hit, TEXT("HitDirection")), ELearningAgentsOptionalObservation::Valid, TEXT("Hit"))
		: O::MakeOptionalNullObservation(InObservationObject, TEXT("Hit")));
	TSet<FLearningAgentsObservationObjectElement> Sounds;
	for (const TArray<float>& V : D.Sounds) { Sounds.Add(O::MakeContinuousObservation(InObservationObject, V, TEXT("Sound"))); }
	E.Add(TEXT("Sounds"), O::MakeSetObservation(InObservationObject, Sounds, TEXT("Sounds")));
	OutObservationObjectElement = O::MakeStructObservation(InObservationObject, E, TEXT("Combat"));
}

void UCombatInteractor::SpecifyAgentAction_Implementation(FLearningAgentsActionSchemaElement& OutActionSchemaElement, ULearningAgentsActionSchema* InActionSchema)
{
	using A = ULearningAgentsActions;
	TMap<FName, FLearningAgentsActionSchemaElement> E;
	E.Add(TEXT("Move"), A::SpecifyContinuousAction(InActionSchema, 2, 1.f, TEXT("Move")));
	E.Add(TEXT("Aim"), A::SpecifyContinuousAction(InActionSchema, 2, 1.f, TEXT("Aim")));
	E.Add(TEXT("Fire"), A::SpecifyBoolAction(InActionSchema, FirePrior, TEXT("Fire")));
	E.Add(TEXT("Reload"), A::SpecifyBoolAction(InActionSchema, ReloadPrior, TEXT("Reload")));
	E.Add(TEXT("Crouch"), A::SpecifyBoolAction(InActionSchema, CrouchPrior, TEXT("Crouch")));
	E.Add(TEXT("Walk"), A::SpecifyBoolAction(InActionSchema, WalkPrior, TEXT("Walk")));
	E.Add(TEXT("Jump"), A::SpecifyBoolAction(InActionSchema, JumpPrior, TEXT("Jump")));
	OutActionSchemaElement = A::SpecifyStructAction(InActionSchema, E, TEXT("Combat"));
}

void UCombatInteractor::PerformAgentAction_Implementation(const ULearningAgentsActionObject* InActionObject, const FLearningAgentsActionObjectElement& InActionObjectElement, const int32 AgentId)
{
	using A = ULearningAgentsActions;
	TMap<FName, FLearningAgentsActionObjectElement> E;
	if (!A::GetStructAction(E, InActionObject, InActionObjectElement, TEXT("Combat")))
	{
		return;
	}
	FPSCombatObs::FAgentAction Act;
	TArray<float> V;
	if (const FLearningAgentsActionObjectElement* M = E.Find(TEXT("Move")); M && A::GetContinuousAction(V, InActionObject, *M, TEXT("Move")) && V.Num() == 2)
	{
		Act.Move = FVector2D(V[0], V[1]);
	}
	if (const FLearningAgentsActionObjectElement* M = E.Find(TEXT("Aim")); M && A::GetContinuousAction(V, InActionObject, *M, TEXT("Aim")) && V.Num() == 2)
	{
		Act.Aim = FVector2D(V[0], V[1]);
	}
	auto Bool = [&](const TCHAR* Name, bool& bOut)
	{
		if (const FLearningAgentsActionObjectElement* B = E.Find(FName(Name)))
		{
			A::GetBoolAction(bOut, InActionObject, *B, FName(Name));
		}
	};
	Bool(TEXT("Fire"), Act.bFire);
	Bool(TEXT("Reload"), Act.bReload);
	Bool(TEXT("Crouch"), Act.bCrouch);
	Bool(TEXT("Walk"), Act.bWalk);
	Bool(TEXT("Jump"), Act.bJump);
	if (AAgentDirector* Task = TaskOf(this))
	{
		Task->SetAction(AgentId, Act);
	}
}

// GameJam (vendored change): UCombatTrainingEnv removed; training stays in FPS-Agent.
