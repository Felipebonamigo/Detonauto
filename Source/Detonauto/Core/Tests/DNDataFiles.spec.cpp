// Testa os CSVs REAIS de Data/ (na raiz do repositório): precisam importar sem problema e passar em todos os
// validadores, com zero erros.
//
// Caminho: FPaths::ProjectDir() é a pasta do Detonauto.uproject, que é a raiz do repositório. O test.ps1 abre
// o jogo com o caminho absoluto do .uproject, então Data/ é achada sem configuração nenhuma.

#include "Misc/AutomationTest.h"
#include "Core/Data/DNDataValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDataFilesSpec, "Detonauto.Dados.Arquivos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataFilesSpec)

void FDNDataFilesSpec::Define()
{
	Describe("Data/*.csv reais", [this]()
	{
		It("importam como DataTable e passam em todos os validadores, com zero erros", [this]()
		{
			FDNData Data;
			const TArray<FString> LoadErrors = FDNDataLoader::LoadAll(Data);
			for (const FString& E : LoadErrors) { AddError(E); }
			TestEqual("erros de leitura", LoadErrors.Num(), 0);

			const TArray<FString> Errors = FDNDataValidation::ValidateAll(Data);
			for (const FString& E : Errors) { AddError(E); }
			TestEqual("erros de validacao", Errors.Num(), 0);
		});

		It("a pasta Data/ esta onde o teste espera (raiz do projeto)", [this]()
		{
			const FString Path = FDNDataLoader::DataPath(TEXT("Regions.csv"));
			TestTrue(FString::Printf(TEXT("%s existe"), *Path), IFileManager::Get().FileExists(*Path));
		});

		It("so o Brasil como regiao, com Rio de Janeiro e Sao Paulo, ainda nao jogaveis", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			TestEqual("uma regiao", Data.Regions.Num(), 1);
			if (Data.Regions.Num() == 1) { TestEqual("id da regiao", Data.Regions[0].Id, FString(TEXT("brasil"))); }

			TestEqual("duas cidades", Data.Cities.Num(), 2);
			const FDNCityEntry* Rio = Data.Cities.FindByPredicate([](const FDNCityEntry& C) { return C.Id == TEXT("rio-de-janeiro"); });
			const FDNCityEntry* Sp = Data.Cities.FindByPredicate([](const FDNCityEntry& C) { return C.Id == TEXT("sao-paulo"); });
			if (TestNotNull("Rio de Janeiro existe", Rio))
			{
				TestEqual("nome do Rio", Rio->Row.Name, FString(TEXT("Rio de Janeiro")));
				TestTrue("latitude do Rio", FMath::Abs(Rio->Row.Latitude - (-22.9068)) < 1e-9);
				TestTrue("longitude do Rio", FMath::Abs(Rio->Row.Longitude - (-43.1729)) < 1e-9);
				TestFalse("Rio ainda nao jogavel", Rio->Row.Playable);
			}
			if (TestNotNull("Sao Paulo existe", Sp))
			{
				TestEqual("nome com acento", Sp->Row.Name, FString(TEXT("São Paulo")));
				TestTrue("latitude de Sao Paulo", FMath::Abs(Sp->Row.Latitude - (-23.5505)) < 1e-9);
				TestTrue("longitude de Sao Paulo", FMath::Abs(Sp->Row.Longitude - (-46.6333)) < 1e-9);
				TestFalse("Sao Paulo ainda nao jogavel", Sp->Row.Playable);
			}
		});

		// Regra do CLAUDE.md: nenhum fato sobre as cidades sem fonte conferida.
		It("nenhum texto de ponto turistico entra sem revisao", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			for (const FDNLandmarkEntry& L : Data.Landmarks)
			{
				TestTrue(FString::Printf(TEXT("%s esta marcado para revisao"), *L.Id), L.Row.NeedsReview);
				TestTrue(FString::Printf(TEXT("%s tem texto vazio"), *L.Id), L.Row.ShortText.IsEmpty());
			}
		});

		It("os cruzamentos de exemplo estao marcados como exemplo no id", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			for (const FDNCrossingEntry& C : Data.Crossings)
			{
				TestTrue(FString::Printf(TEXT("%s comeca com 'exemplo'"), *C.Id), C.Id.StartsWith(TEXT("exemplo")));
			}
		});

		It("a configuracao do trafego tem uma linha com os valores iniciais do contrato", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			if (TestEqual("uma linha", Data.TrafficSettings.Num(), 1))
			{
				const FDNTrafficSettingsRow& S = Data.TrafficSettings[0].Row;
				TestEqual("id", Data.TrafficSettings[0].Id, FString(TEXT("padrao")));
				TestEqual("WakeDistanceM", S.WakeDistanceM, 6.0f);
				TestEqual("WakeAheadM", S.WakeAheadM, 25.0f);
				TestEqual("WakeAheadMinClosingKmh", S.WakeAheadMinClosingKmh, 20.0f);
				TestEqual("DespawnDistanceM", S.DespawnDistanceM, 200.0f);
				TestEqual("StillSpeedMs", S.StillSpeedMs, 2.0f);
				TestEqual("MaxSpawnPerFrame", S.MaxSpawnPerFrame, 2);
				TestEqual("FollowMinGapM", S.FollowMinGapM, 8.0f);
				TestEqual("FollowCatchUpGapM", S.FollowCatchUpGapM, 20.0f);
				TestEqual("RollingFriction", S.RollingFriction, 0.02f);
				// deriva: o CSV é igual ao padrão da struct pura em todos os campos novos
				const FDNTrafficSettings Csv = S.ToSettings();
				const FDNTrafficSettings Def;
				TestEqual("deriva FollowMinGapM", Csv.FollowMinGapM, Def.FollowMinGapM);
				TestEqual("deriva FollowCatchUpGapM", Csv.FollowCatchUpGapM, Def.FollowCatchUpGapM);
				TestEqual("deriva RollingFriction", Csv.RollingFriction, Def.RollingFriction);
			}
		});

		It("o transito tem de 3 a 5 veiculos de exemplo", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			TestTrue(FString::Printf(TEXT("%d veiculos"), Data.Vehicles.Num()), Data.Vehicles.Num() >= 3 && Data.Vehicles.Num() <= 5);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
