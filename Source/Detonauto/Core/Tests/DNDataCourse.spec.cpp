// Testes de Data/CoursePoints.csv (docs/percurso-1.md): validaÃ§Ã£o por campo, o CSV real e o subsistema. Faixas: Core/Data/DNDataValidation.h.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/Data/DNDataLoader.h"
#include "Core/Data/DNDataValidation.h"
#include "Core/DNCourse.h"
#include "Core/DNGameDataSubsystem.h"
#include "Core/Tests/DNTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDataCourseSpec, "Detonauto.Dados.Percurso",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	const float NaNf = std::numeric_limits<float>::quiet_NaN();

	static FString JoinErrors(const TArray<FString>& Errors) { return Errors.IsEmpty() ? FString(TEXT("(nenhum erro)")) : FString::Join(Errors, TEXT(" | ")); }

	static bool Cites(const TArray<FString>& Errors, const TCHAR* Needle)
	{
		for (const FString& E : Errors) { if (E.Contains(Needle)) { return true; } }
		return false;
	}

	void Bad(const TCHAR* What, const TCHAR* Needle, TFunction<void(TArray<FDNCoursePointEntry>&)> Tweak)
	{
		TArray<FDNCoursePointEntry> Rows = DNTest::CourseEntries();
		Tweak(Rows);
		const TArray<FString> Errors = FDNDataValidation::ValidateCoursePoints(Rows);
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Needle, *JoinErrors(Errors)), Cites(Errors, Needle));
	}
END_DEFINE_SPEC(FDNDataCourseSpec)

void FDNDataCourseSpec::Define()
{
	Describe("CoursePoints.csv: validacao", [this]()
	{
		It("o percurso valido passa", [this]()
		{
			TestEqual("erros", FDNDataValidation::ValidateCoursePoints(DNTest::CourseEntries()).Num(), 0);
		});

		It("tabela vazia e erro", [this]()
		{
			Bad(TEXT("vazia"), TEXT("CoursePoints.csv"), [](TArray<FDNCoursePointEntry>& R) { R.Reset(); });
		});

		It("percurso vazio de nome, com 1 ponto, ou ordem repetida", [this]()
		{
			Bad(TEXT("sem nome"), TEXT("'Course'"), [](TArray<FDNCoursePointEntry>& R) { R[0].Row.Course = TEXT(" "); });
			Bad(TEXT("1 ponto"), TEXT("pelo menos 2"), [](TArray<FDNCoursePointEntry>& R) { R.SetNum(1); });
			Bad(TEXT("ordem repetida"), TEXT("Order"), [](TArray<FDNCoursePointEntry>& R) { R[0].Row.Order = 0; });
			Bad(TEXT("ordem negativa"), TEXT("Order"), [](TArray<FDNCoursePointEntry>& R) { R[0].Row.Order = -1; });
		});

		It("coordenadas finitas e dentro de 10 000 000 cm", [this]()
		{
			Bad(TEXT("NaN"), TEXT("XCm"), [this](TArray<FDNCoursePointEntry>& R) { R[0].Row.XCm = NaNf; });
			Bad(TEXT("longe"), TEXT("YCm"), [](TArray<FDNCoursePointEntry>& R) { R[0].Row.YCm = 2.0e7f; });
			Bad(TEXT("Z"), TEXT("ZCm"), [](TArray<FDNCoursePointEntry>& R) { R[0].Row.ZCm = -2.0e7f; });
		});

		It("largura em [400, 3000] e igual no percurso", [this]()
		{
			Bad(TEXT("estreita"), TEXT("WidthCm"), [](TArray<FDNCoursePointEntry>& R) { for (auto& E : R) { E.Row.WidthCm = 399.f; } });
			Bad(TEXT("larga"), TEXT("WidthCm"), [](TArray<FDNCoursePointEntry>& R) { for (auto& E : R) { E.Row.WidthCm = 3001.f; } });
			Bad(TEXT("diferente"), TEXT("WidthCm"), [](TArray<FDNCoursePointEntry>& R) { R[1].Row.WidthCm = 1500.f; });
		});

		It("pontos vizinhos a pelo menos 100 cm", [this]()
		{
			Bad(TEXT("colados"), TEXT("100"), [](TArray<FDNCoursePointEntry>& R) { R[0].Row.XCm = 1000.f; R[0].Row.YCm = 50.f; });
		});

		It("dois percursos na mesma tabela, cada um com seus pontos", [this]()
		{
			TArray<FDNCoursePointEntry> R = DNTest::CourseEntries();
			R.Add(DNTest::CoursePoint(TEXT("outra"), 0, 0.f, 0.f, 1000.f));
			R.Add(DNTest::CoursePoint(TEXT("outra"), 1, 0.f, 2000.f, 1000.f));
			TestEqual("erros", FDNDataValidation::ValidateCoursePoints(R).Num(), 0);
		});
	});

	Describe("CoursePoints.csv real", [this]()
	{
		It("o percurso1 existe, e valido e tem uns 2 km", [this]()
		{
			FDNData Data;
			const TArray<FString> LoadErrors = FDNDataLoader::LoadAll(Data);
			TestEqual("leitura", LoadErrors.Num(), 0);
			const TArray<FString> Errors = FDNDataValidation::ValidateCoursePoints(Data.CoursePoints);
			TestEqual(*JoinErrors(Errors), Errors.Num(), 0);
			TArray<FVector> Pts;
			for (const FDNCoursePointEntry& E : Data.CoursePoints)
			{
				if (E.Row.Course == TEXT("percurso1")) { Pts.Add(FVector(E.Row.XCm, E.Row.YCm, E.Row.ZCm)); }
			}
			const float Length = FDNCourse::Length(Pts);
			TestTrue(FString::Printf(TEXT("comprimento %.0f cm"), Length), Length > 150000.f && Length < 250000.f);
		});
	});

	Describe("Subsistema", [this]()
	{
		It("GetCourse devolve os pontos na ordem de Order e a largura, sem diferenciar a caixa", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			AddExpectedError(TEXT("Dados do jogo:"), EAutomationExpectedErrorFlags::Contains, 0); // as outras tabelas vazias dÃ£o erro
			FDNData Data;
			Data.CoursePoints = DNTest::CourseEntries();
			Sub->ApplyData(MoveTemp(Data), {});
			TArray<FVector> Pts;
			float Width = 0.f;
			TestTrue("achou", Sub->GetCourse(TEXT("RUA"), Pts, Width));
			TestEqual("n", Pts.Num(), 3);
			TestEqual("largura", Width, 1400.f);
			if (Pts.Num() == 3)
			{
				TestTrue("ordenado", Pts[0].Equals(FVector(0, 0, 0)) && Pts[1].Equals(FVector(1000, 0, 0)) && Pts[2].Equals(FVector(1000, 500, 0)));
			}
			TestFalse("percurso que nao existe", Sub->GetCourse(TEXT("nada"), Pts, Width));
		});
	});
}

#endif
