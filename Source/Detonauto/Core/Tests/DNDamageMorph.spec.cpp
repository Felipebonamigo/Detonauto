// Testes do peso dos alvos de deformação por zona (lote 17, docs/regras-dano-visual.md, "Regra pura"). Puro: sem mundo, sem malha.
//
// Decisões:
//  M1 o alvo do peso é pow(dano da zona, MorphCurveExponent): com 0,7 o amassado aparece cedo e satura devagar;
//  M2 o peso sobe no máximo MorphRisePerSecond x Dt por chamada e nunca diminui (o metal não desamassa);
//  M3 dano NaN conta como 0 (o peso fica onde estava); dano e peso anterior fora de [0, 1] são limitados; anterior NaN vale 0;
//     Dt <= 0 ou NaN não sobe nada;
//  M4 configuração inválida (expoente ou subida <= 0, NaN ou infinito) usa o padrão (0,7 e 10/s);
//  M5 as 5 zonas Front, Rear, Left, Right e Roof têm alvo; a Under não.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNDamageMorph.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDamageMorphSpec, "Detonauto.Dano.AlvosDeDeformacao",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNDamageConfig Config; // expoente 0,7, subida 10/s
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
END_DEFINE_SPEC(FDNDamageMorphSpec)

void FDNDamageMorphSpec::Define()
{
	It("M1: com tempo de sobra, o peso e pow(dano, 0,7)", [this]()
	{
		TestEqual("0,5", FDNDamageMorph::Weight(Config, 0.5f, 0.f, 10.f), FMath::Pow(0.5f, 0.7f), 1e-5f);
		TestEqual("0,1", FDNDamageMorph::Weight(Config, 0.1f, 0.f, 10.f), FMath::Pow(0.1f, 0.7f), 1e-5f);
		TestEqual("1", FDNDamageMorph::Weight(Config, 1.f, 0.f, 10.f), 1.f, 1e-6f);
		TestEqual("0", FDNDamageMorph::Weight(Config, 0.f, 0.f, 10.f), 0.f);
		FDNDamageConfig Linear;
		Linear.MorphCurveExponent = 1.f;
		TestEqual("expoente 1: linear", FDNDamageMorph::Weight(Linear, 0.5f, 0.f, 10.f), 0.5f, 1e-6f);
	});

	It("M2: a subida e limitada a 10/s (0,1 s ate o cheio) e o peso nunca diminui", [this]()
	{
		TestEqual("1 quadro de 1/60 s", FDNDamageMorph::Weight(Config, 1.f, 0.f, 1.f / 60.f), 10.f / 60.f, 1e-5f);
		float W = 0.f;
		for (int32 i = 0; i < 6; ++i) { W = FDNDamageMorph::Weight(Config, 1.f, W, 1.f / 60.f); }
		TestEqual("6 quadros: 1,0", W, 1.f, 1e-5f);
		TestEqual("dano menor depois: nao volta", FDNDamageMorph::Weight(Config, 0.2f, 0.8f, 1.f), 0.8f);
		TestEqual("subida configuravel", [] { FDNDamageConfig C; C.MorphRisePerSecond = 2.f; return FDNDamageMorph::Weight(C, 1.f, 0.f, 0.1f); }(), 0.2f, 1e-5f);
	});

	It("M3: entradas invalidas", [this]()
	{
		TestEqual("dano NaN: fica", FDNDamageMorph::Weight(Config, NaNf, 0.3f, 1.f), 0.3f);
		TestEqual("dano 2: limitado a 1", FDNDamageMorph::Weight(Config, 2.f, 0.f, 1.f), 1.f);
		TestEqual("dano negativo: fica", FDNDamageMorph::Weight(Config, -1.f, 0.3f, 1.f), 0.3f);
		TestEqual("anterior NaN vale 0", FDNDamageMorph::Weight(Config, 0.f, NaNf, 1.f), 0.f);
		TestEqual("anterior 1,5 limitado a 1", FDNDamageMorph::Weight(Config, 0.f, 1.5f, 1.f), 1.f);
		TestEqual("Dt 0: nao sobe", FDNDamageMorph::Weight(Config, 1.f, 0.2f, 0.f), 0.2f);
		TestEqual("Dt negativo: nao sobe", FDNDamageMorph::Weight(Config, 1.f, 0.2f, -1.f), 0.2f);
		TestEqual("Dt NaN: nao sobe", FDNDamageMorph::Weight(Config, 1.f, 0.2f, NaNf), 0.2f);
	});

	It("M4: configuracao invalida usa o padrao (0,7 e 10/s)", [this]()
	{
		FDNDamageConfig Bad;
		Bad.MorphCurveExponent = 0.f;
		Bad.MorphRisePerSecond = NaNf;
		TestEqual("expoente 0 vira 0,7", FDNDamageMorph::Weight(Bad, 0.5f, 0.f, 10.f), FMath::Pow(0.5f, 0.7f), 1e-5f);
		TestEqual("subida NaN vira 10/s", FDNDamageMorph::Weight(Bad, 1.f, 0.f, 0.05f), 0.5f, 1e-5f);
	});

	It("M5: Weights cobre as 5 zonas com alvo, na ordem de EDNDamageZone, e ignora a Under", [this]()
	{
		FDNDamageState S;
		S.ZoneDamage[static_cast<int32>(EDNDamageZone::Front)] = 1.f;
		S.ZoneDamage[static_cast<int32>(EDNDamageZone::Roof)] = 0.5f;
		S.ZoneDamage[static_cast<int32>(EDNDamageZone::Under)] = 1.f;
		float W[FDNDamageMorph::ZoneCount] = {};
		FDNDamageMorph::Weights(Config, S, W, 10.f);
		TestEqual("frente", W[0], 1.f, 1e-6f);
		TestEqual("traseira", W[1], 0.f);
		TestEqual("teto", W[4], FMath::Pow(0.5f, 0.7f), 1e-5f);
		TestEqual("5 alvos", FDNDamageMorph::ZoneCount, 5);
		// os nomes têm de bater com o script do Blender na ordem de EDNDamageZone: trocar dois amassaria o lado errado
		const TCHAR* Expected[] = { TEXT("DN_Front"), TEXT("DN_Rear"), TEXT("DN_Left"), TEXT("DN_Right"), TEXT("DN_Roof") };
		for (int32 Zone = 0; Zone < FDNDamageMorph::ZoneCount; ++Zone)
		{
			TestEqual(FString::Printf(TEXT("nome do alvo %d"), Zone), FString(FDNDamageMorph::TargetName(Zone)), FString(Expected[Zone]));
		}
		TestEqual("zona fora: sem nome", FString(FDNDamageMorph::TargetName(5)), FString());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
