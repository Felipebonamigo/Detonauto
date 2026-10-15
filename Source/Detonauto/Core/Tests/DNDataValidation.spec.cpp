// Testes dos validadores dos CSVs de Data/ (cidades, regiões, pontos turísticos, cruzamentos e tráfego).
// Dados montados em memória, um caso por regra. Os arquivos reais são testados em DNDataFiles.spec.cpp.
//
// Cada validador recebe as linhas já lidas e devolve a lista de erros (vazia = tudo certo).

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/Tests/DNTestData.h"
#include "Core/Data/DNDataValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FDNRegionEntry Region(const TCHAR* Id, const TCHAR* Name = TEXT("Brasil"), int32 Order = 1, bool bUnlocked = true)
	{
		FDNRegionEntry E;
		E.Id = Id;
		E.Row.Name = Name;
		E.Row.Order = Order;
		E.Row.Unlocked = bUnlocked;
		return E;
	}

	FDNCityEntry City(const TCHAR* Id, const TCHAR* RegionId = TEXT("brasil"), int32 Order = 1, double Lat = -22.9, double Lon = -43.2,
		const TCHAR* Name = TEXT("Rio"), const TCHAR* District = TEXT(""), bool bPlayable = false)
	{
		FDNCityEntry E;
		E.Id = Id;
		E.Row.Name = Name;
		E.Row.RegionId = RegionId;
		E.Row.Latitude = Lat;
		E.Row.Longitude = Lon;
		E.Row.District = District;
		E.Row.Order = Order;
		E.Row.Unlocked = true;
		E.Row.Playable = bPlayable;
		return E;
	}

	FDNLandmarkEntry Landmark(const TCHAR* Id, const TCHAR* CityId = TEXT("rio"), const TCHAR* Name = TEXT("Ponto"), const TCHAR* ShortText = TEXT(""),
		const TCHAR* SourceNote = TEXT(""), bool bNeedsReview = true)
	{
		FDNLandmarkEntry E;
		E.Id = Id;
		E.Row.CityId = CityId;
		E.Row.Name = Name;
		E.Row.Kind = TEXT("monumento");
		E.Row.ShortText = ShortText;
		E.Row.SourceNote = SourceNote;
		E.Row.NeedsReview = bNeedsReview;
		return E;
	}

	FDNCrossingEntry Crossing(const TCHAR* Id, const TCHAR* CityId = TEXT("rio"), int32 Order = 1, int32 Bronze = 1000, int32 Silver = 5000, int32 Gold = 12000)
	{
		FDNCrossingEntry E;
		E.Id = Id;
		E.Row.CityId = CityId;
		E.Row.Order = Order;
		E.Row.Bronze = Bronze;
		E.Row.Silver = Silver;
		E.Row.Gold = Gold;
		return E;
	}

	FDNTrafficSettingsEntry Settings(const TCHAR* Id = TEXT("padrao"), float Wake = 6.f, float Ahead = 25.f, float Closing = 20.f, float Despawn = 200.f, float Still = 2.f, int32 MaxSpawn = 2)
	{
		FDNTrafficSettingsEntry E;
		E.Id = Id;
		E.Row.WakeDistanceM = Wake;
		E.Row.WakeAheadM = Ahead;
		E.Row.WakeAheadMinClosingKmh = Closing;
		E.Row.DespawnDistanceM = Despawn;
		E.Row.StillSpeedMs = Still;
		E.Row.MaxSpawnPerFrame = MaxSpawn;
		return E;
	}

	FDNTrafficVehicleEntry Vehicle(const TCHAR* Id, const TCHAR* Name = TEXT("Compacto Modelo 1"), int32 Value = 800, float Mass = 950.f, const TCHAR* Class = TEXT("compact"))
	{
		FDNTrafficVehicleEntry E;
		E.Id = Id;
		E.Row.Name = Name;
		E.Row.ValueCash = Value;
		E.Row.MassKg = Mass;
		E.Row.Class = Class;
		return E;
	}

	/** Verdadeiro se algum erro contém todos os trechos. */
	bool HasError(const TArray<FString>& Errors, std::initializer_list<const TCHAR*> Needles)
	{
		for (const FString& E : Errors)
		{
			bool bAll = true;
			for (const TCHAR* N : Needles) { bAll = bAll && E.Contains(N); }
			if (bAll) { return true; }
		}
		return false;
	}

	FString Join(const TArray<FString>& Errors) { return Errors.IsEmpty() ? FString(TEXT("(nenhum erro)")) : FString::Join(Errors, TEXT(" | ")); }

	void ExpectError(FAutomationTestBase& T, const TCHAR* What, const TArray<FString>& Errors, std::initializer_list<const TCHAR*> Needles)
	{
		T.TestTrue(FString::Printf(TEXT("%s: esperava um erro com os trechos pedidos; veio: %s"), What, *Join(Errors)), HasError(Errors, Needles));
	}

	void ExpectClean(FAutomationTestBase& T, const TCHAR* What, const TArray<FString>& Errors)
	{
		T.TestTrue(FString::Printf(TEXT("%s: esperava zero erros; veio: %s"), What, *Join(Errors)), Errors.IsEmpty());
	}

	const double NaN = std::numeric_limits<double>::quiet_NaN();
}

