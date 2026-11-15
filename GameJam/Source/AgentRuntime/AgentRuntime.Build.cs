using System.IO;
using UnrealBuildTool;

/**
 * Hosts the FPS-Agent combat policy as the game's enemies (contract v1.2: FPS-Agent-Constraints.md + v1.2 addendum).
 * Public/FPSAgent/ is vendored from FPS-Agent (see VENDORED.md); everything else here is the game's integration.
 */
public class AgentRuntime : ModuleRules
{
	public AgentRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// The vendored sources include each other relative to their own root, e.g. "Common/FPSAgentTypes.h" (kept verbatim).
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public", "FPSAgent"));

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "AIModule", "DeveloperSettings" });

		// Learning Agents inference only (contract §2): the training modules stay out of the game.
		PublicDependencyModuleNames.AddRange(new string[] { "Learning", "LearningAgents" });
	}
}
