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

		// Dependências mínimas. ChaosVehicles (plugin ChaosVehiclesPlugin, ligado no .uproject) e EnhancedInput entram com o carro
		// (Vehicle/DNVehicle); PhysicsCore dá o tipo do corpo físico que o Chaos usa.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Json", "InputCore", "EnhancedInput", "ChaosVehicles", "PhysicsCore" });

		// Um módulo só, com subpastas (Core, Vehicle, Damage...): os includes são "Core/DNScore.h" etc.
		PublicIncludePaths.Add(ModuleDirectory);
	}
}