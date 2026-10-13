// Testes do progresso do menu de cidades (docs/menu-mapa.md, "Regras de progresso").
// Regras: um cruzamento esta FEITO com bronze ou melhor; os contadores sao feitos sobre o total, por cidade,
// por regiao e no geral, com o numero de ouros em cada nivel; uma cidade com Playable = false aparece no mapa,
// mas nao entra no total jogavel. Funcoes puras, sem mundo.
//
// Decisoes de implementacao (as que o documento nao cobre; ver Core/DNProgress.h):
//  P1 a lista de entrada traz TODOS os cruzamentos do jogo, cada um com a medalha do jogador (None se nao jogou);
//     o total de uma cidade e o numero de cruzamentos dela.
//  P2 id de cruzamento repetido: vale o primeiro, o resto vira aviso.
//  P3 cruzamento de cidade desconhecida: ignorado, com aviso. Nunca quebra.
//  P4 cidade nao jogavel: aparece com 0/0 e 0 ouros; os cruzamentos dela ficam de fora do total, sem aviso.
//  P5 id de cidade repetido na tabela: vale o primeiro, o resto vira aviso. Id vazio: ignorado, com aviso.

#include "Misc/AutomationTest.h"
#include "Core/DNProgress.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FDNCrossingProgress X(const TCHAR* Crossing, const TCHAR* City, EDNMedal Medal = EDNMedal::None)
	{
		FDNCrossingProgress C;
		C.CrossingId = FName(Crossing);
		C.CityId = FName(City);
		C.Medal = Medal;
		return C;
	}

	FDNCityRegion City(const TCHAR* City, const TCHAR* Region, bool bPlayable = true)
	{
		FDNCityRegion R;
		R.CityId = FName(City);
		R.RegionId = FName(Region);
		R.bPlayable = bPlayable;
		return R;
	}

	FString Show(const FDNProgressCount& C) { return FString::Printf(TEXT("%d/%d, %d ouro(s)"), C.Done, C.Total, C.Gold); }

	void ExpectCount(FAutomationTestBase& T, const TCHAR* What, const FDNProgressCount& Actual, int32 Done, int32 Total, int32 Gold)
	{
		T.TestTrue(FString::Printf(TEXT("%s: esperado %d/%d com %d ouro(s), veio %s"), What, Done, Total, Gold, *Show(Actual)),
			Actual.Done == Done && Actual.Total == Total && Actual.Gold == Gold);
	}

	const FDNProgressCount& ByCity(const FDNProgressResult& R, const TCHAR* Id)
	{
		static const FDNCityProgress Empty;
		const FDNCityProgress* Found = R.ByCity.Find(FName(Id));
		return Found ? Found->Count : Empty.Count;
	}

	const FDNProgressCount& ByRegion(const FDNProgressResult& R, const TCHAR* Id)
	{
		static const FDNProgressCount Empty;
		const FDNProgressCount* Found = R.ByRegion.Find(FName(Id));
		return Found ? *Found : Empty;
	}
}

BEGIN_DEFINE_SPEC(FDNProgressSpec, "Detonauto.Progresso",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNProgressSpec)

