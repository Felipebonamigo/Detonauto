// Testes da assistência de derrapagem arcade (lote 14, J2): regra pura que devolve a aceleração de guinada [graus/s²] que corrige o
// carro quando o ângulo de deslize passa de um limite, proporcional ao excesso, com teto, para o carro derrapar e voltar a ser
// controlável sem rodar. Sem mundo, sem física.
//
// Decisões (o pedido não cobre; ver Core/DNDriftAssist.h):
//  A1 o ângulo de deslize é atan2(Vy, Vx) da velocidade no espaço do carro (positivo = a velocidade está à direita do nariz);
//  A2 a correção gira o nariz para a direção da velocidade: o sinal da saída é o sinal do ângulo (positivo = guinar para a direita);
//  A3 excesso = |ângulo| - início, e só conta se positivo (exatamente no início: zero); saída = ganho x excesso, no máximo Max;
//  A4 abaixo de 10 km/h não age; sobe linear até 30 km/h (evita tremer parado e em manobra de estacionar);
//  A5 se o carro já gira para a direção certa a 120 graus/s ou mais, não empurra (evita passar do ponto); a meio caminho, metade;
//  A6 ganho zero ou teto zero desliga a assistência; NaN, infinito ou negativo em qualquer entrada vale zero.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNDriftAssist.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDriftAssistSpec, "Detonauto.Carro.AssistenciaDerrapagem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	const float Inff = std::numeric_limits<float>::infinity();
	FDNDriftAssistConfig Config() const
	{
		FDNDriftAssistConfig C;
		C.StartDeg = 20.f;
		C.Gain = 10.f;
		C.Max = 300.f;
		return C;
	}
END_DEFINE_SPEC(FDNDriftAssistSpec)

