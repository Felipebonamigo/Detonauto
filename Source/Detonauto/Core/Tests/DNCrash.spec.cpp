// Testes da pontuação do modo Crash (docs/regras-de-jogo.md, seção 2, e as decisões C1 a C5).
// Funções puras: nada de mundo, Actor ou física.
//
// Os comentários "Cn" apontam para a tabela "Decisões de implementação" do contrato.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNCrash.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Destrói um veículo e devolve os pontos somados. */
	int32 Destroy(const FDNCrashConfig& C, FDNCrashState& S, int32 Value, float SecondsSincePrevious = 0.f)
	{
		FDNCrash::Tick(C, S, SecondsSincePrevious, 10.f); // anda o relógio, com veículos em movimento
		return FDNCrash::RegisterDestruction(C, S, Value);
	}
}

BEGIN_DEFINE_SPEC(FDNCrashPointsSpec, "Detonauto.Crash.Pontos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNCrashConfig Config;
END_DEFINE_SPEC(FDNCrashPointsSpec)

void FDNCrashPointsSpec::Define()
{
	Describe("Conta dos pontos (C1, C2)", [this]()
	{
		It("valor vezes multiplicador, sem cadeia", [this]()
		{
			TestEqual("1000 x 1", FDNCrash::ComputePoints(Config, 1000, 1, 0), 1000);
			TestEqual("1000 x 3", FDNCrash::ComputePoints(Config, 1000, 3, 0), 3000);
			TestEqual("1000 x 8", FDNCrash::ComputePoints(Config, 1000, 8, 0), 8000);
		});

		It("bonus da cadeia primeiro (+10% por elo), multiplicador depois", [this]()
		{
			TestEqual("1 elo: 1100", FDNCrash::ComputePoints(Config, 1000, 1, 1), 1100);
			TestEqual("3 elos x2: 1300 x 2", FDNCrash::ComputePoints(Config, 1000, 2, 3), 2600);
		});

		It("o bonus para em +100% (10 elos) e nao passa disso", [this]()
		{
			TestEqual("10 elos: dobra", FDNCrash::ComputePoints(Config, 1000, 1, 10), 2000);
			TestEqual("11 elos: continua dobrando", FDNCrash::ComputePoints(Config, 1000, 1, 11), 2000);
			TestEqual("500 elos: continua dobrando", FDNCrash::ComputePoints(Config, 1000, 1, 500), 2000);
		});

		// C2: inteiro de 64 bits, truncado para baixo, e só depois multiplicado.
		It("trunca o bonus para baixo antes de multiplicar", [this]()
		{
			TestEqual("15 + 10% = 16,5 vira 16", FDNCrash::ComputePoints(Config, 15, 1, 1), 16);
			TestEqual("16 x 3 = 48 (nao 49,5)", FDNCrash::ComputePoints(Config, 15, 3, 1), 48);
			TestEqual("1 + 10% = 1,1 vira 1", FDNCrash::ComputePoints(Config, 1, 1, 1), 1);
		});

		It("satura em MAX_int32 sem dar a volta", [this]()
		{
			TestEqual("valor gigante x 8", FDNCrash::ComputePoints(Config, MAX_int32, 8, 10), MAX_int32);
			TestEqual("justo no teto", FDNCrash::ComputePoints(Config, MAX_int32, 1, 0), MAX_int32);
		});

		It("valor zero ou negativo vale 0 pontos", [this]()
		{
			TestEqual("zero", FDNCrash::ComputePoints(Config, 0, 4, 2), 0);
			TestEqual("negativo", FDNCrash::ComputePoints(Config, -50, 4, 2), 0);
			TestEqual("menor int32", FDNCrash::ComputePoints(Config, MIN_int32, 4, 2), 0);
		});

		It("multiplicador fora de 1..8 e limitado", [this]()
		{
			TestEqual("zero vira 1", FDNCrash::ComputePoints(Config, 1000, 0, 0), 1000);
			TestEqual("negativo vira 1", FDNCrash::ComputePoints(Config, 1000, -3, 0), 1000);
			TestEqual("9 vira 8", FDNCrash::ComputePoints(Config, 1000, 9, 0), 8000);
		});
	});

	Describe("Pontuacao acumulada", [this]()
	{
		It("soma os pontos de cada veiculo no total", [this]()
		{
			FDNCrashState S;
			Destroy(Config, S, 1000);
			Destroy(Config, S, 500, 5.f);
			TestEqual("total", S.Score.Total, 1500);
			TestEqual("veiculos destruidos", S.DestroyedCount, 2);
		});

		It("a funcao devolve os pontos somados", [this]()
		{
			FDNCrashState S;
			TestEqual("primeiro", Destroy(Config, S, 1000), 1000);
		});

		It("o total satura em MAX_int32", [this]()
		{
			FDNCrashState S;
			Destroy(Config, S, MAX_int32 - 10);
			Destroy(Config, S, 1000, 5.f);
			TestEqual("total", S.Score.Total, MAX_int32);
		});

		// C5: valor <= 0 é ignorado: não vira elo da cadeia e não conta como primeiro impacto.
		It("valor zero ou negativo e ignorado por inteiro", [this]()
		{
			FDNCrashState S;
			TestEqual("zero", Destroy(Config, S, 0), 0);
			TestEqual("negativo", Destroy(Config, S, -9), 0);
			TestEqual("total", S.Score.Total, 0);
			TestEqual("nao contou veiculo", S.DestroyedCount, 0);
			TestFalse("nao e primeiro impacto", S.bFirstImpactHappened);
		});
	});
}

BEGIN_DEFINE_SPEC(FDNCrashChainSpec, "Detonauto.Crash.Cadeia",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNCrashConfig Config;
END_DEFINE_SPEC(FDNCrashChainSpec)

void FDNCrashChainSpec::Define()
{
	Describe("Multiplicador", [this]()
	{
		It("comeca em 1 e sobe 1 por multiplicador coletado", [this]()
		{
			FDNCrashState S;
			TestEqual("inicio", S.Multiplier, 1);
			FDNCrash::CollectMultiplier(Config, S);
			FDNCrash::CollectMultiplier(Config, S);
			TestEqual("dois coletados", S.Multiplier, 3);
		});

		It("para em 8", [this]()
		{
			FDNCrashState S;
			for (int32 i = 0; i < 20; ++i) { FDNCrash::CollectMultiplier(Config, S); }
			TestEqual("teto", S.Multiplier, 8);
		});

		It("vale o multiplicador do instante da destruicao", [this]()
		{
			FDNCrashState S;
			FDNCrash::CollectMultiplier(Config, S);        // x2
			const int32 First = Destroy(Config, S, 1000);
			FDNCrash::CollectMultiplier(Config, S);        // x3 depois
			TestEqual("o primeiro ficou em x2", First, 2000);
			TestEqual("total nao mudou com a coleta", S.Score.Total, 2000);
		});
	});

	Describe("Cadeia de destruicoes", [this]()
	{
		It("o primeiro veiculo nao tem bonus", [this]()
		{
			FDNCrashState S;
			TestEqual("primeiro", Destroy(Config, S, 1000), 1000);
		});

		It("o segundo dentro de 1,5 s soma 10%", [this]()
		{
			FDNCrashState S;
			Destroy(Config, S, 1000);
			TestEqual("segundo", Destroy(Config, S, 1000, 1.0f), 1100);
		});

		It("acumula 10% por elo ate +100% (o 11o ja esta no teto)", [this]()
		{
			FDNCrashState S;
			int32 Last = 0;
			for (int32 i = 0; i < 12; ++i) { Last = Destroy(Config, S, 1000, 0.5f); }
			TestEqual("12o veiculo", Last, 2000);
		});

		It("o 11o veiculo da cadeia chega a +100%", [this]()
		{
			FDNCrashState S;
			int32 Eleventh = 0;
			for (int32 i = 0; i < 11; ++i) { Eleventh = Destroy(Config, S, 1000, 0.5f); }
			TestEqual("11o veiculo", Eleventh, 2000);
		});

		It("exatamente 1,5 s depois ainda continua a cadeia", [this]()
		{
			FDNCrashState S;
			Destroy(Config, S, 1000);
			TestEqual("1,5 s", Destroy(Config, S, 1000, 1.5f), 1100);
		});

		It("mais de 1,5 s depois reinicia a cadeia", [this]()
		{
			FDNCrashState S;
			Destroy(Config, S, 1000);
			TestEqual("1,51 s", Destroy(Config, S, 1000, 1.51f), 1000);
			TestEqual("e a cadeia recomeca daqui", Destroy(Config, S, 1000, 0.5f), 1100);
		});

		It("o bonus usa o multiplicador do instante e a conta em inteiros", [this]()
		{
			FDNCrashState S;
			FDNCrash::CollectMultiplier(Config, S); // x2
			Destroy(Config, S, 15);
			TestEqual("15 com +10% truncado a 16, x2", Destroy(Config, S, 15, 1.0f), 32);
		});
	});
}

BEGIN_DEFINE_SPEC(FDNCrashRoundSpec, "Detonauto.Crash.Rodada",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNCrashConfig Config;
END_DEFINE_SPEC(FDNCrashRoundSpec)

void FDNCrashRoundSpec::Define()
{
	Describe("Fim da rodada (C3, C4)", [this]()
	{
		It("nao termina antes do primeiro impacto, por mais tempo que passe", [this]()
		{
			FDNCrashState S;
			FDNCrash::Tick(Config, S, 100.f, 0.f);
			FDNCrash::Tick(Config, S, 100.f, 0.f);
			TestFalse("terminou", S.bEnded);
		});

		It("termina com 3,0 s seguidos sem veiculo acima de 2 m/s", [this]()
		{
			FDNCrashState S;
			FDNCrash::RegisterImpact(Config, S);
			FDNCrash::Tick(Config, S, 2.75f, 1.f);
			TestFalse("2,75 s", S.bEnded);
			FDNCrash::Tick(Config, S, 0.25f, 1.f);
			TestTrue("3,0 s", S.bEnded);
		});

		It("exatamente 2,0 m/s conta como parado; acima de 2,0 reinicia a contagem", [this]()
		{
			FDNCrashState S;
			FDNCrash::RegisterImpact(Config, S);
			FDNCrash::Tick(Config, S, 2.0f, 2.0f);
			FDNCrash::Tick(Config, S, 0.5f, 2.01f); // algum veiculo voltou a andar
			FDNCrash::Tick(Config, S, 2.75f, 2.0f);
			TestFalse("contagem reiniciada", S.bEnded);
			FDNCrash::Tick(Config, S, 0.25f, 2.0f);
			TestTrue("3,0 s seguidos depois do reinicio", S.bEnded);
		});

		It("termina aos 30,0 s depois do primeiro impacto mesmo com veiculos em movimento", [this]()
		{
			FDNCrashState S;
			FDNCrash::RegisterImpact(Config, S);
			FDNCrash::Tick(Config, S, 29.75f, 50.f);
			TestFalse("29,75 s", S.bEnded);
			FDNCrash::Tick(Config, S, 0.25f, 50.f);
			TestTrue("30,0 s", S.bEnded);
		});

		It("o que vier primeiro encerra: 3 s parado antes de 30 s", [this]()
		{
			FDNCrashState S;
			FDNCrash::RegisterImpact(Config, S);
			FDNCrash::Tick(Config, S, 5.f, 50.f);
			FDNCrash::Tick(Config, S, 3.f, 0.f);
			TestTrue("terminou aos 8 s", S.bEnded);
		});

		It("a destruicao de um veiculo conta como primeiro impacto", [this]()
		{
			FDNCrashState S;
			FDNCrash::RegisterDestruction(Config, S, 1000);
			TestTrue("impacto registrado", S.bFirstImpactHappened);
			FDNCrash::Tick(Config, S, 30.f, 50.f);
			TestTrue("terminou aos 30 s", S.bEnded);
		});

		It("o relogio do primeiro impacto nao reinicia com os impactos seguintes", [this]()
		{
			FDNCrashState S;
			FDNCrash::RegisterImpact(Config, S);
			FDNCrash::Tick(Config, S, 20.f, 50.f);
			FDNCrash::RegisterImpact(Config, S);
			FDNCrash::Tick(Config, S, 10.f, 50.f);
			TestTrue("30 s desde o PRIMEIRO", S.bEnded);
		});

		It("depois do fim, pontos e multiplicadores sao ignorados", [this]()
		{
			FDNCrashState S;
			Destroy(Config, S, 1000);
			FDNCrash::Tick(Config, S, 30.f, 50.f);
			TestTrue("terminou", S.bEnded);
			TestEqual("destruicao tardia", FDNCrash::RegisterDestruction(Config, S, 5000), 0);
			FDNCrash::CollectMultiplier(Config, S);
			TestEqual("total", S.Score.Total, 1000);
			TestEqual("multiplicador", S.Multiplier, 1);
		});

		It("tempo negativo ou NaN nao anda o relogio", [this]()
		{
			FDNCrashState S;
			FDNCrash::RegisterImpact(Config, S);
			FDNCrash::Tick(Config, S, -5.f, 0.f);
			FDNCrash::Tick(Config, S, std::numeric_limits<float>::quiet_NaN(), 0.f);
			TestFalse("nao terminou", S.bEnded);
			TestEqual("relogio parado", S.SecondsSinceFirstImpact, 0.f);
		});

		It("velocidade NaN conta como em movimento (nao encerra por engano)", [this]()
		{
			FDNCrashState S;
			FDNCrash::RegisterImpact(Config, S);
			FDNCrash::Tick(Config, S, 10.f, std::numeric_limits<float>::quiet_NaN());
			TestFalse("terminou", S.bEnded);
		});

		It("Reset volta ao estado inicial", [this]()
		{
			FDNCrashState S;
			Destroy(Config, S, 1000);
			FDNCrash::CollectMultiplier(Config, S);
			FDNCrash::Tick(Config, S, 30.f, 50.f);
			S.Reset();
			TestEqual("total", S.Score.Total, 0);
			TestEqual("multiplicador", S.Multiplier, 1);
			TestFalse("fim", S.bEnded);
			TestFalse("impacto", S.bFirstImpactHappened);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
