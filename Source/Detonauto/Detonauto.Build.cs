using UnrealBuildTool;

public class Detonauto : ModuleRules
{
	public Detonauto(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Sem unity build: os helpers de cada arquivo (principalmente nos testes) ficam em namespace anônimo com nomes
		// curtos (City, Join, NaN...), e o unity juntava arquivos diferentes e fazia os nomes colidirem toda vez que um
		// arquivo novo mudava o agrupamento. Projeto pequeno: o custo de compilação é baixo.
		bUseUnity = false;

		// Dependências mínimas. EnhancedInput, ChaosVehicles etc. entram quando houver código que use.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Json" });

		// Um módulo só, com subpastas (Core, Vehicle, Damage...): os includes são "Core/DNScore.h" etc.
		PublicIncludePaths.Add(ModuleDirectory);
	}
}