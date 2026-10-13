// Testes do tráfego simplificado (docs/regras-trafego.md). Funções puras: sem UObject, sem mundo.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNTraffic.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNTrafficSpawnSpec, "Detonauto.Trafego.Spawns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNTrafficSpawnSpec)

void FDNTrafficSpawnSpec::Define()
{
	Describe("PlanSpawns = clamp(Alvo - Ativos, 0, MaxSpawnPorQuadro)", [this]()
	{
		It("cria ate o maximo por quadro quando falta muito", [this]()
		{
			TestEqual("faltam 6, maximo 2", FDNTraffic::PlanSpawns(10, 4, 2), 2);
		});

		It("cria so o que falta quando falta menos que o maximo", [this]()
		{
			TestEqual("falta 1, maximo 2", FDNTraffic::PlanSpawns(5, 4, 2), 1);
			TestEqual("falta 2, maximo 2", FDNTraffic::PlanSpawns(6, 4, 2), 2);
			TestEqual("falta 3, maximo 2", FDNTraffic::PlanSpawns(7, 4, 2), 2);
		});

		It("nao cria nada quando ja ha o alvo ou mais", [this]()
		{
			TestEqual("igual", FDNTraffic::PlanSpawns(4, 4, 2), 0);
			TestEqual("a mais", FDNTraffic::PlanSpawns(4, 9, 2), 0);
			TestEqual("zero e zero", FDNTraffic::PlanSpawns(0, 0, 2), 0);
		});

		It("maximo por quadro zero ou negativo nao cria nada", [this]()
		{
			TestEqual("maximo 0", FDNTraffic::PlanSpawns(10, 0, 0), 0);
			TestEqual("maximo negativo", FDNTraffic::PlanSpawns(10, 0, -3), 0);
		});

		It("alvo negativo nao cria nada; ativos negativos valem zero", [this]()
		{
			TestEqual("alvo -5", FDNTraffic::PlanSpawns(-5, 0, 2), 0);
			TestEqual("ativos -3 contam como 0", FDNTraffic::PlanSpawns(1, -3, 2), 1);
			TestEqual("ativos -3, alvo 10", FDNTraffic::PlanSpawns(10, -3, 2), 2);
		});

		It("maximo grande: cria tudo o que falta", [this]()
		{
			TestEqual("falta 10, maximo 1000", FDNTraffic::PlanSpawns(10, 0, 1000), 10);
		});

		It("nao estoura o int32 nos extremos", [this]()
		{
			TestEqual("MAX menos MIN", FDNTraffic::PlanSpawns(MAX_int32, MIN_int32, 2), 2);
			TestEqual("MIN menos MAX", FDNTraffic::PlanSpawns(MIN_int32, MAX_int32, 2), 0);
			TestEqual("MAX com maximo MAX", FDNTraffic::PlanSpawns(MAX_int32, 0, MAX_int32), MAX_int32);
			TestEqual("MAX menos 1 ativo", FDNTraffic::PlanSpawns(MAX_int32, 1, MAX_int32), MAX_int32 - 1);
		});
	});
}


namespace
{
	/** Veículo Rail "neutro": nada acorda nem despawna. */
	FDNTrafficVehicleInput Neutral()
	{
		return FDNTrafficVehicleInput();
	}

	const TCHAR* Name(EDNTrafficLevel L)
	{
		switch (L)
		{
		case EDNTrafficLevel::Rail: return TEXT("Rail");
		case EDNTrafficLevel::Physics: return TEXT("Physics");
		default: return TEXT("Despawn");
		}
	}

	void ExpectLevel(FAutomationTestBase& T, const TCHAR* What, EDNTrafficLevel Actual, EDNTrafficLevel Expected)
	{
		T.TestTrue(FString::Printf(TEXT("%s: esperado %s, veio %s"), What, Name(Expected), Name(Actual)), Actual == Expected);
	}

	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Inf = std::numeric_limits<float>::infinity();
}

