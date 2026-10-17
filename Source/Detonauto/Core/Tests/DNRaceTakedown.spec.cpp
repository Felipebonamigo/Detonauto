// Testes do rival derrubado (W1 a W4): fica destroçado por um tempo, volta uma vez, derrubar de novo não reinicia a contagem.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNRaceTakedown.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNRaceTakedownSpec, "Detonauto.CorridaTakedown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
END_DEFINE_SPEC(FDNRaceTakedownSpec)

void FDNRaceTakedownSpec::Define()
{
	Describe("Rival batido (W1)", [this]()
	{
		It("o dano total subiu o minimo desde o contato", [this]()
		{
			TestTrue("exato 0,25", FDNRaceTakedown::RivalCrashed(0.1f, 0.35f, false, 0.25f));
			TestTrue("mais", FDNRaceTakedown::RivalCrashed(0.0f, 0.6f, false, 0.25f));
			TestFalse("menos", FDNRaceTakedown::RivalCrashed(0.1f, 0.34f, false, 0.25f));
			TestFalse("sem ganho", FDNRaceTakedown::RivalCrashed(0.3f, 0.3f, false, 0.25f));
		});

		It("destruido vale mesmo sem ganho medido", [this]()
		{
			TestTrue("destruido", FDNRaceTakedown::RivalCrashed(0.5f, 0.5f, true, 0.25f));
		});

		It("dano que desce (carro reposto) nao conta como ganho", [this]()
		{
			TestFalse("desceu", FDNRaceTakedown::RivalCrashed(0.6f, 0.0f, false, 0.25f));
		});

		It("valor nao finito nao conta", [this]()
		{
			TestFalse("antes NaN", FDNRaceTakedown::RivalCrashed(NaNf, 0.9f, false, 0.25f));
			TestFalse("agora NaN", FDNRaceTakedown::RivalCrashed(0.0f, NaNf, false, 0.25f));
			TestFalse("minimo NaN", FDNRaceTakedown::RivalCrashed(0.0f, 0.9f, false, NaNf));
			TestFalse("antes -infinito", FDNRaceTakedown::RivalCrashed(-std::numeric_limits<float>::infinity(), 0.1f, false, 0.25f));
			TestFalse("agora infinito", FDNRaceTakedown::RivalCrashed(0.0f, std::numeric_limits<float>::infinity(), false, 0.25f));
			TestFalse("minimo -infinito", FDNRaceTakedown::RivalCrashed(0.0f, 0.1f, false, -std::numeric_limits<float>::infinity()));
		});
	});

	Describe("Rival destrocado (W2 a W4)", [this]()
	{
		It("sem derrubar, nada acontece", [this]()
		{
			FDNWreckState S;
			TestFalse("volta", FDNRaceTakedown::Step(S, 1.f, false, 3.f));
			TestFalse("destrocado", S.bWrecked);
		});

		It("derrubado fica destrocado pelo tempo e volta uma vez", [this]()
		{
			FDNWreckState S;
			TestFalse("no quadro do takedown nao volta", FDNRaceTakedown::Step(S, 0.f, true, 3.f));
			TestTrue("destrocado", S.bWrecked);
			TestFalse("1 s depois", FDNRaceTakedown::Step(S, 1.f, false, 3.f));
			TestFalse("2 s depois", FDNRaceTakedown::Step(S, 1.f, false, 3.f));
			TestTrue("volta aos 3 s", FDNRaceTakedown::Step(S, 1.f, false, 3.f));
			TestFalse("depois de voltar nao esta mais destrocado", S.bWrecked);
			TestFalse("nao volta de novo", FDNRaceTakedown::Step(S, 1.f, false, 3.f));
		});

		It("um quadro grande passa do tempo e volta", [this]()
		{
			FDNWreckState S;
			FDNRaceTakedown::Step(S, 0.f, true, 3.f);
			TestTrue("pulo de 10 s", FDNRaceTakedown::Step(S, 10.f, false, 3.f));
		});

		It("derrubar de novo enquanto destrocado nao reinicia a contagem", [this]()
		{
			FDNWreckState S;
			FDNRaceTakedown::Step(S, 0.f, true, 3.f);
			FDNRaceTakedown::Step(S, 2.f, false, 3.f);
			TestFalse("novo takedown aos 2 s", FDNRaceTakedown::Step(S, 0.f, true, 3.f));
			TestTrue("ainda volta aos 3 s do primeiro", FDNRaceTakedown::Step(S, 1.f, false, 3.f));
		});

		It("tempo de quadro negativo ou NaN nao anda a contagem", [this]()
		{
			FDNWreckState S;
			FDNRaceTakedown::Step(S, 0.f, true, 3.f);
			TestFalse("negativo", FDNRaceTakedown::Step(S, -5.f, false, 3.f));
			TestFalse("NaN", FDNRaceTakedown::Step(S, NaNf, false, 3.f));
			TestEqual("a contagem fica em 3 s", S.LeftS, 3.f);
		});

		It("tempo de destroco zero ou invalido volta no quadro seguinte", [this]()
		{
			FDNWreckState S;
			FDNRaceTakedown::Step(S, 0.f, true, 0.f);
			TestFalse("quadro de tempo zero nao anda a contagem", FDNRaceTakedown::Step(S, 0.f, false, 0.f));
			TestTrue("zero: volta no proximo quadro", FDNRaceTakedown::Step(S, 0.016f, false, 0.f));
			FDNWreckState N;
			FDNRaceTakedown::Step(N, 0.f, true, NaNf);
			TestTrue("NaN: volta no proximo quadro", FDNRaceTakedown::Step(N, 0.016f, false, NaNf));
		});
	});
}

#endif
