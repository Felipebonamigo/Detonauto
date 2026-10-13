// Testes da cola fina entre o Core e o motor que roda sem mundo de jogo:
//  - UDNGameDataSubsystem: carrega os CSVs reais, valida, e erro NAO derruba o jogo (so marca invalido e loga);
//  - UDNRecordsSubsystem: arquivo de save com escrita atomica e arquivo corrompido preservado.
// O que so da para testar jogando esta em docs/pendente-playtest.md.
//
// Decisoes de implementacao (o contrato do lote 7 nao cobre; ver os headers):
//  S1 dados invalidos: as configuracoes de turbo, Crash e takedown voltam ao padrao da struct pura SE a tabela delas
//     for invalida (o resto do jogo segue com o que carregou), cada uma isolada das outras tabelas;
//  S2 RegisterResult grava o arquivo sozinho quando bate recorde (falha de gravacao e logada, o recorde fica na memoria);
//  S3 arquivo vazio ou ilegivel conta como corrompido (Deserialize devolve erro): vai para .corrompido-<data>;
//  S4 arquivo que nao existe e a primeira execucao: registro vazio, sem erro.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "Core/DNGameDataSubsystem.h"
#include "Core/DNRecordsSubsystem.h"
#include "Core/Tests/DNTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Pasta temporaria propria do teste, dentro de Saved/Automation. Apagada no fim. */
	struct FTempDir
	{
		FString Dir;
		FTempDir() : Dir(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("DNSubsystems"), FGuid::NewGuid().ToString(EGuidFormats::Digits))) {}
		~FTempDir() { IFileManager::Get().DeleteDirectory(*Dir, false, true); }
		FString File(const TCHAR* Name) const { return FPaths::Combine(Dir, Name); }
	};

	FString Read(const FString& Path)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *Path);
		return Text;
	}

	bool Write(const FString& Path, const FString& Text)
	{
		return FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	/** Arquivos da pasta cujo nome comeca com o prefixo. */
	TArray<FString> Find(const FTempDir& Temp, const FString& Pattern)
	{
		TArray<FString> Found;
		IFileManager::Get().FindFiles(Found, *FPaths::Combine(Temp.Dir, Pattern), true, false);
		return Found;
	}

	UDNRecordsSubsystem* NewRecords()
	{
		return DNTest::NewSubsystem<UDNRecordsSubsystem>();
	}
}

BEGIN_DEFINE_SPEC(FDNGameDataSubsystemSpec, "Detonauto.Subsistemas.Dados",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNGameDataSubsystemSpec)

