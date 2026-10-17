// Testes de Data/Rivals.csv (marco 1b): validação por campo, ToConfig, o CSV real e o subsistema. Faixas: Core/Data/DNDataValidation.h.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/Data/DNDataLoader.h"
#include "Core/Data/DNDataValidation.h"
#include "Core/DNGameDataSubsystem.h"
#include "Core/Tests/DNTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDataRivalSpec, "Detonauto.Dados.Rivais",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	const float NaNf = std::numeric_limits<float>::quiet_NaN();

	static FString JoinErrors(const TArray<FString>& Errors) { return Errors.IsEmpty() ? FString(TEXT("(nenhum erro)")) : FString::Join(Errors, TEXT(" | ")); }

	static bool Cites(const TArray<FString>& Errors, const TCHAR* Needle)
	{
		for (const FString& E : Errors) { if (E.Contains(Needle)) { return true; } }
		return false;
	}

	void Bad(const TCHAR* What, const TCHAR* Needle, TFunction<void(TArray<FDNRivalEntry>&)> Tweak)
	{
		TArray<FDNRivalEntry> Rows = { DNTest::RivalEntry() };
		Tweak(Rows);
		const TArray<FString> Errors = FDNDataValidation::ValidateRivals(Rows);
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Needle, *JoinErrors(Errors)), Cites(Errors, Needle));
	}
END_DEFINE_SPEC(FDNDataRivalSpec)

