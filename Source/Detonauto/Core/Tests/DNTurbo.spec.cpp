// Testes do turbo (docs/regras-de-jogo.md, seção 1). Funções puras: o tempo e as entradas do quadro
// entram como parâmetro. Nada de mundo, Actor ou física.
//
// Os comentários "Tn" apontam para a tabela "Decisões de implementação" do contrato, que fecha os pontos que o
// texto original deixava abertos. Trocar a decisão = trocar o teste.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNTurbo.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr float Tol = 1e-3f;

	FDNNearMissEvent Pass(int32 Id, float LateralCm = 100.f, bool bMoving = true)
	{
		FDNNearMissEvent E;
		E.VehicleId = Id;
		E.LateralDistanceCm = LateralCm;
		E.bVehicleMoving = bMoving;
		return E;
	}

	// um quadro de turbo desligado com os dados informados
	void Step(const FDNTurboConfig& C, FDNTurboState& S, float Dt, float SpeedKmh = 0.f, bool bWrongWay = false,
		float DriftDeg = 0.f, bool bAir = false, bool bHeld = false, bool bTakedown = false, TArray<FDNNearMissEvent> Passes = {})
	{
		FDNTurboFrameInput In;
		In.DeltaTime = Dt;
		In.SpeedKmh = SpeedKmh;
		In.bWrongWay = bWrongWay;
		In.DriftAngleDeg = DriftDeg;
		In.bAirborne = bAir;
		In.bButtonHeld = bHeld;
		In.bTakedown = bTakedown;
		In.NearMisses = MoveTemp(Passes);
		FDNTurbo::Tick(C, S, In);
	}
}