BEGIN_DEFINE_SPEC(FDNDataRegionsSpec, "Detonauto.Dados.Regioes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataRegionsSpec)

void FDNDataRegionsSpec::Define()
{
	Describe("Regions.csv", [this]()
	{
		It("regioes validas e a lista vazia nao dao erro", [this]()
		{
			ExpectClean(*this, TEXT("duas regioes"), FDNDataValidation::ValidateRegions({ Region(TEXT("brasil"), TEXT("Brasil"), 1), Region(TEXT("europa"), TEXT("Europa"), 2) }));
			ExpectClean(*this, TEXT("vazia"), FDNDataValidation::ValidateRegions({}));
		});

		It("id vazio e recusado", [this]()
		{
			ExpectError(*this, TEXT("id vazio"), FDNDataValidation::ValidateRegions({ Region(TEXT("")) }), { TEXT("Regions"), TEXT("id") });
		});

		It("id repetido e recusado, e a mensagem diz qual", [this]()
		{
			ExpectError(*this, TEXT("id repetido"), FDNDataValidation::ValidateRegions({ Region(TEXT("brasil"), TEXT("A"), 1), Region(TEXT("brasil"), TEXT("B"), 2) }), { TEXT("brasil"), TEXT("repetido") });
		});

		// Os ids viram FName, que nao diferencia maiusculas de minusculas: "Brasil" e "brasil" sao o mesmo id.
		It("ids que so diferem em maiusculas e minusculas sao duplicados, e a mensagem diz isso", [this]()
		{
			ExpectError(*this, TEXT("Brasil x brasil"), FDNDataValidation::ValidateRegions({ Region(TEXT("Brasil"), TEXT("A"), 1), Region(TEXT("brasil"), TEXT("B"), 2) }), { TEXT("Brasil"), TEXT("brasil"), TEXT("são o mesmo id") });
		});

		It("o id so pode ter letras sem acento, numeros, hifen e sublinhado", [this]()
		{
			ExpectClean(*this, TEXT("id valido"), FDNDataValidation::ValidateRegions({ Region(TEXT("america-do_sul2"), TEXT("A"), 1) }));
			ExpectError(*this, TEXT("espaco"), FDNDataValidation::ValidateRegions({ Region(TEXT("america do sul")) }), { TEXT("america do sul"), TEXT("caractere") });
			ExpectError(*this, TEXT("acento"), FDNDataValidation::ValidateRegions({ Region(TEXT("região")) }), { TEXT("região"), TEXT("caractere") });
			ExpectError(*this, TEXT("ponto"), FDNDataValidation::ValidateRegions({ Region(TEXT("a.b")) }), { TEXT("a.b"), TEXT("caractere") });
			ExpectError(*this, TEXT("barra"), FDNDataValidation::ValidateRegions({ Region(TEXT("a/b")) }), { TEXT("a/b"), TEXT("caractere") });
			ExpectError(*this, TEXT("virgula"), FDNDataValidation::ValidateRegions({ Region(TEXT("a,b")) }), { TEXT("a,b"), TEXT("caractere") });
		});

		It("nome vazio e recusado", [this]()
		{
			ExpectError(*this, TEXT("nome vazio"), FDNDataValidation::ValidateRegions({ Region(TEXT("brasil"), TEXT("")) }), { TEXT("brasil"), TEXT("nome") });
		});

		It("ordem repetida e recusada", [this]()
		{
			ExpectError(*this, TEXT("ordem repetida"), FDNDataValidation::ValidateRegions({ Region(TEXT("a"), TEXT("A"), 1), Region(TEXT("b"), TEXT("B"), 1) }), { TEXT("ordem"), TEXT("1") });
		});

		It("ordem zero ou negativa e recusada", [this]()
		{
			ExpectError(*this, TEXT("ordem 0"), FDNDataValidation::ValidateRegions({ Region(TEXT("a"), TEXT("A"), 0) }), { TEXT("a"), TEXT("ordem") });
			ExpectError(*this, TEXT("ordem -3"), FDNDataValidation::ValidateRegions({ Region(TEXT("a"), TEXT("A"), -3) }), { TEXT("a"), TEXT("ordem") });
		});

		It("acumula todos os erros em vez de parar no primeiro", [this]()
		{
			const TArray<FString> Errors = FDNDataValidation::ValidateRegions({ Region(TEXT("a"), TEXT(""), 0), Region(TEXT("a"), TEXT("B"), 0) });
			TestTrue(FString::Printf(TEXT("mais de um erro; veio: %s"), *Join(Errors)), Errors.Num() >= 3);
		});
	});
}

BEGIN_DEFINE_SPEC(FDNDataCitiesSpec, "Detonauto.Dados.Cidades",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TArray<FDNRegionEntry> Regions;
END_DEFINE_SPEC(FDNDataCitiesSpec)

void FDNDataCitiesSpec::Define()
{
	BeforeEach([this]()
	{
		Regions = { Region(TEXT("brasil"), TEXT("Brasil"), 1) };
	});

	Describe("Cities.csv", [this]()
	{
		It("cidades validas nao dao erro", [this]()
		{
			ExpectClean(*this, TEXT("duas cidades"), FDNDataValidation::ValidateCities({ City(TEXT("rio"), TEXT("brasil"), 1), City(TEXT("sp"), TEXT("brasil"), 2, -23.55, -46.63, TEXT("Sao Paulo")) }, Regions));
		});

		It("id vazio e id repetido sao recusados", [this]()
		{
			ExpectError(*this, TEXT("id vazio"), FDNDataValidation::ValidateCities({ City(TEXT("")) }, Regions), { TEXT("Cities"), TEXT("id") });
			ExpectError(*this, TEXT("id repetido"), FDNDataValidation::ValidateCities({ City(TEXT("rio"), TEXT("brasil"), 1), City(TEXT("rio"), TEXT("brasil"), 2) }, Regions), { TEXT("rio"), TEXT("repetido") });
		});

		It("referencia a regiao que nao existe e recusada", [this]()
		{
			ExpectError(*this, TEXT("regiao desconhecida"), FDNDataValidation::ValidateCities({ City(TEXT("rio"), TEXT("atlantida")) }, Regions), { TEXT("rio"), TEXT("atlantida") });
		});

		It("regiao vazia e recusada", [this]()
		{
			ExpectError(*this, TEXT("regiao vazia"), FDNDataValidation::ValidateCities({ City(TEXT("rio"), TEXT("")) }, Regions), { TEXT("rio"), TEXT("regi") });
		});

		It("a referencia a regiao ignora a caixa, como o FName", [this]()
		{
			ExpectClean(*this, TEXT("Brasil refere brasil"), FDNDataValidation::ValidateCities({ City(TEXT("rio"), TEXT("Brasil")) }, Regions));
		});

		It("ids de cidade que so diferem na caixa sao duplicados; id com caractere invalido e recusado", [this]()
		{
			ExpectError(*this, TEXT("Rio x rio"), FDNDataValidation::ValidateCities({ City(TEXT("Rio"), TEXT("brasil"), 1), City(TEXT("rio"), TEXT("brasil"), 2) }, Regions), { TEXT("Rio"), TEXT("rio"), TEXT("são o mesmo id") });
			ExpectError(*this, TEXT("espaco"), FDNDataValidation::ValidateCities({ City(TEXT("rio de janeiro")) }, Regions), { TEXT("rio de janeiro"), TEXT("caractere") });
			ExpectError(*this, TEXT("acento"), FDNDataValidation::ValidateCities({ City(TEXT("são-paulo")) }, Regions), { TEXT("são-paulo"), TEXT("caractere") });
		});

		It("latitude e longitude nos limites exatos sao validas", [this]()
		{
			ExpectClean(*this, TEXT("limites"), FDNDataValidation::ValidateCities({ City(TEXT("a"), TEXT("brasil"), 1, 90.0, 180.0), City(TEXT("b"), TEXT("brasil"), 2, -90.0, -180.0) }, Regions));
		});

		It("latitude ou longitude fora do intervalo e recusada", [this]()
		{
			ExpectError(*this, TEXT("lat 90,0001"), FDNDataValidation::ValidateCities({ City(TEXT("a"), TEXT("brasil"), 1, 90.0001, 0.0) }, Regions), { TEXT("a"), TEXT("latitude") });
			ExpectError(*this, TEXT("lat -91"), FDNDataValidation::ValidateCities({ City(TEXT("a"), TEXT("brasil"), 1, -91.0, 0.0) }, Regions), { TEXT("a"), TEXT("latitude") });
			ExpectError(*this, TEXT("lon 180,0001"), FDNDataValidation::ValidateCities({ City(TEXT("a"), TEXT("brasil"), 1, 0.0, 180.0001) }, Regions), { TEXT("a"), TEXT("longitude") });
			ExpectError(*this, TEXT("lon -181"), FDNDataValidation::ValidateCities({ City(TEXT("a"), TEXT("brasil"), 1, 0.0, -181.0) }, Regions), { TEXT("a"), TEXT("longitude") });
		});

		It("NaN em latitude ou longitude e recusado", [this]()
		{
			ExpectError(*this, TEXT("lat NaN"), FDNDataValidation::ValidateCities({ City(TEXT("a"), TEXT("brasil"), 1, NaN, 0.0) }, Regions), { TEXT("a"), TEXT("latitude") });
			ExpectError(*this, TEXT("lon NaN"), FDNDataValidation::ValidateCities({ City(TEXT("a"), TEXT("brasil"), 1, 0.0, NaN) }, Regions), { TEXT("a"), TEXT("longitude") });
		});

		It("nome vazio e recusado", [this]()
		{
			ExpectError(*this, TEXT("nome vazio"), FDNDataValidation::ValidateCities({ City(TEXT("rio"), TEXT("brasil"), 1, 0.0, 0.0, TEXT("")) }, Regions), { TEXT("rio"), TEXT("nome") });
		});

		It("ordem repetida na MESMA regiao e recusada; em regioes diferentes e permitida", [this]()
		{
			Regions = { Region(TEXT("brasil"), TEXT("Brasil"), 1), Region(TEXT("europa"), TEXT("Europa"), 2) };
			ExpectError(*this, TEXT("mesma regiao"), FDNDataValidation::ValidateCities({ City(TEXT("a"), TEXT("brasil"), 1), City(TEXT("b"), TEXT("brasil"), 1) }, Regions), { TEXT("ordem"), TEXT("brasil") });
			ExpectClean(*this, TEXT("regioes diferentes"), FDNDataValidation::ValidateCities({ City(TEXT("a"), TEXT("brasil"), 1), City(TEXT("b"), TEXT("europa"), 1) }, Regions));
		});

		It("ordem zero ou negativa e recusada", [this]()
		{
			ExpectError(*this, TEXT("ordem 0"), FDNDataValidation::ValidateCities({ City(TEXT("a"), TEXT("brasil"), 0) }, Regions), { TEXT("a"), TEXT("ordem") });
		});

		It("cidade jogavel precisa de distrito; nao jogavel pode ficar sem", [this]()
		{
			ExpectError(*this, TEXT("jogavel sem distrito"), FDNDataValidation::ValidateCities({ City(TEXT("rio"), TEXT("brasil"), 1, -22.9, -43.2, TEXT("Rio"), TEXT(""), true) }, Regions), { TEXT("rio"), TEXT("distrito") });
			ExpectClean(*this, TEXT("jogavel com distrito"), FDNDataValidation::ValidateCities({ City(TEXT("rio"), TEXT("brasil"), 1, -22.9, -43.2, TEXT("Rio"), TEXT("distrito-rio"), true) }, Regions));
			ExpectClean(*this, TEXT("nao jogavel sem distrito"), FDNDataValidation::ValidateCities({ City(TEXT("rio"), TEXT("brasil"), 1, -22.9, -43.2, TEXT("Rio"), TEXT(""), false) }, Regions));
		});
	});
}

BEGIN_DEFINE_SPEC(FDNDataLandmarksSpec, "Detonauto.Dados.Pontos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TArray<FDNCityEntry> Cities;
END_DEFINE_SPEC(FDNDataLandmarksSpec)

void FDNDataLandmarksSpec::Define()
{
	BeforeEach([this]()
	{
		Cities = { City(TEXT("rio")) };
	});

	Describe("Landmarks.csv", [this]()
	{
		It("sem linhas, e linhas marcadas para revisao com texto vazio, nao dao erro", [this]()
		{
			ExpectClean(*this, TEXT("vazia"), FDNDataValidation::ValidateLandmarks({}, Cities));
			ExpectClean(*this, TEXT("para revisar"), FDNDataValidation::ValidateLandmarks({ Landmark(TEXT("p1"), TEXT("rio"), TEXT("Ponto"), TEXT(""), TEXT(""), true) }, Cities));
		});

		It("texto com fonte e revisao concluida nao da erro", [this]()
		{
			ExpectClean(*this, TEXT("revisado"), FDNDataValidation::ValidateLandmarks({ Landmark(TEXT("p1"), TEXT("rio"), TEXT("Ponto"), TEXT("Texto conferido."), TEXT("Fonte X"), false) }, Cities));
		});

		It("id vazio e id repetido sao recusados", [this]()
		{
			ExpectError(*this, TEXT("id vazio"), FDNDataValidation::ValidateLandmarks({ Landmark(TEXT("")) }, Cities), { TEXT("Landmarks"), TEXT("id") });
			ExpectError(*this, TEXT("id repetido"), FDNDataValidation::ValidateLandmarks({ Landmark(TEXT("p1")), Landmark(TEXT("p1")) }, Cities), { TEXT("p1"), TEXT("repetido") });
		});

		It("ids de ponto que so diferem na caixa sao duplicados; id com caractere invalido e recusado", [this]()
		{
			ExpectError(*this, TEXT("P1 x p1"), FDNDataValidation::ValidateLandmarks({ Landmark(TEXT("P1")), Landmark(TEXT("p1")) }, Cities), { TEXT("P1"), TEXT("p1"), TEXT("são o mesmo id") });
			ExpectError(*this, TEXT("espaco"), FDNDataValidation::ValidateLandmarks({ Landmark(TEXT("ponto um")) }, Cities), { TEXT("ponto um"), TEXT("caractere") });
		});

		It("a referencia a cidade ignora a caixa, como o FName", [this]()
		{
			ExpectClean(*this, TEXT("Rio refere rio"), FDNDataValidation::ValidateLandmarks({ Landmark(TEXT("p1"), TEXT("Rio")) }, Cities));
		});

		It("referencia a cidade que nao existe e recusada", [this]()
		{
			ExpectError(*this, TEXT("cidade desconhecida"), FDNDataValidation::ValidateLandmarks({ Landmark(TEXT("p1"), TEXT("atlantida")) }, Cities), { TEXT("p1"), TEXT("atlantida") });
		});

		It("nome vazio e recusado", [this]()
		{
			ExpectError(*this, TEXT("nome vazio"), FDNDataValidation::ValidateLandmarks({ Landmark(TEXT("p1"), TEXT("rio"), TEXT("")) }, Cities), { TEXT("p1"), TEXT("nome") });
		});

		// Regra do CLAUDE.md: texto sobre cidade só entra depois de conferido em fonte confiável.
		It("texto ainda marcado para revisao e recusado", [this]()
		{
			ExpectError(*this, TEXT("texto + revisao pendente"), FDNDataValidation::ValidateLandmarks({ Landmark(TEXT("p1"), TEXT("rio"), TEXT("Ponto"), TEXT("Um texto."), TEXT("Fonte"), true) }, Cities), { TEXT("p1"), TEXT("revis") });
		});

		It("texto sem fonte e recusado", [this]()
		{
			ExpectError(*this, TEXT("texto sem fonte"), FDNDataValidation::ValidateLandmarks({ Landmark(TEXT("p1"), TEXT("rio"), TEXT("Ponto"), TEXT("Um texto."), TEXT(""), false) }, Cities), { TEXT("p1"), TEXT("fonte") });
		});
	});
}

BEGIN_DEFINE_SPEC(FDNDataCrossingsSpec, "Detonauto.Dados.Cruzamentos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TArray<FDNCityEntry> Cities;
END_DEFINE_SPEC(FDNDataCrossingsSpec)

void FDNDataCrossingsSpec::Define()
{
	BeforeEach([this]()
	{
		Cities = { City(TEXT("rio")), City(TEXT("sp"), TEXT("brasil"), 2) };
	});

	Describe("Crossings.csv", [this]()
	{
		It("cruzamentos validos e a lista vazia nao dao erro", [this]()
		{
			ExpectClean(*this, TEXT("dois"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("rio"), 1), Crossing(TEXT("c2"), TEXT("rio"), 2) }, Cities));
			ExpectClean(*this, TEXT("vazia"), FDNDataValidation::ValidateCrossings({}, Cities));
		});

		It("id vazio e id repetido sao recusados", [this]()
		{
			ExpectError(*this, TEXT("id vazio"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("")) }, Cities), { TEXT("Crossings"), TEXT("id") });
			ExpectError(*this, TEXT("id repetido"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("rio"), 1), Crossing(TEXT("c1"), TEXT("rio"), 2) }, Cities), { TEXT("c1"), TEXT("repetido") });
		});

		It("ids de cruzamento que so diferem na caixa sao duplicados; id com caractere invalido e recusado", [this]()
		{
			ExpectError(*this, TEXT("C1 x c1"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("C1"), TEXT("rio"), 1), Crossing(TEXT("c1"), TEXT("rio"), 2) }, Cities), { TEXT("C1"), TEXT("c1"), TEXT("são o mesmo id") });
			ExpectError(*this, TEXT("espaco"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("cruz 1")) }, Cities), { TEXT("cruz 1"), TEXT("caractere") });
		});

		It("a referencia a cidade ignora a caixa, como o FName", [this]()
		{
			ExpectClean(*this, TEXT("Rio refere rio"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("Rio")) }, Cities));
		});

		It("referencia a cidade que nao existe e recusada", [this]()
		{
			ExpectError(*this, TEXT("cidade desconhecida"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("atlantida")) }, Cities), { TEXT("c1"), TEXT("atlantida") });
		});

		It("ordem repetida na MESMA cidade e recusada; em cidades diferentes e permitida", [this]()
		{
			ExpectError(*this, TEXT("mesma cidade"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("rio"), 1), Crossing(TEXT("c2"), TEXT("rio"), 1) }, Cities), { TEXT("ordem"), TEXT("rio") });
			ExpectClean(*this, TEXT("cidades diferentes"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("rio"), 1), Crossing(TEXT("c2"), TEXT("sp"), 1) }, Cities));
		});

		It("ordem zero ou negativa e recusada", [this]()
		{
			ExpectError(*this, TEXT("ordem 0"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("rio"), 0) }, Cities), { TEXT("c1"), TEXT("ordem") });
		});

		// Limites de medalha: os mesmos de FDNMedals::Validate (maiores que zero e estritamente crescentes).
		It("limites de medalha invalidos sao recusados", [this]()
		{
			ExpectError(*this, TEXT("bronze 0"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("rio"), 1, 0, 5000, 12000) }, Cities), { TEXT("c1"), TEXT("bronze") });
			ExpectError(*this, TEXT("prata = bronze"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("rio"), 1, 1000, 1000, 12000) }, Cities), { TEXT("c1"), TEXT("prata") });
			ExpectError(*this, TEXT("prata < bronze"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("rio"), 1, 1000, 500, 12000) }, Cities), { TEXT("c1"), TEXT("prata") });
			ExpectError(*this, TEXT("ouro = prata"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("rio"), 1, 1000, 5000, 5000) }, Cities), { TEXT("c1"), TEXT("ouro") });
			ExpectError(*this, TEXT("ouro < prata"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("rio"), 1, 1000, 5000, 100) }, Cities), { TEXT("c1"), TEXT("ouro") });
		});

		It("o menor conjunto valido de limites (1, 2, 3) passa", [this]()
		{
			ExpectClean(*this, TEXT("1 2 3"), FDNDataValidation::ValidateCrossings({ Crossing(TEXT("c1"), TEXT("rio"), 1, 1, 2, 3) }, Cities));
		});
	});
}

