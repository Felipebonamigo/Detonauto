// Testes da regra do mobiliário destrutível (docs/regras-mobiliario.md): o carro derruba a peça e segue. Puro: sem mundo, sem física.
//
// Decisões (as do contrato, numeradas como lá, mais as escolhas que ele deixou abertas):
//  B1 limiar: quebra se a velocidade HORIZONTAL do carro >= MinBreakSpeedKmh do tipo; abaixo, a peça é sólida (nada muda aqui);
//  B2 perda = min(1 - M/(M + k*m), MaxSpeedLossPct) x f, f = |v^ . d^| (horizontais; d^ do carro até a peça). Só o módulo muda;
//  B3 a peça sai com v^ x |v| x LaunchFactor + LaunchUpMs para cima, no máximo MaxLaunchMs. Quem tomba na base gira em torno de
//     (cima x v^), com a velocidade horizontal de lançamento / 2 m (braço de tombo), no máximo 8 rad/s (escolha deste lote);
//  B4 a variação de velocidade do carro (|v| - |v'|) vai para o dano de sempre;
//  B5 pontos do tipo só quando quebra;
//  B6 entradas inválidas: velocidade NaN não quebra; massa do carro inválida vale a do carro padrão; massa da peça inválida vale o
//     padrão do tipo; d^ nula ou NaN vale f = 1; k ou teto inválidos valem o padrão;
//  B7 batida em peça nunca conta como a batida do jogador na rodada de Crash nem dispara a câmera de batida;
//  B8 sono: a peça solta dorme depois de SleepAfterS seguidos abaixo de SleepSpeedMs.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNBreakable.h"
#include "Core/DNCar.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNBreakableSpec, "Detonauto.Mobiliario.Regra",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	FDNBreakableSettings Settings;

	static FDNBreakableKind Kind(float MassKg, float MinKmh = 20.f, bool bTopples = false)
	{
		FDNBreakableKind K;
		K.MassKg = MassKg;
		K.MinBreakSpeedKmh = MinKmh;
		K.LaunchFactor = 0.7f;
		K.LaunchUpMs = 2.f;
		K.bTopplesAtBase = bTopples;
		K.Points = 50;
		return K;
	}

	/** Carro de 1.500 kg a Kmh para a frente (+X), peça logo à frente. */
	static FDNBreakInput Ahead(float Kmh, FVector ToProp = FVector(1.f, 0.f, 0.f))
	{
		FDNBreakInput In;
		In.CarVelocityCmS = FVector(Kmh / 0.036f, 0.f, 0.f);
		In.CarMassKg = 1500.f;
		In.CarToProp = ToProp;
		return In;
	}

	static float Kmh(const FVector& V) { return static_cast<float>(V.Size()) * 0.036f; }
END_DEFINE_SPEC(FDNBreakableSpec)

