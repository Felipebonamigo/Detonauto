// Testes dos efeitos de batida (lote 19, docs/regras-efeitos.md): quando cada efeito dispara e quanto, com os números do contrato e
// as decisões E1 a E8 (Core/DNEffects.h). Limiares inclusivos; entrada não finita não dispara.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNEffects.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FDNEffectSpec Spec(EDNEffect Effect, float TriggerMin, float TriggerFull, float MinSpeedKmh, int32 CountMin, int32 CountMax, float PerUnit, float Extra)
	{
		FDNEffectSpec S;
		S.Effect = Effect;
		S.TriggerMin = TriggerMin;
		S.TriggerFull = TriggerFull;
		S.MinSpeedKmh = MinSpeedKmh;
		S.CountMin = CountMin;
		S.CountMax = CountMax;
		S.CountPerUnit = PerUnit;
		S.Extra = Extra;
		return S;
	}

	// as linhas do contrato (e das decisões E2 a E7)
	FDNEffectSpec Spark() { return Spec(EDNEffect::SparkImpact, 4.f, 0.f, 0.f, 10, 120, 6.f, 0.f); }
	FDNEffectSpec Scrape() { return Spec(EDNEffect::SparkScrape, 3.f, 0.f, 0.f, 0, 300, 15.f, 0.f); }
	FDNEffectSpec Shards() { return Spec(EDNEffect::GlassShards, 0.f, 0.f, 0.f, 60, 150, 250.f, 20.f); }
	FDNEffectSpec Plastic() { return Spec(EDNEffect::PlasticChips, 12.f, 30.f, 0.f, 10, 30, 0.f, 0.f); }
	FDNEffectSpec Tire() { return Spec(EDNEffect::TireSmoke, 8.f, 25.f, 20.f, 0, 0, 0.f, 0.6f); }
	FDNEffectSpec Smoke() { return Spec(EDNEffect::EngineSmoke, 0.6f, 0.8f, 0.f, 0, 0, 0.f, 0.f); }
	FDNEffectSpec Fire() { return Spec(EDNEffect::EngineFire, 0.9f, 0.f, 0.f, 0, 0, 0.f, 0.7f); }
}