BEGIN_DEFINE_SPEC(FDNTurboGainSpec, "Detonauto.Turbo.Ganho",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNTurboConfig Config;
END_DEFINE_SPEC(FDNTurboGainSpec)

void FDNTurboGainSpec::Define()
{
	Describe("Contramao", [this]()
	{
		It("da 12 por segundo acima de 80 km/h", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.5f, 100.f, true);
			TestNearlyEqual("0,5 s a 12/s", S.Bar, 6.f, Tol);
		});

		It("nao da nada a exatamente 80 km/h (so acima)", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 1.f, 80.f, true);
			TestNearlyEqual("barra", S.Bar, 0.f, Tol);
			Step(Config, S, 1.f, 80.01f, true);
			TestTrue("um pouco acima ganha", S.Bar > 11.f);
		});

		It("nao da nada fora da faixa contraria", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 1.f, 150.f, false);
			TestNearlyEqual("barra", S.Bar, 0.f, Tol);
		});

		It("a funcao pura devolve o ganho do quadro", [this]()
		{
			TestNearlyEqual("ganho", FDNTurbo::WrongWayGain(Config, 120.f, true, 0.25f), 3.f, Tol);
			TestNearlyEqual("sem contramao", FDNTurbo::WrongWayGain(Config, 120.f, false, 0.25f), 0.f, Tol);
		});
	});

	Describe("Derrapada", [this]()
	{
		It("da 8 por segundo entre 15 e 70 graus acima de 60 km/h", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 1.f, 100.f, false, 40.f);
			TestNearlyEqual("barra", S.Bar, 8.f, Tol);
		});

		// T5 (docs/regras-de-jogo.md, "Decisões de implementação"): "entre 15 e 70 graus" lido como inclusivo nos dois extremos.
		It("inclui exatamente 15 e 70 graus e exclui fora do intervalo", [this]()
		{
			TestNearlyEqual("15 graus", FDNTurbo::DriftGain(Config, 15.f, 100.f, 1.f), 8.f, Tol);
			TestNearlyEqual("70 graus", FDNTurbo::DriftGain(Config, 70.f, 100.f, 1.f), 8.f, Tol);
			TestNearlyEqual("14,99 graus", FDNTurbo::DriftGain(Config, 14.99f, 100.f, 1.f), 0.f, Tol);
			TestNearlyEqual("70,01 graus", FDNTurbo::DriftGain(Config, 70.01f, 100.f, 1.f), 0.f, Tol);
		});

		It("nao da nada a exatamente 60 km/h (so acima)", [this]()
		{
			TestNearlyEqual("60 km/h", FDNTurbo::DriftGain(Config, 40.f, 60.f, 1.f), 0.f, Tol);
			TestNearlyEqual("60,01 km/h", FDNTurbo::DriftGain(Config, 40.f, 60.01f, 1.f), 8.f, Tol);
		});

		It("o angulo negativo (derrapar para o outro lado) conta pelo valor absoluto", [this]()
		{
			TestNearlyEqual("-40 graus", FDNTurbo::DriftGain(Config, -40.f, 100.f, 1.f), 8.f, Tol);
		});
	});

	Describe("Passada raspando", [this]()
	{
		It("da 6 por evento abaixo de 150 cm, acima de 60 km/h, em veiculo em movimento", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.f, 100.f, false, 0.f, false, false, false, { Pass(1) });
			TestNearlyEqual("barra", S.Bar, 6.f, Tol);
		});

		It("exatamente 150 cm nao conta (so abaixo)", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.f, 100.f, false, 0.f, false, false, false, { Pass(1, 150.f) });
			TestNearlyEqual("150 cm", S.Bar, 0.f, Tol);
			Step(Config, S, 0.f, 100.f, false, 0.f, false, false, false, { Pass(2, 149.99f) });
			TestNearlyEqual("149,99 cm", S.Bar, 6.f, Tol);
		});

		It("exatamente 60 km/h nao conta (so acima)", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.f, 60.f, false, 0.f, false, false, false, { Pass(1) });
			TestNearlyEqual("barra", S.Bar, 0.f, Tol);
		});

		It("veiculo parado nao conta", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.f, 100.f, false, 0.f, false, false, false, { Pass(1, 100.f, false) });
			TestNearlyEqual("barra", S.Bar, 0.f, Tol);
		});

		// T2 (docs/regras-de-jogo.md, "Decisões de implementação"): "uma vez por veículo" lido como uma vez por veículo em toda a rodada.
		It("conta uma vez por veiculo", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.f, 100.f, false, 0.f, false, false, false, { Pass(7) });
			Step(Config, S, 0.f, 100.f, false, 0.f, false, false, false, { Pass(7) });
			TestNearlyEqual("so o primeiro", S.Bar, 6.f, Tol);
		});

		It("a cadeia multiplica 1,0 1,25 1,5 1,75 2,0", [this]()
		{
			FDNTurboState S;
			const float Expected[5] = { 6.f, 7.5f, 9.f, 10.5f, 12.f };
			for (int32 i = 0; i < 5; ++i)
			{
				const float Before = S.Bar;
				Step(Config, S, 0.5f, 100.f, false, 0.f, false, false, false, { Pass(10 + i) });
				TestNearlyEqual(FString::Printf(TEXT("passada %d"), i + 1), S.Bar - Before, Expected[i], Tol);
			}
		});

		// T3 (docs/regras-de-jogo.md, "Decisões de implementação"): depois da quinta passada o multiplicador fica em 2,0.
		It("a sexta passada da cadeia continua em 2,0", [this]()
		{
			FDNTurboState S;
			for (int32 i = 0; i < 6; ++i)
			{
				Step(Config, S, 0.5f, 100.f, false, 0.f, false, false, false, { Pass(10 + i) });
				S.Bar = 0.f; // zera para medir cada ganho
			}
			Step(Config, S, 0.5f, 100.f, false, 0.f, false, false, false, { Pass(99) });
			TestNearlyEqual("setima passada", S.Bar, 12.f, Tol);
		});

		// T4 (docs/regras-de-jogo.md, "Decisões de implementação"): exatamente 2,0 s ainda continua a cadeia; "mais de 2,0 s" reinicia.
		It("exatamente 2,0 s depois continua a cadeia", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.f, 100.f, false, 0.f, false, false, false, { Pass(1) });
			Step(Config, S, 2.0f, 100.f, false, 0.f, false, false, false, { Pass(2) });
			TestNearlyEqual("6 + 6x1,25", S.Bar, 13.5f, Tol);
		});

		It("mais de 2,0 s depois reinicia a cadeia", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.f, 100.f, false, 0.f, false, false, false, { Pass(1) });
			Step(Config, S, 2.01f, 100.f, false, 0.f, false, false, false, { Pass(2) });
			TestNearlyEqual("6 + 6x1,0", S.Bar, 12.f, Tol);
		});

		It("eventos no mesmo quadro encadeiam em ordem", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.f, 100.f, false, 0.f, false, false, false, { Pass(1), Pass(2) });
			TestNearlyEqual("6 + 7,5", S.Bar, 13.5f, Tol);
		});

		It("evento que nao cumpre as condicoes nao mexe na cadeia", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.f, 100.f, false, 0.f, false, false, false, { Pass(1) });
			Step(Config, S, 0.1f, 100.f, false, 0.f, false, false, false, { Pass(2, 400.f) });
			Step(Config, S, 0.1f, 100.f, false, 0.f, false, false, false, { Pass(3) });
			TestNearlyEqual("6 + 7,5 (a falha nao conta como elo)", S.Bar, 13.5f, Tol);
		});
	});

	Describe("Salto", [this]()
	{
		It("nao da nada ate 0,3 s no ar", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.3f, 0.f, false, 0.f, true);
			TestNearlyEqual("0,3 s exatos", S.Bar, 0.f, Tol);
		});

		It("depois de 0,3 s da 10 por segundo so sobre o tempo que passou do limiar", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.3f, 0.f, false, 0.f, true);
			Step(Config, S, 0.5f, 0.f, false, 0.f, true);
			TestNearlyEqual("0,5 s a 10/s", S.Bar, 5.f, Tol);
		});

		It("um quadro que atravessa o limiar conta so a parte depois dele", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.5f, 0.f, false, 0.f, true);
			TestNearlyEqual("0,2 s a 10/s", S.Bar, 2.f, Tol);
		});

		It("limita em 20 por salto", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 5.f, 0.f, false, 0.f, true);
			TestNearlyEqual("teto do salto", S.Bar, 20.f, Tol);
			Step(Config, S, 5.f, 0.f, false, 0.f, true);
			TestNearlyEqual("continua em 20", S.Bar, 20.f, Tol);
		});

		It("o teto vale por salto: aterrissar e saltar de novo zera o limite", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 5.f, 0.f, false, 0.f, true);
			Step(Config, S, 0.1f, 0.f, false, 0.f, false);
			Step(Config, S, 5.f, 0.f, false, 0.f, true);
			TestNearlyEqual("dois saltos", S.Bar, 40.f, Tol);
		});
	});

	Describe("Takedown", [this]()
	{
		It("leva a barra a 100", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, false, true);
			TestNearlyEqual("barra", S.Bar, 100.f, Tol);
		});

		It("a funcao pura leva a barra a 100 a partir de qualquer valor", [this]()
		{
			FDNTurboState S;
			S.Bar = 37.f;
			FDNTurbo::ApplyTakedown(Config, S);
			TestNearlyEqual("barra", S.Bar, 100.f, Tol);
		});
	});

	Describe("Limites e entradas invalidas", [this]()
	{
		It("o ganho nunca passa de 100", [this]()
		{
			FDNTurboState S;
			S.Bar = 99.f;
			Step(Config, S, 10.f, 100.f, true, 40.f);
			TestNearlyEqual("barra", S.Bar, 100.f, Tol);
		});

		// T1 (docs/regras-de-jogo.md, "Decisões de implementação"): fontes de ganho no mesmo quadro somam.
		It("fontes diferentes no mesmo quadro somam", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 1.f, 100.f, true, 40.f);
			TestNearlyEqual("12 + 8", S.Bar, 20.f, Tol);
		});

		It("tempo negativo ou NaN nao ganha nem gasta", [this]()
		{
			FDNTurboState S;
			S.Bar = 50.f;
			Step(Config, S, -1.f, 100.f, true, 40.f);
			TestNearlyEqual("tempo negativo", S.Bar, 50.f, Tol);
			Step(Config, S, std::numeric_limits<float>::quiet_NaN(), 100.f, true, 40.f);
			TestNearlyEqual("NaN", S.Bar, 50.f, Tol);
		});

		It("velocidade NaN nao da ganho", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 1.f, std::numeric_limits<float>::quiet_NaN(), true, 40.f);
			TestNearlyEqual("barra", S.Bar, 0.f, Tol);
		});
	});
}