BEGIN_DEFINE_SPEC(FDNTrafficLevelSpec, "Detonauto.Trafego.Niveis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNTrafficSettings Settings;
END_DEFINE_SPEC(FDNTrafficLevelSpec)

void FDNTrafficLevelSpec::Define()
{
	Describe("Rail fica Rail sem motivo", [this]()
	{
		It("a entrada neutra mantem o nivel Rail", [this]()
		{
			ExpectLevel(*this, TEXT("neutro"), FDNTraffic::NextLevel(Settings, Neutral()), EDNTrafficLevel::Rail);
		});
	});

	Describe("Rail para Physics: impacto", [this]()
	{
		It("qualquer impacto acima de zero acorda, venha de onde vier", [this]()
		{
			FDNTrafficVehicleInput In = Neutral();
			In.ImpactForce = 0.001f;
			ExpectLevel(*this, TEXT("impacto minusculo"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
			In.ImpactForce = 50000.f;
			ExpectLevel(*this, TEXT("impacto grande"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
		});

		It("impacto zero, negativo ou NaN nao acorda", [this]()
		{
			FDNTrafficVehicleInput In = Neutral();
			In.ImpactForce = 0.f;
			ExpectLevel(*this, TEXT("zero"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
			In.ImpactForce = -5.f;
			ExpectLevel(*this, TEXT("negativo"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
			In.ImpactForce = NaN;
			ExpectLevel(*this, TEXT("NaN"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
		});
	});

	Describe("Rail para Physics: veiculo Physics perto (6,0 m, inclusive)", [this]()
	{
		It("6,0 m exatos acordam; 6,0001 m nao; mais perto acorda", [this]()
		{
			FDNTrafficVehicleInput In = Neutral();
			In.NearestPhysicsDistanceM = 6.0f;
			ExpectLevel(*this, TEXT("6,0 m"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
			In.NearestPhysicsDistanceM = 6.001f;
			ExpectLevel(*this, TEXT("6,001 m"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
			In.NearestPhysicsDistanceM = 5.999f;
			ExpectLevel(*this, TEXT("5,999 m"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
			In.NearestPhysicsDistanceM = 0.f;
			ExpectLevel(*this, TEXT("0 m"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
		});

		It("sem veiculo Physics por perto (padrao) ou distancia NaN nao acorda", [this]()
		{
			ExpectLevel(*this, TEXT("padrao"), FDNTraffic::NextLevel(Settings, Neutral()), EDNTrafficLevel::Rail);
			FDNTrafficVehicleInput In = Neutral();
			In.NearestPhysicsDistanceM = NaN;
			ExpectLevel(*this, TEXT("NaN"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
		});
	});

	Describe("Rail para Physics: jogador na frente e se aproximando (Q1, Q2)", [this]()
	{
		FDNTrafficVehicleInput Ahead;
		Ahead.PlayerDistanceM = 20.f;
		Ahead.PlayerProjectionOnVehicleForwardM = 19.f;
		Ahead.ClosingSpeedKmh = 50.f;

		It("a menos de 25 m, na frente e com 20 km/h ou mais de aproximacao: acorda", [this, Ahead]()
		{
			ExpectLevel(*this, TEXT("tudo valendo"), FDNTraffic::NextLevel(Settings, Ahead), EDNTrafficLevel::Physics);
		});

		It("exatamente 25 m nao acorda (estrito); 24,99 acorda", [this, Ahead]()
		{
			FDNTrafficVehicleInput In = Ahead;
			In.PlayerDistanceM = 25.0f;
			ExpectLevel(*this, TEXT("25,0 m"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
			In.PlayerDistanceM = 24.99f;
			ExpectLevel(*this, TEXT("24,99 m"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
		});

		It("exatamente 20 km/h de aproximacao acorda (inclusivo); 19,99 nao", [this, Ahead]()
		{
			FDNTrafficVehicleInput In = Ahead;
			In.ClosingSpeedKmh = 20.0f;
			ExpectLevel(*this, TEXT("20,0 km/h"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
			In.ClosingSpeedKmh = 19.99f;
			ExpectLevel(*this, TEXT("19,99 km/h"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
			In.ClosingSpeedKmh = -30.f;
			ExpectLevel(*this, TEXT("se afastando"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
		});

		It("o jogador precisa estar NA FRENTE (projecao maior que zero, estrito)", [this, Ahead]()
		{
			FDNTrafficVehicleInput In = Ahead;
			In.PlayerProjectionOnVehicleForwardM = 0.001f;
			ExpectLevel(*this, TEXT("0,001 m a frente"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
			In.PlayerProjectionOnVehicleForwardM = 0.f;
			ExpectLevel(*this, TEXT("ao lado (0)"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
			In.PlayerProjectionOnVehicleForwardM = -5.f;
			ExpectLevel(*this, TEXT("atras"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
		});

		It("NaN em qualquer das tres entradas nao acorda", [this, Ahead]()
		{
			FDNTrafficVehicleInput In = Ahead;
			In.PlayerDistanceM = NaN;
			ExpectLevel(*this, TEXT("distancia NaN"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
			In = Ahead; In.PlayerProjectionOnVehicleForwardM = NaN;
			ExpectLevel(*this, TEXT("projecao NaN"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
			In = Ahead; In.ClosingSpeedKmh = NaN;
			ExpectLevel(*this, TEXT("aproximacao NaN"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
		});
	});

	Describe("Os limites vem da configuracao", [this]()
	{
		It("mudar WakeDistanceM, WakeAheadM e a aproximacao muda o resultado", [this]()
		{
			FDNTrafficSettings S;
			S.WakeDistanceM = 10.f;
			FDNTrafficVehicleInput In = Neutral();
			In.NearestPhysicsDistanceM = 8.f;
			ExpectLevel(*this, TEXT("8 m com limite 10"), FDNTraffic::NextLevel(S, In), EDNTrafficLevel::Physics);
			ExpectLevel(*this, TEXT("8 m com limite 6"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);

			S = FDNTrafficSettings();
			S.WakeAheadM = 40.f;
			S.WakeAheadMinClosingKmh = 50.f;
			FDNTrafficVehicleInput Ahead = Neutral();
			Ahead.PlayerDistanceM = 30.f; Ahead.PlayerProjectionOnVehicleForwardM = 29.f; Ahead.ClosingSpeedKmh = 49.f;
			ExpectLevel(*this, TEXT("49 km/h com minimo 50"), FDNTraffic::NextLevel(S, Ahead), EDNTrafficLevel::Rail);
			Ahead.ClosingSpeedKmh = 50.f;
			ExpectLevel(*this, TEXT("50 km/h com minimo 50"), FDNTraffic::NextLevel(S, Ahead), EDNTrafficLevel::Physics);
		});
	});

	Describe("Physics nunca volta a Rail e Despawn e final", [this]()
	{
		It("Physics sem nenhum motivo continua Physics", [this]()
		{
			FDNTrafficVehicleInput In = Neutral();
			In.Level = EDNTrafficLevel::Physics;
			ExpectLevel(*this, TEXT("neutro"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
		});

		It("Despawn continua Despawn, mesmo com impacto ou veiculo Physics perto", [this]()
		{
			FDNTrafficVehicleInput In = Neutral();
			In.Level = EDNTrafficLevel::Despawn;
			In.ImpactForce = 100.f;
			In.NearestPhysicsDistanceM = 1.f;
			ExpectLevel(*this, TEXT("despawn"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Despawn);
		});
	});

	Describe("Despawn: a mais de 200 m e atras do jogador", [this]()
	{
		FDNTrafficVehicleInput Behind;
		Behind.PlayerDistanceM = 250.f;
		Behind.VehicleProjectionOnPlayerForwardM = -240.f;

		It("Rail longe e atras some", [this, Behind]()
		{
			ExpectLevel(*this, TEXT("Rail"), FDNTraffic::NextLevel(Settings, Behind), EDNTrafficLevel::Despawn);
		});

		It("exatamente 200 m nao some (estrito); 200,01 some", [this, Behind]()
		{
			FDNTrafficVehicleInput In = Behind;
			In.PlayerDistanceM = 200.0f;
			ExpectLevel(*this, TEXT("200,0 m"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
			In.PlayerDistanceM = 200.01f;
			ExpectLevel(*this, TEXT("200,01 m"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Despawn);
		});

		It("precisa estar ATRAS do jogador (projecao menor que zero, estrito)", [this, Behind]()
		{
			FDNTrafficVehicleInput In = Behind;
			In.VehicleProjectionOnPlayerForwardM = -0.001f;
			ExpectLevel(*this, TEXT("-0,001"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Despawn);
			In.VehicleProjectionOnPlayerForwardM = 0.f;
			ExpectLevel(*this, TEXT("0"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
			In.VehicleProjectionOnPlayerForwardM = 100.f;
			ExpectLevel(*this, TEXT("na frente do jogador, longe"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
		});

		It("NaN nao faz o veiculo sumir", [this, Behind]()
		{
			FDNTrafficVehicleInput In = Behind;
			In.PlayerDistanceM = NaN;
			ExpectLevel(*this, TEXT("distancia NaN"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
			In = Behind; In.VehicleProjectionOnPlayerForwardM = NaN;
			ExpectLevel(*this, TEXT("projecao NaN"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Rail);
		});

		It("Physics longe e atras some quando nao ha rodada de Crash", [this, Behind]()
		{
			FDNTrafficVehicleInput In = Behind;
			In.Level = EDNTrafficLevel::Physics;
			In.SpeedMs = 30.f;
			ExpectLevel(*this, TEXT("rodada inativa"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Despawn);
		});
	});

	Describe("Protecao na rodada de Crash (Q4)", [this]()
	{
		FDNTrafficVehicleInput FarPhysics;
		FarPhysics.Level = EDNTrafficLevel::Physics;
		FarPhysics.PlayerDistanceM = 250.f;
		FarPhysics.VehicleProjectionOnPlayerForwardM = -240.f;
		FarPhysics.bCrashRoundActive = true;

		It("Physics acima de 2,0 m/s nao some com a rodada ativa", [this, FarPhysics]()
		{
			FDNTrafficVehicleInput In = FarPhysics;
			In.SpeedMs = 2.01f;
			ExpectLevel(*this, TEXT("2,01 m/s"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
			In.SpeedMs = 40.f;
			ExpectLevel(*this, TEXT("40 m/s"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
		});

		It("exatamente 2,0 m/s ja pode sumir (estrito); parado some", [this, FarPhysics]()
		{
			FDNTrafficVehicleInput In = FarPhysics;
			In.SpeedMs = 2.0f;
			ExpectLevel(*this, TEXT("2,0 m/s"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Despawn);
			In.SpeedMs = 0.f;
			ExpectLevel(*this, TEXT("parado"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Despawn);
		});

		It("a velocidade conta pelo modulo, e NaN conta como em movimento (protege)", [this, FarPhysics]()
		{
			FDNTrafficVehicleInput In = FarPhysics;
			In.SpeedMs = -10.f;
			ExpectLevel(*this, TEXT("-10 m/s"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
			In.SpeedMs = NaN;
			ExpectLevel(*this, TEXT("NaN"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
		});

		It("Rail nunca e protegido: some mesmo andando na trilha com a rodada ativa", [this, FarPhysics]()
		{
			FDNTrafficVehicleInput In = FarPhysics;
			In.Level = EDNTrafficLevel::Rail;
			In.SpeedMs = 20.f;
			ExpectLevel(*this, TEXT("Rail"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Despawn);
		});

		It("o limite de repouso vem da configuracao", [this, FarPhysics]()
		{
			FDNTrafficSettings S;
			S.StillSpeedMs = 5.f;
			FDNTrafficVehicleInput In = FarPhysics;
			In.SpeedMs = 4.f;
			ExpectLevel(*this, TEXT("4 m/s com limite 5"), FDNTraffic::NextLevel(S, In), EDNTrafficLevel::Despawn);
			ExpectLevel(*this, TEXT("4 m/s com limite 2"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
		});
	});

	// Q3: primeiro acorda, depois despawn sobre o nivel resultante.
	Describe("Ordem: acordar primeiro, despawn depois (Q3)", [this]()
	{
		FDNTrafficVehicleInput HitFarBehind;
		HitFarBehind.Level = EDNTrafficLevel::Rail;
		HitFarBehind.ImpactForce = 1000.f;
		HitFarBehind.PlayerDistanceM = 300.f;
		HitFarBehind.VehicleProjectionOnPlayerForwardM = -290.f;
		HitFarBehind.SpeedMs = 15.f;

		It("Rail atingido, longe e atras, com rodada ativa e em movimento: vira Physics e fica", [this, HitFarBehind]()
		{
			FDNTrafficVehicleInput In = HitFarBehind;
			In.bCrashRoundActive = true;
			ExpectLevel(*this, TEXT("rodada ativa"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Physics);
		});

		It("o mesmo caso sem rodada ativa: some", [this, HitFarBehind]()
		{
			ExpectLevel(*this, TEXT("rodada inativa"), FDNTraffic::NextLevel(Settings, HitFarBehind), EDNTrafficLevel::Despawn);
		});

		It("o mesmo caso com rodada ativa mas parado (<= 2,0 m/s): some", [this, HitFarBehind]()
		{
			FDNTrafficVehicleInput In = HitFarBehind;
			In.bCrashRoundActive = true;
			In.SpeedMs = 1.f;
			ExpectLevel(*this, TEXT("parado"), FDNTraffic::NextLevel(Settings, In), EDNTrafficLevel::Despawn);
		});
	});
}

BEGIN_DEFINE_SPEC(FDNTrafficRailSpec, "Detonauto.Trafego.Trilha",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNTrafficRailSpec)

void FDNTrafficRailSpec::Define()
{
	Describe("Andar sem loop", [this]()
	{
		It("avanca velocidade vezes tempo", [this]()
		{
			const FDNRailAdvance R = FDNTraffic::AdvanceOnRail(10.f, 20.f, 0.5f, 100.f, false);
			TestEqual("distancia", R.Distance, 20.f);
			TestFalse("fim", R.bReachedEnd);
			TestFalse("erro", R.bError);
		});

		It("satura no comprimento e informa que chegou ao fim", [this]()
		{
			const FDNRailAdvance R = FDNTraffic::AdvanceOnRail(90.f, 20.f, 1.f, 100.f, false);
			TestEqual("distancia", R.Distance, 100.f);
			TestTrue("fim", R.bReachedEnd);
			TestFalse("erro", R.bError);
		});

		It("chegar EXATAMENTE no comprimento informa o fim", [this]()
		{
			const FDNRailAdvance R = FDNTraffic::AdvanceOnRail(80.f, 20.f, 1.f, 100.f, false);
			TestEqual("distancia", R.Distance, 100.f);
			TestTrue("fim", R.bReachedEnd);
		});

		It("estar no fim e tentar andar de novo continua informando o fim", [this]()
		{
			const FDNRailAdvance R = FDNTraffic::AdvanceOnRail(100.f, 5.f, 1.f, 100.f, false);
			TestEqual("distancia", R.Distance, 100.f);
			TestTrue("fim", R.bReachedEnd);
			TestFalse("erro", R.bError);
		});

		It("a distancia inicial exatamente em 0 e exatamente no comprimento sao validas", [this]()
		{
			TestFalse("0", FDNTraffic::AdvanceOnRail(0.f, 1.f, 1.f, 100.f, false).bError);
			TestFalse("comprimento", FDNTraffic::AdvanceOnRail(100.f, 1.f, 1.f, 100.f, false).bError);
		});
	});

	Describe("Casos em que nao move (Dt, velocidade)", [this]()
	{
		It("Dt zero ou negativo nao move e nao e erro", [this]()
		{
			for (float Dt : { 0.f, -0.5f })
			{
				const FDNRailAdvance R = FDNTraffic::AdvanceOnRail(10.f, 20.f, Dt, 100.f, false);
				TestEqual("distancia", R.Distance, 10.f);
				TestFalse("fim", R.bReachedEnd);
				TestFalse("erro", R.bError);
			}
		});

		It("velocidade negativa nao move e nao e erro", [this]()
		{
			const FDNRailAdvance R = FDNTraffic::AdvanceOnRail(10.f, -3.f, 1.f, 100.f, false);
			TestEqual("distancia", R.Distance, 10.f);
			TestFalse("fim", R.bReachedEnd);
			TestFalse("erro", R.bError);
		});

		It("velocidade zero move zero, sem erro, e no fim NAO informa fim (nao houve avanco)", [this]()
		{
			const FDNRailAdvance R = FDNTraffic::AdvanceOnRail(10.f, 0.f, 1.f, 100.f, false);
			TestEqual("distancia", R.Distance, 10.f);
			TestFalse("erro", R.bError);
			const FDNRailAdvance AtEnd = FDNTraffic::AdvanceOnRail(100.f, 0.f, 1.f, 100.f, false);
			TestEqual("distancia no fim", AtEnd.Distance, 100.f);
			TestFalse("fim", AtEnd.bReachedEnd);
			TestFalse("erro no fim", AtEnd.bError);
		});

		It("no fim, Dt zero ou velocidade negativa tambem nao informam fim", [this]()
		{
			TestFalse("Dt 0", FDNTraffic::AdvanceOnRail(100.f, 5.f, 0.f, 100.f, false).bReachedEnd);
			TestFalse("velocidade negativa", FDNTraffic::AdvanceOnRail(100.f, -5.f, 1.f, 100.f, false).bReachedEnd);
		});
	});

	Describe("Erros: nao move, informa erro e devolve a distancia recebida", [this]()
	{
		It("comprimento zero, negativo ou nao finito", [this]()
		{
			for (float Length : { 0.f, -10.f, NaN, Inf })
			{
				const FDNRailAdvance R = FDNTraffic::AdvanceOnRail(5.f, 10.f, 1.f, Length, false);
				TestTrue(FString::Printf(TEXT("erro com comprimento %f"), Length), R.bError);
				TestEqual("distancia devolvida", R.Distance, 5.f);
				TestFalse("fim", R.bReachedEnd);
			}
		});

		It("distancia, velocidade ou Dt nao finitos", [this]()
		{
			TestTrue("distancia NaN", FDNTraffic::AdvanceOnRail(NaN, 10.f, 1.f, 100.f, false).bError);
			TestTrue("distancia inf", FDNTraffic::AdvanceOnRail(Inf, 10.f, 1.f, 100.f, false).bError);
			TestTrue("velocidade NaN", FDNTraffic::AdvanceOnRail(5.f, NaN, 1.f, 100.f, false).bError);
			TestTrue("velocidade inf", FDNTraffic::AdvanceOnRail(5.f, Inf, 1.f, 100.f, false).bError);
			TestTrue("Dt NaN", FDNTraffic::AdvanceOnRail(5.f, 10.f, NaN, 100.f, false).bError);
			TestTrue("Dt inf", FDNTraffic::AdvanceOnRail(5.f, 10.f, Inf, 100.f, false).bError);
			TestTrue("com loop tambem", FDNTraffic::AdvanceOnRail(NaN, 10.f, 1.f, 100.f, true).bError);
		});

		// Q6b: sem loop, a distância inicial precisa estar em [0, Comprimento].
		It("sem loop, distancia inicial fora de [0, comprimento] e erro", [this]()
		{
			const FDNRailAdvance Neg = FDNTraffic::AdvanceOnRail(-0.1f, 10.f, 1.f, 100.f, false);
			TestTrue("negativa", Neg.bError);
			TestEqual("devolve a recebida", Neg.Distance, -0.1f);
			const FDNRailAdvance Over = FDNTraffic::AdvanceOnRail(100.1f, 10.f, 1.f, 100.f, false);
			TestTrue("maior que o comprimento", Over.bError);
			TestEqual("devolve a recebida", Over.Distance, 100.1f);
			TestFalse("nao informa fim", Over.bReachedEnd);
		});
	});

	Describe("Andar com loop (resultado sempre em [0, comprimento))", [this]()
	{
		It("da a volta pelo modulo", [this]()
		{
			TestEqual("95 + 10", FDNTraffic::AdvanceOnRail(95.f, 10.f, 1.f, 100.f, true).Distance, 5.f);
			TestEqual("250 em 100", FDNTraffic::AdvanceOnRail(0.f, 250.f, 1.f, 100.f, true).Distance, 50.f);
		});

		It("chegar exatamente no comprimento volta a 0, nao fica no comprimento", [this]()
		{
			TestEqual("80 + 20", FDNTraffic::AdvanceOnRail(80.f, 20.f, 1.f, 100.f, true).Distance, 0.f);
		});

		It("bChegouAoFim e sempre falso com loop", [this]()
		{
			TestFalse("passou do fim", FDNTraffic::AdvanceOnRail(95.f, 10.f, 1.f, 100.f, true).bReachedEnd);
			TestFalse("exato", FDNTraffic::AdvanceOnRail(80.f, 20.f, 1.f, 100.f, true).bReachedEnd);
		});

		It("aceita qualquer distancia finita e normaliza, inclusive negativa e maior que o comprimento", [this]()
		{
			TestEqual("-30 em 100", FDNTraffic::AdvanceOnRail(-30.f, 0.f, 1.f, 100.f, true).Distance, 70.f);
			TestEqual("130 em 100", FDNTraffic::AdvanceOnRail(130.f, 0.f, 1.f, 100.f, true).Distance, 30.f);
			TestEqual("exatamente o comprimento vira 0", FDNTraffic::AdvanceOnRail(100.f, 0.f, 1.f, 100.f, true).Distance, 0.f);
			TestFalse("sem erro", FDNTraffic::AdvanceOnRail(-30.f, 5.f, 1.f, 100.f, true).bError);
		});

		It("negativo minusculo NAO devolve o comprimento (arredondamento)", [this]()
		{
			const FDNRailAdvance R = FDNTraffic::AdvanceOnRail(-1.0e-30f, 0.f, 1.f, 100.f, true);
			TestTrue(FString::Printf(TEXT("resultado %.9f precisa estar em [0, 100)"), R.Distance), R.Distance >= 0.f && R.Distance < 100.f);
			const FDNRailAdvance Moving = FDNTraffic::AdvanceOnRail(-1.0e-30f, 1.0e-30f, 1.f, 100.f, true);
			TestTrue("andando", Moving.Distance >= 0.f && Moving.Distance < 100.f);
		});

		It("varredura: o resultado fica em [0, comprimento) para muitos valores, inclusive extremos", [this]()
		{
			FRandomStream Random(2026);
			const float Lengths[] = { 1.f, 7.3f, 100.f, 12345.678f, 1.0e6f };
			for (const float L : Lengths)
			{
				for (int32 i = 0; i < 2000; ++i)
				{
					const float D = Random.FRandRange(-3.f * L, 3.f * L);
					const float V = Random.FRandRange(0.f, 2.f * L);
					const float Dt = Random.FRandRange(0.f, 2.f);
					const FDNRailAdvance R = FDNTraffic::AdvanceOnRail(D, V, Dt, L, true);
					if (!TestTrue(FString::Printf(TEXT("L=%f D=%f V=%f Dt=%f deu %.9f"), L, D, V, Dt, R.Distance), !R.bError && R.Distance >= 0.f && R.Distance < L))
					{
						return;
					}
				}
			}
			const float Huge = 1.0e30f;
			const FDNRailAdvance R = FDNTraffic::AdvanceOnRail(Huge, 1.f, 1.f, 100.f, true);
			TestTrue("distancia enorme", !R.bError && R.Distance >= 0.f && R.Distance < 100.f);
		});

		It("Dt zero, velocidade negativa ou zero com loop nao movem, mas ainda normalizam", [this]()
		{
			TestEqual("Dt 0", FDNTraffic::AdvanceOnRail(130.f, 20.f, 0.f, 100.f, true).Distance, 30.f);
			TestEqual("velocidade negativa", FDNTraffic::AdvanceOnRail(130.f, -20.f, 1.f, 100.f, true).Distance, 30.f);
		});
	});
}
#endif // WITH_DEV_AUTOMATION_TESTS
