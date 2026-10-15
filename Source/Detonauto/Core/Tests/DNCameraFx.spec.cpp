// Testes da câmera de velocidade e de batida (lote 18, docs/regras-camera.md). Puro: sem mundo. Todo tempo é tempo real.
//
// Decisões (as do contrato, mais as escolhas que ele deixou abertas):
//  F1 campo de visão: Base + 8 x S(v) x SpeedFovScale + 12 x TurboBlend, no máximo 115; S = smoothstep de 60 a 200 km/h;
//     velocidade NaN ou negativa conta como 0;
//  F2 o mostrado anda até o alvo a no máximo 30 graus/s; RealDt inválido não mexe;
//  S1 tremor: amplitude 0,35 x S(v) x ShakeScale x GlobalScale, S de 150 a 250 km/h, exatamente 0 abaixo de 150; Perlin 1D
//     (FMath::PerlinNoise1D, determinístico) em ShakeHz, eixos defasados; nunca passa da amplitude;
//  B1 desfoque nas bordas: 0,5 x S(v) de 100 a 250 km/h;
//  C1 câmera de batida: dispara com DeltaV >= 15 m/s de fonte que conta (mobiliário nunca), só em Idle;
//  C2 escala de tempo e peso: Entering desce de 1 a 0,25 em 0,15 s com saída suave (1 - (1 - u)^2: no meio, 0,4375 e peso 0,75);
//     Holding 1,5 s em 0,25 e peso 1; Exiting volta a 1 em 0,4 s com smoothstep (no meio, 0,625 e peso 0,5); Cooldown 2 s com
//     escala 1 e peso 0, sem disparar; a sobra de um passo passa para a fase seguinte;
//  W1 controle do destroço: só no Entering e no Holding; perpendicular à velocidade horizontal, para o lado da tela pedido;
//     6 m/s² x |entrada| x RealDt, no máximo 6 m/s por batida; sem velocidade, vale a direita da tela; inválido, zero.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNCameraFx.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNCameraFxSpec, "Detonauto.Camera.Fx",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	FDNCameraFxSettings S;

	/** Avança a câmera de batida e confere a fase, a escala e o peso. */
	void Expect(FDNCrashCamState& State, float Dt, EDNCrashCamPhase Phase, float Scale, float Weight, const TCHAR* What)
	{
		FDNCrashCam::Step(S, State, Dt);
		TestEqual(FString::Printf(TEXT("%s: fase"), What), static_cast<int32>(State.Phase), static_cast<int32>(Phase));
		TestEqual(FString::Printf(TEXT("%s: escala"), What), FDNCrashCam::TimeScale(S, State), Scale, 1e-3f);
		TestEqual(FString::Printf(TEXT("%s: peso"), What), FDNCrashCam::BlendWeight(S, State), Weight, 1e-3f);
	}
END_DEFINE_SPEC(FDNCameraFxSpec)

