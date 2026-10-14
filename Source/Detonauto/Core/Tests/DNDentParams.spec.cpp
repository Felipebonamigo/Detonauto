// Testes da parte pura do protótipo de amassado (lote 10): como os amassados viram parâmetros do material, de que lado
// do carro foi uma batida e que variação de velocidade ela causou. Nada aqui depende de mundo nem de física.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNDentParams.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDentParamsSpec, "Detonauto.Amassado",
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

	// ---------------- Lote 11: calibracao da batida real ----------------

	Describe("Variacao de velocidade antes e depois do contato", [this]()
	{
		It("e o modulo da diferenca das velocidades lineares, em m/s (entrada em cm/s)", [this]()
		{
			// de frente a 100 km/h (2778 cm/s) para parado: 27,78 m/s
			TestEqual("frente ate parar", FDNDentParams::DeltaVMsBetween(FVector(2778, 0, 0), FVector::ZeroVector), 27.78f, 1e-3f);
			// com quique: de +2778 para -1000: 37,78 m/s
			TestEqual("com quique", FDNDentParams::DeltaVMsBetween(FVector(2778, 0, 0), FVector(-1000, 0, 0)), 37.78f, 1e-3f);
			// de lado: so a componente que mudou
			TestEqual("lateral", FDNDentParams::DeltaVMsBetween(FVector(1000, 0, 0), FVector(1000, 300, 400)), 5.f, 1e-4f);
			TestEqual("sem mudanca", FDNDentParams::DeltaVMsBetween(FVector(500, 5, 5), FVector(500, 5, 5)), 0.f);
		});

		It("a ordem nao importa e NaN ou infinito valem 0", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			TestEqual("simetrico", FDNDentParams::DeltaVMsBetween(FVector::ZeroVector, FVector(2778, 0, 0)), 27.78f, 1e-3f);
			TestEqual("NaN antes", FDNDentParams::DeltaVMsBetween(FVector(NaN, 0, 0), FVector::ZeroVector), 0.f);
			TestEqual("infinito depois", FDNDentParams::DeltaVMsBetween(FVector::ZeroVector, FVector(0, std::numeric_limits<float>::infinity(), 0)), 0.f);
		});
	});

	Describe("Agrupar os contatos de uma batida", [this]()
	{
		It("o primeiro contato abre a janela; so o MAIOR valor da janela sai, com o ponto dele", [this]()
		{
			FDNImpactGrouper G;
			TestFalse("fechado no comeco", G.IsOpen());
			G.Add(10.0, 8.f, FVector(1, 0, 0));
			TestTrue("aberto", G.IsOpen());
			G.Add(10.05, 15.f, FVector(2, 0, 0));   // o maior
			G.Add(10.10, 12.f, FVector(3, 0, 0));   // o quique: menor, nao conta de novo
			FDNImpactGrouper::FGroup Out;
			TestFalse("antes de 0,15 s nao sai", G.TryFlush(10.14, Out));
			if (!TestTrue("em 0,15 s sai (inclusivo)", G.TryFlush(10.15, Out))) { return; }
			TestEqual("o maior", Out.DeltaVMs, 15.f);
			TestEqual("o ponto do maior", Out.LocalPoint, FVector(2, 0, 0));
			TestFalse("fechou", G.IsOpen());
		});

		It("empate no maior fica com o primeiro; valores iguais nao somam", [this]()
		{
			FDNImpactGrouper G;
			G.Add(0.0, 10.f, FVector(1, 0, 0));
			G.Add(0.02, 10.f, FVector(2, 0, 0));
			FDNImpactGrouper::FGroup Out;
			G.TryFlush(1.0, Out);
			TestEqual("nao somou", Out.DeltaVMs, 10.f);
			TestEqual("ficou o primeiro", Out.LocalPoint, FVector(1, 0, 0));
		});

		It("depois de sair, o proximo contato abre outra janela, independente", [this]()
		{
			FDNImpactGrouper G;
			FDNImpactGrouper::FGroup Out;
			G.Add(0.0, 10.f, FVector::ZeroVector);
			TestTrue("1a sai", G.TryFlush(0.2, Out));
			TestFalse("sem janela aberta nao sai nada", G.TryFlush(0.5, Out));
			G.Add(0.6, 4.f, FVector(7, 0, 0));
			TestTrue("2a sai", G.TryFlush(0.75, Out));
			TestEqual("a 2a nao herda o maior da 1a", Out.DeltaVMs, 4.f);
			TestTrue("a janela da 2a conta do primeiro contato dela", FMath::IsNearlyEqual(Out.StartTime, 0.6));
		});

		It("valor invalido (0, negativo, NaN, infinito) nao abre nem muda a janela", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			FDNImpactGrouper G;
			for (const float V : { 0.f, -3.f, NaN, std::numeric_limits<float>::infinity() }) { G.Add(0.0, V, FVector::ZeroVector); }
			TestFalse("nao abriu", G.IsOpen());
			G.Add(1.0, 6.f, FVector(1, 0, 0));
			G.Add(1.01, NaN, FVector(9, 0, 0));
			FDNImpactGrouper::FGroup Out;
			G.TryFlush(2.0, Out);
			TestEqual("so o valido", Out.DeltaVMs, 6.f);
		});

		It("fronteira exata: com o primeiro contato em 0,0 s, em 0,15 s sai; um pouco antes nao", [this]()
		{
			FDNImpactGrouper Early;
			Early.Add(0.0, 5.f, FVector::ZeroVector);
			FDNImpactGrouper::FGroup Out;
			TestFalse("0,149 nao sai", Early.TryFlush(0.149, Out));
			FDNImpactGrouper Exact;
			Exact.Add(0.0, 5.f, FVector::ZeroVector);
			TestTrue("0,15 sai (inclusivo)", Exact.TryFlush(FDNImpactGrouper::WindowSeconds, Out));
			FDNImpactGrouper Late;
			Late.Add(0.0, 5.f, FVector::ZeroVector);
			TestTrue("0,151 sai", Late.TryFlush(0.151, Out));
		});

		It("a janela e de 0,15 s", [this]()
		{
			TestEqual("janela", static_cast<float>(FDNImpactGrouper::WindowSeconds), 0.15f);
		});
	});

	Describe("Ponto do amassado: sobe do chao e entra na caixa do corpo", [this]()
	{
		const FVector Center(0, 0, 60);
		const FVector Extent(230, 100, 60); // o corpo vai de Z = 0 a Z = 120

		It("ponto abaixo de 20 cm acima da base sobe ate 20 cm", [this, Center, Extent]()
		{
			TestEqual("no chao", FDNDentParams::ProjectToBody(FVector(230, 0, 1.8f), Center, Extent, 20.f), FVector(230, 0, 20));
			TestEqual("abaixo da base", FDNDentParams::ProjectToBody(FVector(230, 0, -30), Center, Extent, 20.f), FVector(230, 0, 20));
			TestEqual("a 19,9", static_cast<float>(FDNDentParams::ProjectToBody(FVector(0, 100, 19.9f), Center, Extent, 20.f).Z), 20.f);
		});

		It("ponto de 20 cm para cima fica onde esta", [this, Center, Extent]()
		{
			TestEqual("exatamente 20", FDNDentParams::ProjectToBody(FVector(100, -50, 20), Center, Extent, 20.f), FVector(100, -50, 20));
			TestEqual("alto", FDNDentParams::ProjectToBody(FVector(-100, 30, 90), Center, Extent, 20.f), FVector(-100, 30, 90));
		});

		It("ponto fora da caixa vem para a superficie dela", [this, Center, Extent]()
		{
			TestEqual("na frente da caixa", FDNDentParams::ProjectToBody(FVector(400, 10, 60), Center, Extent, 20.f), FVector(230, 10, 60));
			TestEqual("ao lado", FDNDentParams::ProjectToBody(FVector(0, -300, 60), Center, Extent, 20.f), FVector(0, -100, 60));
			TestEqual("acima", FDNDentParams::ProjectToBody(FVector(0, 0, 500), Center, Extent, 20.f), FVector(0, 0, 120));
		});

		It("entrada invalida ou altura minima maior que o corpo nao quebra", [this, Center, Extent]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			TestEqual("NaN cai no centro", FDNDentParams::ProjectToBody(FVector(NaN, 0, 0), Center, Extent, 20.f), Center);
			// altura minima absurda: nunca sai da caixa
			TestEqual("minima de 500 cm", static_cast<float>(FDNDentParams::ProjectToBody(FVector(0, 0, 10), Center, Extent, 500.f).Z), 120.f);
			TestEqual("minima negativa vale 0", static_cast<float>(FDNDentParams::ProjectToBody(FVector(0, 0, 10), Center, Extent, -5.f).Z), 10.f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