BEGIN_DEFINE_SPEC(FDNTurboUseSpec, "Detonauto.Turbo.Uso",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNTurboConfig Config;
END_DEFINE_SPEC(FDNTurboUseSpec)

void FDNTurboUseSpec::Define()
{
	Describe("Ligar e desligar", [this]()
	{
		It("nao liga com a barra em 0, mesmo segurando o botao", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.1f, 0.f, false, 0.f, false, true);
			TestFalse("ligado", S.bActive);
		});

		It("liga ao segurar o botao com a barra acima de 0", [this]()
		{
			FDNTurboState S;
			S.Bar = 40.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			TestTrue("ligado", S.bActive);
		});

		It("consome 25 por segundo e a barra cheia dura 4 s", [this]()
		{
			FDNTurboState S;
			S.Bar = 100.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 1.f, 0.f, false, 0.f, false, true);
			TestNearlyEqual("depois de 1 s", S.Bar, 75.f, Tol);
			Step(Config, S, 2.f, 0.f, false, 0.f, false, true);
			TestNearlyEqual("depois de 3 s", S.Bar, 25.f, Tol);
		});

		It("desliga ao soltar o botao e guarda o que sobrou", [this]()
		{
			FDNTurboState S;
			S.Bar = 60.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 0.4f, 0.f, false, 0.f, false, true);
			Step(Config, S, 0.4f, 0.f, false, 0.f, false, false);
			TestFalse("ligado", S.bActive);
			TestNearlyEqual("sobrou", S.Bar, 50.f, Tol);
		});

		It("desliga quando a barra chega a 0", [this]()
		{
			FDNTurboState S;
			S.Bar = 25.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 1.f, 0.f, false, 0.f, false, true);
			TestFalse("ligado", S.bActive);
			TestNearlyEqual("barra", S.Bar, 0.f, Tol);
		});

		It("a barra nunca fica negativa com um quadro longo", [this]()
		{
			FDNTurboState S;
			S.Bar = 10.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 10.f, 0.f, false, 0.f, false, true);
			TestNearlyEqual("barra", S.Bar, 0.f, Tol);
		});

		// T6/O1 (docs/regras-de-jogo.md): depois de a barra zerar com o botão ainda apertado, o turbo só
		// volta a ligar com um novo aperto (soltar e apertar de novo): consequência da regra do aperto.
		It("zerou com o botao apertado: um takedown nao religa sozinho", [this]()
		{
			FDNTurboState S;
			S.Bar = 25.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 1.f, 0.f, false, 0.f, false, true); // barra a 0, desliga
			TestFalse("desligou", S.bActive);
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true, true); // takedown, botao ainda apertado
			TestNearlyEqual("a barra encheu", S.Bar, 100.f, Tol);
			TestFalse("nao religou", S.bActive);
		});

		It("depois de soltar e apertar de novo, liga", [this]()
		{
			FDNTurboState S;
			S.Bar = 25.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 1.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true, true);
			Step(Config, S, 0.f, 0.f, false, 0.f, false, false); // solta
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);  // aperta
			TestTrue("ligou", S.bActive);
		});

		// O1 (docs/regras-de-jogo.md): o turbo liga no APERTO (borda de subida do botão) e só se a barra
		// tiver mais de 0 nesse quadro. Segurar o botão com a barra vazia não liga nada.
		It("segurar o botao com a barra vazia nao liga, e a barra vai acumulando", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.1f, 0.f, false, 0.f, false, true); // aperta com a barra em 0
			TestFalse("nao ligou", S.bActive);
			Step(Config, S, 0.1f, 100.f, true, 0.f, false, true); // segura, contramao enche
			TestFalse("segue desligado", S.bActive);
			TestNearlyEqual("acumulou 1,2", S.Bar, 1.2f, Tol);
			Step(Config, S, 0.1f, 100.f, true, 0.f, false, true);
			TestNearlyEqual("acumulou 2,4", S.Bar, 2.4f, Tol);
			TestFalse("continua desligado", S.bActive);
		});

		It("soltar e apertar de novo religa quando a barra tem carga", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.1f, 0.f, false, 0.f, false, true);
			Step(Config, S, 1.f, 100.f, true, 0.f, false, true); // barra enche segurando
			TestFalse("segurar nao liga", S.bActive);
			Step(Config, S, 0.f, 0.f, false, 0.f, false, false); // solta
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);  // aperta
			TestTrue("o novo aperto liga", S.bActive);
		});

		It("takedown com o botao segurado e o turbo desligado nao liga sozinho", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.1f, 0.f, false, 0.f, false, true); // aperta com 0, nao liga
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true, true); // takedown, botao segurado
			TestNearlyEqual("a barra encheu", S.Bar, 100.f, Tol);
			TestFalse("nao ligou sozinho", S.bActive);
		});

		// Quadro do aperto: vale a barra já com os ganhos e o takedown desse quadro.
		It("aperto no mesmo quadro de um takedown liga (a barra ja esta cheia)", [this]()
		{
			FDNTurboState S;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true, true);
			TestTrue("ligou", S.bActive);
		});

		It("so o aperto liga: manter apertado depois de soltar a barra a zero nao religa", [this]()
		{
			FDNTurboState S;
			S.Bar = 25.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 1.f, 0.f, false, 0.f, false, true); // zera
			TestFalse("desligou", S.bActive);
			Step(Config, S, 0.f, 100.f, true, 0.f, false, true, true); // enche e segue apertado
			TestFalse("nao religou", S.bActive);
		});
		It("soltar o botao sem zerar nao exige nada alem de apertar de novo", [this]()
		{
			FDNTurboState S;
			S.Bar = 60.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 0.4f, 0.f, false, 0.f, false, false);
			TestFalse("desligou", S.bActive);
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			TestTrue("ligou de novo", S.bActive);
		});
	});
	Describe("Com o turbo ligado so o takedown da ganho", [this]()
	{
		It("contramao, derrapada, salto e passada nao ganham", [this]()
		{
			FDNTurboState S;
			S.Bar = 50.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 0.f, 100.f, true, 40.f, true, true, false, { Pass(1) });
			TestNearlyEqual("barra", S.Bar, 50.f, Tol);
			Step(Config, S, 0.5f, 100.f, true, 40.f, true, true, false, { Pass(2) });
			TestNearlyEqual("so consumo: 50 - 12,5", S.Bar, 37.5f, Tol);
		});

		It("takedown ligado leva a barra a 100", [this]()
		{
			FDNTurboState S;
			S.Bar = 50.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true, true);
			TestNearlyEqual("barra", S.Bar, 100.f, Tol);
		});
	});
}