void FDNCameraFxSpec::Define()
{
	BeforeEach([this]() { S = FDNCameraFxSettings(); });

	It("F1: o campo de visao abre com a velocidade e o turbo, no teto de 115", [this]()
	{
		TestEqual("abaixo de 60 km/h: o do modo", FDNSpeedFov::Target(S, 95.f, 59.f, 1.f, 0.f), 95.f, 1e-3f);
		TestEqual("130 km/h: metade do caminho (smoothstep 0,5)", FDNSpeedFov::Target(S, 95.f, 130.f, 1.f, 0.f), 99.f, 1e-3f);
		TestEqual("200 km/h: soma 8", FDNSpeedFov::Target(S, 95.f, 200.f, 1.f, 0.f), 103.f, 1e-3f);
		TestEqual("300 km/h: continua 8", FDNSpeedFov::Target(S, 95.f, 300.f, 1.f, 0.f), 103.f, 1e-3f);
		TestEqual("motorista (0,5): soma 4", FDNSpeedFov::Target(S, 95.f, 200.f, 0.5f, 0.f), 99.f, 1e-3f);
		TestEqual("turbo cheio: soma 12", FDNSpeedFov::Target(S, 95.f, 0.f, 1.f, 1.f), 107.f, 1e-3f);
		TestEqual("teto de 115", FDNSpeedFov::Target(S, 100.f, 250.f, 1.f, 1.f), 115.f, 1e-3f);
		TestEqual("velocidade NaN vale 0", FDNSpeedFov::Target(S, 95.f, NaNf, 1.f, 0.f), 95.f, 1e-3f);
		TestEqual("velocidade negativa vale 0", FDNSpeedFov::Target(S, 95.f, -100.f, 1.f, 0.f), 95.f, 1e-3f);
	});

	It("F2: o campo de visao mostrado anda no maximo 30 graus/s, sem passar do alvo", [this]()
	{
		TestEqual("sobe 3 graus em 0,1 s", FDNSpeedFov::Step(S, 95.f, 120.f, 0.1f), 98.f, 1e-3f);
		TestEqual("desce 3 graus em 0,1 s", FDNSpeedFov::Step(S, 120.f, 95.f, 0.1f), 117.f, 1e-3f);
		TestEqual("chega no alvo sem passar", FDNSpeedFov::Step(S, 95.f, 96.f, 0.1f), 96.f, 1e-3f);
		TestEqual("Dt NaN nao mexe", FDNSpeedFov::Step(S, 95.f, 120.f, NaNf), 95.f, 1e-3f);
		TestEqual("Dt negativo nao mexe", FDNSpeedFov::Step(S, 95.f, 120.f, -1.f), 95.f, 1e-3f);
	});

	It("S1: o tremor e zero abaixo de 150 km/h e nunca passa da amplitude", [this]()
	{
		TestEqual("amplitude a 150", FDNSpeedShake::Amplitude(S, 150.f, 1.f, 1.f), 0.f);
		TestEqual("amplitude a 250", FDNSpeedShake::Amplitude(S, 250.f, 1.f, 1.f), 0.35f, 1e-4f);
		TestEqual("amplitude no capo (1,2)", FDNSpeedShake::Amplitude(S, 250.f, 1.2f, 1.f), 0.42f, 1e-4f);
		TestEqual("desligado no menu", FDNSpeedShake::Amplitude(S, 250.f, 1.f, 0.f), 0.f);
		bool bMoved = false;
		for (int32 i = 0; i < 200; ++i)
		{
			const double T = i * 0.0137;
			TestEqual("abaixo de 150 e exatamente zero", FDNSpeedShake::Offset(S, 149.f, 1.f, 1.f, T), FVector2D::ZeroVector);
			const FVector2D O = FDNSpeedShake::Offset(S, 250.f, 1.f, 1.f, T);
			TestTrue("nunca passa da amplitude", FMath::Abs(O.X) <= 0.35 + 1e-4 && FMath::Abs(O.Y) <= 0.35 + 1e-4);
			bMoved |= FMath::Abs(O.X) > 0.05 && FMath::Abs(O.Y) > 0.05;
			TestEqual("deterministico", FDNSpeedShake::Offset(S, 250.f, 1.f, 1.f, T), O);
		}
		TestTrue("a 250 km/h treme nos dois eixos", bMoved);
		const FVector2D A = FDNSpeedShake::Offset(S, 250.f, 1.f, 1.f, 0.37);
		TestTrue("eixos defasados (arfagem e guinada diferentes)", !FMath::IsNearlyEqual(A.X, A.Y, 1e-3));
	});

	It("B1: o desfoque nas bordas sobe de 100 a 250 km/h ate 0,5", [this]()
	{
		TestEqual("100 km/h", FDNEdgeBlur::Strength(S, 100.f), 0.f, 1e-4f);
		TestEqual("175 km/h", FDNEdgeBlur::Strength(S, 175.f), 0.25f, 1e-4f);
		TestEqual("250 km/h", FDNEdgeBlur::Strength(S, 250.f), 0.5f, 1e-4f);
		TestEqual("NaN", FDNEdgeBlur::Strength(S, NaNf), 0.f, 1e-4f);
	});

	It("C1: a camera de batida dispara com 15 m/s de fonte que conta, e so em Idle", [this]()
	{
		FDNCrashCamState St;
		TestFalse("14,99 m/s nao", FDNCrashCam::TryTrigger(S, St, 14.99f, EDNImpactSource::World));
		TestFalse("mobiliario nunca", FDNCrashCam::TryTrigger(S, St, 40.f, EDNImpactSource::Prop));
		TestFalse("NaN nao", FDNCrashCam::TryTrigger(S, St, NaNf, EDNImpactSource::World));
		TestEqual("continua em Idle", static_cast<int32>(St.Phase), static_cast<int32>(EDNCrashCamPhase::Idle));
		TestTrue("15 m/s na parede", FDNCrashCam::TryTrigger(S, St, 15.f, EDNImpactSource::World));
		TestEqual("Entering", static_cast<int32>(St.Phase), static_cast<int32>(EDNCrashCamPhase::Entering));
		TestFalse("de novo durante a camera lenta: ignora", FDNCrashCam::TryTrigger(S, St, 30.f, EDNImpactSource::Traffic));
		FDNCrashCamState Tr;
		TestTrue("trafego tambem", FDNCrashCam::TryTrigger(S, Tr, 20.f, EDNImpactSource::Traffic));
	});

	It("C2: escala de tempo e peso nos instantes de borda e no meio de cada fase, e a espera", [this]()
	{
		FDNCrashCamState St;
		FDNCrashCam::TryTrigger(S, St, 20.f, EDNImpactSource::World);
		Expect(St, 0.f, EDNCrashCamPhase::Entering, 1.f, 0.f, TEXT("0 s"));
		Expect(St, 0.075f, EDNCrashCamPhase::Entering, 0.4375f, 0.75f, TEXT("0,075 s"));
		Expect(St, 0.075f, EDNCrashCamPhase::Holding, 0.25f, 1.f, TEXT("0,15 s"));
		Expect(St, 0.75f, EDNCrashCamPhase::Holding, 0.25f, 1.f, TEXT("0,9 s"));
		Expect(St, 0.75f, EDNCrashCamPhase::Exiting, 0.25f, 1.f, TEXT("1,65 s"));
		Expect(St, 0.2f, EDNCrashCamPhase::Exiting, 0.625f, 0.5f, TEXT("1,85 s"));
		Expect(St, 0.2f, EDNCrashCamPhase::Cooldown, 1.f, 0.f, TEXT("2,05 s"));
		TestFalse("na espera nao dispara", FDNCrashCam::TryTrigger(S, St, 30.f, EDNImpactSource::World));
		Expect(St, 1.98f, EDNCrashCamPhase::Cooldown, 1.f, 0.f, TEXT("4,03 s"));
		TestFalse("ainda na espera", FDNCrashCam::TryTrigger(S, St, 30.f, EDNImpactSource::World));
		Expect(St, 0.03f, EDNCrashCamPhase::Idle, 1.f, 0.f, TEXT("4,06 s"));
		TestTrue("depois da espera dispara de novo", FDNCrashCam::TryTrigger(S, St, 30.f, EDNImpactSource::World));
	});

	It("C2: um passo grande atravessa as fases com a sobra, e Dt invalido nao mexe", [this]()
	{
		FDNCrashCamState St;
		FDNCrashCam::TryTrigger(S, St, 20.f, EDNImpactSource::World);
		Expect(St, 1.0f, EDNCrashCamPhase::Holding, 0.25f, 1.f, TEXT("1 s de uma vez"));
		TestEqual("sobra no Holding: 0,85 s", St.PhaseSeconds, 0.85f, 1e-3f);
		Expect(St, NaNf, EDNCrashCamPhase::Holding, 0.25f, 1.f, TEXT("Dt NaN"));
		Expect(St, -1.f, EDNCrashCamPhase::Holding, 0.25f, 1.f, TEXT("Dt negativo"));
		Expect(St, 10.f, EDNCrashCamPhase::Idle, 1.f, 0.f, TEXT("10 s de uma vez"));
	});

	It("C2: o controle do destroco vale no Entering e no Holding", [this]()
	{
		FDNCrashCamState St;
		TestFalse("Idle", FDNCrashCam::SteerActive(St));
		FDNCrashCam::TryTrigger(S, St, 20.f, EDNImpactSource::World);
		TestTrue("Entering", FDNCrashCam::SteerActive(St));
		FDNCrashCam::Step(S, St, 0.2f);
		TestTrue("Holding", FDNCrashCam::SteerActive(St));
		FDNCrashCam::Step(S, St, 1.5f);
		TestFalse("Exiting", FDNCrashCam::SteerActive(St));
	});

	It("W1: o controle do destroco empurra para o lado pedido da tela, a 6 m/s2, no maximo 6 m/s por batida", [this]()
	{
		const FVector Car(2000.f, 0.f, -300.f); // 20 m/s para +X, caindo um pouco
		const FVector Right(0.f, 1.f, 0.f);     // a direita da tela e +Y (camera atras do carro)
		float Used = 0.f;
		const FVector V = FDNWreckSteer::DeltaVelocity(S, true, 1.f, Car, Right, 0.1f, Used);
		TestEqual("direita: 0,6 m/s para +Y", V, FVector(0.f, 60.f, 0.f), 1e-2f);
		TestEqual("usado 0,6 m/s", Used, 0.6f, 1e-4f);
		float UsedLeft = 0.f;
		TestEqual("esquerda: -Y", FDNWreckSteer::DeltaVelocity(S, true, -1.f, Car, Right, 0.1f, UsedLeft), FVector(0.f, -60.f, 0.f), 1e-2f);
		float UsedHalf = 0.f;
		TestEqual("meia entrada: metade", FDNWreckSteer::DeltaVelocity(S, true, 0.5f, Car, Right, 0.1f, UsedHalf), FVector(0.f, 30.f, 0.f), 1e-2f);
		float UsedFront = 0.f;
		TestEqual("camera na frente do carro (direita da tela = -Y): direita empurra para -Y",
			FDNWreckSteer::DeltaVelocity(S, true, 1.f, Car, FVector(0.f, -1.f, 0.f), 0.1f, UsedFront), FVector(0.f, -60.f, 0.f), 1e-2f);
		float UsedCap = 5.9f;
		TestEqual("teto: so o que sobra (0,1 m/s)", FDNWreckSteer::DeltaVelocity(S, true, 1.f, Car, Right, 0.1f, UsedCap).Size(), 10.0, 1e-2);
		TestEqual("teto: usado chega a 6", UsedCap, 6.f, 1e-4f);
		TestEqual("teto: depois, nada", FDNWreckSteer::DeltaVelocity(S, true, 1.f, Car, Right, 0.1f, UsedCap), FVector::ZeroVector);
		float UsedStill = 0.f;
		TestEqual("parado: vale a direita da tela", FDNWreckSteer::DeltaVelocity(S, true, 1.f, FVector::ZeroVector, Right, 0.1f, UsedStill),
			FVector(0.f, 60.f, 0.f), 1e-2f);
		float UsedOff = 0.f;
		TestEqual("fora da camera de batida: nada", FDNWreckSteer::DeltaVelocity(S, false, 1.f, Car, Right, 0.1f, UsedOff), FVector::ZeroVector);
		TestEqual("fora: nao gasta", UsedOff, 0.f);
		float UsedNaN = 0.f;
		TestEqual("entrada NaN: nada", FDNWreckSteer::DeltaVelocity(S, true, NaNf, Car, Right, 0.1f, UsedNaN), FVector::ZeroVector);
		TestEqual("Dt NaN: nada", FDNWreckSteer::DeltaVelocity(S, true, 1.f, Car, Right, NaNf, UsedNaN), FVector::ZeroVector);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