void FDNDataRivalSpec::Define()
{
	Describe("Rivals.csv: validacao", [this]()
	{
		It("o rival valido passa; de 1 a 7 linhas", [this]()
		{
			TestEqual("1 rival", FDNDataValidation::ValidateRivals({ DNTest::RivalEntry() }).Num(), 0);
			TArray<FDNRivalEntry> Seven;
			for (int32 i = 0; i < 7; ++i) { Seven.Add(DNTest::RivalEntry(*FString::Printf(TEXT("r%d"), i))); }
			TestEqual("7 rivais", FDNDataValidation::ValidateRivals(Seven).Num(), 0);
		});

		It("vazia ou com 8 linhas e erro", [this]()
		{
			Bad(TEXT("vazia"), TEXT("Rivals.csv"), [](TArray<FDNRivalEntry>& R) { R.Reset(); });
			Bad(TEXT("8 linhas"), TEXT("no maximo 7"), [](TArray<FDNRivalEntry>& R) { for (int32 i = 0; i < 7; ++i) { R.Add(DNTest::RivalEntry(*FString::Printf(TEXT("r%d"), i))); } });
		});

		It("nome vazio e recusado", [this]()
		{
			Bad(TEXT("nome vazio"), TEXT("'Name'"), [](TArray<FDNRivalEntry>& R) { R[0].Row.Name = TEXT("  "); });
		});

		It("StraightKmh em (20, 400]", [this]()
		{
			Bad(TEXT("20"), TEXT("StraightKmh"), [](TArray<FDNRivalEntry>& R) { R[0].Row.StraightKmh = 20.f; R[0].Row.CornerKmh = 10.f; });
			Bad(TEXT("401"), TEXT("StraightKmh"), [](TArray<FDNRivalEntry>& R) { R[0].Row.StraightKmh = 401.f; });
			Bad(TEXT("NaN"), TEXT("StraightKmh"), [this](TArray<FDNRivalEntry>& R) { R[0].Row.StraightKmh = NaNf; });
			TArray<FDNRivalEntry> Edge = { DNTest::RivalEntry(TEXT("a"), TEXT("A"), 400.f, 50.f) };
			TestEqual("400 passa", FDNDataValidation::ValidateRivals(Edge).Num(), 0);
		});

		It("CornerKmh em (5, StraightKmh]", [this]()
		{
			Bad(TEXT("5"), TEXT("CornerKmh"), [](TArray<FDNRivalEntry>& R) { R[0].Row.CornerKmh = 5.f; });
			Bad(TEXT("maior que a reta"), TEXT("CornerKmh"), [](TArray<FDNRivalEntry>& R) { R[0].Row.CornerKmh = 111.f; });
			TArray<FDNRivalEntry> Edge = { DNTest::RivalEntry(TEXT("a"), TEXT("A"), 110.f, 110.f) };
			TestEqual("igual a reta passa", FDNDataValidation::ValidateRivals(Edge).Num(), 0);
		});

		It("angulos em (1, 180]", [this]()
		{
			Bad(TEXT("curva 1"), TEXT("CornerFullDeg"), [](TArray<FDNRivalEntry>& R) { R[0].Row.CornerFullDeg = 1.f; });
			Bad(TEXT("curva 181"), TEXT("CornerFullDeg"), [](TArray<FDNRivalEntry>& R) { R[0].Row.CornerFullDeg = 181.f; });
			Bad(TEXT("volante 1"), TEXT("SteerFullDeg"), [](TArray<FDNRivalEntry>& R) { R[0].Row.SteerFullDeg = 1.f; });
			Bad(TEXT("volante 181"), TEXT("SteerFullDeg"), [](TArray<FDNRivalEntry>& R) { R[0].Row.SteerFullDeg = 181.f; });
		});

		It("mira: minima [100, 10 000], por km/h [0, 200]", [this]()
		{
			Bad(TEXT("minima 99"), TEXT("LookAheadMinCm"), [](TArray<FDNRivalEntry>& R) { R[0].Row.LookAheadMinCm = 99.f; });
			Bad(TEXT("minima 10001"), TEXT("LookAheadMinCm"), [](TArray<FDNRivalEntry>& R) { R[0].Row.LookAheadMinCm = 10001.f; });
			Bad(TEXT("por km/h negativo"), TEXT("LookAheadPerKmhCm"), [](TArray<FDNRivalEntry>& R) { R[0].Row.LookAheadPerKmhCm = -1.f; });
			Bad(TEXT("por km/h 201"), TEXT("LookAheadPerKmhCm"), [](TArray<FDNRivalEntry>& R) { R[0].Row.LookAheadPerKmhCm = 201.f; });
			TArray<FDNRivalEntry> Edge = { DNTest::RivalEntry() };
			Edge[0].Row.LookAheadMinCm = 100.f;
			Edge[0].Row.LookAheadPerKmhCm = 0.f;
			TestEqual("limites exatos passam", FDNDataValidation::ValidateRivals(Edge).Num(), 0);
		});

		It("id repetido e recusado", [this]()
		{
			Bad(TEXT("id repetido"), TEXT("rival-1"), [](TArray<FDNRivalEntry>& R) { R.Add(DNTest::RivalEntry()); });
		});
	});

	Describe("ToConfig e o CSV real", [this]()
	{
		It("ToConfig copia os seis campos", [this]()
		{
			FDNRivalRow R;
			R.StraightKmh = 1.f; R.CornerKmh = 2.f; R.CornerFullDeg = 3.f; R.SteerFullDeg = 4.f; R.LookAheadMinCm = 5.f; R.LookAheadPerKmhCm = 6.f;
			const FDNRivalConfig C = R.ToConfig();
			TestTrue("campos", C.StraightKmh == 1.f && C.CornerKmh == 2.f && C.CornerFullDeg == 3.f && C.SteerFullDeg == 4.f && C.LookAheadMinCm == 5.f && C.LookAheadPerKmhCm == 6.f);
		});

		It("o CSV real tem 3 rivais, e valido, do mais rapido ao mais lento", [this]()
		{
			FDNData Data;
			TestEqual("leitura", FDNDataLoader::LoadAll(Data).Num(), 0);
			const TArray<FString> Errors = FDNDataValidation::ValidateRivals(Data.Rivals);
			TestEqual(*JoinErrors(Errors), Errors.Num(), 0);
			if (TestEqual("tres rivais", Data.Rivals.Num(), 3))
			{
				TestTrue("ordem decrescente de velocidade", Data.Rivals[0].Row.StraightKmh > Data.Rivals[1].Row.StraightKmh && Data.Rivals[1].Row.StraightKmh > Data.Rivals[2].Row.StraightKmh);
			}
		});
	});

	Describe("Subsistema", [this]()
	{
		It("GetRivals devolve nome e nivel na ordem do arquivo; tabela invalida: vazio", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			AddExpectedError(TEXT("Dados do jogo:"), EAutomationExpectedErrorFlags::Contains, 0);
			FDNData Data;
			Data.Rivals = { DNTest::RivalEntry(TEXT("a"), TEXT("Primeiro"), 120.f, 55.f), DNTest::RivalEntry(TEXT("b"), TEXT("Segundo"), 100.f, 48.f) };
			Sub->ApplyData(MoveTemp(Data), {});
			const TArray<TPair<FString, FDNRivalConfig>> R = Sub->GetRivals();
			if (TestEqual("dois", R.Num(), 2))
			{
				TestEqual("nome 0", R[0].Key, FString(TEXT("Primeiro")));
				TestEqual("reta 0", R[0].Value.StraightKmh, 120.f);
				TestEqual("curva 1", R[1].Value.CornerKmh, 48.f);
			}
			FDNData Bad;
			Bad.Rivals = { DNTest::RivalEntry() };
			Bad.Rivals[0].Row.CornerKmh = 999.f;
			Sub->ApplyData(MoveTemp(Bad), {});
			TestEqual("invalida: vazio", Sub->GetRivals().Num(), 0);
		});
	});
}

#endif
