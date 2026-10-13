// Testes da parte pura do protótipo de amassado (lote 10): como os amassados viram parâmetros do material, de que lado
// do carro foi uma batida e que variação de velocidade ela causou. Nada aqui depende de mundo nem de física.

#include <limits>
#include "Misc/AutomationTest.h"
#include "DNDentParams.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDentParamsSpec, "VeiculoTeste.Amassado",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDentParamsSpec)

void FDNDentParamsSpec::Define()
{
	Describe("Nomes dos parametros do material", [this]()
	{
		It("sao D<i>P (posicao), D<i>R (raio) e D<i>D (profundidade), de 0 a 7, os mesmos de Tools/Unreal/create_dent_material.py", [this]()
		{
			TestEqual("slots", FDNDentParams::MaxSlots, 8);
			TestEqual("P 0", FDNDentParams::PositionName(0).ToString(), FString(TEXT("D0P")));
			TestEqual("P 7", FDNDentParams::PositionName(7).ToString(), FString(TEXT("D7P")));
			TestEqual("R 2", FDNDentParams::RadiusName(2).ToString(), FString(TEXT("D2R")));
			TestEqual("D 0", FDNDentParams::DepthName(0).ToString(), FString(TEXT("D0D")));
			TestEqual("D 3", FDNDentParams::DepthName(3).ToString(), FString(TEXT("D3D")));
		});
	});

	Describe("Slot de um amassado", [this]()
	{
		It("posicao no vetor (X, Y, Z); raio e profundidade em escalares separados", [this]()
		{
			TArray<FDNDent> Dents;
			FDNDent D;
			D.LocalPositionCm = FVector(10, 20, 30);
			D.DepthCm = 12.f;
			D.RadiusCm = 42.f;
			Dents.Add(D);
			const FDNDentParams::FSlot S = FDNDentParams::Slot(Dents, 0);
			TestEqual("posicao", S.Position, FLinearColor(10.f, 20.f, 30.f, 0.f));
			TestEqual("raio", S.Radius, 42.f);
			TestEqual("profundidade", S.Depth, 12.f);
		});

		It("slot sem amassado, ou indice fora de 0..7, desliga (raio 0)", [this]()
		{
			TArray<FDNDent> Dents;
			Dents.AddDefaulted();
			for (const int32 Index : { 1, 7, -1, 8, 100 })
			{
				const FDNDentParams::FSlot S = FDNDentParams::Slot(Dents, Index);
				TestEqual(FString::Printf(TEXT("indice %d: raio"), Index), S.Radius, 0.f);
				TestEqual(FString::Printf(TEXT("indice %d: profundidade"), Index), S.Depth, 0.f);
			}
		});

		It("com mais de 8 amassados na lista, so os 8 primeiros viram slot (o material nao tem mais)", [this]()
		{
			TArray<FDNDent> Dents;
			for (int32 i = 0; i < 10; ++i)
			{
				FDNDent D;
				D.RadiusCm = 30.f;
				D.DepthCm = 5.f;
				Dents.Add(D);
			}
			TestEqual("o 8o (indice 7) liga", FDNDentParams::Slot(Dents, 7).Radius, 30.f);
			TestEqual("o 9o (indice 8) fica desligado", FDNDentParams::Slot(Dents, 8).Radius, 0.f);
			TestEqual("o 10o (indice 9) fica desligado", FDNDentParams::Slot(Dents, 9).Depth, 0.f);
		});

		It("amassado com raio <= 0 ou NaN vira slot desligado", [this]()
		{
			TArray<FDNDent> Dents;
			FDNDent A; A.RadiusCm = 0.f; A.DepthCm = 10.f; Dents.Add(A);
			FDNDent B; B.RadiusCm = std::numeric_limits<float>::quiet_NaN(); B.DepthCm = 10.f; Dents.Add(B);
			FDNDent C; C.RadiusCm = 30.f; C.DepthCm = std::numeric_limits<float>::infinity(); Dents.Add(C);
			for (int32 i = 0; i < 3; ++i)
			{
				const FDNDentParams::FSlot S = FDNDentParams::Slot(Dents, i);
				TestEqual(FString::Printf(TEXT("%d: raio desligado"), i), S.Radius, 0.f);
				TestEqual(FString::Printf(TEXT("%d: profundidade zerada"), i), S.Depth, 0.f);
			}
		});
	});

	Describe("Quantos amassados o material comporta", [this]()
	{
		It("o limite da configuracao e cortado em 8 (e fica em pelo menos 1)", [this]()
		{
			TestEqual("24 -> 8", FDNDentParams::EffectiveMaxDents(24), 8);
			TestEqual("8 -> 8", FDNDentParams::EffectiveMaxDents(8), 8);
			TestEqual("3 -> 3", FDNDentParams::EffectiveMaxDents(3), 3);
			TestEqual("0 -> 1", FDNDentParams::EffectiveMaxDents(0), 1);
			TestEqual("-5 -> 1", FDNDentParams::EffectiveMaxDents(-5), 1);
			TestEqual("o padrao do Core cabe", FDNDentParams::EffectiveMaxDents(FDNDamageConfig().MaxDents), FDNDamageConfig().MaxDents);
		});
	});

	Describe("Lado do carro de um ponto de batida", [this]()
	{
		const FVector Center(0, 0, 70);
		const FVector Extent(230, 100, 70);

		It("o eixo com maior proporcao da caixa decide, e o sinal diz o lado", [this, Center, Extent]()
		{
			TestTrue("frente", FDNDentParams::ZoneOfLocalPoint(FVector(230, 0, 70), Center, Extent) == EDNDamageZone::Front);
			TestTrue("traseira", FDNDentParams::ZoneOfLocalPoint(FVector(-230, 0, 70), Center, Extent) == EDNDamageZone::Rear);
			TestTrue("direita", FDNDentParams::ZoneOfLocalPoint(FVector(0, 100, 70), Center, Extent) == EDNDamageZone::Right);
			TestTrue("esquerda", FDNDentParams::ZoneOfLocalPoint(FVector(0, -100, 70), Center, Extent) == EDNDamageZone::Left);
			TestTrue("teto", FDNDentParams::ZoneOfLocalPoint(FVector(0, 0, 140), Center, Extent) == EDNDamageZone::Roof);
			TestTrue("fundo", FDNDentParams::ZoneOfLocalPoint(FVector(0, 0, 0), Center, Extent) == EDNDamageZone::Under);
		});

		It("quina da frente com a lateral: ganha o eixo mais proximo da borda da caixa", [this, Center, Extent]()
		{
			TestTrue("mais perto da frente", FDNDentParams::ZoneOfLocalPoint(FVector(220, 60, 70), Center, Extent) == EDNDamageZone::Front);
			TestTrue("mais perto da lateral", FDNDentParams::ZoneOfLocalPoint(FVector(100, 98, 70), Center, Extent) == EDNDamageZone::Right);
		});

		It("caixa sem tamanho ou ponto invalido nao quebra: cai na frente", [this, Center]()
		{
			TestTrue("extensao zero", FDNDentParams::ZoneOfLocalPoint(FVector(1, 2, 3), Center, FVector::ZeroVector) == EDNDamageZone::Front);
			TestTrue("ponto NaN", FDNDentParams::ZoneOfLocalPoint(FVector(std::numeric_limits<float>::quiet_NaN(), 0, 0), Center, FVector(1, 1, 1)) == EDNDamageZone::Front);
		});
	});

	Describe("Variacao de velocidade de um impulso", [this]()
	{
		It("impulso (kg cm/s) / massa (kg) / 100 = m/s", [this]()
		{
			TestEqual("27,78 m/s com 1500 kg", FDNDentParams::DeltaVMsFromImpulse(1500.f * 2778.f, 1500.f), 27.78f, 1e-3f);
			TestEqual("10 m/s com 1200 kg", FDNDentParams::DeltaVMsFromImpulse(1200.f * 1000.f, 1200.f), 10.f, 1e-4f);
		});

		It("massa zero, negativa ou NaN, e impulso negativo ou NaN, valem 0", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			TestEqual("massa 0", FDNDentParams::DeltaVMsFromImpulse(1000.f, 0.f), 0.f);
			TestEqual("massa negativa", FDNDentParams::DeltaVMsFromImpulse(1000.f, -5.f), 0.f);
			TestEqual("massa NaN", FDNDentParams::DeltaVMsFromImpulse(1000.f, NaN), 0.f);
			TestEqual("impulso negativo", FDNDentParams::DeltaVMsFromImpulse(-1000.f, 1500.f), 0.f);
			TestEqual("impulso NaN", FDNDentParams::DeltaVMsFromImpulse(NaN, 1500.f), 0.f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
