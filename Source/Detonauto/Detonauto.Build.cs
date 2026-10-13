using UnrealBuildTool;

public class Detonauto : ModuleRules
{
	public Detonauto(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Dependências mínimas. EnhancedInput, ChaosVehicles etc. entram quando houver código que use.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Json" });

		// Um módulo só, com subpastas (Core, Vehicle, Damage...): os includes são "Core/DNScore.h" etc.
		PublicIncludePaths.Add(ModuleDirectory);
	}
}