void FDNDriftAssistSpec::Define()
{
	Describe("SlipAngleDeg", [this]()
	{
		It("A1: atan2 da velocidade local; positivo quando a velocidade esta a direita do nariz", [this]()
		{
			TestEqual("reto", FDNDriftAssist::SlipAngleDeg(FVector(1000, 0, 0)), 0.f, 1e-4f);
			TestEqual("45 a direita", FDNDriftAssist::SlipAngleDeg(FVector(1000, 1000, 0)), 45.f, 1e-3f);
			TestEqual("45 a esquerda", FDNDriftAssist::SlipAngleDeg(FVector(1000, -1000, 0)), -45.f, 1e-3f);
			TestEqual("de lado", FDNDriftAssist::SlipAngleDeg(FVector(0, 1000, 0)), 90.f, 1e-3f);
			TestEqual("de re (anda para tras): 180", FMath::Abs(FDNDriftAssist::SlipAngleDeg(FVector(-1000, 0, 0))), 180.f, 1e-3f);
		});

		It("a altura (Z) nao entra; parado ou NaN: zero", [this]()
		{
			TestEqual("Z ignorado", FDNDriftAssist::SlipAngleDeg(FVector(1000, 1000, 5000)), 45.f, 1e-3f);
			TestEqual("parado", FDNDriftAssist::SlipAngleDeg(FVector::ZeroVector), 0.f);
			TestEqual("NaN", FDNDriftAssist::SlipAngleDeg(FVector(NaNf, 1, 0)), 0.f);
		});
	});

	Describe("YawAcceleration", [this]()
	{
		It("A3: abaixo do inicio, e exatamente no inicio: zero", [this]()
		{
			TestEqual("10 graus", FDNDriftAssist::YawAcceleration(Config(), 10.f, 80.f, 0.f), 0.f);
			TestEqual("no inicio", FDNDriftAssist::YawAcceleration(Config(), 20.f, 80.f, 0.f), 0.f);
		});

		It("A3: acima do inicio, proporcional ao excesso (ganho x excesso)", [this]()
		{
			TestEqual("21 graus: 1 de excesso", FDNDriftAssist::YawAcceleration(Config(), 21.f, 80.f, 0.f), 10.f, 1e-3f);
			TestEqual("30 graus: 10 de excesso", FDNDriftAssist::YawAcceleration(Config(), 30.f, 80.f, 0.f), 100.f, 1e-3f);
			TestEqual("50 graus: 30 de excesso, no teto", FDNDriftAssist::YawAcceleration(Config(), 50.f, 80.f, 0.f), 300.f, 1e-3f);
		});

		It("A3: teto Max: muito alem do inicio nao passa do teto", [this]()
		{
			TestEqual("90 graus", FDNDriftAssist::YawAcceleration(Config(), 90.f, 80.f, 0.f), 300.f, 1e-3f);
			FDNDriftAssistConfig C = Config();
			C.Max = 50.f;
			TestEqual("teto 50", FDNDriftAssist::YawAcceleration(C, 40.f, 80.f, 0.f), 50.f, 1e-3f);
		});

		It("A2: o sinal da saida e o sinal do angulo (gira o nariz para a velocidade)", [this]()
		{
			TestTrue("direita: positivo", FDNDriftAssist::YawAcceleration(Config(), 30.f, 80.f, 0.f) > 0.f);
			TestTrue("esquerda: negativo", FDNDriftAssist::YawAcceleration(Config(), -30.f, 80.f, 0.f) < 0.f);
			TestEqual("simetria", FDNDriftAssist::YawAcceleration(Config(), -30.f, 80.f, 0.f), -FDNDriftAssist::YawAcceleration(Config(), 30.f, 80.f, 0.f), 1e-3f);
		});

		It("A4: abaixo de 10 km/h nao age; entre 10 e 30 sobe linear; de 30 em diante e cheio", [this]()
		{
			const float Full = FDNDriftAssist::YawAcceleration(Config(), 30.f, 80.f, 0.f);
			TestEqual("5 km/h", FDNDriftAssist::YawAcceleration(Config(), 30.f, 5.f, 0.f), 0.f);
			TestEqual("10 km/h", FDNDriftAssist::YawAcceleration(Config(), 30.f, 10.f, 0.f), 0.f);
			TestEqual("20 km/h: metade", FDNDriftAssist::YawAcceleration(Config(), 30.f, 20.f, 0.f), Full * 0.5f, 1e-3f);
			TestEqual("30 km/h: cheio", FDNDriftAssist::YawAcceleration(Config(), 30.f, 30.f, 0.f), Full, 1e-3f);
			TestEqual("200 km/h: cheio", FDNDriftAssist::YawAcceleration(Config(), 30.f, 200.f, 0.f), Full, 1e-3f);
		});

		It("A5: se ja gira na direcao certa, empurra menos; a 120 graus/s ou mais, nada; girando para o lado errado, cheio", [this]()
		{
			const float Full = FDNDriftAssist::YawAcceleration(Config(), 30.f, 80.f, 0.f);
			TestEqual("60 graus/s certo: metade", FDNDriftAssist::YawAcceleration(Config(), 30.f, 80.f, 60.f), Full * 0.5f, 1e-3f);
			TestEqual("120 graus/s certo: nada", FDNDriftAssist::YawAcceleration(Config(), 30.f, 80.f, 120.f), 0.f, 1e-3f);
			TestEqual("200 graus/s certo: nada (nao fica negativo)", FDNDriftAssist::YawAcceleration(Config(), 30.f, 80.f, 200.f), 0.f, 1e-3f);
			TestEqual("girando para o lado errado: cheio", FDNDriftAssist::YawAcceleration(Config(), 30.f, 80.f, -90.f), Full, 1e-3f);
			// o lado certo depende do sinal do angulo
			TestEqual("angulo negativo, guinada negativa = certo", FDNDriftAssist::YawAcceleration(Config(), -30.f, 80.f, -60.f), -Full * 0.5f, 1e-3f);
			TestEqual("angulo negativo, guinada positiva = errado: cheio", FDNDriftAssist::YawAcceleration(Config(), -30.f, 80.f, 60.f), -Full, 1e-3f);
		});

		It("A6: ganho zero ou teto zero desliga", [this]()
		{
			FDNDriftAssistConfig C = Config();
			C.Gain = 0.f;
			TestEqual("ganho 0", FDNDriftAssist::YawAcceleration(C, 60.f, 80.f, 0.f), 0.f);
			C = Config();
			C.Max = 0.f;
			TestEqual("teto 0", FDNDriftAssist::YawAcceleration(C, 60.f, 80.f, 0.f), 0.f);
			TestEqual("padrao desligado", FDNDriftAssist::YawAcceleration(FDNDriftAssistConfig(), 60.f, 80.f, 0.f), 0.f);
		});

		It("A6: NaN, infinito e configuracao negativa valem zero", [this]()
		{
			TestEqual("angulo NaN", FDNDriftAssist::YawAcceleration(Config(), NaNf, 80.f, 0.f), 0.f);
			TestEqual("velocidade NaN", FDNDriftAssist::YawAcceleration(Config(), 30.f, NaNf, 0.f), 0.f);
			TestEqual("guinada NaN", FDNDriftAssist::YawAcceleration(Config(), 30.f, 80.f, NaNf), 0.f);
			TestEqual("angulo infinito", FDNDriftAssist::YawAcceleration(Config(), Inff, 80.f, 0.f), 0.f);
			FDNDriftAssistConfig Neg = Config();
			Neg.Gain = -10.f;
			TestEqual("ganho negativo", FDNDriftAssist::YawAcceleration(Neg, 30.f, 80.f, 0.f), 0.f);
			Neg = Config();
			Neg.Max = -5.f;
			TestEqual("teto negativo", FDNDriftAssist::YawAcceleration(Neg, 30.f, 80.f, 0.f), 0.f);
		});

		It("o inicio vem da configuracao: mudar muda onde comeca", [this]()
		{
			FDNDriftAssistConfig C = Config();
			C.StartDeg = 35.f;
			TestEqual("30 graus agora e abaixo", FDNDriftAssist::YawAcceleration(C, 30.f, 80.f, 0.f), 0.f);
			TestEqual("40 graus: 5 de excesso", FDNDriftAssist::YawAcceleration(C, 40.f, 80.f, 0.f), 50.f, 1e-3f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
