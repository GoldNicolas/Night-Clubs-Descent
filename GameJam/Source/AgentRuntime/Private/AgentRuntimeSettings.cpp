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
	// the walk line: run-S1-walk-20261114-152929 (73.0% clear), to be copied in and made active next.
	S1.Mobility = EAgentMobility::Full;
	S1.TimeLimitSeconds = 30.f;
	S1.SnapshotDir = TEXT("AgentModels/run-S1-20261113-L009");
	S1.Sha256Encoder = TEXT("dd2af18737d1a007a880b666c33166a035716179ed6b1c48b3aebb28c02a7ace");
	S1.Sha256Policy = TEXT("bdad01630cd1e63823b3b071192b0d57df3248bd17e0b49069e4fc7741aa2659");
	S1.Sha256Decoder = TEXT("548be15693d6814cff35de5d6bac28a2b9e0decda9013f073a08b1124a9d407c");
	Models.Add(S1);

	ActiveRunId = S1.RunId;
}

const FAgentModelEntry* UAgentRuntimeSettings::FindModel(const FString& RunId) const
{
	const FString Id = RunId.StartsWith(TEXT("run-")) ? RunId.Mid(4) : RunId;
	return Models.FindByPredicate([&Id](const FAgentModelEntry& E) { return E.RunId.Equals(Id, ESearchCase::IgnoreCase); });
}
