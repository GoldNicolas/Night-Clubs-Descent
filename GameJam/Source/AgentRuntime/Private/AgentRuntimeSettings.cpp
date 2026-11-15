#include "AgentRuntimeSettings.h"

UAgentRuntimeSettings::UAgentRuntimeSettings()
{
	CategoryName = TEXT("Game");

	// Snapshots handed over from FPS-Agent (Content/AgentModels/run-<id>/, SHA-256 checked against the release's SHA256SUMS on copy).
	FAgentModelEntry S0;
	S0.RunId = TEXT("S0-20261112-152039");
	S0.Stage = EAgentStage::S0;
	S0.Mobility = EAgentMobility::Static; // pre-tag run name: static (addendum §2)
	S0.TimeLimitSeconds = 20.f;
	S0.SnapshotDir = TEXT("AgentModels/run-S0-20261112-152039");
	S0.Sha256Encoder = TEXT("93457d752dbc95c950cb2443ae937bd934fd9daf532bec0adf3fb0aa5d4e7959");
	S0.Sha256Policy = TEXT("4c899d2ad0f7e113e3695f081d08951cb9c99bc636618b5f2e88f7a2d6ce5fa1");
	S0.Sha256Decoder = TEXT("8d57659cbbf0eaf8272a6d2b0839cf50d6a507ced70e2392d48ed9ab8d5fca18");
	Models.Add(S0);

	FAgentModelEntry S1;
	S1.RunId = TEXT("S1-20261113-L009");
	S1.Stage = EAgentStage::S1;
	// Pre-tag run name: full (addendum §2). NOT a stand-in for the game's walk line: with crouch+jump masked it drops from 76% to 62%
	// clear (addendum §3, corrected 2026-11-14). Kept for comparison tests (agent.Mobility full shows it b-hopping). The game's snapshot is
	// the walk line registered below: run-S1-walk-20261114-152929 (73.0% clear).
	S1.Mobility = EAgentMobility::Full;
	S1.TimeLimitSeconds = 30.f;
	S1.SnapshotDir = TEXT("AgentModels/run-S1-20261113-L009");
	S1.Sha256Encoder = TEXT("dd2af18737d1a007a880b666c33166a035716179ed6b1c48b3aebb28c02a7ace");
	S1.Sha256Policy = TEXT("bdad01630cd1e63823b3b071192b0d57df3248bd17e0b49069e4fc7741aa2659");
	S1.Sha256Decoder = TEXT("548be15693d6814cff35de5d6bac28a2b9e0decda9013f073a08b1124a9d407c");
	Models.Add(S1);

	// Game's walk-line snapshot: D-033, contract v1.2. The run report's only overrides disable crouch and jump.
	FAgentModelEntry S1Walk;
	S1Walk.RunId = TEXT("S1-walk-20261114-152929");
	S1Walk.Stage = EAgentStage::S1;
	S1Walk.Mobility = EAgentMobility::Walk;
	S1Walk.TimeLimitSeconds = 30.f;
	S1Walk.SnapshotDir = TEXT("AgentModels/run-S1-walk-20261114-152929");
	S1Walk.SetString = TEXT("Combat.Abilities.bCrouch=false;Combat.Abilities.bJump=false");
	S1Walk.Sha256Encoder = TEXT("6b660c73bc4d302027f6aaf676d767d86ba92a819190231141bbb18efaa814c6");
	S1Walk.Sha256Policy = TEXT("0d23dd222147e938829b9df5b6f6b6b300bab3e000b05ecf0c8f4d429e8954d6");
	S1Walk.Sha256Decoder = TEXT("bf9d2653b3e7e695f3c730c58e9d324cb5fa89ba0f95dd658021fed361d719dd");
	Models.Add(S1Walk);

	ActiveRunId = S1Walk.RunId;

	// The Rodin mob skins (2026-11-15), imported onto SK_Mannequin at 176 cm. Each wears the acid-green enemy marker.
	EnemySkins = {
		FSoftObjectPath(TEXT("/Game/Characters/Rodin/Enemy_SuitGoon/SKM_Enemy_SuitGoon.SKM_Enemy_SuitGoon")),
		FSoftObjectPath(TEXT("/Game/Characters/Rodin/Enemy_Bouncer/SKM_Enemy_Bouncer.SKM_Enemy_Bouncer")),
		FSoftObjectPath(TEXT("/Game/Characters/Rodin/Enemy_Enforcer/SKM_Enemy_Enforcer.SKM_Enemy_Enforcer")),
	};
	// The M16A1 pack's own gun animations (SK_M16_Skeleton); ignored for a gun model on another skeleton.
	EnemyWeaponFireAnim = FSoftObjectPath(TEXT("/Game/M16A1/Animation/Anim_Shoot.Anim_Shoot"));
	EnemyWeaponReloadAnim = FSoftObjectPath(TEXT("/Game/M16A1/Animation/Anim_Reload.Anim_Reload"));
	// The template's third-person rifle reload (UE5 mannequin skeleton, like the Rodin skins).
	EnemyBodyReloadAnim = FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload.MM_Rifle_Reload"));
	// The template's mannequin clips (UE5 skeleton). Deaths are named by the side the hit lands on (checked: a front hit throws
	// the body back, a left hit to the right).
	auto Clip = [](const TCHAR* Folder, const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/%s/%s.%s"), Folder, Name, Name));
	};
	EnemyBodyFireAnim = Clip(TEXT("Rifle"), TEXT("MM_Rifle_Fire"));
	EnemyHitReactFromFront = { Clip(TEXT("Rifle/HitReact"), TEXT("MM_HitReact_Front_Lgt_01")), Clip(TEXT("Rifle/HitReact"), TEXT("MM_HitReact_Front_Lgt_02")),
		Clip(TEXT("Rifle/HitReact"), TEXT("MM_HitReact_Front_Lgt_03")), Clip(TEXT("Rifle/HitReact"), TEXT("MM_HitReact_Front_Lgt_04")),
		Clip(TEXT("Rifle/HitReact"), TEXT("MM_HitReact_Front_Med_01")), Clip(TEXT("Rifle/HitReact"), TEXT("MM_HitReact_Front_Med_02")) };
	EnemyHitReactFromBack = { Clip(TEXT("Rifle/HitReact"), TEXT("MM_HitReact_Back_Med_01")) };
	EnemyDeathShotFromFront = { Clip(TEXT("Death"), TEXT("MM_Death_Front_01")), Clip(TEXT("Death"), TEXT("MM_Death_Front_02")), Clip(TEXT("Death"), TEXT("MM_Death_Front_03")) };
	EnemyDeathShotFromBack = { Clip(TEXT("Death"), TEXT("MM_Death_Back_01")) };
	EnemyDeathShotFromLeft = { Clip(TEXT("Death"), TEXT("MM_Death_Left_01")) };
	EnemyDeathShotFromRight = { Clip(TEXT("Death"), TEXT("MM_Death_Right_01")) };
}

const FAgentModelEntry* UAgentRuntimeSettings::FindModel(const FString& RunId) const
{
	const FString Id = RunId.StartsWith(TEXT("run-")) ? RunId.Mid(4) : RunId;
	return Models.FindByPredicate([&Id](const FAgentModelEntry& E) { return E.RunId.Equals(Id, ESearchCase::IgnoreCase); });
}
