// Testes das regras puras do dano (docs/regras-dano.md): limiar, dano por zona, amassados com fusão, peças soltas,
// veículo destruído e o pool de peças soltas. Funções puras, sem mundo.
//
// Decisões de implementação (o contrato não cobre; ver Core/DNDamage.h):
//  D1 no limiar exato (DeltaV == DentMinDeltaVMs) o impacto causa dano de valor 0 e um amassado de profundidade 0
//     (o contrato diz "gera um registro" e "2,0 causa");
//  D2 AddedZoneDamage é o aumento REAL da zona, depois da saturação em 1,0;
//  D3 a fusão compara a distância com a metade do raio do amassado EXISTENTE (inclusive); havendo vários ao alcance,
//     funde no mais próximo (empate: o mais antigo);
//  D4 posição não finita: o amassado é ignorado, o dano segue;
//  D5 zona fora do enum: o impacto é ignorado;
//  D6 as peças são avaliadas em todas as zonas (a regra do dano da zona vale para as peças de qualquer zona);
//  D7 configuração inválida não quebra: valores <= 0 que viram divisor são trocados por um mínimo minúsculo e as
//     contagens ficam em pelo menos 1 (o validador do CSV é quem recusa).

#include "Misc/AutomationTest.h"
#include "Core/DNDamage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FDNImpact Hit(float DeltaV, EDNDamageZone Zone = EDNDamageZone::Front, FVector Position = FVector::ZeroVector)
	{
		FDNImpact I;
		I.DeltaVMs = DeltaV;
		I.Zone = Zone;
		I.LocalPositionCm = Position;
		return I;
	}

	FString ShowParts(const TArray<EDNDamagePart>& Parts)
	{
		FString Text;
		for (const EDNDamagePart P : Parts) { Text += FString::Printf(TEXT("%d "), static_cast<int32>(P)); }
		return Text.IsEmpty() ? FString(TEXT("(nenhuma)")) : Text;
	}

	bool SameParts(const TArray<EDNDamagePart>& A, std::initializer_list<EDNDamagePart> B)
	{
		if (A.Num() != static_cast<int32>(B.size())) { return false; }
		int32 i = 0;
		for (const EDNDamagePart P : B) { if (A[i++] != P) { return false; } }
		return true;
	}

	bool Untouched(const FDNDamageState& S)
	{
		for (int32 i = 0; i < FDNDamageState::ZoneCount; ++i) { if (S.ZoneDamage[i] != 0.0f) { return false; } }
		for (int32 i = 0; i < FDNDamageState::PartCount; ++i) { if (S.bPartDetached[i]) { return false; } }
		return S.Dents.IsEmpty() && !S.bDestroyed;
	}
}

