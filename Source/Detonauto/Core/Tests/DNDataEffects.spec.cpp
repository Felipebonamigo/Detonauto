// Testes de Data/Effects.csv e Data/EffectsSettings.csv (lote 19, docs/regras-efeitos.md): cópia da linha para a regra, validação
// por campo, os CSVs reais iguais à tabela do contrato e o subsistema. Faixas: Core/Data/DNDataValidation.h.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/Data/DNDataLoader.h"
#include "Core/Data/DNDataValidation.h"
#include "Core/DNGameDataSubsystem.h"
#include "Core/Tests/DNTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDataEffectsSpec, "Detonauto.Dados.Efeitos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	const float NaNf = std::numeric_limits<float>::quiet_NaN();

	static FString JoinErrors(const TArray<FString>& Errors) { return Errors.IsEmpty() ? FString(TEXT("(nenhum erro)")) : FString::Join(Errors, TEXT(" | ")); }

	static bool Cites(const TArray<FString>& Errors, const TCHAR* Needle)
	{
		for (const FString& E : Errors) { if (E.Contains(Needle)) { return true; } }
		return false;
	}

	/** A tabela do contrato com a linha Index mexida: precisa dar erro citando Needle. */
	void BadEffect(const TCHAR* What, int32 Index, const TCHAR* Needle, TFunction<void(FDNEffectRow&)> Tweak)
	{
		TArray<FDNEffectEntry> Rows = DNTest::EffectEntries();
		Tweak(Rows[Index].Row);
		const TArray<FString> Errors = FDNDataValidation::ValidateEffects(Rows, DNTest::EffectProps());
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Needle, *JoinErrors(Errors)), Cites(Errors, Needle));
	}

	void BadSettings(const TCHAR* What, const TCHAR* Needle, TFunction<void(FDNEffectsSettingsRow&)> Tweak)
	{
		FDNEffectsSettingsRow R = DNTest::EffectsSettingsRow();
		Tweak(R);
		const TArray<FString> Errors = FDNDataValidation::ValidateEffectsSettings({ DNTest::Entry<FDNEffectsSettingsEntry>(R) });
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Needle, *JoinErrors(Errors)), Cites(Errors, Needle));
	}
END_DEFINE_SPEC(FDNDataEffectsSpec)

void FDNDataEffectsSpec::Define()
{
	// posições na tabela do contrato (a ordem do enum)
	constexpr int32 Spark = 0, Shards = 2, Plastic = 3, Tire = 4, Smoke = 5, Fire = 6, Dust = 7;

	Describe("Effects.csv", [this, Spark, Shards, Plastic, Tire, Smoke, Fire, Dust]()
	{
		It("ToSpec copia todos os campos e separa os tipos do mobiliario", [this]()
		{
			const FDNEffectRow R = DNTest::EffectRow(TEXT("propdust"), 1.f, 2.f, 3.f, 4, 5, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f, 12, TEXT(" poste ; arvore;;"));
			FDNEffectSpec S;
			if (!TestTrue("le", R.ToSpec(S)))
			{
				return;
			}
			TestTrue("Effect", S.Effect == EDNEffect::PropDust);
			TestEqual("TriggerMin", S.TriggerMin, 1.f);
			TestEqual("TriggerFull", S.TriggerFull, 2.f);
			TestEqual("MinSpeedKmh", S.MinSpeedKmh, 3.f);
			TestEqual("CountMin", S.CountMin, 4);
			TestEqual("CountMax", S.CountMax, 5);
			TestEqual("CountPerUnit", S.CountPerUnit, 6.f);
			TestEqual("Extra", S.Extra, 7.f);
			TestEqual("LifeMinS", S.LifeMinS, 8.f);
			TestEqual("LifeMaxS", S.LifeMaxS, 9.f);
			TestEqual("SpeedMinMs", S.SpeedMinMs, 10.f);
			TestEqual("SpeedMaxMs", S.SpeedMaxMs, 11.f);
			TestEqual("PoolMax", S.PoolMax, 12);
			TestTrue("Props sem espacos nem vazios", S.Props.Num() == 2 && S.Props[0] == TEXT("poste") && S.Props[1] == TEXT("arvore"));
			FDNEffectSpec S2;
			TestFalse("efeito desconhecido nao le", DNTest::EffectRow(TEXT("Explosao"), 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1).ToSpec(S2));
		});

		It("a tabela do contrato e valida; vazia, sem um efeito ou com um repetido nao", [this, Spark]()
		{
			const TArray<FString> Ok = FDNDataValidation::ValidateEffects(DNTest::EffectEntries(), DNTest::EffectProps());
			TestTrue(FString::Printf(TEXT("contrato: %s"), *JoinErrors(Ok)), Ok.IsEmpty());
			TestFalse("vazia", FDNDataValidation::ValidateEffects({}, DNTest::EffectProps()).IsEmpty());
			TArray<FDNEffectEntry> Missing = DNTest::EffectEntries();
			Missing.RemoveAt(Spark);
			TestTrue("sem a faisca de impacto: cita o efeito", Cites(FDNDataValidation::ValidateEffects(Missing, DNTest::EffectProps()), TEXT("SparkImpact")));
			TArray<FDNEffectEntry> Twice = DNTest::EffectEntries();
			Twice.Add(DNTest::Entry<FDNEffectEntry>(Twice[Spark].Row, TEXT("faisca-2")));
			TestTrue("faisca duas vezes: cita o efeito", Cites(FDNDataValidation::ValidateEffects(Twice, DNTest::EffectProps()), TEXT("SparkImpact")));
		});

		It("valida cada campo", [this, Spark, Shards, Plastic, Tire, Smoke, Fire, Dust]()
		{
			BadEffect(TEXT("efeito desconhecido"), Spark, TEXT("Effect"), [](FDNEffectRow& R) { R.Effect = TEXT("Explosao"); });
			BadEffect(TEXT("limiar negativo"), Spark, TEXT("TriggerMin"), [](FDNEffectRow& R) { R.TriggerMin = -1.f; });
			BadEffect(TEXT("limiar NaN"), Spark, TEXT("TriggerMin"), [this](FDNEffectRow& R) { R.TriggerMin = NaNf; });
			BadEffect(TEXT("rampa das lascas sem subida"), Plastic, TEXT("TriggerFull"), [](FDNEffectRow& R) { R.TriggerFull = R.TriggerMin; });
			BadEffect(TEXT("rampa do pneu sem subida"), Tire, TEXT("TriggerFull"), [](FDNEffectRow& R) { R.TriggerFull = 5.f; });
			BadEffect(TEXT("fumaca com dano 1,1"), Smoke, TEXT("TriggerFull"), [](FDNEffectRow& R) { R.TriggerFull = 1.1f; });
			BadEffect(TEXT("fogo com dano 1,1"), Fire, TEXT("TriggerMin"), [](FDNEffectRow& R) { R.TriggerMin = 1.1f; });
			BadEffect(TEXT("velocidade 301"), Tire, TEXT("MinSpeedKmh"), [](FDNEffectRow& R) { R.MinSpeedKmh = 301.f; });
			BadEffect(TEXT("piso negativo"), Spark, TEXT("CountMin"), [](FDNEffectRow& R) { R.CountMin = -1; });
			BadEffect(TEXT("teto abaixo do piso"), Spark, TEXT("CountMax"), [](FDNEffectRow& R) { R.CountMax = 5; });
			BadEffect(TEXT("teto 5001"), Shards, TEXT("CountMax"), [](FDNEffectRow& R) { R.CountMax = 5001; });
			BadEffect(TEXT("por unidade NaN"), Spark, TEXT("CountPerUnit"), [this](FDNEffectRow& R) { R.CountPerUnit = NaNf; });
			BadEffect(TEXT("patinando 1,1"), Tire, TEXT("Extra"), [](FDNEffectRow& R) { R.Extra = 1.1f; });
			BadEffect(TEXT("fogo destruido 1,1"), Fire, TEXT("Extra"), [](FDNEffectRow& R) { R.Extra = 1.1f; });
			BadEffect(TEXT("brilhos negativos"), Shards, TEXT("Extra"), [](FDNEffectRow& R) { R.Extra = -1.f; });
			BadEffect(TEXT("vida 0"), Spark, TEXT("LifeMinS"), [](FDNEffectRow& R) { R.LifeMinS = 0.f; });
			BadEffect(TEXT("vida maxima abaixo da minima"), Spark, TEXT("LifeMaxS"), [](FDNEffectRow& R) { R.LifeMaxS = 0.1f; });
			BadEffect(TEXT("velocidade maxima abaixo da minima"), Spark, TEXT("SpeedMaxMs"), [](FDNEffectRow& R) { R.SpeedMaxMs = 1.f; });
			BadEffect(TEXT("velocidade 101"), Spark, TEXT("SpeedMaxMs"), [](FDNEffectRow& R) { R.SpeedMaxMs = 101.f; });
			BadEffect(TEXT("pool 0"), Spark, TEXT("PoolMax"), [](FDNEffectRow& R) { R.PoolMax = 0; });
			BadEffect(TEXT("pool 65"), Spark, TEXT("PoolMax"), [](FDNEffectRow& R) { R.PoolMax = 65; });
			BadEffect(TEXT("mobiliario na faisca"), Spark, TEXT("Props"), [](FDNEffectRow& R) { R.Props = TEXT("poste"); });
			BadEffect(TEXT("po sem mobiliario"), Dust, TEXT("Props"), [](FDNEffectRow& R) { R.Props = TEXT(""); });
			BadEffect(TEXT("tipo que nao existe"), Dust, TEXT("banco"), [](FDNEffectRow& R) { R.Props = TEXT("poste;banco"); });
		});

		It("o CSV real carrega, e valido com o Breakables.csv real e tem a tabela do contrato, na ordem", [this]()
		{
			TArray<FDNEffectEntry> Rows;
			const TArray<FString> LoadErrors = FDNDataLoader::LoadEntries<FDNEffectRow>(TEXT("Data/Effects.csv"), Rows);
			TestTrue(FString::Printf(TEXT("carga: %s"), *JoinErrors(LoadErrors)), LoadErrors.IsEmpty());
			TArray<FDNBreakableEntry> Breakables;
			FDNDataLoader::LoadEntries<FDNBreakableRow>(TEXT("Data/Breakables.csv"), Breakables);
			const TArray<FString> Errors = FDNDataValidation::ValidateEffects(Rows, Breakables);
			TestTrue(FString::Printf(TEXT("validacao: %s"), *JoinErrors(Errors)), Errors.IsEmpty());
			const TArray<FDNEffectEntry> Contract = DNTest::EffectEntries();
			if (!TestEqual("9 efeitos", Rows.Num(), Contract.Num()))
			{
				return;
			}
			for (int32 i = 0; i < Rows.Num(); ++i)
			{
				const FDNEffectRow& Got = Rows[i].Row;
				const FDNEffectRow& Want = Contract[i].Row;
				const FString Label = FString::Printf(TEXT("linha %d (%s)"), i, *Rows[i].Id);
				TestEqual(Label + TEXT(": id"), Rows[i].Id, Contract[i].Id);
				TestEqual(Label + TEXT(": efeito"), Got.Effect, Want.Effect);
				TestEqual(Label + TEXT(": TriggerMin"), Got.TriggerMin, Want.TriggerMin);
				TestEqual(Label + TEXT(": TriggerFull"), Got.TriggerFull, Want.TriggerFull);
				TestEqual(Label + TEXT(": MinSpeedKmh"), Got.MinSpeedKmh, Want.MinSpeedKmh);
				TestEqual(Label + TEXT(": CountMin"), Got.CountMin, Want.CountMin);
				TestEqual(Label + TEXT(": CountMax"), Got.CountMax, Want.CountMax);
				TestEqual(Label + TEXT(": CountPerUnit"), Got.CountPerUnit, Want.CountPerUnit);
				TestEqual(Label + TEXT(": Extra"), Got.Extra, Want.Extra);
				TestEqual(Label + TEXT(": LifeMinS"), Got.LifeMinS, Want.LifeMinS);
				TestEqual(Label + TEXT(": LifeMaxS"), Got.LifeMaxS, Want.LifeMaxS);
				TestEqual(Label + TEXT(": SpeedMinMs"), Got.SpeedMinMs, Want.SpeedMinMs);
				TestEqual(Label + TEXT(": SpeedMaxMs"), Got.SpeedMaxMs, Want.SpeedMaxMs);
				TestEqual(Label + TEXT(": PoolMax"), Got.PoolMax, Want.PoolMax);
				TestEqual(Label + TEXT(": Props"), Got.Props, Want.Props);
			}
		});
	});

	Describe("EffectsSettings.csv", [this]()
	{
		It("o padrao da struct e valido; nenhuma ou duas linhas nao; cada campo na faixa", [this]()
		{
			TestTrue("padrao", FDNDataValidation::ValidateEffectsSettings({ DNTest::Entry<FDNEffectsSettingsEntry>(DNTest::EffectsSettingsRow()) }).IsEmpty());
			TestFalse("nenhuma", FDNDataValidation::ValidateEffectsSettings({}).IsEmpty());
			TestFalse("duas", FDNDataValidation::ValidateEffectsSettings({ DNTest::Entry<FDNEffectsSettingsEntry>(DNTest::EffectsSettingsRow()),
				DNTest::Entry<FDNEffectsSettingsEntry>(DNTest::EffectsSettingsRow(), TEXT("outra")) }).IsEmpty());
			BadSettings(TEXT("GPU 0"), TEXT("MaxGpuParticles"), [](FDNEffectsSettingsRow& R) { R.MaxGpuParticles = 0; });
			BadSettings(TEXT("GPU 100001"), TEXT("MaxGpuParticles"), [](FDNEffectsSettingsRow& R) { R.MaxGpuParticles = 100001; });
			BadSettings(TEXT("fumaca 0"), TEXT("MaxSmokeSprites"), [](FDNEffectsSettingsRow& R) { R.MaxSmokeSprites = 0; });
			BadSettings(TEXT("perto negativo"), TEXT("SmokeFadeNearCm"), [](FDNEffectsSettingsRow& R) { R.SmokeFadeNearCm = -1.f; });
			BadSettings(TEXT("longe antes do perto"), TEXT("SmokeFadeFarCm"), [](FDNEffectsSettingsRow& R) { R.SmokeFadeFarCm = R.SmokeFadeNearCm; });
			BadSettings(TEXT("longe NaN"), TEXT("SmokeFadeFarCm"), [this](FDNEffectsSettingsRow& R) { R.SmokeFadeFarCm = NaNf; });
		});

		It("o CSV real carrega, e valido e e igual ao padrao da struct (os valores do contrato)", [this]()
		{
			TArray<FDNEffectsSettingsEntry> Rows;
			const TArray<FString> LoadErrors = FDNDataLoader::LoadEntries<FDNEffectsSettingsRow>(TEXT("Data/EffectsSettings.csv"), Rows);
			TestTrue(FString::Printf(TEXT("carga: %s"), *JoinErrors(LoadErrors)), LoadErrors.IsEmpty());
			TestTrue(FString::Printf(TEXT("validacao: %s"), *JoinErrors(FDNDataValidation::ValidateEffectsSettings(Rows))), FDNDataValidation::ValidateEffectsSettings(Rows).IsEmpty());
			if (!TestEqual("uma linha", Rows.Num(), 1))
			{
				return;
			}
			const FDNEffectsSettings Csv = Rows[0].Row.ToSettings();
			const FDNEffectsSettings Def;
			TestEqual("MaxGpuParticles 2000", Csv.MaxGpuParticles, Def.MaxGpuParticles);
			TestEqual("MaxSmokeSprites 400", Csv.MaxSmokeSprites, Def.MaxSmokeSprites);
			TestEqual("SmokeFadeNearCm 150", Csv.SmokeFadeNearCm, Def.SmokeFadeNearCm);
			TestEqual("SmokeFadeFarCm", Csv.SmokeFadeFarCm, Def.SmokeFadeFarCm);
		});
	});

	Describe("UDNGameDataSubsystem", [this]()
	{
		It("entrega cada efeito e os tetos; tabela invalida: nenhum efeito e os tetos padrao", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			AddExpectedError(TEXT("Dados do jogo:"), EAutomationExpectedErrorFlags::Contains, 0); // as outras tabelas vazias dão erro
			FDNData Data;
			Data.Effects = DNTest::EffectEntries();
			Data.Breakables = DNTest::EffectProps();
			FDNEffectsSettings Small;
			Small.MaxGpuParticles = 500;
			Data.EffectsSettings = { DNTest::Entry<FDNEffectsSettingsEntry>(DNTest::EffectsSettingsRow(Small)) };
			Sub->ApplyData(Data, {});
			for (int32 i = 0; i <= static_cast<int32>(EDNEffect::PropLeaves); ++i)
			{
				FDNEffectSpec S;
				const EDNEffect E = static_cast<EDNEffect>(i);
				TestTrue(FString::Printf(TEXT("efeito %s"), FDNEffects::EffectName(E)), Sub->GetEffect(E, S) && S.Effect == E);
			}
			FDNEffectSpec Tire;
			TestTrue("fumaca de pneu com os numeros do CSV", Sub->GetEffect(EDNEffect::TireSmoke, Tire) && Tire.TriggerMin == 8.f && Tire.MinSpeedKmh == 20.f);
			TestEqual("tetos do CSV", Sub->GetEffectsSettings().MaxGpuParticles, 500);
			Data.Effects[0].Row.PoolMax = 0;
			Data.EffectsSettings[0].Row.MaxGpuParticles = 0;
			Sub->ApplyData(Data, {});
			FDNEffectSpec Untouched;
			Untouched.CountMax = 77;
			TestFalse("tabela invalida: nenhum efeito", Sub->GetEffect(EDNEffect::TireSmoke, Untouched));
			TestEqual("nao mexe em Out", Untouched.CountMax, 77);
			TestEqual("tetos invalidos: padrao", Sub->GetEffectsSettings().MaxGpuParticles, FDNEffectsSettings().MaxGpuParticles);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