BEGIN_DEFINE_SPEC(FDNDataTrafficSpec, "Detonauto.Dados.Transito",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataTrafficSpec)

void FDNDataTrafficSpec::Define()
{
	Describe("TrafficVehicles.csv", [this]()
	{
		It("veiculos validos nao dao erro", [this]()
		{
			ExpectClean(*this, TEXT("dois"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("v1")), Vehicle(TEXT("v2"), TEXT("Sedan Modelo 2"), 1500, 1300.f, TEXT("sedan")) }));
		});

		It("id vazio e id repetido sao recusados", [this]()
		{
			ExpectError(*this, TEXT("id vazio"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("")) }), { TEXT("TrafficVehicles"), TEXT("id") });
			ExpectError(*this, TEXT("id repetido"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("v1")), Vehicle(TEXT("v1")) }), { TEXT("v1"), TEXT("repetido") });
		});

		It("ids de veiculo que so diferem na caixa sao duplicados; id com caractere invalido e recusado", [this]()
		{
			ExpectError(*this, TEXT("V1 x v1"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("V1")), Vehicle(TEXT("v1")) }), { TEXT("V1"), TEXT("v1"), TEXT("são o mesmo id") });
			ExpectError(*this, TEXT("espaco"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("carro um")) }), { TEXT("carro um"), TEXT("caractere") });
		});

		It("nome e classe vazios sao recusados", [this]()
		{
			ExpectError(*this, TEXT("nome vazio"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("v1"), TEXT("")) }), { TEXT("v1"), TEXT("nome") });
			ExpectError(*this, TEXT("classe vazia"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("v1"), TEXT("Nome"), 800, 950.f, TEXT("")) }), { TEXT("v1"), TEXT("classe") });
		});

		It("valor em dinheiro zero ou negativo e recusado; 1 passa", [this]()
		{
			ExpectError(*this, TEXT("valor 0"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("v1"), TEXT("Nome"), 0) }), { TEXT("v1"), TEXT("valor") });
			ExpectError(*this, TEXT("valor -5"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("v1"), TEXT("Nome"), -5) }), { TEXT("v1"), TEXT("valor") });
			ExpectClean(*this, TEXT("valor 1"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("v1"), TEXT("Nome"), 1) }));
		});

		It("massa zero, negativa ou NaN e recusada", [this]()
		{
			ExpectError(*this, TEXT("massa 0"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("v1"), TEXT("Nome"), 800, 0.f) }), { TEXT("v1"), TEXT("massa") });
			ExpectError(*this, TEXT("massa -1"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("v1"), TEXT("Nome"), 800, -1.f) }), { TEXT("v1"), TEXT("massa") });
			ExpectError(*this, TEXT("massa NaN"), FDNDataValidation::ValidateTrafficVehicles({ Vehicle(TEXT("v1"), TEXT("Nome"), 800, std::numeric_limits<float>::quiet_NaN()) }), { TEXT("v1"), TEXT("massa") });
		});
	});
}

