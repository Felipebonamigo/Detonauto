// Testes da corrida ponto a ponto (docs/fatia-jogavel.md): progresso que nunca desce, chegada inclusiva e uma vez só, ordem e posição.
// Decisões R1 a R5: Core/DNRace.h.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNRace.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FDNRacerState Racer(float ProgressCm, bool bFinished = false, float FinishTimeS = 0.0f)
	{
		FDNRacerState R;
		R.ProgressCm = ProgressCm;
		R.bFinished = bFinished;
		R.FinishTimeS = FinishTimeS;
		return R;
	}
}

BEGIN_DEFINE_SPEC(FDNRaceSpec, "Detonauto.Corrida",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	const float Inf = std::numeric_limits<float>::infinity();
END_DEFINE_SPEC(FDNRaceSpec)

void FDNRaceSpec::Define()
{
	Describe("Progresso e chegada (R1 a R3)", [this]()
	{
		It("o progresso sobe e nunca desce", [this]()
		{
			FDNRacerState R;
			FDNRace::Step(R, 1000.f, 1.f, 50000.f);
			TestEqual("subiu", R.ProgressCm, 1000.f);
			FDNRace::Step(R, 400.f, 2.f, 50000.f);
			TestEqual("voltou: fica", R.ProgressCm, 1000.f);
			FDNRace::Step(R, 1000.f, 3.f, 50000.f);
			TestEqual("igual: fica", R.ProgressCm, 1000.f);
			FDNRace::Step(R, 1500.f, 4.f, 50000.f);
			TestEqual("subiu de novo", R.ProgressCm, 1500.f);
		});

		It("progresso nao finito ou negativo e ignorado", [this]()
		{
			FDNRacerState R = Racer(800.f);
			FDNRace::Step(R, NaNf, 1.f, 50000.f);
			FDNRace::Step(R, Inf, 1.f, 50000.f);
			FDNRace::Step(R, -5.f, 1.f, 50000.f);
			TestEqual("o anterior vale", R.ProgressCm, 800.f);
			TestFalse("nao chegou", R.bFinished);
		});

		It("chega quando alcanca o comprimento (inclusivo), e o tempo e gravado uma vez", [this]()
		{
			FDNRacerState R;
			TestFalse("49 999,9: nao chegou", FDNRace::Step(R, 49999.9f, 10.f, 50000.f));
			TestFalse("ainda na pista", R.bFinished);
			TestTrue("50 000: chegou agora", FDNRace::Step(R, 50000.f, 11.5f, 50000.f));
			TestTrue("marcado", R.bFinished);
			TestEqual("tempo", R.FinishTimeS, 11.5f);
			TestFalse("depois nao chega de novo", FDNRace::Step(R, 51000.f, 14.f, 50000.f));
			TestEqual("o tempo nao muda", R.FinishTimeS, 11.5f);
		});

		It("passou do comprimento num quadro so: chegou", [this]()
		{
			FDNRacerState R = Racer(49000.f);
			TestTrue("pulo", FDNRace::Step(R, 52000.f, 7.f, 50000.f));
			TestEqual("tempo", R.FinishTimeS, 7.f);
		});

		It("comprimento invalido: ninguem chega", [this]()
		{
			for (const float Length : { 0.f, -100.f, NaNf, Inf })
			{
				FDNRacerState R;
				TestFalse(FString::Printf(TEXT("comprimento %f"), Length), FDNRace::Step(R, 1.0e6f, 5.f, Length));
				TestFalse("sem chegada", R.bFinished);
			}
		});
	});

	Describe("Ordem e posicao (R4, R5)", [this]()
	{
		It("quem chegou vem antes, por tempo; os outros por progresso", [this]()
		{
			const TArray<FDNRacerState> R = { Racer(30000.f), Racer(50000.f, true, 40.f), Racer(45000.f), Racer(50000.f, true, 38.f) };
			const TArray<int32> Order = FDNRace::Order(R);
			TestTrue("3, 1, 2, 0", Order.Num() == 4 && Order[0] == 3 && Order[1] == 1 && Order[2] == 2 && Order[3] == 0);
			TestEqual("posicao do 3", FDNRace::Position(R, 3), 1);
			TestEqual("posicao do 1", FDNRace::Position(R, 1), 2);
			TestEqual("posicao do 2", FDNRace::Position(R, 2), 3);
			TestEqual("posicao do 0", FDNRace::Position(R, 0), 4);
		});

		It("empate fica com o indice menor", [this]()
		{
			const TArray<FDNRacerState> R = { Racer(100.f), Racer(100.f), Racer(100.f, true, 9.f), Racer(100.f, true, 9.f) };
			const TArray<int32> Order = FDNRace::Order(R);
			TestTrue("2, 3, 0, 1", Order.Num() == 4 && Order[0] == 2 && Order[1] == 3 && Order[2] == 0 && Order[3] == 1);
		});

		It("indice fora da lista: posicao 0; lista vazia: ordem vazia", [this]()
		{
			const TArray<FDNRacerState> R = { Racer(1.f) };
			TestEqual("indice 5", FDNRace::Position(R, 5), 0);
			TestEqual("indice -1", FDNRace::Position(R, -1), 0);
			TestEqual("vazia", FDNRace::Order(TArray<FDNRacerState>()).Num(), 0);
		});

		It("um corredor so e o primeiro", [this]()
		{
			const TArray<FDNRacerState> R = { Racer(0.f) };
			TestEqual("posicao", FDNRace::Position(R, 0), 1);
		});
	});
}

#endif