BEGIN_DEFINE_SPEC(FDNDamageSpec, "Detonauto.Dano",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	const float Inff = std::numeric_limits<float>::infinity();
END_DEFINE_SPEC(FDNDamageSpec)

void FDNDamageSpec::Define()
{
	Describe("Limiar e entrada invalida", [this]()
	{
		It("abaixo do limiar (1,99) nao causa dano algum", [this]()
		{
			FDNDamageState S;
			const FDNDamageResult R = FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(1.99f));
			TestEqual("dano adicionado", R.AddedZoneDamage, 0.f);
			TestFalse("sem amassado", R.bDentAdded);
			TestEqual("sem pecas", R.DetachedParts.Num(), 0);
			TestFalse("nao destruiu", R.bJustDestroyed);
			TestTrue("estado intacto", Untouched(S));
		});

		It("no limiar (2,0) causa: amassado de profundidade 0 e dano 0 (D1)", [this]()
		{
			FDNDamageState S;
			const FDNDamageResult R = FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(2.0f, EDNDamageZone::Left, FVector(1, 2, 3)));
			TestTrue("amassado", R.bDentAdded);
			TestEqual("profundidade", R.Dent.DepthCm, 0.f);
			TestEqual("raio", R.Dent.RadiusCm, 30.f);
			TestEqual("posicao", R.Dent.LocalPositionCm, FVector(1, 2, 3));
			TestEqual("dano adicionado", R.AddedZoneDamage, 0.f);
			TestEqual("um amassado guardado", S.Dents.Num(), 1);
			TestEqual("dano da zona", S.ZoneDamage[static_cast<int32>(EDNDamageZone::Left)], 0.f);
		});

		It("acima do limiar (2,01) causa dano e amassado proporcionais", [this]()
		{
			FDNDamageState S;
			const FDNDamageResult R = FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(2.01f));
			TestEqual("dano", R.AddedZoneDamage, 0.01f / 30.f, 1e-6f);
			TestEqual("profundidade", R.Dent.DepthCm, 0.015f, 1e-5f);
			TestEqual("raio", R.Dent.RadiusCm, 30.015f, 1e-4f);
		});

		It("NaN, infinito e negativo em DeltaV sao ignorados", [this]()
		{
			for (const float V : { NaNf, Inff, -Inff, -1.f, -0.001f, -1000.f })
			{
				FDNDamageState S;
				const FDNDamageResult R = FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(V));
				TestFalse(FString::Printf(TEXT("%f: sem amassado"), V), R.bDentAdded);
				TestEqual(FString::Printf(TEXT("%f: sem dano"), V), R.AddedZoneDamage, 0.f);
				TestTrue(FString::Printf(TEXT("%f: estado intacto"), V), Untouched(S));
			}
		});

		It("negativo e ignorado MESMO com limiar zero na configuracao", [this]()
		{
			FDNDamageConfig C;
			C.DentMinDeltaVMs = 0.f;
			for (const float V : { -0.001f, -1.f })
			{
				FDNDamageState S;
				const FDNDamageResult R = FDNDamage::ApplyImpact(C, S, Hit(V));
				TestFalse(FString::Printf(TEXT("%f: sem amassado"), V), R.bDentAdded);
				TestTrue(FString::Printf(TEXT("%f: estado intacto"), V), Untouched(S));
			}
			FDNDamageState Zero;
			TestTrue("com limiar 0, DeltaV 0 conta (>= inclusivo)", FDNDamage::ApplyImpact(C, Zero, Hit(0.f)).bDentAdded);
		});

		It("D5: zona fora do enum e ignorada", [this]()
		{
			FDNDamageState S;
			FDNImpact I = Hit(20.f);
			I.Zone = static_cast<EDNDamageZone>(200);
			const FDNDamageResult R = FDNDamage::ApplyImpact(FDNDamageConfig(), S, I);
			TestFalse("sem amassado", R.bDentAdded);
			TestTrue("estado intacto", Untouched(S));
		});
	});

	Describe("Dano da zona", [this]()
	{
		It("soma (DeltaV - 2) / 30 so na zona atingida", [this]()
		{
			FDNDamageState S;
			const FDNDamageResult R = FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(17.f, EDNDamageZone::Rear));
			TestEqual("adicionado", R.AddedZoneDamage, 0.5f);
			TestEqual("traseira", S.ZoneDamage[static_cast<int32>(EDNDamageZone::Rear)], 0.5f);
			TestEqual("frente intacta", S.ZoneDamage[static_cast<int32>(EDNDamageZone::Front)], 0.f);
			TestEqual("esquerda intacta", S.ZoneDamage[static_cast<int32>(EDNDamageZone::Left)], 0.f);
		});

		It("cada zona acumula a sua", [this]()
		{
			FDNDamageState S;
			const EDNDamageZone Zones[] = { EDNDamageZone::Front, EDNDamageZone::Rear, EDNDamageZone::Left, EDNDamageZone::Right, EDNDamageZone::Roof, EDNDamageZone::Under };
			for (int32 i = 0; i < 6; ++i) { FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(2.f + 3.f * (i + 1), Zones[i], FVector(i * 1000.f, 0, 0))); }
			for (int32 i = 0; i < 6; ++i) { TestEqual(FString::Printf(TEXT("zona %d"), i), S.ZoneDamage[i], 0.1f * (i + 1), 1e-5f); }
		});

		It("satura em 1,0 e AddedZoneDamage e o aumento real (D2)", [this]()
		{
			FDNDamageState S;
			const FDNDamageConfig C;
			TestEqual("1o: 0,5", FDNDamage::ApplyImpact(C, S, Hit(17.f)).AddedZoneDamage, 0.5f);
			TestEqual("2o: mais 0,5 (chega a 1,0)", FDNDamage::ApplyImpact(C, S, Hit(17.f, EDNDamageZone::Front, FVector(500, 0, 0))).AddedZoneDamage, 0.5f);
			TestEqual("zona em 1,0", S.ZoneDamage[0], 1.f);
			TestEqual("3o: ja saturada, nada a mais", FDNDamage::ApplyImpact(C, S, Hit(17.f, EDNDamageZone::Front, FVector(1000, 0, 0))).AddedZoneDamage, 0.f);
			TestEqual("continua 1,0", S.ZoneDamage[0], 1.f);
			FDNDamageState Big;
			TestEqual("impacto enorme: 1,0", FDNDamage::ApplyImpact(C, Big, Hit(1.0e9f)).AddedZoneDamage, 1.f);
			TestEqual("zona em 1,0", Big.ZoneDamage[0], 1.f);
		});

		It("o dano cabe exatamente em 32 m/s (2 + 30)", [this]()
		{
			FDNDamageState S;
			FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(32.f, EDNDamageZone::Left));
			TestEqual("1,0", S.ZoneDamage[static_cast<int32>(EDNDamageZone::Left)], 1.f);
		});

		It("TotalDamage e a media das 5 zonas e ignora a Under", [this]()
		{
			FDNDamageState S;
			TestEqual("zerado", FDNDamage::TotalDamage(S), 0.f);
			S.ZoneDamage[static_cast<int32>(EDNDamageZone::Front)] = 1.f;
			S.ZoneDamage[static_cast<int32>(EDNDamageZone::Under)] = 1.f;
			TestEqual("1 de 5 zonas contam; a Under nao", FDNDamage::TotalDamage(S), 0.2f);
			S.ZoneDamage[static_cast<int32>(EDNDamageZone::Roof)] = 0.5f;
			TestEqual("1,5 / 5", FDNDamage::TotalDamage(S), 0.3f, 1e-6f);
		});
	});

	Describe("Amassados", [this]()
	{
		It("profundidade = (DeltaV - 2) x 1,5 e raio = 30 + profundidade", [this]()
		{
			FDNDamageState S;
			const FDNDamageResult R = FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(6.f, EDNDamageZone::Front, FVector(10, 20, 30)));
			TestTrue("amassado", R.bDentAdded);
			TestEqual("profundidade", R.Dent.DepthCm, 6.f);
			TestEqual("raio", R.Dent.RadiusCm, 36.f);
			TestEqual("posicao", R.Dent.LocalPositionCm, FVector(10, 20, 30));
			if (TestEqual("guardado", S.Dents.Num(), 1)) { TestEqual("profundidade guardada", S.Dents[0].DepthCm, 6.f); }
		});

		It("a profundidade satura em 25 cm", [this]()
		{
			FDNDamageState S;
			const FDNDamageResult R = FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(100.f));
			TestEqual("profundidade", R.Dent.DepthCm, 25.f);
			TestEqual("raio", R.Dent.RadiusCm, 55.f);
		});

		It("funde no existente a menos da metade do raio dele: centro do mais fundo, profundidade somada, maior raio", [this]()
		{
			FDNDamageState S;
			const FDNDamageConfig C;
			FDNDamage::ApplyImpact(C, S, Hit(4.f, EDNDamageZone::Front, FVector(0, 0, 0)));      // profundidade 3, raio 33 (metade 16,5)
			const FDNDamageResult R = FDNDamage::ApplyImpact(C, S, Hit(10.f, EDNDamageZone::Front, FVector(10, 0, 0))); // profundidade 12, raio 42
			TestTrue("DentAdded continua verdadeiro (houve amassado novo, fundido)", R.bDentAdded);
			if (!TestEqual("um amassado so", S.Dents.Num(), 1)) { return; }
			TestEqual("centro do mais fundo (o novo)", S.Dents[0].LocalPositionCm, FVector(10, 0, 0));
			TestEqual("profundidade somada", S.Dents[0].DepthCm, 15.f);
			TestEqual("maior raio", S.Dents[0].RadiusCm, 42.f);

			FDNDamageState T;
			FDNDamage::ApplyImpact(C, T, Hit(10.f, EDNDamageZone::Front, FVector(0, 0, 0)));     // profundidade 12, raio 42
			FDNDamage::ApplyImpact(C, T, Hit(4.f, EDNDamageZone::Front, FVector(10, 0, 0)));     // profundidade 3, raio 33
			if (!TestEqual("T: um amassado so", T.Dents.Num(), 1)) { return; }
			TestEqual("T: o centro fica no mais fundo (o existente)", T.Dents[0].LocalPositionCm, FVector(0, 0, 0));
			TestEqual("T: profundidade somada", T.Dents[0].DepthCm, 15.f);
			TestEqual("T: maior raio", T.Dents[0].RadiusCm, 42.f);
		});

		It("a profundidade somada satura em 25", [this]()
		{
			FDNDamageState S;
			const FDNDamageConfig C;
			FDNDamage::ApplyImpact(C, S, Hit(2.f + 20.f / 1.5f));
			FDNDamage::ApplyImpact(C, S, Hit(2.f + 20.f / 1.5f, EDNDamageZone::Front, FVector(5, 0, 0)));
			if (TestEqual("fundiu", S.Dents.Num(), 1)) { TestEqual("profundidade", S.Dents[0].DepthCm, 25.f); }
		});

		It("a fusao a EXATAMENTE metade do raio conta (inclusivo); um pouco mais longe nao", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState In;
			FDNDamage::ApplyImpact(C, In, Hit(6.f, EDNDamageZone::Front, FVector::ZeroVector)); // raio 36, metade 18
			FDNDamage::ApplyImpact(C, In, Hit(6.f, EDNDamageZone::Front, FVector(18, 0, 0)));
			TestEqual("a 18,00: funde", In.Dents.Num(), 1);
			FDNDamageState Out;
			FDNDamage::ApplyImpact(C, Out, Hit(6.f, EDNDamageZone::Front, FVector::ZeroVector));
			FDNDamage::ApplyImpact(C, Out, Hit(6.f, EDNDamageZone::Front, FVector(18.01f, 0, 0)));
			TestEqual("a 18,01: nao funde", Out.Dents.Num(), 2);
		});

		It("D3: a metade vem do raio do EXISTENTE, nao do novo", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState S;
			FDNDamage::ApplyImpact(C, S, Hit(100.f, EDNDamageZone::Front, FVector::ZeroVector)); // raio 55, metade 27,5
			FDNDamage::ApplyImpact(C, S, Hit(3.f, EDNDamageZone::Front, FVector(25, 0, 0)));     // raio novo 31,5 (metade 15,75): so o do existente alcanca
			TestEqual("fundiu", S.Dents.Num(), 1);
		});

		It("D3: com varios ao alcance, funde no mais proximo", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState S;
			FDNDamage::ApplyImpact(C, S, Hit(8.f, EDNDamageZone::Front, FVector(0, 0, 0)));     // A: profundidade 9, raio 39, metade 19,5
			FDNDamage::ApplyImpact(C, S, Hit(8.f, EDNDamageZone::Front, FVector(20, 0, 0)));    // B: a 20 de A, fora do alcance: separado
			if (!TestEqual("dois separados", S.Dents.Num(), 2)) { return; }
			FDNDamage::ApplyImpact(C, S, Hit(4.f, EDNDamageZone::Front, FVector(8, 0, 0)));     // profundidade 3: a 8 de A e a 12 de B, ambos ao alcance
			if (!TestEqual("continuam dois", S.Dents.Num(), 2)) { return; }
			TestEqual("A engordou (o mais proximo): 9 + 3", S.Dents[0].DepthCm, 12.f);
			TestEqual("B intacto", S.Dents[1].DepthCm, 9.f);

			// agora o novo fica mais perto do B (que e o segundo da lista): a fusao nao pode ir para o primeiro ao alcance
			FDNDamageState T;
			FDNDamage::ApplyImpact(C, T, Hit(8.f, EDNDamageZone::Front, FVector(0, 0, 0)));
			FDNDamage::ApplyImpact(C, T, Hit(8.f, EDNDamageZone::Front, FVector(20, 0, 0)));
			FDNDamage::ApplyImpact(C, T, Hit(4.f, EDNDamageZone::Front, FVector(12, 0, 0)));    // a 12 de A e a 8 de B
			if (!TestEqual("T: continuam dois", T.Dents.Num(), 2)) { return; }
			TestEqual("T: A intacto", T.Dents[0].DepthCm, 9.f);
			TestEqual("T: B engordou (o mais proximo): 9 + 3", T.Dents[1].DepthCm, 12.f);
		});

		It("D3: com varios ao alcance e a mesma distancia, funde no mais antigo", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState S;
			FDNDamage::ApplyImpact(C, S, Hit(8.f, EDNDamageZone::Front, FVector(0, 0, 0)));     // A: profundidade 9, raio 39, metade 19,5
			FDNDamage::ApplyImpact(C, S, Hit(8.f, EDNDamageZone::Front, FVector(20, 0, 0)));    // B: igual a A, a 20
			if (!TestEqual("dois separados", S.Dents.Num(), 2)) { return; }
			FDNDamage::ApplyImpact(C, S, Hit(4.f, EDNDamageZone::Front, FVector(10, 0, 0)));    // profundidade 3, a 10 de cada um
			if (!TestEqual("continuam dois", S.Dents.Num(), 2)) { return; }
			TestEqual("o mais antigo (A) engordou", S.Dents[0].DepthCm, 12.f);
			TestEqual("B intacto", S.Dents[1].DepthCm, 9.f);
		});

		It("com o limite de 8 cheio e sem fusao, substitui o mais raso; o novo vira o mais recente", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState S;
			const float Depths[8] = { 9.f, 6.f, 7.5f, 3.f, 12.f, 4.5f, 10.5f, 15.f }; // profundidades; o mais raso e o indice 3 (3,0)
			for (int32 i = 0; i < 8; ++i)
			{
				FDNDamage::ApplyImpact(C, S, Hit(2.f + Depths[i] / 1.5f, EDNDamageZone::Front, FVector(i * 1000.f, 0, 0)));
			}
			if (!TestEqual("cheio", S.Dents.Num(), 8)) { return; }
			const FDNDamageResult R = FDNDamage::ApplyImpact(C, S, Hit(2.f + 5.f / 1.5f, EDNDamageZone::Front, FVector(9000.f, 0, 0)));
			TestTrue("amassado novo", R.bDentAdded);
			if (!TestEqual("continua 8", S.Dents.Num(), 8)) { return; }
			bool bOldGone = true;
			for (const FDNDent& D : S.Dents) { if (FMath::IsNearlyEqual(D.DepthCm, 3.f)) { bOldGone = false; } }
			TestTrue("o mais raso (3 cm) saiu", bOldGone);
			TestEqual("o novo e o ultimo", S.Dents[7].LocalPositionCm, FVector(9000.f, 0, 0));
			TestEqual("a ordem dos outros se manteve", S.Dents[3].LocalPositionCm, FVector(4000.f, 0, 0));
		});

		It("empate no mais raso: sai o mais antigo", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState S;
			const float Depths[8] = { 9.f, 3.f, 7.5f, 12.f, 3.f, 4.5f, 10.5f, 15.f }; // dois com 3 cm: indices 1 e 4
			for (int32 i = 0; i < 8; ++i)
			{
				FDNDamage::ApplyImpact(C, S, Hit(2.f + Depths[i] / 1.5f, EDNDamageZone::Front, FVector(i * 1000.f, 0, 0)));
			}
			FDNDamage::ApplyImpact(C, S, Hit(2.f + 6.f / 1.5f, EDNDamageZone::Front, FVector(9000.f, 0, 0)));
			bool bFirstGone = true, bSecondStays = false;
			for (const FDNDent& D : S.Dents)
			{
				if (D.LocalPositionCm.X == 1000.f) { bFirstGone = false; }
				if (D.LocalPositionCm.X == 4000.f) { bSecondStays = true; }
			}
			TestTrue("o indice 1 (mais antigo) saiu", bFirstGone);
			TestTrue("o indice 4 ficou", bSecondStays);
		});

		It("com o limite cheio, se a fusao e possivel funde em vez de substituir", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState S;
			for (int32 i = 0; i < 8; ++i) { FDNDamage::ApplyImpact(C, S, Hit(8.f, EDNDamageZone::Front, FVector(i * 1000.f, 0, 0))); }
			FDNDamage::ApplyImpact(C, S, Hit(8.f, EDNDamageZone::Front, FVector(1005.f, 0, 0)));
			TestEqual("continua 8", S.Dents.Num(), 8);
			TestEqual("o do indice 1 engordou", S.Dents[1].DepthCm, 18.f);
		});

		It("MaxDents menor na configuracao vale", [this]()
		{
			FDNDamageConfig C;
			C.MaxDents = 2;
			FDNDamageState S;
			for (int32 i = 0; i < 5; ++i) { FDNDamage::ApplyImpact(C, S, Hit(8.f, EDNDamageZone::Front, FVector(i * 1000.f, 0, 0))); }
			TestEqual("so 2", S.Dents.Num(), 2);
		});

		It("D4: posicao nao finita: sem amassado, o dano segue", [this]()
		{
			FDNDamageState S;
			const FDNDamageResult R = FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(17.f, EDNDamageZone::Front, FVector(NaNf, 0, 0)));
			TestFalse("sem amassado", R.bDentAdded);
			TestEqual("sem amassado guardado", S.Dents.Num(), 0);
			TestEqual("dano aplicado", R.AddedZoneDamage, 0.5f);
			const FDNDamageResult R2 = FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(17.f, EDNDamageZone::Rear, FVector(0, Inff, 0)));
			TestFalse("infinito: sem amassado", R2.bDentAdded);
			TestEqual("infinito: dano aplicado", R2.AddedZoneDamage, 0.5f);
		});
	});

	Describe("Pecas", [this]()
	{
		It("cada peca pertence a uma zona", [this]()
		{
			TestTrue("capo", FDNDamage::ZoneOfPart(EDNDamagePart::Hood) == EDNDamageZone::Front);
			TestTrue("para-choque dianteiro", FDNDamage::ZoneOfPart(EDNDamagePart::BumperFront) == EDNDamageZone::Front);
			TestTrue("vidro dianteiro", FDNDamage::ZoneOfPart(EDNDamagePart::GlassFront) == EDNDamageZone::Front);
			TestTrue("porta-malas", FDNDamage::ZoneOfPart(EDNDamagePart::Trunk) == EDNDamageZone::Rear);
			TestTrue("para-choque traseiro", FDNDamage::ZoneOfPart(EDNDamagePart::BumperRear) == EDNDamageZone::Rear);
			TestTrue("porta esquerda", FDNDamage::ZoneOfPart(EDNDamagePart::DoorLeft) == EDNDamageZone::Left);
			TestTrue("porta direita", FDNDamage::ZoneOfPart(EDNDamagePart::DoorRight) == EDNDamageZone::Right);
		});

		It("DeltaV >= 12 solta as pecas da zona atingida, em ordem do enum; 11,99 nao", [this]()
		{
			FDNDamageState S;
			TestEqual("11,99: nenhuma", FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(11.99f)).DetachedParts.Num(), 0);
			FDNDamageState T;
			const FDNDamageResult R = FDNDamage::ApplyImpact(FDNDamageConfig(), T, Hit(12.f));
			TestTrue(FString::Printf(TEXT("12,0: capo, para-choque e vidro; veio %s"), *ShowParts(R.DetachedParts)),
				SameParts(R.DetachedParts, { EDNDamagePart::Hood, EDNDamagePart::BumperFront, EDNDamagePart::GlassFront }));
			TestTrue("estado marca as tres", T.bPartDetached[static_cast<int32>(EDNDamagePart::Hood)] && T.bPartDetached[static_cast<int32>(EDNDamagePart::BumperFront)] && T.bPartDetached[static_cast<int32>(EDNDamagePart::GlassFront)]);
			TestFalse("o porta-malas nao", T.bPartDetached[static_cast<int32>(EDNDamagePart::Trunk)]);
		});

		It("cada zona solta as suas pecas; Roof e Under nao tem peca", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState Rear, Left, Right, Roof, Under;
			TestTrue("traseira", SameParts(FDNDamage::ApplyImpact(C, Rear, Hit(12.f, EDNDamageZone::Rear)).DetachedParts, { EDNDamagePart::Trunk, EDNDamagePart::BumperRear }));
			TestTrue("esquerda", SameParts(FDNDamage::ApplyImpact(C, Left, Hit(12.f, EDNDamageZone::Left)).DetachedParts, { EDNDamagePart::DoorLeft }));
			TestTrue("direita", SameParts(FDNDamage::ApplyImpact(C, Right, Hit(12.f, EDNDamageZone::Right)).DetachedParts, { EDNDamagePart::DoorRight }));
			TestEqual("teto", FDNDamage::ApplyImpact(C, Roof, Hit(12.f, EDNDamageZone::Roof)).DetachedParts.Num(), 0);
			TestEqual("fundo", FDNDamage::ApplyImpact(C, Under, Hit(12.f, EDNDamageZone::Under)).DetachedParts.Num(), 0);
		});

		It("o dano acumulado da zona >= 0,6 solta as pecas mesmo com impactos fracos", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState S;
			// 10 m/s: 0,2667 por impacto, abaixo dos 12 m/s: 2 impactos = 0,53 (nada); o 3o = 0,80 (solta)
			TestEqual("1o", FDNDamage::ApplyImpact(C, S, Hit(10.f, EDNDamageZone::Left, FVector(0, 0, 0))).DetachedParts.Num(), 0);
			TestEqual("2o", FDNDamage::ApplyImpact(C, S, Hit(10.f, EDNDamageZone::Left, FVector(1000, 0, 0))).DetachedParts.Num(), 0);
			const FDNDamageResult R = FDNDamage::ApplyImpact(C, S, Hit(10.f, EDNDamageZone::Left, FVector(2000, 0, 0)));
			TestTrue("3o solta a porta esquerda", SameParts(R.DetachedParts, { EDNDamagePart::DoorLeft }));
		});

		It("o dano da zona exatamente no limite conta (inclusivo)", [this]()
		{
			FDNDamageConfig C;
			C.PartDetachDeltaVMs = 1000.f;   // isola a regra do dano da zona
			C.PartDetachZoneDamage = 0.5f;
			FDNDamageState S;
			TestEqual("16,99: 0,4997 nao", FDNDamage::ApplyImpact(C, S, Hit(16.99f, EDNDamageZone::Right)).DetachedParts.Num(), 0);
			FDNDamageState T;
			TestTrue("17,0: 0,5 exato solta", SameParts(FDNDamage::ApplyImpact(C, T, Hit(17.f, EDNDamageZone::Right)).DetachedParts, { EDNDamagePart::DoorRight }));
		});

		It("peca ja solta nunca solta de novo", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState S;
			TestEqual("1o impacto", FDNDamage::ApplyImpact(C, S, Hit(20.f)).DetachedParts.Num(), 3);
			const FDNDamageResult R = FDNDamage::ApplyImpact(C, S, Hit(30.f, EDNDamageZone::Front, FVector(1000, 0, 0)));
			TestEqual("2o impacto: nada novo", R.DetachedParts.Num(), 0);
			TestTrue("seguem soltas", S.bPartDetached[static_cast<int32>(EDNDamagePart::Hood)]);
			// uma peca de OUTRA zona ainda solta depois
			TestTrue("a traseira ainda solta", SameParts(FDNDamage::ApplyImpact(C, S, Hit(20.f, EDNDamageZone::Rear, FVector(2000, 0, 0))).DetachedParts, { EDNDamagePart::Trunk, EDNDamagePart::BumperRear }));
		});

		It("impacto abaixo do limiar nao solta nada nem com peca pendente", [this]()
		{
			FDNDamageState S;
			TestEqual("nada", FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(1.5f)).DetachedParts.Num(), 0);
		});
	});

	Describe("Veiculo destruido", [this]()
	{
		It("frente em 1,0 destroi na hora, uma vez so", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState S;
			const FDNDamageResult R = FDNDamage::ApplyImpact(C, S, Hit(32.f, EDNDamageZone::Front));
			TestTrue("acabou de ser destruido", R.bJustDestroyed);
			TestTrue("estado", S.bDestroyed);
			const FDNDamageResult R2 = FDNDamage::ApplyImpact(C, S, Hit(32.f, EDNDamageZone::Front, FVector(1000, 0, 0)));
			TestFalse("nao devolve de novo", R2.bJustDestroyed);
			TestTrue("segue destruido", S.bDestroyed);
		});

		It("traseira em 1,0 tambem destroi", [this]()
		{
			FDNDamageState S;
			TestTrue("destruido", FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(32.f, EDNDamageZone::Rear)).bJustDestroyed);
		});

		It("em 0,999 da frente ainda nao", [this]()
		{
			FDNDamageState S;
			TestFalse("nao destruido", FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(31.97f, EDNDamageZone::Front)).bJustDestroyed);
			TestFalse("estado", S.bDestroyed);
		});

		It("so Left, Right, Roof ou Under em 1,0 NAO destroi (total 0,2)", [this]()
		{
			for (const EDNDamageZone Z : { EDNDamageZone::Left, EDNDamageZone::Right, EDNDamageZone::Roof, EDNDamageZone::Under })
			{
				FDNDamageState S;
				const FDNDamageResult R = FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(32.f, Z));
				TestFalse(FString::Printf(TEXT("zona %d"), static_cast<int32>(Z)), R.bJustDestroyed);
				TestFalse("estado", S.bDestroyed);
			}
		});

		It("total >= 0,5 destroi (inclusivo): cinco zonas a 0,5", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState S;
			const EDNDamageZone Zones[] = { EDNDamageZone::Front, EDNDamageZone::Rear, EDNDamageZone::Left, EDNDamageZone::Right, EDNDamageZone::Roof };
			for (int32 i = 0; i < 4; ++i)
			{
				TestFalse(FString::Printf(TEXT("impacto %d"), i + 1), FDNDamage::ApplyImpact(C, S, Hit(17.f, Zones[i], FVector(i * 1000.f, 0, 0))).bJustDestroyed);
			}
			TestEqual("total 0,4", FDNDamage::TotalDamage(S), 0.4f);
			const FDNDamageResult R = FDNDamage::ApplyImpact(C, S, Hit(17.f, Zones[4], FVector(4000, 0, 0)));
			TestEqual("total 0,5", FDNDamage::TotalDamage(S), 0.5f);
			TestTrue("o quinto destroi", R.bJustDestroyed);
		});

		It("a zona Under nao conta no total", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState S;
			for (const EDNDamageZone Z : { EDNDamageZone::Front, EDNDamageZone::Rear, EDNDamageZone::Left, EDNDamageZone::Right })
			{
				FDNDamage::ApplyImpact(C, S, Hit(17.f, Z, FVector(static_cast<float>(Z) * 1000.f, 0, 0)));
			}
			const FDNDamageResult R = FDNDamage::ApplyImpact(C, S, Hit(32.f, EDNDamageZone::Under, FVector(9000, 0, 0)));
			TestEqual("total segue em 0,4", FDNDamage::TotalDamage(S), 0.4f);
			TestFalse("nao destroi pela Under", R.bJustDestroyed);
		});

		It("o limite de destruicao da configuracao vale", [this]()
		{
			FDNDamageConfig C;
			C.DestroyedTotalDamage = 0.2f;
			FDNDamageState S;
			TestTrue("1,0 em uma zona = 0,2 do total", FDNDamage::ApplyImpact(C, S, Hit(32.f, EDNDamageZone::Left)).bJustDestroyed);
		});

		It("depois de destruido continua acumulando dano, amassados e pecas, sem devolver bJustDestroyed", [this]()
		{
			const FDNDamageConfig C;
			FDNDamageState S;
			FDNDamage::ApplyImpact(C, S, Hit(32.f, EDNDamageZone::Front));
			const FDNDamageResult R = FDNDamage::ApplyImpact(C, S, Hit(20.f, EDNDamageZone::Rear, FVector(5000, 0, 0)));
			TestFalse("nao devolve", R.bJustDestroyed);
			TestTrue("dano da traseira segue", R.AddedZoneDamage > 0.f);
			TestTrue("amassado novo", R.bDentAdded);
			TestEqual("dois amassados", S.Dents.Num(), 2);
			TestTrue("pecas da traseira soltam", SameParts(R.DetachedParts, { EDNDamagePart::Trunk, EDNDamagePart::BumperRear }));
		});
	});

	Describe("Pool de pecas soltas", [this]()
	{
		auto Check = [this](const TCHAR* What, int32 Max, int32 Active, int32 Requested, int32 ExpectedAccepted, int32 ExpectedRecycle)
		{
			const FDNPartPoolPlan P = FDNPartPool::Plan(Max, Active, Requested);
			TestEqual(FString::Printf(TEXT("%s: aceitas"), What), P.Accepted, ExpectedAccepted);
			TestEqual(FString::Printf(TEXT("%s: recicladas"), What), P.Recycle, ExpectedRecycle);
		};

		It("cabe: aceita tudo e nao recicla", [this, Check]()
		{
			Check(TEXT("vazio"), 24, 0, 5, 5, 0);
			Check(TEXT("cheio exato (20 + 4)"), 24, 20, 4, 4, 0);
			Check(TEXT("nada pedido"), 24, 24, 0, 0, 0);
		});

		It("passa do limite: recicla as mais antigas na conta Active + Requested - MaxActive", [this, Check]()
		{
			Check(TEXT("20 + 5"), 24, 20, 5, 5, 1);
			Check(TEXT("24 + 5"), 24, 24, 5, 5, 5);
			Check(TEXT("10 + 20"), 24, 10, 20, 20, 6);
		});

		It("nunca recicla mais do que Active; Requested maior que MaxActive aceita so MaxActive", [this, Check]()
		{
			Check(TEXT("10 + 30"), 24, 10, 30, 24, 10);
			Check(TEXT("0 + 30"), 24, 0, 30, 24, 0);
			Check(TEXT("24 + 100"), 24, 24, 100, 24, 24);
		});

		It("entradas negativas valem 0", [this, Check]()
		{
			Check(TEXT("Active negativo"), 24, -5, 3, 3, 0);
			Check(TEXT("Active negativo e Requested grande"), 24, -5, 30, 24, 0);
			Check(TEXT("Requested negativo"), 24, 5, -3, 0, 0);
			Check(TEXT("MaxActive negativo"), -1, 5, 5, 0, 5);
			Check(TEXT("MaxActive 0"), 0, 3, 2, 0, 3);
		});

		It("numeros enormes nao estouram", [this, Check]()
		{
			Check(TEXT("MAX_int32 nos dois"), 24, MAX_int32, MAX_int32, 24, MAX_int32);
			Check(TEXT("MAX_int32 de MaxActive"), MAX_int32, MAX_int32, MAX_int32, MAX_int32, MAX_int32);
		});
	});

	Describe("Configuracao invalida nao quebra (D7)", [this]()
	{
		It("ZoneDestroyDeltaVMs zero ou negativo: nao divide por zero, a zona satura", [this]()
		{
			for (const float V : { 0.f, -5.f })
			{
				FDNDamageConfig C;
				C.ZoneDestroyDeltaVMs = V;
				FDNDamageState S;
				const FDNDamageResult R = FDNDamage::ApplyImpact(C, S, Hit(3.f));
				TestTrue(FString::Printf(TEXT("%f: dano finito"), V), FMath::IsFinite(R.AddedZoneDamage));
				TestEqual(FString::Printf(TEXT("%f: satura"), V), S.ZoneDamage[0], 1.f);
			}
		});

		It("MaxDents zero ou negativo fica em pelo menos 1", [this]()
		{
			FDNDamageConfig C;
			C.MaxDents = 0;
			FDNDamageState S;
			for (int32 i = 0; i < 3; ++i) { FDNDamage::ApplyImpact(C, S, Hit(8.f, EDNDamageZone::Front, FVector(i * 1000.f, 0, 0))); }
			TestEqual("um so", S.Dents.Num(), 1);
		});

		It("DentMaxDepthCm negativo e DentDepthPerMs negativo nao geram profundidade negativa", [this]()
		{
			FDNDamageConfig C;
			C.DentMaxDepthCm = -4.f;
			C.DentDepthPerMs = -2.f;
			FDNDamageState S;
			const FDNDamageResult R = FDNDamage::ApplyImpact(C, S, Hit(10.f));
			TestTrue("profundidade >= 0", R.Dent.DepthCm >= 0.f);
		});
	});

	Describe("Estado", [this]()
	{
		It("Reset volta tudo ao inicio", [this]()
		{
			FDNDamageState S;
			FDNDamage::ApplyImpact(FDNDamageConfig(), S, Hit(40.f));
			TestFalse("tinha dano", Untouched(S));
			S.Reset();
			TestTrue("zerado", Untouched(S));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