BEGIN_DEFINE_SPEC(FDNDataTrafficSettingsSpec, "Detonauto.Dados.ConfigTrafego",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataTrafficSettingsSpec)

void FDNDataTrafficSettingsSpec::Define()
{
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	const float Inff = std::numeric_limits<float>::infinity();

	Describe("TrafficSettings.csv", [this, NaNf, Inff]()
	{
		It("a configuracao inicial (6, 25, 20, 200, 2, 2) e valida", [this]()
		{
			ExpectClean(*this, TEXT("inicial"), FDNDataValidation::ValidateTrafficSettings({ Settings() }));
		});

		It("exige EXATAMENTE uma linha", [this]()
		{
			ExpectError(*this, TEXT("zero linhas"), FDNDataValidation::ValidateTrafficSettings({}), { TEXT("TrafficSettings"), TEXT("uma linha") });
			ExpectError(*this, TEXT("duas linhas"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("a")), Settings(TEXT("b")) }), { TEXT("TrafficSettings"), TEXT("uma linha") });
		});

		It("id vazio, repetido ou com caractere invalido e recusado", [this]()
		{
			ExpectError(*this, TEXT("id vazio"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("")) }), { TEXT("TrafficSettings"), TEXT("id") });
			ExpectError(*this, TEXT("espaco"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao 1")) }), { TEXT("padrao 1"), TEXT("caractere") });
		});

		It("cada valor precisa ser maior que zero", [this]()
		{
			ExpectError(*this, TEXT("WakeDistanceM 0"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 0.f) }), { TEXT("WakeDistanceM"), TEXT("maior que zero") });
			ExpectError(*this, TEXT("WakeAheadM -1"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 6.f, -1.f) }), { TEXT("WakeAheadM"), TEXT("maior que zero") });
			ExpectError(*this, TEXT("WakeAheadMinClosingKmh 0"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 6.f, 25.f, 0.f) }), { TEXT("WakeAheadMinClosingKmh"), TEXT("maior que zero") });
			ExpectError(*this, TEXT("DespawnDistanceM 0"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 6.f, 25.f, 20.f, 0.f) }), { TEXT("DespawnDistanceM"), TEXT("maior que zero") });
			ExpectError(*this, TEXT("StillSpeedMs 0"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 6.f, 25.f, 20.f, 200.f, 0.f) }), { TEXT("StillSpeedMs"), TEXT("maior que zero") });
			ExpectError(*this, TEXT("MaxSpawnPerFrame 0"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 6.f, 25.f, 20.f, 200.f, 2.f, 0) }), { TEXT("MaxSpawnPerFrame"), TEXT("maior que zero") });
			ExpectError(*this, TEXT("MaxSpawnPerFrame -2"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 6.f, 25.f, 20.f, 200.f, 2.f, -2) }), { TEXT("MaxSpawnPerFrame"), TEXT("maior que zero") });
		});

		It("NaN e infinito sao recusados em cada campo decimal", [this, NaNf, Inff]()
		{
			ExpectError(*this, TEXT("WakeDistanceM NaN"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), NaNf) }), { TEXT("WakeDistanceM"), TEXT("finito") });
			ExpectError(*this, TEXT("WakeAheadM inf"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 6.f, Inff) }), { TEXT("WakeAheadM"), TEXT("finito") });
			ExpectError(*this, TEXT("closing NaN"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 6.f, 25.f, NaNf) }), { TEXT("WakeAheadMinClosingKmh"), TEXT("finito") });
			ExpectError(*this, TEXT("DespawnDistanceM inf"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 6.f, 25.f, 20.f, Inff) }), { TEXT("DespawnDistanceM"), TEXT("finito") });
			ExpectError(*this, TEXT("StillSpeedMs NaN"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 6.f, 25.f, 20.f, 200.f, NaNf) }), { TEXT("StillSpeedMs"), TEXT("finito") });
		});

		It("exige WakeDistanceM < WakeAheadM < DespawnDistanceM (estrito)", [this]()
		{
			ExpectClean(*this, TEXT("1 < 2 < 3"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 1.f, 2.f, 20.f, 3.f) }));
			ExpectError(*this, TEXT("Wake = WakeAhead"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 25.f, 25.f) }), { TEXT("WakeDistanceM"), TEXT("WakeAheadM"), TEXT("menor") });
			ExpectError(*this, TEXT("Wake > WakeAhead"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 30.f, 25.f) }), { TEXT("WakeDistanceM"), TEXT("WakeAheadM"), TEXT("menor") });
			ExpectError(*this, TEXT("WakeAhead = Despawn"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 6.f, 200.f, 20.f, 200.f) }), { TEXT("WakeAheadM"), TEXT("DespawnDistanceM"), TEXT("menor") });
			ExpectError(*this, TEXT("WakeAhead > Despawn"), FDNDataValidation::ValidateTrafficSettings({ Settings(TEXT("padrao"), 6.f, 300.f, 20.f, 200.f) }), { TEXT("WakeAheadM"), TEXT("DespawnDistanceM"), TEXT("menor") });
		});

		// lote 13b: seguir o carro da frente e rolar depois de acordar
		auto Follow = [](float MinGap, float CatchUp, float Rolling)
		{
			FDNTrafficSettingsEntry E = Settings();
			E.Row.FollowMinGapM = MinGap;
			E.Row.FollowCatchUpGapM = CatchUp;
			E.Row.RollingFriction = Rolling;
			return E;
		};

		It("folgas de seguir e atrito de rolar: maiores que zero e finitos", [this, Follow, NaNf, Inff]()
		{
			ExpectClean(*this, TEXT("8, 20, 0,02"), FDNDataValidation::ValidateTrafficSettings({ Follow(8.f, 20.f, 0.02f) }));
			ExpectError(*this, TEXT("FollowMinGapM 0"), FDNDataValidation::ValidateTrafficSettings({ Follow(0.f, 20.f, 0.02f) }), { TEXT("FollowMinGapM"), TEXT("maior que zero") });
			ExpectError(*this, TEXT("FollowCatchUpGapM NaN"), FDNDataValidation::ValidateTrafficSettings({ Follow(8.f, NaNf, 0.02f) }), { TEXT("FollowCatchUpGapM"), TEXT("finito") });
			ExpectError(*this, TEXT("RollingFriction 0"), FDNDataValidation::ValidateTrafficSettings({ Follow(8.f, 20.f, 0.f) }), { TEXT("RollingFriction"), TEXT("maior que zero") });
			ExpectError(*this, TEXT("RollingFriction inf"), FDNDataValidation::ValidateTrafficSettings({ Follow(8.f, 20.f, Inff) }), { TEXT("RollingFriction"), TEXT("finito") });
		});

		It("exige FollowMinGapM < FollowCatchUpGapM (estrito) e RollingFriction de no maximo 1", [this, Follow]()
		{
			ExpectError(*this, TEXT("min = alcance"), FDNDataValidation::ValidateTrafficSettings({ Follow(20.f, 20.f, 0.02f) }), { TEXT("FollowMinGapM"), TEXT("FollowCatchUpGapM"), TEXT("menor") });
			ExpectError(*this, TEXT("min > alcance"), FDNDataValidation::ValidateTrafficSettings({ Follow(25.f, 20.f, 0.02f) }), { TEXT("FollowMinGapM"), TEXT("FollowCatchUpGapM"), TEXT("menor") });
			ExpectClean(*this, TEXT("atrito 1"), FDNDataValidation::ValidateTrafficSettings({ Follow(8.f, 20.f, 1.f) }));
			ExpectError(*this, TEXT("atrito 1,01"), FDNDataValidation::ValidateTrafficSettings({ Follow(8.f, 20.f, 1.01f) }), { TEXT("RollingFriction"), TEXT("1") });
		});
	});
}
BEGIN_DEFINE_SPEC(FDNDataLoaderSpec, "Detonauto.Dados.Carregador",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataLoaderSpec)