BEGIN_DEFINE_SPEC(FDNTurboExtendedSpec, "Detonauto.Turbo.Prolongado",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNTurboConfig Config;
END_DEFINE_SPEC(FDNTurboExtendedSpec)

void FDNTurboExtendedSpec::Define()
{
	Describe("Turbo prolongado", [this]()
	{
		// T7 (docs/regras-de-jogo.md, "Decisões de implementação"): ao chegar a 0 segurando, a barra vai a 50 e o turbo continua no mesmo uso.
		It("ligou com 100 e segurou ate 0 sem soltar: ganha 50 e continua ligado", [this]()
		{
			FDNTurboState S;
			S.Bar = 100.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 4.f, 0.f, false, 0.f, false, true);
			TestNearlyEqual("prêmio", S.Bar, 50.f, Tol);
			TestTrue("continua ligado", S.bActive);
		});

		It("so da o premio uma vez por uso", [this]()
		{
			FDNTurboState S;
			S.Bar = 100.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 4.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 2.f, 0.f, false, 0.f, false, true);
			TestNearlyEqual("barra", S.Bar, 0.f, Tol);
			TestFalse("desligou de vez", S.bActive);
		});

		It("soltar o botao em qualquer momento cancela o premio", [this]()
		{
			FDNTurboState S;
			S.Bar = 100.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 3.9f, 0.f, false, 0.f, false, true);
			Step(Config, S, 0.f, 0.f, false, 0.f, false, false);
			TestFalse("desligou", S.bActive);
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true); // aperta de novo com ~2,5
			Step(Config, S, 1.f, 0.f, false, 0.f, false, true);
			TestNearlyEqual("sem premio: 2,5 - 25 vira 0", S.Bar, 0.f, Tol);
		});

		It("ligar com 99,9 nao da direito ao premio", [this]()
		{
			FDNTurboState S;
			S.Bar = 99.9f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 4.f, 0.f, false, 0.f, false, true);
			TestNearlyEqual("barra", S.Bar, 0.f, Tol);
		});

		It("ligou abaixo de 100 e um takedown encheu a barra: sem premio", [this]()
		{
			FDNTurboState S;
			S.Bar = 60.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true, true); // takedown: 100
			Step(Config, S, 4.f, 0.f, false, 0.f, false, true);
			TestNearlyEqual("barra", S.Bar, 0.f, Tol);
		});

		It("ligou com 100 e um takedown no meio mantem o direito ao premio", [this]()
		{
			FDNTurboState S;
			S.Bar = 100.f;
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 2.f, 0.f, false, 0.f, false, true);
			Step(Config, S, 0.f, 0.f, false, 0.f, false, true, true);
			Step(Config, S, 4.f, 0.f, false, 0.f, false, true);
			TestNearlyEqual("premio", S.Bar, 50.f, Tol);
		});

		It("o premio nunca passa do limite da barra", [this]()
		{
			FDNTurboConfig Big = Config;
			Big.ExtendedBonus = 500.f;
			FDNTurboState S;
			S.Bar = 100.f;
			FDNTurboFrameInput In;
			In.bButtonHeld = true;
			FDNTurbo::Tick(Big, S, In);
			In.DeltaTime = 4.f;
			FDNTurbo::Tick(Big, S, In);
			TestNearlyEqual("barra", S.Bar, 100.f, Tol);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