void FDNGameDataSubsystemSpec::Define()
{
	Describe("UDNGameDataSubsystem", [this]()
	{
		It("carrega os CSVs reais, valida, e entrega as configuracoes iguais ao padrao", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			Sub->Reload();
			TestTrue(FString::Printf(TEXT("dados validos; erros: %s"), *FString::Join(Sub->GetErrors(), TEXT(" | "))), Sub->AreTablesValid());
			TestEqual("sem erros", Sub->GetErrors().Num(), 0);
			TestTrue("regioes", Sub->GetData().Regions.Num() > 0);
			TestTrue("cidades", Sub->GetData().Cities.Num() > 0);
			TestEqual("uma linha de turbo", Sub->GetData().TurboConfig.Num(), 1);
			TestEqual("uma linha de Crash", Sub->GetData().CrashConfig.Num(), 1);
			TestEqual("uma linha de takedown", Sub->GetData().TakedownConfig.Num(), 1);
			TestEqual("uma linha de trafego", Sub->GetData().TrafficSettings.Num(), 1);
			TestEqual("MaxBar vem do CSV", Sub->GetTurboConfig().MaxBar, FDNTurboConfig().MaxBar);
			TestEqual("MaxMultiplier vem do CSV", Sub->GetCrashConfig().MaxMultiplier, FDNCrashConfig().MaxMultiplier);
			TestEqual("ZoneHalfAngleDegrees vem do CSV", Sub->GetTakedownConfig().ZoneHalfAngleDegrees, FDNTakedownConfig().ZoneHalfAngleDegrees);
		});

		It("antes de carregar nada: invalido e com as configuracoes padrao", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			TestFalse("invalido", Sub->AreTablesValid());
			TestEqual("MaxBar padrao", Sub->GetTurboConfig().MaxBar, FDNTurboConfig().MaxBar);
		});

		It("o valor vem do CSV (nao do padrao): linha com outro numero chega ao Get", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			FDNData Data;
			Data.TurboConfig = { DNTest::TurboEntry() };
			Data.TurboConfig[0].Row.MaxBar = 200.f;
			Data.TurboConfig[0].Row.TakedownSetsBarTo = 150.f;
			Data.CrashConfig = { DNTest::CrashEntry() };
			Data.CrashConfig[0].Row.MaxMultiplier = 4;
			Data.TakedownConfig = { DNTest::TakedownEntry() };
			Data.TakedownConfig[0].Row.MinRelativeSpeedKmh = 33.f;
			AddExpectedError(TEXT("Dados do jogo:"), EAutomationExpectedErrorFlags::Contains, 0); // outras tabelas vazias dao erro
			Sub->ApplyData(MoveTemp(Data), {});
			TestEqual("MaxBar", Sub->GetTurboConfig().MaxBar, 200.f);
			TestEqual("TakedownSetsBarTo", Sub->GetTurboConfig().TakedownSetsBarTo, 150.f);
			TestEqual("MaxMultiplier", Sub->GetCrashConfig().MaxMultiplier, 4);
			TestEqual("MinRelativeSpeedKmh", Sub->GetTakedownConfig().MinRelativeSpeedKmh, 33.f);
		});

		It("erro de validacao NAO derruba o jogo: marca invalido, guarda os erros, e a configuracao ruim volta ao padrao", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			FDNData Data;
			Data.TurboConfig = { DNTest::TurboEntry() };
			Data.TurboConfig[0].Row.MaxBar = -5.f; // invalida
			Data.CrashConfig = { DNTest::CrashEntry() };
			Data.CrashConfig[0].Row.MaxMultiplier = 4; // valida: continua valendo
			AddExpectedError(TEXT("Dados do jogo:"), EAutomationExpectedErrorFlags::Contains, 0);
			Sub->ApplyData(MoveTemp(Data), { TEXT("Fake.csv: erro do carregador") });
			TestFalse("dados invalidos", Sub->AreTablesValid());
			TestTrue("o erro do carregador esta na lista", Sub->GetErrors().ContainsByPredicate([](const FString& E) { return E.Contains(TEXT("Fake.csv")); }));
			TestTrue("o erro de validacao esta na lista", Sub->GetErrors().ContainsByPredicate([](const FString& E) { return E.Contains(TEXT("MaxBar")); }));
			TestEqual("turbo invalido volta ao padrao", Sub->GetTurboConfig().MaxBar, FDNTurboConfig().MaxBar);
			TestEqual("Crash valido segue valendo", Sub->GetCrashConfig().MaxMultiplier, 4);
		});

		It("sem nenhuma linha de configuracao: padrao, e nao quebra", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			AddExpectedError(TEXT("Dados do jogo:"), EAutomationExpectedErrorFlags::Contains, 0);
			Sub->ApplyData(FDNData(), {});
			TestFalse("invalido", Sub->AreTablesValid());
			TestEqual("padrao", Sub->GetCrashConfig().MaxMultiplier, FDNCrashConfig().MaxMultiplier);
			TestEqual("padrao", Sub->GetTakedownConfig().MinRelativeSpeedKmh, FDNTakedownConfig().MinRelativeSpeedKmh);
		});

		It("recarregar substitui o estado anterior (nao acumula erros)", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			AddExpectedError(TEXT("Dados do jogo:"), EAutomationExpectedErrorFlags::Contains, 0);
			Sub->ApplyData(FDNData(), { TEXT("x.csv: erro") });
			TestFalse("invalido", Sub->AreTablesValid());
			Sub->Reload();
			TestTrue("valido depois de recarregar os arquivos reais", Sub->AreTablesValid());
			TestEqual("sem erros", Sub->GetErrors().Num(), 0);
		});
	});
}