void FDNDataLoaderSpec::Define()
{
	Describe("Leitura do CSV como DataTable", [this]()
	{
		It("le as linhas na ordem do arquivo, com a primeira coluna como id", [this]()
		{
			TArray<FDNRegionEntry> Out;
			const TArray<FString> Errors = FDNDataLoader::ParseEntries<FDNRegionRow>(TEXT("Id,Name,Order,Unlocked\nbrasil,Brasil,1,true\neuropa,Europa,2,false\n"), TEXT("Regions.csv"), Out);
			ExpectClean(*this, TEXT("leitura"), Errors);
			if (TestEqual("duas linhas", Out.Num(), 2))
			{
				TestEqual("id 1", Out[0].Id, FString(TEXT("brasil")));
				TestEqual("nome 1", Out[0].Row.Name, FString(TEXT("Brasil")));
				TestEqual("ordem 2", Out[1].Row.Order, 2);
				TestFalse("europa bloqueada", Out[1].Row.Unlocked);
			}
		});

		It("acentos sobrevivem (UTF-8)", [this]()
		{
			TArray<FDNCityEntry> Out;
			FDNDataLoader::ParseEntries<FDNCityRow>(TEXT("Id,Name,RegionId,Latitude,Longitude,District,Order,Unlocked,Playable\nsp,São Paulo,brasil,-23.5505,-46.6333,,2,true,false\n"), TEXT("Cities.csv"), Out);
			if (TestEqual("uma linha", Out.Num(), 1))
			{
				TestEqual("nome com acento", Out[0].Row.Name, FString(TEXT("São Paulo")));
				TestTrue("latitude", FMath::Abs(Out[0].Row.Latitude - (-23.5505)) < 1e-9);
				TestEqual("distrito vazio", Out[0].Row.District, FString());
			}
		});

		// O Git no Windows entrega os CSVs com CRLF (text=auto): o carregador precisa ler igual.
		It("le o mesmo conteudo com quebras de linha do Windows (CRLF)", [this]()
		{
			TArray<FDNCityEntry> Out;
			const TArray<FString> Errors = FDNDataLoader::ParseEntries<FDNCityRow>(
				TEXT("Id,Name,RegionId,Latitude,Longitude,District,Order,Unlocked,Playable\r\nrio,Rio,brasil,-22.9,-43.2,,1,true,false\r\nsp,São Paulo,brasil,-23.5,-46.6,,2,true,false\r\n"), TEXT("Cities.csv"), Out);
			ExpectClean(*this, TEXT("CRLF"), Errors);
			if (TestEqual("duas linhas", Out.Num(), 2))
			{
				TestEqual("id sem sobra de \\r", Out[1].Id, FString(TEXT("sp")));
				TestEqual("ultima coluna sem sobra de \\r", Out[1].Row.Playable, false);
				TestEqual("nome com acento", Out[1].Row.Name, FString(TEXT("São Paulo")));
			}
		});

		It("cabecalho sem linhas da lista vazia e nenhum erro", [this]()
		{
			TArray<FDNLandmarkEntry> Out;
			ExpectClean(*this, TEXT("so cabecalho"), FDNDataLoader::ParseEntries<FDNLandmarkRow>(TEXT("Id,CityId,Name,Kind,ShortText,SourceNote,NeedsReview\n"), TEXT("Landmarks.csv"), Out));
			TestEqual("zero linhas", Out.Num(), 0);
		});

		It("coluna faltando e relatada como erro", [this]()
		{
			TArray<FDNRegionEntry> Out;
			const TArray<FString> Errors = FDNDataLoader::ParseEntries<FDNRegionRow>(TEXT("Id,Name,Order\nbrasil,Brasil,1\n"), TEXT("Regions.csv"), Out);
			TestFalse(FString::Printf(TEXT("esperava erro de coluna faltando; veio: %s"), *Join(Errors)), Errors.IsEmpty());
			TestTrue("o erro cita o arquivo", HasError(Errors, { TEXT("Regions.csv") }));
		});

		It("id repetido no arquivo e relatado como erro", [this]()
		{
			TArray<FDNRegionEntry> Out;
			const TArray<FString> Errors = FDNDataLoader::ParseEntries<FDNRegionRow>(TEXT("Id,Name,Order,Unlocked\nbrasil,Brasil,1,true\nbrasil,Outro,2,true\n"), TEXT("Regions.csv"), Out);
			TestFalse(FString::Printf(TEXT("esperava erro de id repetido; veio: %s"), *Join(Errors)), Errors.IsEmpty());
		});

		It("valor que nao e numero e relatado como erro", [this]()
		{
			TArray<FDNRegionEntry> Out;
			const TArray<FString> Errors = FDNDataLoader::ParseEntries<FDNRegionRow>(TEXT("Id,Name,Order,Unlocked\nbrasil,Brasil,abc,true\n"), TEXT("Regions.csv"), Out);
			TestFalse(FString::Printf(TEXT("esperava erro de numero invalido; veio: %s"), *Join(Errors)), Errors.IsEmpty());
		});

		It("arquivo inexistente e relatado como erro", [this]()
		{
			TArray<FDNRegionEntry> Out;
			const TArray<FString> Errors = FDNDataLoader::LoadEntries<FDNRegionRow>(TEXT("Data/NaoExiste.csv"), Out);
			TestTrue(FString::Printf(TEXT("esperava erro de arquivo; veio: %s"), *Join(Errors)), HasError(Errors, { TEXT("NaoExiste.csv") }));
		});
	});
}

