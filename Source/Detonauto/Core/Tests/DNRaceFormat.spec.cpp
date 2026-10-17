// Testes dos textos da corrida (F1 a F3): tempo truncado em m:ss.d, valores inválidos, posição.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNRaceFormat.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNRaceFormatSpec, "Detonauto.CorridaTexto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	const float Inf = std::numeric_limits<float>::infinity();
END_DEFINE_SPEC(FDNRaceFormatSpec)

void FDNRaceFormatSpec::Define()
{
	Describe("Tempo (F1, F2)", [this]()
	{
		It("m:ss.d", [this]()
		{
			TestEqual("zero", FDNRaceFormat::Time(0.f), FString(TEXT("0:00.0")));
			TestEqual("5,3 s", FDNRaceFormat::Time(5.3f), FString(TEXT("0:05.3")));
			TestEqual("80,9 s", FDNRaceFormat::Time(80.9f), FString(TEXT("1:20.9")));
			TestEqual("1 h menos 0,1", FDNRaceFormat::Time(3599.95f), FString(TEXT("59:59.9")));
		});

		It("trunca e nao arredonda", [this]()
		{
			TestEqual("5,99 s", FDNRaceFormat::Time(5.99f), FString(TEXT("0:05.9")));
			TestEqual("59,99 s", FDNRaceFormat::Time(59.99f), FString(TEXT("0:59.9")));
			TestEqual("119,96 s", FDNRaceFormat::Time(119.96f), FString(TEXT("1:59.9")));
		});

		It("negativo ou nao finito vira tracos; muito longo prende em 99:59.9", [this]()
		{
			TestEqual("negativo", FDNRaceFormat::Time(-1.f), FString(TEXT("--:--.-")));
			TestEqual("NaN", FDNRaceFormat::Time(NaNf), FString(TEXT("--:--.-")));
			TestEqual("infinito", FDNRaceFormat::Time(Inf), FString(TEXT("--:--.-")));
			TestEqual("100 min", FDNRaceFormat::Time(6000.f), FString(TEXT("99:59.9")));
			TestEqual("1 milhao de s", FDNRaceFormat::Time(1.0e6f), FString(TEXT("99:59.9")));
		});
	});

	Describe("Posicao (F3)", [this]()
	{
		It("N com o ordinal", [this]()
		{
			TestEqual("1", FDNRaceFormat::Place(1), FString(TEXT("1º")));
			TestEqual("4", FDNRaceFormat::Place(4), FString(TEXT("4º")));
			TestEqual("99", FDNRaceFormat::Place(99), FString(TEXT("99º")));
		});

		It("fora de 1 a 99 vira traco", [this]()
		{
			TestEqual("0", FDNRaceFormat::Place(0), FString(TEXT("-")));
			TestEqual("-3", FDNRaceFormat::Place(-3), FString(TEXT("-")));
			TestEqual("100", FDNRaceFormat::Place(100), FString(TEXT("-")));
		});
	});
}

#endif