void FDNProgressSpec::Define()
{
	Describe("Feito, total e ouros", [this]()
	{
		It("bronze, prata e ouro contam como feito; nenhuma nao conta", [this]()
		{
			const TArray<FDNCrossingProgress> Crossings = { X(TEXT("c1"), TEXT("rio"), EDNMedal::Bronze), X(TEXT("c2"), TEXT("rio"), EDNMedal::Silver),
				X(TEXT("c3"), TEXT("rio"), EDNMedal::Gold), X(TEXT("c4"), TEXT("rio"), EDNMedal::None) };
			const FDNProgressResult R = FDNProgress::Compute(Crossings, { City(TEXT("rio"), TEXT("brasil")) });
			ExpectCount(*this, TEXT("rio"), ByCity(R, TEXT("rio")), 3, 4, 1);
		});

		It("so os ouros contam como ouro", [this]()
		{
			const TArray<FDNCrossingProgress> Crossings = { X(TEXT("c1"), TEXT("rio"), EDNMedal::Gold), X(TEXT("c2"), TEXT("rio"), EDNMedal::Gold),
				X(TEXT("c3"), TEXT("rio"), EDNMedal::Silver) };
			const FDNProgressResult R = FDNProgress::Compute(Crossings, { City(TEXT("rio"), TEXT("brasil")) });
			ExpectCount(*this, TEXT("rio"), ByCity(R, TEXT("rio")), 3, 3, 2);
		});

		It("nenhum cruzamento jogado: 0 feitos sobre o total", [this]()
		{
			const TArray<FDNCrossingProgress> Crossings = { X(TEXT("c1"), TEXT("rio")), X(TEXT("c2"), TEXT("rio")) };
			const FDNProgressResult R = FDNProgress::Compute(Crossings, { City(TEXT("rio"), TEXT("brasil")) });
			ExpectCount(*this, TEXT("rio"), ByCity(R, TEXT("rio")), 0, 2, 0);
		});

		It("tudo feito: total igual ao feito", [this]()
		{
			const TArray<FDNCrossingProgress> Crossings = { X(TEXT("c1"), TEXT("rio"), EDNMedal::Bronze), X(TEXT("c2"), TEXT("rio"), EDNMedal::Bronze) };
			const FDNProgressResult R = FDNProgress::Compute(Crossings, { City(TEXT("rio"), TEXT("brasil")) });
			ExpectCount(*this, TEXT("rio"), ByCity(R, TEXT("rio")), 2, 2, 0);
		});
	});

	Describe("Por cidade, por regiao e no geral", [this]()
	{
		It("soma as cidades de cada regiao e todas no geral", [this]()
		{
			const TArray<FDNCrossingProgress> Crossings = {
				X(TEXT("c1"), TEXT("rio"), EDNMedal::Gold), X(TEXT("c2"), TEXT("rio")),
				X(TEXT("c3"), TEXT("sp"), EDNMedal::Bronze), X(TEXT("c4"), TEXT("sp"), EDNMedal::Gold), X(TEXT("c5"), TEXT("sp")),
				X(TEXT("c6"), TEXT("paris"), EDNMedal::Silver) };
			const TArray<FDNCityRegion> Cities = { City(TEXT("rio"), TEXT("brasil")), City(TEXT("sp"), TEXT("brasil")), City(TEXT("paris"), TEXT("europa")) };
			const FDNProgressResult R = FDNProgress::Compute(Crossings, Cities);
			ExpectCount(*this, TEXT("rio"), ByCity(R, TEXT("rio")), 1, 2, 1);
			ExpectCount(*this, TEXT("sp"), ByCity(R, TEXT("sp")), 2, 3, 1);
			ExpectCount(*this, TEXT("brasil"), ByRegion(R, TEXT("brasil")), 3, 5, 2);
			ExpectCount(*this, TEXT("europa"), ByRegion(R, TEXT("europa")), 1, 1, 0);
			ExpectCount(*this, TEXT("geral"), R.Overall, 4, 6, 2);
			TestEqual("sem avisos", R.Warnings.Num(), 0);
		});

		It("cidade sem cruzamentos: 0/0 e entra no mapa", [this]()
		{
			const FDNProgressResult R = FDNProgress::Compute({ X(TEXT("c1"), TEXT("rio"), EDNMedal::Gold) }, { City(TEXT("rio"), TEXT("brasil")), City(TEXT("sp"), TEXT("brasil")) });
			TestTrue("sp esta no resultado", R.ByCity.Contains(FName(TEXT("sp"))));
			ExpectCount(*this, TEXT("sp"), ByCity(R, TEXT("sp")), 0, 0, 0);
			ExpectCount(*this, TEXT("geral"), R.Overall, 1, 1, 1);
		});

		It("sem nenhum cruzamento nem cidade: tudo zero", [this]()
		{
			const FDNProgressResult R = FDNProgress::Compute({}, {});
			ExpectCount(*this, TEXT("geral"), R.Overall, 0, 0, 0);
			TestEqual("sem cidades", R.ByCity.Num(), 0);
			TestEqual("sem avisos", R.Warnings.Num(), 0);
		});

		It("cruzamentos sem cidades na tabela: ignorados com um aviso por cruzamento", [this]()
		{
			const FDNProgressResult R = FDNProgress::Compute({ X(TEXT("c1"), TEXT("rio"), EDNMedal::Gold) }, {});
			ExpectCount(*this, TEXT("geral"), R.Overall, 0, 0, 0);
			TestEqual("avisos", R.Warnings.Num(), 1);
		});
	});

	Describe("Cidade nao jogavel (P4)", [this]()
	{
		It("aparece no mapa com 0/0 e nao entra nos totais", [this]()
		{
			const TArray<FDNCrossingProgress> Crossings = { X(TEXT("c1"), TEXT("rio"), EDNMedal::Gold), X(TEXT("c2"), TEXT("sp"), EDNMedal::Gold), X(TEXT("c3"), TEXT("sp")) };
			const TArray<FDNCityRegion> Cities = { City(TEXT("rio"), TEXT("brasil")), City(TEXT("sp"), TEXT("brasil"), false) };
			const FDNProgressResult R = FDNProgress::Compute(Crossings, Cities);
			ExpectCount(*this, TEXT("sp"), ByCity(R, TEXT("sp")), 0, 0, 0);
			TestFalse("sp marcada como nao jogavel", R.ByCity[FName(TEXT("sp"))].bPlayable);
			TestTrue("rio marcada como jogavel", R.ByCity[FName(TEXT("rio"))].bPlayable);
			ExpectCount(*this, TEXT("brasil"), ByRegion(R, TEXT("brasil")), 1, 1, 1);
			ExpectCount(*this, TEXT("geral"), R.Overall, 1, 1, 1);
			TestEqual("os cruzamentos de cidade nao jogavel nao geram aviso", R.Warnings.Num(), 0);
		});

		It("regiao so com cidades nao jogaveis existe com 0/0", [this]()
		{
			const FDNProgressResult R = FDNProgress::Compute({ X(TEXT("c1"), TEXT("paris")) }, { City(TEXT("paris"), TEXT("europa"), false) });
			TestTrue("regiao no resultado", R.ByRegion.Contains(FName(TEXT("europa"))));
			ExpectCount(*this, TEXT("europa"), ByRegion(R, TEXT("europa")), 0, 0, 0);
			ExpectCount(*this, TEXT("geral"), R.Overall, 0, 0, 0);
		});
	});

	Describe("Dados invalidos: ignorar com aviso, nunca quebrar", [this]()
	{
		It("cruzamento de cidade desconhecida e ignorado com aviso (P3)", [this]()
		{
			const FDNProgressResult R = FDNProgress::Compute({ X(TEXT("c1"), TEXT("rio"), EDNMedal::Gold), X(TEXT("c2"), TEXT("atlantida"), EDNMedal::Gold) },
				{ City(TEXT("rio"), TEXT("brasil")) });
			ExpectCount(*this, TEXT("geral"), R.Overall, 1, 1, 1);
			TestEqual("um aviso", R.Warnings.Num(), 1);
			TestTrue("o aviso cita o cruzamento", R.Warnings.Num() == 1 && R.Warnings[0].Contains(TEXT("c2")));
		});

		It("id de cruzamento repetido: vale o primeiro, o resto vira aviso (P2)", [this]()
		{
			const FDNProgressResult R = FDNProgress::Compute({ X(TEXT("c1"), TEXT("rio"), EDNMedal::Bronze), X(TEXT("c1"), TEXT("rio"), EDNMedal::Gold) },
				{ City(TEXT("rio"), TEXT("brasil")) });
			ExpectCount(*this, TEXT("rio"), ByCity(R, TEXT("rio")), 1, 1, 0);
			TestEqual("um aviso", R.Warnings.Num(), 1);
		});

		It("o mesmo cruzamento listado em duas cidades: vale o primeiro, um aviso", [this]()
		{
			const FDNProgressResult R = FDNProgress::Compute({ X(TEXT("c1"), TEXT("rio"), EDNMedal::Gold), X(TEXT("c1"), TEXT("sp"), EDNMedal::Gold) },
				{ City(TEXT("rio"), TEXT("brasil")), City(TEXT("sp"), TEXT("brasil")) });
			ExpectCount(*this, TEXT("rio"), ByCity(R, TEXT("rio")), 1, 1, 1);
			ExpectCount(*this, TEXT("sp"), ByCity(R, TEXT("sp")), 0, 0, 0);
			TestEqual("um aviso", R.Warnings.Num(), 1);
		});

		It("id de cruzamento vazio e ignorado com aviso", [this]()
		{
			FDNCrossingProgress Bad;
			Bad.CityId = FName(TEXT("rio"));
			Bad.Medal = EDNMedal::Gold;
			const FDNProgressResult R = FDNProgress::Compute({ Bad }, { City(TEXT("rio"), TEXT("brasil")) });
			ExpectCount(*this, TEXT("rio"), ByCity(R, TEXT("rio")), 0, 0, 0);
			TestEqual("um aviso", R.Warnings.Num(), 1);
		});

		It("id de cidade repetido na tabela: vale o primeiro, o resto vira aviso (P5)", [this]()
		{
			const FDNProgressResult R = FDNProgress::Compute({ X(TEXT("c1"), TEXT("rio"), EDNMedal::Gold) },
				{ City(TEXT("rio"), TEXT("brasil")), City(TEXT("rio"), TEXT("europa")) });
			ExpectCount(*this, TEXT("brasil"), ByRegion(R, TEXT("brasil")), 1, 1, 1);
			TestFalse("a segunda regiao nao nasce da duplicata", R.ByRegion.Contains(FName(TEXT("europa"))));
			TestEqual("um aviso", R.Warnings.Num(), 1);
		});

		It("id de cidade vazio na tabela e ignorado com aviso", [this]()
		{
			FDNCityRegion Bad;
			Bad.RegionId = FName(TEXT("brasil"));
			const FDNProgressResult R = FDNProgress::Compute({}, { Bad });
			TestEqual("nenhuma cidade", R.ByCity.Num(), 0);
			TestEqual("um aviso", R.Warnings.Num(), 1);
		});

		It("cidade sem regiao (id de regiao vazio): ignorada com aviso", [this]()
		{
			FDNCityRegion Bad;
			Bad.CityId = FName(TEXT("rio"));
			const FDNProgressResult R = FDNProgress::Compute({ X(TEXT("c1"), TEXT("rio"), EDNMedal::Gold) }, { Bad });
			ExpectCount(*this, TEXT("geral"), R.Overall, 0, 0, 0);
			TestTrue("pelo menos um aviso", R.Warnings.Num() >= 1);
		});
	});

	Describe("Ordem da entrada", [this]()
	{
		It("o resultado nao depende da ordem dos cruzamentos", [this]()
		{
			TArray<FDNCrossingProgress> A = { X(TEXT("c1"), TEXT("rio"), EDNMedal::Gold), X(TEXT("c2"), TEXT("sp"), EDNMedal::Bronze), X(TEXT("c3"), TEXT("rio")) };
			TArray<FDNCrossingProgress> B = { A[2], A[0], A[1] };
			const TArray<FDNCityRegion> Cities = { City(TEXT("rio"), TEXT("brasil")), City(TEXT("sp"), TEXT("brasil")) };
			const FDNProgressResult RA = FDNProgress::Compute(A, Cities);
			const FDNProgressResult RB = FDNProgress::Compute(B, Cities);
			ExpectCount(*this, TEXT("geral A"), RA.Overall, 2, 3, 1);
			ExpectCount(*this, TEXT("geral B"), RB.Overall, RA.Overall.Done, RA.Overall.Total, RA.Overall.Gold);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
