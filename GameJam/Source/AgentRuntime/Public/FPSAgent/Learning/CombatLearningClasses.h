#pragma once

#include "CoreMinimal.h"
#include "LearningAgentsInteractor.h"
#include "CombatLearningClasses.generated.h"

/**
 * Combat observations and actions (spec §2). Thin: all state lives in ACombatTaskActor / UCombatArena; this class only
 * converts float arrays to Learning Agents elements, so the RL stack could be swapped without touching gameplay.
 *
 * Observation "Combat" (struct):
 * - Self: continuous 15.
 * - Rays: continuous 72.
 * - Enemies: set of up to 5 x continuous 15.
 * - Hit: optional continuous 4.
 * - Sounds: set of up to 4 x continuous 5.
 *
 * Action "Combat" (struct):
 * - Move: continuous 2.
 * - Aim: continuous 2.
 * - Fire, Reload, Crouch, Walk, Jump: bool.
 */
UCLASS()
class AGENTRUNTIME_API UCombatInteractor : public ULearningAgentsInteractor
{
	GENERATED_BODY()

public:
	virtual void SpecifyAgentObservation_Implementation(FLearningAgentsObservationSchemaElement& OutObservationSchemaElement, ULearningAgentsObservationSchema* InObservationSchema) override;
	virtual void GatherAgentObservation_Implementation(FLearningAgentsObservationObjectElement& OutObservationObjectElement, ULearningAgentsObservationObject* InObservationObject, const int32 AgentId) override;
	virtual void SpecifyAgentAction_Implementation(FLearningAgentsActionSchemaElement& OutActionSchemaElement, ULearningAgentsActionSchema* InActionSchema) override;
	virtual void PerformAgentAction_Implementation(const ULearningAgentsActionObject* InActionObject, const FLearningAgentsActionObjectElement& InActionObjectElement, const int32 AgentId) override;

	/**
	 * Initial probabilities of the on/off actions (network initialisation, not mechanics).
	 * - Fire: 0.5, so the agent explores shooting.
	 * - Rare actions start rare.
	 */
	static constexpr float FirePrior = 0.5f;
	static constexpr float ReloadPrior = 0.05f;
	static constexpr float CrouchPrior = 0.1f;
	static constexpr float WalkPrior = 0.1f;
	static constexpr float JumpPrior = 0.05f;
};

// GameJam (vendored change): UCombatTrainingEnv removed; training stays in FPS-Agent.
