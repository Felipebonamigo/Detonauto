// Testes do carro (lote 12): a rampa do freio de mao e as relacoes entre os presets de Data/Cars.csv que o Felipe pediu
// (arcade-1 mais leve e macio, arcade-2 com traseira que escorrega, arcade-3 no meio, camera mais longe que a do Padrao).
// Portados do protótipo (Sandbox, DNCarTuning.spec.cpp), agora sobre o CSV real lido por FDNDataLoader.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNCar.h"
#include "Core/Data/DNDataLoader.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNCarSpec, "Detonauto.Carro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNData Data;
	const FDNCarConfigRow* Find(const TCHAR* Id) const
	{
		const FDNCarConfigEntry* E = Data.CarConfigs.FindByPredicate([Id](const FDNCarConfigEntry& X) { return X.Id.Equals(Id, ESearchCase::IgnoreCase); });
		return E ? &E->Row : nullptr;
	}
END_DEFINE_SPEC(FDNCarSpec)

void FDNCarSpec::Define()
{
	Describe("Rampa do freio de mao", [this]()
	{
		It("sobe linearmente ate 1 em InSeconds e desce ate 0 em OutSeconds", [this]()
		{
			float Ramp = 0.f;
			Ramp = FDNHandbrakeRamp::Step(Ramp, true, 0.05f, 0.1f, 0.5f);
			TestEqual("meio caminho", Ramp, 0.5f);
			Ramp = FDNHandbrakeRamp::Step(Ramp, true, 0.05f, 0.1f, 0.5f);
			TestEqual("chegou", Ramp, 1.f);
			Ramp = FDNHandbrakeRamp::Step(Ramp, true, 1.f, 0.1f, 0.5f);
			TestEqual("nao passa de 1", Ramp, 1.f);
			Ramp = FDNHandbrakeRamp::Step(Ramp, false, 0.25f, 0.1f, 0.5f);
			TestEqual("solta: meio caminho", Ramp, 0.5f);
			Ramp = FDNHandbrakeRamp::Step(Ramp, false, 0.25f, 0.1f, 0.5f);
			TestEqual("solta: zero", Ramp, 0.f);
			Ramp = FDNHandbrakeRamp::Step(Ramp, false, 1.f, 0.1f, 0.5f);
			TestEqual("nao passa de 0", Ramp, 0.f);
		});

		It("tempo invalido (zero, negativo, NaN) nao mexe; duracao zero vira salto", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			TestEqual("dt 0", FDNHandbrakeRamp::Step(0.3f, true, 0.f, 0.1f, 0.5f), 0.3f);
			TestEqual("dt negativo", FDNHandbrakeRamp::Step(0.3f, true, -1.f, 0.1f, 0.5f), 0.3f);
			TestEqual("dt NaN", FDNHandbrakeRamp::Step(0.3f, true, NaN, 0.1f, 0.5f), 0.3f);
			TestEqual("atual NaN vira 0", FDNHandbrakeRamp::Step(NaN, false, 0.1f, 0.1f, 0.5f), 0.f);
			TestEqual("sobe instantaneo com duracao 0", FDNHandbrakeRamp::Step(0.f, true, 0.001f, 0.f, 0.5f), 1.f);
			TestEqual("desce instantaneo com duracao 0", FDNHandbrakeRamp::Step(1.f, false, 0.001f, 0.1f, 0.f), 0.f);
		});

		It("multiplicador da aderencia traseira: 1 sem freio de mao, o valor do preset com a rampa cheia, e no meio a media", [this]()
		{
			TestEqual("rampa 0", FDNHandbrakeRamp::RearGripMultiplier(0.35f, 0.f), 1.f);
			TestEqual("rampa 1", FDNHandbrakeRamp::RearGripMultiplier(0.35f, 1.f), 0.35f);
			TestEqual("rampa 0,5", FDNHandbrakeRamp::RearGripMultiplier(0.35f, 0.5f), 0.675f);
			TestEqual("preset 1 nao muda nada", FDNHandbrakeRamp::RearGripMultiplier(1.f, 1.f), 1.f);
			TestEqual("rampa fora de 0..1 e limitada", FDNHandbrakeRamp::RearGripMultiplier(0.35f, 7.f), 0.35f);
		});
	});

	Describe("Presets de Data/Cars.csv", [this]()
	{
		BeforeEach([this]()
		{
			Data = FDNData();
			FDNDataLoader::LoadAll(Data);
		});

		It("a linha Padrao e o carro do template: nada das colunas de afinacao muda o comportamento", [this]()
		{
			const FDNCarConfigRow* P = Find(TEXT("Padrao"));
			if (!TestNotNull("Padrao", P)) { return; }
			TestEqual("massa", P->MassKg, 1500.f);
			TestEqual("aderencia traseira", P->RearGripFactor, 1.f);
			TestEqual("freio de mao: aderencia", P->HandbrakeRearGrip, 1.f);
			TestEqual("COM", P->CenterOfMassDropCm, 0.f);
		});

		It("arcade-1 e mais leve, de centro de massa mais baixo, suspensao mais macia com mais curso e barra mais fraca que o Padrao", [this]()
		{
			const FDNCarConfigRow* P = Find(TEXT("Padrao"));
			const FDNCarConfigRow* A = Find(TEXT("arcade-1"));
			if (!TestNotNull("Padrao", P) || !TestNotNull("arcade-1", A)) { return; }
			TestTrue("massa menor", A->MassKg < P->MassKg);
			TestTrue("centro de massa mais baixo", A->CenterOfMassDropCm > P->CenterOfMassDropCm);
			TestTrue("mola mais macia", A->SuspensionStiffness < P->SuspensionStiffness);
			TestTrue("mais curso", A->SuspensionTravelCm > P->SuspensionTravelCm);
			TestTrue("barra estabilizadora mais fraca", A->RollbarScaling < P->RollbarScaling);
			TestEqual("a traseira nao e mexida", A->RearGripFactor, 1.f);
		});

		It("arcade-2 e o arcade-1 com traseira que escorrega e freio de mao que solta a traseira (com rampa de volta)", [this]()
		{
			const FDNCarConfigRow* A1 = Find(TEXT("arcade-1"));
			const FDNCarConfigRow* A2 = Find(TEXT("arcade-2"));
			if (!TestNotNull("arcade-1", A1) || !TestNotNull("arcade-2", A2)) { return; }
			TestEqual("mesma massa", A2->MassKg, A1->MassKg);
			TestEqual("mesmo centro de massa", A2->CenterOfMassDropCm, A1->CenterOfMassDropCm);
			TestEqual("mesma mola", A2->SuspensionStiffness, A1->SuspensionStiffness);
			TestEqual("mesmo curso", A2->SuspensionTravelCm, A1->SuspensionTravelCm);
			TestEqual("mesma barra", A2->RollbarScaling, A1->RollbarScaling);
			TestTrue("aderencia traseira menor", A2->RearGripFactor < A1->RearGripFactor);
			TestTrue("o freio de mao tira aderencia da traseira", A2->HandbrakeRearGrip < 1.f);
			TestTrue("a rampa de volta e mais lenta que a de ida (recupera suave)", A2->HandbrakeRampOutSeconds > A2->HandbrakeRampInSeconds);
		});

		It("arcade-3 fica entre o arcade-1 e o arcade-2 na aderencia traseira, tem o mesmo freio de mao do arcade-2, e fica entre o Padrao e o arcade-1 na massa", [this]()
		{
			const FDNCarConfigRow* P = Find(TEXT("Padrao"));
			const FDNCarConfigRow* A1 = Find(TEXT("arcade-1"));
			const FDNCarConfigRow* A2 = Find(TEXT("arcade-2"));
			const FDNCarConfigRow* A3 = Find(TEXT("arcade-3"));
			if (!TestNotNull("Padrao", P) || !TestNotNull("arcade-1", A1) || !TestNotNull("arcade-2", A2) || !TestNotNull("arcade-3", A3)) { return; }
			TestTrue("massa entre Padrao e arcade-1", A3->MassKg > A1->MassKg && A3->MassKg < P->MassKg);
			TestTrue("aderencia traseira entre arcade-2 e arcade-1", A3->RearGripFactor > A2->RearGripFactor && A3->RearGripFactor < A1->RearGripFactor);
			TestEqual("freio de mao igual ao do arcade-2 (a diferenca esta na traseira)", A3->HandbrakeRearGrip, A2->HandbrakeRearGrip);
			TestTrue("freio de mao solta a traseira", A3->HandbrakeRearGrip < 1.f);
			TestTrue("mola entre arcade-1 e Padrao", A3->SuspensionStiffness > A1->SuspensionStiffness && A3->SuspensionStiffness < P->SuspensionStiffness);
		});

		It("a camera dos presets e pelo menos 20% mais longe e mais alta que a do Padrao", [this]()
		{
			const FDNCarConfigRow* P = Find(TEXT("Padrao"));
			if (!TestNotNull("Padrao", P)) { return; }
			for (const TCHAR* Name : { TEXT("arcade-1"), TEXT("arcade-2"), TEXT("arcade-3") })
			{
				const FDNCarConfigRow* R = Find(Name);
				if (!TestNotNull(Name, R)) { continue; }
				TestTrue(FString::Printf(TEXT("%s: distancia"), Name), R->CameraDistanceCm >= P->CameraDistanceCm * 1.2f);
				TestTrue(FString::Printf(TEXT("%s: altura"), Name), R->CameraHeightCm > P->CameraHeightCm);
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS