// Testes de Data/Breakables.csv e Data/BreakableSettings.csv (mobiliário destrutível, docs/regras-mobiliario.md): validação por
// campo, os valores iniciais do contrato no CSV real, a linha de configuração igual ao padrão da struct, e a busca no subsistema.
//
// Faixas: MassKg (0, 5000], MinBreakSpeedKmh [0, 200], LaunchFactor [0, 3], LaunchUpMs [0, 20], BreakHeightCm [0, 500], Points >= 0;
// configuração com exatamente uma linha: ArcadeMassFactor (0, 10], MaxSpeedLossPct [0, 100], MaxLaunchMs (0, 100],
// MaxActiveProps >= 1, SleepSpeedMs (0, 10], SleepAfterS (0, 60], PlowFactor [1, 3], PlowUpMs [0, 10]. (IgnoreCarS saiu no 17c:
// destroço nunca mais segura o carro; o carro varre o destroço que alcança, B9.)

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/Data/DNDataLoader.h"
#include "Core/Data/DNDataValidation.h"
#include "Core/DNGameDataSubsystem.h"
#include "Core/Tests/DNTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDataBreakablesSpec, "Detonauto.Dados.Mobiliario",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();

	static FString JoinErrors(const TArray<FString>& Errors) { return Errors.IsEmpty() ? FString(TEXT("(nenhum erro)")) : FString::Join(Errors, TEXT(" | ")); }

	static bool Cites(const TArray<FString>& Errors, const TCHAR* Needle)
	{
		for (const FString& E : Errors) { if (E.Contains(Needle)) { return true; } }
		return false;
	}

	void BadKind(const TCHAR* What, const TCHAR* Field, TFunction<void(FDNBreakableRow&)> Tweak)
	{
		FDNBreakableRow R = DNTest::BreakableRow();
		Tweak(R);
		const TArray<FString> Errors = FDNDataValidation::ValidateBreakables({ DNTest::Entry<FDNBreakableEntry>(R, TEXT("poste")) });
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Field, *JoinErrors(Errors)), Cites(Errors, Field));
	}

	void GoodKind(const TCHAR* What, TFunction<void(FDNBreakableRow&)> Tweak)
	{
		FDNBreakableRow R = DNTest::BreakableRow();
		Tweak(R);
		const TArray<FString> Errors = FDNDataValidation::ValidateBreakables({ DNTest::Entry<FDNBreakableEntry>(R, TEXT("poste")) });
		TestTrue(FString::Printf(TEXT("%s: esperava zero erros; veio: %s"), What, *JoinErrors(Errors)), Errors.IsEmpty());
	}

	void BadSettings(const TCHAR* What, const TCHAR* Field, TFunction<void(FDNBreakableSettingsRow&)> Tweak)
	{
		FDNBreakableSettingsRow R = DNTest::BreakableSettingsRow();
		Tweak(R);
		const TArray<FString> Errors = FDNDataValidation::ValidateBreakableSettings({ DNTest::Entry<FDNBreakableSettingsEntry>(R) });
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Field, *JoinErrors(Errors)), Cites(Errors, Field));
	}
END_DEFINE_SPEC(FDNDataBreakablesSpec)

void FDNDataBreakablesSpec::Define()
{
	Describe("Breakables.csv", [this]()
	{
		It("um tipo valido passa; tabela vazia e ids repetidos nao", [this]()
		{
			GoodKind(TEXT("padrao da struct"), [](FDNBreakableRow&) {});
			TestFalse("vazia", FDNDataValidation::ValidateBreakables({}).IsEmpty());
			TestFalse("repetido", FDNDataValidation::ValidateBreakables({ DNTest::Entry<FDNBreakableEntry>(DNTest::BreakableRow(), TEXT("poste")),
				DNTest::Entry<FDNBreakableEntry>(DNTest::BreakableRow(), TEXT("Poste")) }).IsEmpty());
		});

		It("faixas de cada campo", [this]()
		{
			BadKind(TEXT("massa 0"), TEXT("MassKg"), [](FDNBreakableRow& R) { R.MassKg = 0.f; });
			GoodKind(TEXT("massa 5000"), [](FDNBreakableRow& R) { R.MassKg = 5000.f; });
			BadKind(TEXT("massa NaN"), TEXT("MassKg"), [this](FDNBreakableRow& R) { R.MassKg = NaNf; });
			GoodKind(TEXT("limiar 0"), [](FDNBreakableRow& R) { R.MinBreakSpeedKmh = 0.f; });
			BadKind(TEXT("limiar negativo"), TEXT("MinBreakSpeedKmh"), [](FDNBreakableRow& R) { R.MinBreakSpeedKmh = -1.f; });
			BadKind(TEXT("limiar 201"), TEXT("MinBreakSpeedKmh"), [](FDNBreakableRow& R) { R.MinBreakSpeedKmh = 201.f; });
			BadKind(TEXT("lancamento 3,01"), TEXT("LaunchFactor"), [](FDNBreakableRow& R) { R.LaunchFactor = 3.01f; });
			BadKind(TEXT("subida 21"), TEXT("LaunchUpMs"), [](FDNBreakableRow& R) { R.LaunchUpMs = 21.f; });
			BadKind(TEXT("altura do corte 501"), TEXT("BreakHeightCm"), [](FDNBreakableRow& R) { R.BreakHeightCm = 501.f; });
			BadKind(TEXT("pontos negativos"), TEXT("Points"), [](FDNBreakableRow& R) { R.Points = -1; });
		});

		It("ToKind copia os sete campos", [this]()
		{
			FDNBreakableRow R;
			R.MassKg = 250.f; R.MinBreakSpeedKmh = 20.f; R.LaunchFactor = 0.7f; R.LaunchUpMs = 2.f; R.TopplesAtBase = true; R.BreakHeightCm = 10.f; R.Points = 50;
			const FDNBreakableKind K = R.ToKind();
			TestEqual("MassKg", K.MassKg, 250.f);
			TestEqual("MinBreakSpeedKmh", K.MinBreakSpeedKmh, 20.f);
			TestEqual("LaunchFactor", K.LaunchFactor, 0.7f);
			TestEqual("LaunchUpMs", K.LaunchUpMs, 2.f);
			TestTrue("TopplesAtBase", K.bTopplesAtBase);
			TestEqual("BreakHeightCm", K.BreakHeightCm, 10.f);
			TestEqual("Points", K.Points, 50);
		});
	});

	Describe("BreakableSettings.csv", [this]()
	{
		It("exatamente uma linha e as faixas", [this]()
		{
			TestTrue("padrao da struct", FDNDataValidation::ValidateBreakableSettings({ DNTest::Entry<FDNBreakableSettingsEntry>(DNTest::BreakableSettingsRow()) }).IsEmpty());
			TestFalse("nenhuma", FDNDataValidation::ValidateBreakableSettings({}).IsEmpty());
			TestFalse("duas", FDNDataValidation::ValidateBreakableSettings({ DNTest::Entry<FDNBreakableSettingsEntry>(DNTest::BreakableSettingsRow()),
				DNTest::Entry<FDNBreakableSettingsEntry>(DNTest::BreakableSettingsRow(), TEXT("outra")) }).IsEmpty());
			BadSettings(TEXT("k 0"), TEXT("ArcadeMassFactor"), [](FDNBreakableSettingsRow& R) { R.ArcadeMassFactor = 0.f; });
			BadSettings(TEXT("teto 101"), TEXT("MaxSpeedLossPct"), [](FDNBreakableSettingsRow& R) { R.MaxSpeedLossPct = 101.f; });
			BadSettings(TEXT("lancamento 0"), TEXT("MaxLaunchMs"), [](FDNBreakableSettingsRow& R) { R.MaxLaunchMs = 0.f; });
			BadSettings(TEXT("orcamento 0"), TEXT("MaxActiveProps"), [](FDNBreakableSettingsRow& R) { R.MaxActiveProps = 0; });
			BadSettings(TEXT("sono 0"), TEXT("SleepSpeedMs"), [](FDNBreakableSettingsRow& R) { R.SleepSpeedMs = 0.f; });
			BadSettings(TEXT("sono depois NaN"), TEXT("SleepAfterS"), [this](FDNBreakableSettingsRow& R) { R.SleepAfterS = NaNf; });
			BadSettings(TEXT("varrer a 0,9"), TEXT("PlowFactor"), [](FDNBreakableSettingsRow& R) { R.PlowFactor = 0.9f; });
			BadSettings(TEXT("varrer a 3,1"), TEXT("PlowFactor"), [](FDNBreakableSettingsRow& R) { R.PlowFactor = 3.1f; });
			BadSettings(TEXT("pulo de 11 m/s"), TEXT("PlowUpMs"), [](FDNBreakableSettingsRow& R) { R.PlowUpMs = 11.f; });
			BadSettings(TEXT("pulo NaN"), TEXT("PlowUpMs"), [this](FDNBreakableSettingsRow& R) { R.PlowUpMs = NaNf; });
		});

		It("ToSettings copia os oito campos", [this]()
		{
			FDNBreakableSettingsRow R;
			R.ArcadeMassFactor = 2.f; R.MaxSpeedLossPct = 30.f; R.MaxLaunchMs = 20.f; R.MaxActiveProps = 8; R.SleepSpeedMs = 0.3f; R.SleepAfterS = 2.f; R.PlowFactor = 1.5f; R.PlowUpMs = 3.f;
			const FDNBreakableSettings S = R.ToSettings();
			TestEqual("ArcadeMassFactor", S.ArcadeMassFactor, 2.f);
			TestEqual("MaxSpeedLossPct", S.MaxSpeedLossPct, 30.f);
			TestEqual("MaxLaunchMs", S.MaxLaunchMs, 20.f);
			TestEqual("MaxActiveProps", S.MaxActiveProps, 8);
			TestEqual("SleepSpeedMs", S.SleepSpeedMs, 0.3f);
			TestEqual("SleepAfterS", S.SleepAfterS, 2.f);
			TestEqual("PlowFactor", S.PlowFactor, 1.5f);
			TestEqual("PlowUpMs", S.PlowUpMs, 3.f);
		});
	});

	Describe("os CSVs reais", [this]()
	{
		It("Breakables.csv carrega, e valido e tem os valores iniciais do contrato", [this]()
		{
			TArray<FDNBreakableEntry> Rows;
			const TArray<FString> LoadErrors = FDNDataLoader::LoadEntries<FDNBreakableRow>(TEXT("Data/Breakables.csv"), Rows);
			TestTrue(FString::Printf(TEXT("carga: %s"), *JoinErrors(LoadErrors)), LoadErrors.IsEmpty());
			TestTrue(FString::Printf(TEXT("validacao: %s"), *JoinErrors(FDNDataValidation::ValidateBreakables(Rows))), FDNDataValidation::ValidateBreakables(Rows).IsEmpty());
			const struct { const TCHAR* Id; float Mass; float MinKmh; float Launch; float Up; bool bTopples; float Height; int32 Points; } Contract[] = {
				{ TEXT("poste"), 250.f, 20.f, 0.7f, 2.f, true, 10.f, 50 },
				{ TEXT("arvore"), 400.f, 25.f, 0.5f, 1.f, true, 15.f, 50 }, // 15 cm (revisão do 17c): abaixo da altura livre do carro
				{ TEXT("banco"), 60.f, 8.f, 1.1f, 3.f, false, 0.f, 20 },
				{ TEXT("lixeira"), 25.f, 5.f, 1.3f, 4.f, false, 0.f, 10 },
				{ TEXT("hidrante"), 120.f, 10.f, 0.9f, 2.f, false, 0.f, 30 },
				{ TEXT("placa"), 30.f, 8.f, 1.2f, 3.f, true, 0.f, 20 },
				{ TEXT("paraciclo"), 40.f, 8.f, 1.0f, 2.f, false, 0.f, 10 } };
			TestEqual("tipos", Rows.Num(), static_cast<int32>(UE_ARRAY_COUNT(Contract)));
			for (const auto& C : Contract)
			{
				const FDNBreakableEntry* E = Rows.FindByPredicate([&C](const FDNBreakableEntry& X) { return X.Id == C.Id; });
				if (!TestNotNull(C.Id, E))
				{
					continue;
				}
				const FDNBreakableKind K = E->Row.ToKind();
				TestEqual(FString::Printf(TEXT("%s MassKg"), C.Id), K.MassKg, C.Mass);
				TestEqual(FString::Printf(TEXT("%s MinBreakSpeedKmh"), C.Id), K.MinBreakSpeedKmh, C.MinKmh);
				TestEqual(FString::Printf(TEXT("%s LaunchFactor"), C.Id), K.LaunchFactor, C.Launch);
				TestEqual(FString::Printf(TEXT("%s LaunchUpMs"), C.Id), K.LaunchUpMs, C.Up);
				TestEqual(FString::Printf(TEXT("%s TopplesAtBase"), C.Id), K.bTopplesAtBase, C.bTopples);
				TestEqual(FString::Printf(TEXT("%s BreakHeightCm"), C.Id), K.BreakHeightCm, C.Height);
				TestEqual(FString::Printf(TEXT("%s Points"), C.Id), K.Points, C.Points);
			}
		});

		It("BreakableSettings.csv carrega, e valido e e igual ao padrao da struct (os valores do contrato)", [this]()
		{
			TArray<FDNBreakableSettingsEntry> Rows;
			const TArray<FString> LoadErrors = FDNDataLoader::LoadEntries<FDNBreakableSettingsRow>(TEXT("Data/BreakableSettings.csv"), Rows);
			TestTrue(FString::Printf(TEXT("carga: %s"), *JoinErrors(LoadErrors)), LoadErrors.IsEmpty());
			TestTrue(FString::Printf(TEXT("validacao: %s"), *JoinErrors(FDNDataValidation::ValidateBreakableSettings(Rows))), FDNDataValidation::ValidateBreakableSettings(Rows).IsEmpty());
			if (!TestEqual("uma linha", Rows.Num(), 1))
			{
				return;
			}
			const FDNBreakableSettings Csv = Rows[0].Row.ToSettings();
			const FDNBreakableSettings Def;
			TestEqual("ArcadeMassFactor", Csv.ArcadeMassFactor, Def.ArcadeMassFactor);
			TestEqual("MaxSpeedLossPct", Csv.MaxSpeedLossPct, Def.MaxSpeedLossPct);
			TestEqual("MaxLaunchMs", Csv.MaxLaunchMs, Def.MaxLaunchMs);
			TestEqual("MaxActiveProps", Csv.MaxActiveProps, Def.MaxActiveProps);
			TestEqual("SleepSpeedMs", Csv.SleepSpeedMs, Def.SleepSpeedMs);
			TestEqual("SleepAfterS", Csv.SleepAfterS, Def.SleepAfterS);
			TestEqual("PlowFactor", Csv.PlowFactor, Def.PlowFactor);
			TestEqual("PlowUpMs", Csv.PlowUpMs, Def.PlowUpMs);
			TestEqual("contrato: k 1", Def.ArcadeMassFactor, 1.f);
			TestEqual("contrato: teto 25%", Def.MaxSpeedLossPct, 25.f);
			TestEqual("contrato: 35 m/s", Def.MaxLaunchMs, 35.f);
			TestEqual("contrato: 16 pecas", Def.MaxActiveProps, 16);
			TestEqual("contrato: 0,2 m/s", Def.SleepSpeedMs, 0.2f);
			TestEqual("contrato: 1 s", Def.SleepAfterS, 1.f);
			TestEqual("17c: varre a 1,1 x", Def.PlowFactor, 1.1f);
			TestEqual("17c: pulo de 1,5 m/s", Def.PlowUpMs, 1.5f);
		});
	});

	Describe("UDNGameDataSubsystem", [this]()
	{
		It("acha o tipo sem diferenciar a caixa; tipo ausente ou tabela invalida devolvem falso; configuracao invalida vale o padrao", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			AddExpectedError(TEXT("Dados do jogo:"), EAutomationExpectedErrorFlags::Contains, 0); // as outras tabelas vazias dão erro
			FDNData Data;
			FDNBreakableKind Lamp;
			Lamp.MassKg = 250.f;
			Data.Breakables = { DNTest::Entry<FDNBreakableEntry>(DNTest::BreakableRow(Lamp), TEXT("poste")) };
			FDNBreakableSettings S;
			S.MaxActiveProps = 4;
			Data.BreakableSettings = { DNTest::Entry<FDNBreakableSettingsEntry>(DNTest::BreakableSettingsRow(S)) };
			Sub->ApplyData(Data, {});
			FDNBreakableKind Out;
			TestTrue("POSTE", Sub->TryGetBreakableKind(TEXT("POSTE"), Out));
			TestEqual("massa", Out.MassKg, 250.f);
			Out.MassKg = -7.f;
			TestFalse("arvore nao existe", Sub->TryGetBreakableKind(TEXT("arvore"), Out));
			TestEqual("Out intacto", Out.MassKg, -7.f);
			TestEqual("configuracao do CSV", Sub->GetBreakableSettings().MaxActiveProps, 4);
			Data.Breakables[0].Row.MassKg = 0.f;
			Data.BreakableSettings[0].Row.MaxActiveProps = 0;
			Sub->ApplyData(Data, {});
			TestFalse("tabela invalida", Sub->TryGetBreakableKind(TEXT("poste"), Out));
			TestEqual("configuracao invalida: padrao", Sub->GetBreakableSettings().MaxActiveProps, FDNBreakableSettings().MaxActiveProps);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