void FDNBreakableSpec::Define()
{
	BeforeEach([this]() { Settings = FDNBreakableSettings(); });

	It("B1: quebra a partir do limiar horizontal; abaixo a peca e solida", [this]()
	{
		TestFalse("19,9 km/h no poste (limiar 20)", FDNBreakable::Hit(Kind(250.f), Settings, Ahead(19.9f)).bBreaks);
		TestTrue("20 km/h", FDNBreakable::Hit(Kind(250.f), Settings, Ahead(20.f)).bBreaks);
		const FDNBreakResult Below = FDNBreakable::Hit(Kind(250.f), Settings, Ahead(10.f));
		TestEqual("abaixo: o carro nao muda aqui (a fisica o para)", Below.CarVelocityCmS, Ahead(10.f).CarVelocityCmS);
		TestEqual("abaixo: a peca nao sai", Below.PropVelocityCmS, FVector::ZeroVector);
		FDNBreakInput Falling = Ahead(5.f);
		Falling.CarVelocityCmS.Z = -3000.f; // caindo rápido, mas devagar na horizontal
		TestFalse("so a velocidade horizontal conta", FDNBreakable::Hit(Kind(250.f), Settings, Falling).bBreaks);
	});

	It("B2: a perda vem da massa (tabela do contrato) e tem teto de 25%", [this]()
	{
		const struct { float Mass; float KmhAfter; } Table[] = { { 25.f, 98.36f }, { 60.f, 96.15f }, { 120.f, 92.59f }, { 250.f, 85.71f }, { 400.f, 78.95f } };
		for (const auto& Row : Table)
		{
			const FDNBreakResult R = FDNBreakable::Hit(Kind(Row.Mass, 5.f), Settings, Ahead(100.f));
			TestEqual(FString::Printf(TEXT("%.0f kg a 100 km/h"), Row.Mass), Kmh(R.CarVelocityCmS), Row.KmhAfter, 0.02f);
		}
		TestEqual("1000 kg bate no teto de 25%", Kmh(FDNBreakable::Hit(Kind(1000.f, 5.f), Settings, Ahead(100.f)).CarVelocityCmS), 75.f, 0.02f);
		Settings.ArcadeMassFactor = 2.f;
		TestEqual("k = 2 dobra a massa da peca", Kmh(FDNBreakable::Hit(Kind(250.f, 5.f), Settings, Ahead(100.f)).CarVelocityCmS), 75.f, 0.02f);
	});

	It("B2: a perda vezes a frontalidade, e so o modulo muda", [this]()
	{
		const FDNBreakResult Side = FDNBreakable::Hit(Kind(250.f, 5.f), Settings, Ahead(100.f, FVector(0.f, 1.f, 0.f)));
		TestEqual("raspao de lado (f = 0): nao perde", Kmh(Side.CarVelocityCmS), 100.f, 0.02f);
		const FDNBreakResult At60 = FDNBreakable::Hit(Kind(250.f, 5.f), Settings, Ahead(100.f, FVector(0.5f, 0.8660254f, 0.f)));
		TestEqual("a 60 graus (f = 0,5)", Kmh(At60.CarVelocityCmS), 100.f * (1.f - 0.5f * 0.142857f), 0.02f);
		// carro rodando de lado ou de ré: a peça fica "atrás" do movimento (produto escalar negativo); a perda nunca vira ganho
		const FDNBreakResult Behind = FDNBreakable::Hit(Kind(250.f, 5.f), Settings, Ahead(100.f, FVector(-0.5f, 0.8660254f, 0.f)));
		TestEqual("peca atras do movimento (f = |-0,5|)", Kmh(Behind.CarVelocityCmS), 100.f * (1.f - 0.5f * 0.142857f), 0.02f);
		FDNBreakInput Diagonal = Ahead(100.f);
		Diagonal.CarVelocityCmS = FVector(1964.f, 1964.f, -50.f);
		const FDNBreakResult D = FDNBreakable::Hit(Kind(250.f, 5.f), Settings, Diagonal);
		TestTrue("mesma direcao", D.CarVelocityCmS.GetSafeNormal().Equals(Diagonal.CarVelocityCmS.GetSafeNormal(), 1e-4f));
	});

	It("B3: a peca e lancada na direcao do carro, com teto, e so quem tomba gira", [this]()
	{
		const FDNBreakResult R = FDNBreakable::Hit(Kind(250.f, 5.f), Settings, Ahead(100.f));
		TestEqual("lancamento", R.PropVelocityCmS, FVector(100.f / 0.036f * 0.7f, 0.f, 200.f), 0.5f);
		TestEqual("sem tombo: sem giro", R.PropAngularRadS, FVector::ZeroVector);
		const FDNBreakResult T = FDNBreakable::Hit(Kind(250.f, 5.f, true), Settings, Ahead(30.f));
		// cima (Z) x frente (X) = +Y: girar em torno de +Y leva o topo para a frente; 30 km/h x 0,7 = 5,83 m/s / 2 m = 2,92 rad/s
		TestEqual("tombo", T.PropAngularRadS, FVector(0.f, 30.f / 3.6f * 0.7f / 2.f, 0.f), 1e-3f);
		TestEqual("teto do tombo", static_cast<float>(FDNBreakable::Hit(Kind(250.f, 5.f, true), Settings, Ahead(200.f)).PropAngularRadS.Size()), 8.f, 1e-3f);
		FDNBreakableKind Fast = Kind(25.f, 5.f);
		Fast.LaunchFactor = 3.f;
		TestEqual("teto do lancamento (35 m/s)", static_cast<float>(FDNBreakable::Hit(Fast, Settings, Ahead(200.f)).PropVelocityCmS.Size()), 3500.f, 0.5f);
	});

	It("B4 e B5: variacao de velocidade para o dano, e pontos so quando quebra", [this]()
	{
		const FDNBreakResult R = FDNBreakable::Hit(Kind(250.f, 5.f), Settings, Ahead(100.f));
		TestEqual("variacao [m/s]", R.CarDeltaVMs, (100.f - 85.714f) / 3.6f, 0.01f);
		TestEqual("pontos", R.Points, 50);
		TestEqual("sem quebra, sem pontos", FDNBreakable::Hit(Kind(250.f), Settings, Ahead(10.f)).Points, 0);
		TestEqual("sem quebra, sem variacao", FDNBreakable::Hit(Kind(250.f), Settings, Ahead(10.f)).CarDeltaVMs, 0.f);
	});

	It("B6: entradas invalidas usam valores seguros", [this]()
	{
		FDNBreakInput NaNSpeed = Ahead(100.f);
		NaNSpeed.CarVelocityCmS.X = NaNf;
		TestFalse("velocidade NaN nao quebra", FDNBreakable::Hit(Kind(250.f), Settings, NaNSpeed).bBreaks);
		FDNBreakInput NoMass = Ahead(100.f);
		NoMass.CarMassKg = 0.f;
		const float DefaultMass = FDNCarConfig().MassKg;
		TestEqual("massa do carro invalida vale a do carro padrao", Kmh(FDNBreakable::Hit(Kind(250.f, 5.f), Settings, NoMass).CarVelocityCmS),
			100.f * DefaultMass / (DefaultMass + 250.f), 0.02f);
		TestEqual("peca sem massa vale o padrao do tipo", Kmh(FDNBreakable::Hit(Kind(NaNf, 5.f), Settings, Ahead(100.f)).CarVelocityCmS),
			100.f * 1500.f / (1500.f + FDNBreakableKind().MassKg), 0.02f);
		TestEqual("direcao nula vale de frente", Kmh(FDNBreakable::Hit(Kind(250.f, 5.f), Settings, Ahead(100.f, FVector::ZeroVector)).CarVelocityCmS), 85.71f, 0.02f);
		Settings.ArcadeMassFactor = NaNf;
		Settings.MaxSpeedLossPct = -3.f;
		TestEqual("k e teto invalidos valem o padrao", Kmh(FDNBreakable::Hit(Kind(250.f, 5.f), Settings, Ahead(100.f)).CarVelocityCmS), 85.71f, 0.02f);
	});

	It("B7: batida em peca nunca conta como batida do jogador nem dispara a camera de batida", [this]()
	{
		TestFalse("peca: rodada de Crash", FDNImpactPolicy::CountsForCrashRound(EDNImpactSource::Prop));
		TestFalse("peca: camera de batida", FDNImpactPolicy::TriggersCrashCam(EDNImpactSource::Prop));
		TestTrue("trafego: rodada de Crash", FDNImpactPolicy::CountsForCrashRound(EDNImpactSource::Traffic));
		TestTrue("trafego: camera de batida", FDNImpactPolicy::TriggersCrashCam(EDNImpactSource::Traffic));
		TestTrue("parede: camera de batida", FDNImpactPolicy::TriggersCrashCam(EDNImpactSource::World));
		TestFalse("parede: rodada de Crash", FDNImpactPolicy::CountsForCrashRound(EDNImpactSource::World));
	});

	It("B8: a peca dorme depois de SleepAfterS seguidos abaixo de SleepSpeedMs", [this]()
	{
		float Still = 0.f;
		TestFalse("rapida", FDNBreakable::StepSleep(Settings, 5.f, 0.5f, Still));
		TestFalse("devagar 0,5 s", FDNBreakable::StepSleep(Settings, 0.1f, 0.5f, Still));
		TestFalse("mexeu: zera", FDNBreakable::StepSleep(Settings, 1.f, 0.1f, Still));
		TestEqual("zerou", Still, 0.f);
		TestFalse("devagar 0,9 s", FDNBreakable::StepSleep(Settings, 0.1f, 0.9f, Still));
		TestTrue("devagar 1,0 s: dorme", FDNBreakable::StepSleep(Settings, 0.1f, 0.1f, Still));
		Still = 0.9f;
		TestFalse("Dt invalido nao conta", FDNBreakable::StepSleep(Settings, 0.f, NaNf, Still));
		TestEqual("Dt invalido nao estraga o tempo parado", Still, 0.9f);
		TestTrue("e o proximo passo valido ainda dorme", FDNBreakable::StepSleep(Settings, 0.f, 0.1f, Still));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