BEGIN_DEFINE_SPEC(FDNDataAllSpec, "Detonauto.Dados.Conjunto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataAllSpec)

void FDNDataAllSpec::Define()
{
	Describe("ValidateAll", [this]()
	{
		It("um conjunto coerente nao da erro", [this]()
		{
			FDNData Data;
			Data.Regions = { Region(TEXT("brasil")) };
			Data.Cities = { City(TEXT("rio")) };
			Data.Landmarks = { Landmark(TEXT("p1")) };
			Data.Crossings = { Crossing(TEXT("c1")) };
			Data.Vehicles = { Vehicle(TEXT("v1")) };
			Data.TrafficSettings = { Settings() };
			Data.TurboConfig = { DNTest::TurboEntry() };
			Data.CrashConfig = { DNTest::CrashEntry() };
			Data.TakedownConfig = { DNTest::TakedownEntry() };
			Data.DamageConfig = { DNTest::DamageEntry() };
			Data.CarConfigs = { DNTest::CarEntry() };
			Data.CameraModes = { DNTest::CameraEntry() };
			Data.Breakables = DNTest::EffectProps(); // poste e arvore: os tipos que Effects.csv cita
			Data.BreakableSettings = { DNTest::Entry<FDNBreakableSettingsEntry>(DNTest::BreakableSettingsRow()) };
			Data.CameraFx = { DNTest::Entry<FDNCameraFxEntry>(DNTest::CameraFxRow()) };
			Data.CarParts = { DNTest::Entry<FDNCarPartEntry>(DNTest::CarPartRow(), TEXT("para-choque")) };
			Data.CarGlass = { DNTest::Entry<FDNCarGlassEntry>(DNTest::CarGlassRow(), TEXT("vidro")) };
			Data.Effects = DNTest::EffectEntries();
			Data.EffectsSettings = { DNTest::Entry<FDNEffectsSettingsEntry>(DNTest::EffectsSettingsRow()) };
			ExpectClean(*this, TEXT("conjunto"), FDNDataValidation::ValidateAll(Data));
		});

		It("junta os erros de todas as tabelas", [this]()
		{
			FDNData Data;
			Data.Regions = { Region(TEXT("brasil")) };
			Data.Cities = { City(TEXT("rio"), TEXT("atlantida")) };
			Data.Landmarks = { Landmark(TEXT("p1"), TEXT("sumiu")) };
			Data.Crossings = { Crossing(TEXT("c1"), TEXT("sumiu")) };
			Data.Vehicles = { Vehicle(TEXT("v1"), TEXT("Nome"), 0) };
			Data.TrafficSettings = { Settings(TEXT("padrao"), 30.f, 25.f) };
			Data.TurboConfig = { DNTest::TurboEntry() };
			Data.TurboConfig[0].Row.MaxBar = 0.f;
			Data.CrashConfig = { DNTest::CrashEntry() };
			Data.CrashConfig[0].Row.MaxMultiplier = 0;
			Data.TakedownConfig = { DNTest::TakedownEntry() };
			Data.TakedownConfig[0].Row.ZoneHalfAngleDegrees = 90.f;
			Data.DamageConfig = { DNTest::DamageEntry() };
			Data.DamageConfig[0].Row.MaxDents = 0;
			Data.CarConfigs = { DNTest::CarEntry() };
			Data.CarConfigs[0].Row.MassKg = 1.f;
			Data.CameraModes = { DNTest::CameraEntry() };
			Data.CameraModes[0].Row.Fov = 1.f;
			Data.Breakables = { DNTest::Entry<FDNBreakableEntry>(DNTest::BreakableRow(), TEXT("poste")) };
			Data.Breakables[0].Row.MinBreakSpeedKmh = -1.f;
			Data.BreakableSettings = { DNTest::Entry<FDNBreakableSettingsEntry>(DNTest::BreakableSettingsRow()) };
			Data.BreakableSettings[0].Row.MaxActiveProps = 0;
			Data.CameraFx = { DNTest::Entry<FDNCameraFxEntry>(DNTest::CameraFxRow()) };
			Data.CameraFx[0].Row.CrashCamTimeScale = 0.f;
			Data.CarParts = { DNTest::Entry<FDNCarPartEntry>(DNTest::CarPartRow(), TEXT("para-choque")) };
			Data.CarParts[0].Row.MassKg = 0.f;
			Data.CarGlass = { DNTest::Entry<FDNCarGlassEntry>(DNTest::CarGlassRow(), TEXT("vidro")) };
			Data.CarGlass[0].Row.CrackFrom = 2.f;
			Data.Effects = DNTest::EffectEntries();
			Data.Effects[0].Row.PoolMax = 0;
			Data.EffectsSettings = { DNTest::Entry<FDNEffectsSettingsEntry>(DNTest::EffectsSettingsRow()) };
			Data.EffectsSettings[0].Row.MaxSmokeSprites = 0;
			const TArray<FString> Errors = FDNDataValidation::ValidateAll(Data);
			ExpectError(*this, TEXT("efeitos"), Errors, { TEXT("Effects.csv"), TEXT("PoolMax") });
			ExpectError(*this, TEXT("tetos dos efeitos"), Errors, { TEXT("EffectsSettings.csv"), TEXT("MaxSmokeSprites") });
			ExpectError(*this, TEXT("pecas do carro"), Errors, { TEXT("CarParts.csv"), TEXT("MassKg") });
			ExpectError(*this, TEXT("vidros do carro"), Errors, { TEXT("CarGlass.csv"), TEXT("CrackFrom") });
			ExpectError(*this, TEXT("config da camera"), Errors, { TEXT("CameraFx.csv"), TEXT("CrashCamTimeScale") });
			ExpectError(*this, TEXT("mobiliario"), Errors, { TEXT("Breakables.csv"), TEXT("MinBreakSpeedKmh") });
			ExpectError(*this, TEXT("config do mobiliario"), Errors, { TEXT("BreakableSettings.csv"), TEXT("MaxActiveProps") });
			ExpectError(*this, TEXT("carros"), Errors, { TEXT("Cars.csv"), TEXT("MassKg") });
			ExpectError(*this, TEXT("cameras"), Errors, { TEXT("Cameras.csv"), TEXT("Fov") });
			ExpectError(*this, TEXT("config do dano"), Errors, { TEXT("Damage.csv"), TEXT("MaxDents") });
			ExpectError(*this, TEXT("config do turbo"), Errors, { TEXT("Turbo.csv"), TEXT("MaxBar") });
			ExpectError(*this, TEXT("config do crash"), Errors, { TEXT("Crash.csv"), TEXT("MaxMultiplier") });
			ExpectError(*this, TEXT("config do takedown"), Errors, { TEXT("Takedown.csv"), TEXT("ZoneHalfAngleDegrees") });
			ExpectError(*this, TEXT("cidade"), Errors, { TEXT("rio"), TEXT("atlantida") });
			ExpectError(*this, TEXT("ponto"), Errors, { TEXT("p1"), TEXT("sumiu") });
			ExpectError(*this, TEXT("cruzamento"), Errors, { TEXT("c1"), TEXT("sumiu") });
			ExpectError(*this, TEXT("veiculo"), Errors, { TEXT("v1"), TEXT("valor") });
			ExpectError(*this, TEXT("configuracao do trafego"), Errors, { TEXT("WakeDistanceM"), TEXT("menor") });
		});

		It("a falta da configuracao do trafego e um erro", [this]()
		{
			FDNData Data;
			Data.Regions = { Region(TEXT("brasil")) };
			ExpectError(*this, TEXT("sem configuracao"), FDNDataValidation::ValidateAll(Data), { TEXT("TrafficSettings"), TEXT("uma linha") });
			const TArray<FString> Errors = FDNDataValidation::ValidateAll(Data);
			ExpectError(*this, TEXT("sem turbo"), Errors, { TEXT("Turbo.csv"), TEXT("uma linha") });
			ExpectError(*this, TEXT("sem crash"), Errors, { TEXT("Crash.csv"), TEXT("uma linha") });
			ExpectError(*this, TEXT("sem takedown"), Errors, { TEXT("Takedown.csv"), TEXT("uma linha") });
			ExpectError(*this, TEXT("sem dano"), Errors, { TEXT("Damage.csv"), TEXT("uma linha") });
			ExpectError(*this, TEXT("sem carros"), Errors, { TEXT("Cars.csv"), TEXT("Padrao") });
			ExpectError(*this, TEXT("sem cameras"), Errors, { TEXT("Cameras.csv"), TEXT("alta-longe") });
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