BEGIN_DEFINE_SPEC(FDNEffectsSpec, "Detonauto.Efeitos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	const float Inf = std::numeric_limits<float>::infinity();
END_DEFINE_SPEC(FDNEffectsSpec)

void FDNEffectsSpec::Define()
{
	Describe("Nomes", [this]()
	{
		It("cada efeito volta ao enum pelo proprio nome, sem diferenciar a caixa; texto desconhecido nao mexe em Out", [this]()
		{
			for (int32 i = 0; i <= static_cast<int32>(EDNEffect::PropLeaves); ++i)
			{
				const EDNEffect E = static_cast<EDNEffect>(i);
				EDNEffect Back = E == EDNEffect::SparkImpact ? EDNEffect::PropLeaves : EDNEffect::SparkImpact;
				const FString Name = FDNEffects::EffectName(E);
				TestFalse(FString::Printf(TEXT("efeito %d tem nome"), i), Name.IsEmpty());
				TestTrue(FString::Printf(TEXT("efeito %d (%s)"), i, *Name), FDNEffects::ParseEffect(Name.ToLower(), Back) && Back == E);
			}
			TestEqual("nome da faisca", FString(FDNEffects::EffectName(EDNEffect::SparkImpact)), FString(TEXT("SparkImpact")));
			EDNEffect Untouched = EDNEffect::TireSmoke;
			TestFalse("desconhecido", FDNEffects::ParseEffect(TEXT("Explosao"), Untouched));
			TestTrue("nao mexe em Out", Untouched == EDNEffect::TireSmoke);
		});
	});

	Describe("Faisca de impacto (E2)", [this]()
	{
		It("dispara com Dv >= 4 (inclusivo); 3,99 nao", [this]()
		{
			TestEqual("3,99", FDNEffects::ImpactSparks(Spark(), 3.99f), 0);
			TestEqual("4: 6 x 4 = 24", FDNEffects::ImpactSparks(Spark(), 4.f), 24);
		});

		It("6 por m/s, entre 10 e 120", [this]()
		{
			TestEqual("10 m/s: 60", FDNEffects::ImpactSparks(Spark(), 10.f), 60);
			TestEqual("20 m/s: 120 (teto)", FDNEffects::ImpactSparks(Spark(), 20.f), 120);
			TestEqual("35 m/s: 120", FDNEffects::ImpactSparks(Spark(), 35.f), 120);
			FDNEffectSpec Low = Spark();
			Low.TriggerMin = 1.f;
			TestEqual("1 m/s com limiar 1: 6 vira o piso 10", FDNEffects::ImpactSparks(Low, 1.f), 10);
		});

		It("Dv nao finito: nada", [this]()
		{
			TestEqual("NaN", FDNEffects::ImpactSparks(Spark(), NaNf), 0);
			TestEqual("infinito", FDNEffects::ImpactSparks(Spark(), Inf), 0);
		});
	});

	Describe("Faisca de raspao (E3)", [this]()
	{
		It("dispara com velocidade >= 3 m/s; 2,99 nao; 15 por m/s ate 300 por segundo", [this]()
		{
			TestEqual("2,99", FDNEffects::ScrapeSparkRate(Scrape(), 2.99f), 0.f);
			TestEqual("3: 45/s", FDNEffects::ScrapeSparkRate(Scrape(), 3.f), 45.f);
			TestEqual("16 m/s: 240/s", FDNEffects::ScrapeSparkRate(Scrape(), 16.f), 240.f);
			TestEqual("40: teto 300/s", FDNEffects::ScrapeSparkRate(Scrape(), 40.f), 300.f);
			TestEqual("NaN", FDNEffects::ScrapeSparkRate(Scrape(), NaNf), 0.f);
		});
	});

	Describe("Cacos de vidro (E4)", [this]()
	{
		It("250 por m2, entre 60 e 150; area invalida: o minimo", [this]()
		{
			TestEqual("lente 0,05 m2: piso 60", FDNEffects::GlassShards(Shards(), 0.05f), 60);
			TestEqual("lateral 0,35 m2: 88", FDNEffects::GlassShards(Shards(), 0.35f), 88);
			TestEqual("traseiro 0,5 m2: 125", FDNEffects::GlassShards(Shards(), 0.5f), 125);
			TestEqual("2 m2: teto 150", FDNEffects::GlassShards(Shards(), 2.f), 150);
			TestEqual("area 0", FDNEffects::GlassShards(Shards(), 0.f), 60);
			TestEqual("area NaN", FDNEffects::GlassShards(Shards(), NaNf), 60);
		});
	});

	Describe("Lascas de plastico (E5)", [this]()
	{
		It("dispara com Dv >= 12; de 10 a 30 linear ate Dv 30", [this]()
		{
			TestEqual("11,99", FDNEffects::PlasticChips(Plastic(), 11.99f), 0);
			TestEqual("12: 10", FDNEffects::PlasticChips(Plastic(), 12.f), 10);
			TestEqual("21: 20", FDNEffects::PlasticChips(Plastic(), 21.f), 20);
			TestEqual("30: 30", FDNEffects::PlasticChips(Plastic(), 30.f), 30);
			TestEqual("50: 30", FDNEffects::PlasticChips(Plastic(), 50.f), 30);
			FDNEffectSpec Flat = Plastic();
			Flat.TriggerFull = Flat.TriggerMin;
			TestEqual("cheio igual ao minimo: o maximo direto", FDNEffects::PlasticChips(Flat, 12.f), 30);
			TestEqual("NaN", FDNEffects::PlasticChips(Plastic(), NaNf), 0);
		});
	});

	Describe("Fumaca de pneu (E6)", [this]()
	{
		It("de 8 graus em diante, taxa = deslizamento / 25 ate 1; 7,99 nao", [this]()
		{
			TestEqual("7,99", FDNEffects::TireSmokeRate(Tire(), 7.99f, 80.f, false), 0.f);
			TestEqual("8: 0,32", FDNEffects::TireSmokeRate(Tire(), 8.f, 80.f, false), 0.32f, 1e-4f);
			TestEqual("-12,5 (o outro lado): 0,5", FDNEffects::TireSmokeRate(Tire(), -12.5f, 80.f, false), 0.5f, 1e-4f);
			TestEqual("40: 1", FDNEffects::TireSmokeRate(Tire(), 40.f, 80.f, false), 1.f);
		});

		It("abaixo de 20 km/h nada, nem patinando; patinando garante 0,6", [this]()
		{
			TestEqual("19,9 km/h", FDNEffects::TireSmokeRate(Tire(), 30.f, 19.9f, true), 0.f);
			TestEqual("20 km/h", FDNEffects::TireSmokeRate(Tire(), 30.f, 20.f, false), 1.f);
			TestEqual("patinando reto", FDNEffects::TireSmokeRate(Tire(), 0.f, 40.f, true), 0.6f);
			TestEqual("patinando e deslizando muito: o maior", FDNEffects::TireSmokeRate(Tire(), 25.f, 40.f, true), 1.f);
			TestEqual("NaN no deslizamento", FDNEffects::TireSmokeRate(Tire(), NaNf, 40.f, false), 0.f);
			TestEqual("NaN na velocidade", FDNEffects::TireSmokeRate(Tire(), 30.f, NaNf, true), 0.f);
		});
	});

	Describe("Motor (E7)", [this]()
	{
		It("clara de 0,6 (inclusivo), escura de 0,8, fogo de 0,9", [this]()
		{
			const FDNEngineSmoke None = FDNEffects::EngineSmoke(Smoke(), Fire(), 0.5999f, false);
			TestTrue("0,5999: nada", !None.bSmoke && !None.bDark && !None.bFire && None.Intensity == 0.f);
			const FDNEngineSmoke Light = FDNEffects::EngineSmoke(Smoke(), Fire(), 0.6f, false);
			TestTrue("0,6: clara", Light.bSmoke && !Light.bDark && !Light.bFire);
			TestEqual("0,6: intensidade 0", Light.Intensity, 0.f);
			TestEqual("0,7: meio caminho", FDNEffects::EngineSmoke(Smoke(), Fire(), 0.7f, false).Intensity, 0.5f, 1e-4f);
			const FDNEngineSmoke Dark = FDNEffects::EngineSmoke(Smoke(), Fire(), 0.8f, false);
			TestTrue("0,8: escura, intensidade 1", Dark.bSmoke && Dark.bDark && !Dark.bFire && Dark.Intensity == 1.f);
			TestFalse("0,8999: sem fogo", FDNEffects::EngineSmoke(Smoke(), Fire(), 0.8999f, false).bFire);
			const FDNEngineSmoke Burning = FDNEffects::EngineSmoke(Smoke(), Fire(), 0.9f, false);
			TestTrue("0,9: fogo com a fumaca escura", Burning.bFire && Burning.bDark);
		});

		It("carro destruido: fogo com a frente em 0,7 (inclusivo)", [this]()
		{
			TestFalse("destruido, 0,6999", FDNEffects::EngineSmoke(Smoke(), Fire(), 0.6999f, true).bFire);
			TestTrue("destruido, 0,7", FDNEffects::EngineSmoke(Smoke(), Fire(), 0.7f, true).bFire);
			TestFalse("inteiro, 0,7", FDNEffects::EngineSmoke(Smoke(), Fire(), 0.7f, false).bFire);
		});

		It("a intensidade so sobe com o dano", [this]()
		{
			float Last = -1.f;
			bool bMonotonic = true;
			for (int32 i = 0; i <= 100; ++i)
			{
				const float Now = FDNEffects::EngineSmoke(Smoke(), Fire(), i / 100.f, false).Intensity;
				bMonotonic &= Now >= Last;
				Last = Now;
			}
			TestTrue("nao desce", bMonotonic);
			const FDNEngineSmoke Bad = FDNEffects::EngineSmoke(Smoke(), Fire(), NaNf, true);
			TestTrue("NaN: nada", !Bad.bSmoke && !Bad.bFire && Bad.Intensity == 0.f);
		});
	});

	Describe("Orcamento (E8)", [this]()
	{
		It("entrega o que cabe ate o teto, nunca negativo", [this]()
		{
			TestEqual("cabe tudo", FDNEffects::FitBudget(120, 1000, 2000), 120);
			TestEqual("cabe parte", FDNEffects::FitBudget(120, 1950, 2000), 50);
			TestEqual("cheio", FDNEffects::FitBudget(120, 2000, 2000), 0);
			TestEqual("acima do teto", FDNEffects::FitBudget(120, 2100, 2000), 0);
			TestEqual("pedido negativo", FDNEffects::FitBudget(-5, 0, 2000), 0);
		});
	});

	Describe("Pool por efeito (E9)", [this]()
	{
		It("vagas livres primeiro; cheio, toma a que termina primeiro", [this]()
		{
			FDNEffectPool Pool;
			bool bRecycled = true;
			TestEqual("1a: vaga 0", Pool.Acquire(2, 0.0, 1.0, bRecycled), 0);
			TestFalse("1a: livre", bRecycled);
			TestEqual("2a (termina em 0,8): vaga 1", Pool.Acquire(2, 0.3, 0.5, bRecycled), 1);
			TestFalse("2a: livre", bRecycled);
			TestEqual("duas ativas", Pool.ActiveCount(0.4), 2);
			TestEqual("3a, cheio: a vaga 1 termina primeiro (0,8 < 1,0)", Pool.Acquire(2, 0.4, 1.0, bRecycled), 1);
			TestTrue("3a: reaproveitou", bRecycled);
			TestEqual("em 1,2 so a vaga 1 segue (ate 1,4)", Pool.ActiveCount(1.2), 1);
			TestEqual("4a em 1,2: a vaga 0 ja terminou", Pool.Acquire(2, 1.2, 1.0, bRecycled), 0);
			TestFalse("4a: livre", bRecycled);
			TestEqual("5a em 1,4, o fim exato da vaga 1: livre (inclusivo)", Pool.Acquire(2, 1.4, 1.0, bRecycled), 1);
			TestFalse("5a: nao reaproveitou", bRecycled);
		});

		It("teto menor que 1 vale 1; vida nao finita ou negativa vale 0; Reset libera tudo", [this]()
		{
			FDNEffectPool Pool;
			bool bRecycled = false;
			TestEqual("teto 0: vaga 0", Pool.Acquire(0, 0.0, 1.0, bRecycled), 0);
			TestEqual("teto 0 de novo: a mesma vaga", Pool.Acquire(0, 0.1, 1.0, bRecycled), 0);
			TestTrue("reaproveitou", bRecycled);
			FDNEffectPool Other;
			Other.Acquire(4, 0.0, std::numeric_limits<double>::quiet_NaN(), bRecycled);
			TestEqual("vida NaN: termina na hora", Other.ActiveCount(0.0), 0);
			Pool.Reset();
			TestEqual("Reset", Pool.ActiveCount(0.1), 0);
		});
	});

	Describe("Particulas vivas (E10)", [this]()
	{
		It("concede ate o teto e libera no fim da vida", [this]()
		{
			FDNParticleBudget Budget;
			TestEqual("120 de 2000", Budget.Request(120, 2000, 0.0, 0.8), 120);
			TestEqual("1950 com 120 vivas: 1880", Budget.Request(1950, 2000, 0.1, 1.5), 1880);
			TestEqual("cheio", Budget.Request(50, 2000, 0.2, 1.0), 0);
			TestEqual("vivas em 0,5", Budget.Active(0.5), 2000);
			TestEqual("em 1,0 as primeiras 120 acabaram", Budget.Active(1.0), 1880);
			TestEqual("cabe de novo", Budget.Request(50, 2000, 1.0, 1.0), 50);
			TestEqual("pedido negativo", Budget.Request(-3, 2000, 1.0, 1.0), 0);
			Budget.Reset();
			TestEqual("Reset", Budget.Active(1.0), 0);
		});
	});

	Describe("Fumaca continua (E11)", [this]()
	{
		It("abaixo do teto 1; acima, cai na proporcao; pedido invalido 1", [this]()
		{
			TestEqual("200 de 400", FDNEffects::SmokeScale(200.f, 400), 1.f);
			TestEqual("400 de 400", FDNEffects::SmokeScale(400.f, 400), 1.f);
			TestEqual("800 de 400", FDNEffects::SmokeScale(800.f, 400), 0.5f);
			TestEqual("nada pedido", FDNEffects::SmokeScale(0.f, 400), 1.f);
			TestEqual("NaN", FDNEffects::SmokeScale(NaNf, 400), 1.f);
			TestEqual("teto 0: nada", FDNEffects::SmokeScale(100.f, 0), 0.f);
		});
	});
}

#endif