BEGIN_DEFINE_SPEC(FDNRecordsSubsystemSpec, "Detonauto.Subsistemas.Registro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNRecordsSubsystemSpec)

void FDNRecordsSubsystemSpec::Define()
{
	Describe("UDNRecordsSubsystem", [this]()
	{
		It("o caminho padrao e Saved/SaveGames/Detonauto.json", [this]()
		{
			const FString Path = FPaths::ConvertRelativePathToFull(UDNRecordsSubsystem::DefaultSavePath());
			const FString Expected = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("Detonauto.json")));
			TestEqual("caminho", Path, Expected);
		});

		It("S4: arquivo que nao existe e a primeira execucao: vazio, sem erro, sem criar nada", [this]()
		{
			FTempDir Temp;
			UDNRecordsSubsystem* Sub = NewRecords();
			TestTrue("carregou", Sub->LoadFromFile(Temp.File(TEXT("Detonauto.json"))));
			TestEqual("vazio", Sub->GetRecords().Num(), 0);
			TestEqual("nada foi criado", Find(Temp, TEXT("*")).Num(), 0);
		});

		It("grava e volta: Save + LoadFromFile em outra instancia; sem .tmp sobrando", [this]()
		{
			FTempDir Temp;
			const FString Path = Temp.File(TEXT("Detonauto.json"));
			UDNRecordsSubsystem* A = NewRecords();
			A->SetSavePath(Path);
			A->RegisterResult(TEXT("exemplo-rio-01"), 4200); // S2: grava sozinho
			A->RegisterResult(TEXT("Sao-Paulo-02"), 0);
			TestTrue("arquivo existe", IFileManager::Get().FileExists(*Path));
			TestEqual("texto do arquivo", Read(Path), A->GetRecords().Serialize());
			TestEqual("nenhum .tmp sobrando", Find(Temp, TEXT("*.tmp")).Num(), 0);

			UDNRecordsSubsystem* B = NewRecords();
			TestTrue("carregou", B->LoadFromFile(Path));
			TestTrue("registro igual", B->GetRecords() == A->GetRecords());
		});

		It("S2: so grava quando bate recorde; pior resultado nao mexe no arquivo", [this]()
		{
			FTempDir Temp;
			const FString Path = Temp.File(TEXT("Detonauto.json"));
			UDNRecordsSubsystem* Sub = NewRecords();
			Sub->SetSavePath(Path);
			TestTrue("recorde", Sub->RegisterResult(TEXT("a"), 1000));
			const FString Before = Read(Path);
			Write(Path, Before + TEXT(" ")); // marca: se regravar, a marca some
			TestFalse("pior", Sub->RegisterResult(TEXT("a"), 500));
			TestFalse("negativo", Sub->RegisterResult(TEXT("a"), -1));
			TestEqual("arquivo intacto", Read(Path), Before + TEXT(" "));
		});

		It("escrita atomica: se nao da para gravar o .tmp, o arquivo antigo fica intacto e Save devolve falso", [this]()
		{
			FTempDir Temp;
			const FString Path = Temp.File(TEXT("Detonauto.json"));
			UDNRecordsSubsystem* Sub = NewRecords();
			Sub->SetSavePath(Path);
			Sub->RegisterResult(TEXT("a"), 100);
			const FString Old = Read(Path);
			TestEqual("gravou", Old, FString(TEXT("{\"version\":1,\"best\":{\"a\":100}}")));

			// Uma PASTA no lugar do .tmp: gravar o temporario falha. Gravar direto no arquivo final funcionaria.
			IFileManager::Get().MakeDirectory(*(Path + TEXT(".tmp")), true);
			AddExpectedError(TEXT("Registro do jogador:"), EAutomationExpectedErrorFlags::Contains, 0);
			Sub->RegisterResult(TEXT("a"), 999); // recorde na memoria; gravacao falha
			TestEqual("o recorde fica na memoria", Sub->GetRecords().BestScore(TEXT("a")).Get(-1), 999);
			TestFalse("Save falha", Sub->Save());
			TestEqual("arquivo antigo intacto", Read(Path), Old);
		});

		It("arquivo corrompido: NAO e sobrescrito; vai para .corrompido-<data> com o conteudo original, e comeca vazio", [this]()
		{
			FTempDir Temp;
			const FString Path = Temp.File(TEXT("Detonauto.json"));
			const FString Garbage = TEXT("isto nao e json {{{");
			Write(Path, Garbage);

			UDNRecordsSubsystem* Sub = NewRecords();
			AddExpectedError(TEXT("Registro do jogador:"), EAutomationExpectedErrorFlags::Contains, 0);
			TestFalse("devolve falso: o arquivo estava corrompido", Sub->LoadFromFile(Path));
			TestEqual("comeca vazio", Sub->GetRecords().Num(), 0);
			TestFalse("o arquivo original saiu do caminho", IFileManager::Get().FileExists(*Path));

			const TArray<FString> Backups = Find(Temp, TEXT("Detonauto.json.corrompido-*"));
			if (!TestEqual("uma copia .corrompido", Backups.Num(), 1)) { return; }
			TestEqual("copia com o conteudo original", Read(FPaths::Combine(Temp.Dir, Backups[0])), Garbage);
			TestEqual("o caminho da copia fica registrado", FPaths::GetCleanFilename(Sub->GetLastCorruptedBackupPath()), Backups[0]);

			// Gravar depois cria um arquivo novo e NAO toca na copia.
			Sub->SetSavePath(Path);
			Sub->RegisterResult(TEXT("a"), 7);
			TestEqual("arquivo novo", Read(Path), FString(TEXT("{\"version\":1,\"best\":{\"a\":7}}")));
			TestEqual("copia intacta", Read(FPaths::Combine(Temp.Dir, Backups[0])), Garbage);
		});

		It("S3: arquivo vazio e versao desconhecida tambem contam como corrompidos e sao preservados", [this]()
		{
			AddExpectedError(TEXT("Registro do jogador:"), EAutomationExpectedErrorFlags::Contains, 0);
			for (const TCHAR* Content : { TEXT(""), TEXT("{\"version\":2,\"best\":{\"a\":1}}"), TEXT("[]"), TEXT("{\"version\":1}") })
			{
				FTempDir Temp;
				const FString Path = Temp.File(TEXT("Detonauto.json"));
				Write(Path, Content);
				UDNRecordsSubsystem* Sub = NewRecords();
				TestFalse(FString::Printf(TEXT("'%s' conta como corrompido"), Content), Sub->LoadFromFile(Path));
				TestEqual("vazio", Sub->GetRecords().Num(), 0);
				const TArray<FString> Backups = Find(Temp, TEXT("Detonauto.json.corrompido-*"));
				if (TestEqual("uma copia", Backups.Num(), 1))
				{
					TestEqual("conteudo preservado", Read(FPaths::Combine(Temp.Dir, Backups[0])), FString(Content));
				}
			}
		});

		It("duas corrupcoes seguidas nao se sobrescrevem (nome da copia unico)", [this]()
		{
			FTempDir Temp;
			const FString Path = Temp.File(TEXT("Detonauto.json"));
			AddExpectedError(TEXT("Registro do jogador:"), EAutomationExpectedErrorFlags::Contains, 0);
			Write(Path, TEXT("lixo 1"));
			NewRecords()->LoadFromFile(Path);
			Write(Path, TEXT("lixo 2"));
			NewRecords()->LoadFromFile(Path);
			TArray<FString> Contents;
			for (const FString& Name : Find(Temp, TEXT("Detonauto.json.corrompido-*"))) { Contents.Add(Read(FPaths::Combine(Temp.Dir, Name))); }
			Contents.Sort();
			if (TestEqual("duas copias", Contents.Num(), 2))
			{
				TestEqual("primeira", Contents[0], FString(TEXT("lixo 1")));
				TestEqual("segunda", Contents[1], FString(TEXT("lixo 2")));
			}
		});

		It("avisos de entrada ruim nao movem o arquivo: so loga e carrega as boas", [this]()
		{
			FTempDir Temp;
			const FString Path = Temp.File(TEXT("Detonauto.json"));
			const FString Content = TEXT("{\"version\":1,\"best\":{\"ok\":10,\"ruim\":-5}}");
			Write(Path, Content);
			UDNRecordsSubsystem* Sub = NewRecords();
			AddExpectedError(TEXT("Entrada 'ruim' ignorada"), EAutomationExpectedErrorFlags::Contains, 1);
			TestTrue("carregou", Sub->LoadFromFile(Path));
			TestEqual("so a boa", Sub->GetRecords().Num(), 1);
			TestEqual("arquivo no lugar", Read(Path), Content);
			TestEqual("sem copia", Find(Temp, TEXT("*.corrompido-*")).Num(), 0);
		});

		It("ProgressoParaMenu: medalhas dos limites atuais viram os contadores do menu", [this]()
		{
			FDNData Data;
			{
				FDNRegionEntry Reg; Reg.Id = TEXT("brasil"); Reg.Row.Name = TEXT("Brasil"); Reg.Row.Order = 1;
				Data.Regions = { Reg };
				FDNCityEntry Rio; Rio.Id = TEXT("rio"); Rio.Row.RegionId = TEXT("brasil"); Rio.Row.Playable = true; Rio.Row.District = TEXT("d"); Rio.Row.Order = 1;
				FDNCityEntry Sp; Sp.Id = TEXT("sp"); Sp.Row.RegionId = TEXT("brasil"); Sp.Row.Playable = false; Sp.Row.Order = 2;
				Data.Cities = { Rio, Sp };
				auto X = [](const TCHAR* Id, const TCHAR* City, int32 Order)
				{
					FDNCrossingEntry C; C.Id = Id; C.Row.CityId = City; C.Row.Order = Order; C.Row.Bronze = 1000; C.Row.Silver = 2000; C.Row.Gold = 3000; return C;
				};
				Data.Crossings = { X(TEXT("rio-1"), TEXT("rio"), 1), X(TEXT("rio-2"), TEXT("rio"), 2), X(TEXT("rio-3"), TEXT("rio"), 3), X(TEXT("sp-1"), TEXT("sp"), 1) };
			}
			FTempDir Temp;
			UDNRecordsSubsystem* Sub = NewRecords();
			Sub->SetSavePath(Temp.File(TEXT("Detonauto.json")));
			Sub->RegisterResult(TEXT("rio-1"), 3500); // ouro
			Sub->RegisterResult(TEXT("rio-2"), 1200); // bronze
			Sub->RegisterResult(TEXT("rio-3"), 500);  // abaixo do bronze: nao conta
			const FDNProgressResult P = Sub->ProgressoParaMenu(Data);
			TestEqual("feitos", P.Overall.Done, 2);
			TestEqual("total jogavel (sp nao entra)", P.Overall.Total, 3);
			TestEqual("ouros", P.Overall.Gold, 1);
			TestEqual("sem avisos", P.Warnings.Num(), 0);
		});

		It("ProgressoParaMenu sem a fonte de dados: resultado vazio e erro logado, sem quebrar", [this]()
		{
			UDNRecordsSubsystem* Sub = NewRecords();
			AddExpectedError(TEXT("Registro do jogador:"), EAutomationExpectedErrorFlags::Contains, 0);
			const FDNProgressResult P = Sub->ProgressoParaMenu();
			TestEqual("total", P.Overall.Total, 0);
		});

		It("ProgressoParaMenu com a fonte de dados dos CSVs reais", [this]()
		{
			UDNGameDataSubsystem* Data = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			Data->Reload();
			UDNRecordsSubsystem* Sub = NewRecords();
			Sub->SetDataSource(Data);
			const FDNProgressResult P = Sub->ProgressoParaMenu();
			TestEqual("nada jogado: nada feito", P.Overall.Done, 0);
			TSet<FName> PlayableCities;
			for (const FDNCityEntry& C : Data->GetData().Cities) { if (C.Row.Playable) { PlayableCities.Add(FName(*C.Id)); } }
			int32 Expected = 0;
			for (const FDNCrossingEntry& X : Data->GetData().Crossings) { if (PlayableCities.Contains(FName(*X.Row.CityId))) { ++Expected; } }
			TestEqual("o total e o de cruzamentos de cidades jogaveis dos CSVs", P.Overall.Total, Expected);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
