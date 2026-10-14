// Testes do CSV de ajuste do dano (Data/Damage.csv): validação por campo e DERIVA entre o CSV e o padrão da struct pura.
//
// Regras de validação (docs/regras-dano.md, "Dados e testes"):
//  - exatamente uma linha (id "padrao"); todos os valores finitos;
//  - limites e fatores (DentMinDeltaVMs, ZoneDestroyDeltaVMs, DentMaxDepthCm, DentDepthPerMs, DentBaseRadiusCm,
//    PartDetachDeltaVMs): maiores que zero;
//  - PartDetachZoneDamage e DestroyedTotalDamage: em (0, 1];
//  - MaxDents e MaxActive: inteiros maiores ou iguais a 1.
// O contrato não pede relação entre os campos (por exemplo PartDetachDeltaVMs >= DentMinDeltaVMs): nenhuma foi inventada.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/Data/DNDataValidation.h"
#include "Core/Tests/DNTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDataDamageSpec, "Detonauto.Dados.ConfigDano",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	const float Inff = std::numeric_limits<float>::infinity();

	static FString JoinErrors(const TArray<FString>& Errors) { return Errors.IsEmpty() ? FString(TEXT("(nenhum erro)")) : FString::Join(Errors, TEXT(" | ")); }

	static bool Cites(const TArray<FString>& Errors, const TCHAR* Needle)
	{
		for (const FString& E : Errors) { if (E.Contains(Needle)) { return true; } }
		return false;
	}

	void Bad(const TCHAR* What, const TCHAR* Field, TFunction<void(FDNDamageConfigRow&)> Tweak)
	{
		FDNDamageConfigRow R = DNTest::DamageRow();
		Tweak(R);
		const TArray<FString> Errors = FDNDataValidation::ValidateDamage({ DNTest::Entry<FDNDamageConfigEntry>(R) });
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Field, *JoinErrors(Errors)), Cites(Errors, Field));
	}

	void Good(const TCHAR* What, TFunction<void(FDNDamageConfigRow&)> Tweak)
	{
		FDNDamageConfigRow R = DNTest::DamageRow();
		Tweak(R);
		const TArray<FString> Errors = FDNDataValidation::ValidateDamage({ DNTest::Entry<FDNDamageConfigEntry>(R) });
		TestTrue(FString::Printf(TEXT("%s: esperava zero erros; veio: %s"), What, *JoinErrors(Errors)), Errors.IsEmpty());
	}
END_DEFINE_SPEC(FDNDataDamageSpec)

void FDNDataDamageSpec::Define()
{
	Describe("Damage.csv", [this]()
	{
		It("a configuracao inicial e valida", [this]()
		{
			TestEqual("erros", FDNDataValidation::ValidateDamage({ DNTest::DamageEntry() }).Num(), 0);
		});

		It("exige EXATAMENTE uma linha", [this]()
		{
			TestTrue("zero", Cites(FDNDataValidation::ValidateDamage({}), TEXT("uma linha")));
			TestTrue("duas", Cites(FDNDataValidation::ValidateDamage({ DNTest::DamageEntry(TEXT("a")), DNTest::DamageEntry(TEXT("b")) }), TEXT("uma linha")));
		});

		It("id vazio ou com caractere invalido e recusado", [this]()
		{
			TestTrue("vazio", Cites(FDNDataValidation::ValidateDamage({ DNTest::DamageEntry(TEXT("")) }), TEXT("id vazio")));
			TestTrue("espaco", Cites(FDNDataValidation::ValidateDamage({ DNTest::DamageEntry(TEXT("pa drao")) }), TEXT("caractere")));
		});

		It("limites e fatores precisam ser maiores que zero", [this]()
		{
			Bad(TEXT("DentMinDeltaVMs 0"), TEXT("DentMinDeltaVMs"), [](FDNDamageConfigRow& R) { R.DentMinDeltaVMs = 0.f; });
			Bad(TEXT("DentMinDeltaVMs -1"), TEXT("DentMinDeltaVMs"), [](FDNDamageConfigRow& R) { R.DentMinDeltaVMs = -1.f; });
			Good(TEXT("DentMinDeltaVMs 0,001"), [](FDNDamageConfigRow& R) { R.DentMinDeltaVMs = 0.001f; });
			Bad(TEXT("ZoneDestroyDeltaVMs 0"), TEXT("ZoneDestroyDeltaVMs"), [](FDNDamageConfigRow& R) { R.ZoneDestroyDeltaVMs = 0.f; });
			Bad(TEXT("DentMaxDepthCm 0"), TEXT("DentMaxDepthCm"), [](FDNDamageConfigRow& R) { R.DentMaxDepthCm = 0.f; });
			Bad(TEXT("DentDepthPerMs -0,5"), TEXT("DentDepthPerMs"), [](FDNDamageConfigRow& R) { R.DentDepthPerMs = -0.5f; });
			Bad(TEXT("DentBaseRadiusCm 0"), TEXT("DentBaseRadiusCm"), [](FDNDamageConfigRow& R) { R.DentBaseRadiusCm = 0.f; });
			Bad(TEXT("PartDetachDeltaVMs 0"), TEXT("PartDetachDeltaVMs"), [](FDNDamageConfigRow& R) { R.PartDetachDeltaVMs = 0.f; });
		});

		It("PartDetachZoneDamage e DestroyedTotalDamage em (0, 1]", [this]()
		{
			Bad(TEXT("PartDetachZoneDamage 0"), TEXT("PartDetachZoneDamage"), [](FDNDamageConfigRow& R) { R.PartDetachZoneDamage = 0.f; });
			Bad(TEXT("PartDetachZoneDamage negativo"), TEXT("PartDetachZoneDamage"), [](FDNDamageConfigRow& R) { R.PartDetachZoneDamage = -0.1f; });
			Good(TEXT("PartDetachZoneDamage 0,001"), [](FDNDamageConfigRow& R) { R.PartDetachZoneDamage = 0.001f; });
			Good(TEXT("PartDetachZoneDamage 1"), [](FDNDamageConfigRow& R) { R.PartDetachZoneDamage = 1.f; });
			Bad(TEXT("PartDetachZoneDamage 1,001"), TEXT("PartDetachZoneDamage"), [](FDNDamageConfigRow& R) { R.PartDetachZoneDamage = 1.001f; });
			Bad(TEXT("DestroyedTotalDamage 0"), TEXT("DestroyedTotalDamage"), [](FDNDamageConfigRow& R) { R.DestroyedTotalDamage = 0.f; });
			Good(TEXT("DestroyedTotalDamage 1"), [](FDNDamageConfigRow& R) { R.DestroyedTotalDamage = 1.f; });
			Bad(TEXT("DestroyedTotalDamage 1,001"), TEXT("DestroyedTotalDamage"), [](FDNDamageConfigRow& R) { R.DestroyedTotalDamage = 1.001f; });
		});

		It("MaxDents e MaxActive: inteiros >= 1", [this]()
		{
			Bad(TEXT("MaxDents 0"), TEXT("MaxDents"), [](FDNDamageConfigRow& R) { R.MaxDents = 0; });
			Bad(TEXT("MaxDents -3"), TEXT("MaxDents"), [](FDNDamageConfigRow& R) { R.MaxDents = -3; });
			Good(TEXT("MaxDents 1"), [](FDNDamageConfigRow& R) { R.MaxDents = 1; });
			Bad(TEXT("MaxActive 0"), TEXT("MaxActive"), [](FDNDamageConfigRow& R) { R.MaxActive = 0; });
			Bad(TEXT("MaxActive -1"), TEXT("MaxActive"), [](FDNDamageConfigRow& R) { R.MaxActive = -1; });
			Good(TEXT("MaxActive 1"), [](FDNDamageConfigRow& R) { R.MaxActive = 1; });
		});

		It("NaN e infinito sao recusados em cada campo decimal", [this]()
		{
			Bad(TEXT("DentMinDeltaVMs NaN"), TEXT("DentMinDeltaVMs"), [this](FDNDamageConfigRow& R) { R.DentMinDeltaVMs = NaNf; });
			Bad(TEXT("ZoneDestroyDeltaVMs inf"), TEXT("ZoneDestroyDeltaVMs"), [this](FDNDamageConfigRow& R) { R.ZoneDestroyDeltaVMs = Inff; });
			Bad(TEXT("DentMaxDepthCm NaN"), TEXT("DentMaxDepthCm"), [this](FDNDamageConfigRow& R) { R.DentMaxDepthCm = NaNf; });
			Bad(TEXT("DentDepthPerMs inf"), TEXT("DentDepthPerMs"), [this](FDNDamageConfigRow& R) { R.DentDepthPerMs = Inff; });
			Bad(TEXT("DentBaseRadiusCm NaN"), TEXT("DentBaseRadiusCm"), [this](FDNDamageConfigRow& R) { R.DentBaseRadiusCm = NaNf; });
			Bad(TEXT("PartDetachDeltaVMs inf"), TEXT("PartDetachDeltaVMs"), [this](FDNDamageConfigRow& R) { R.PartDetachDeltaVMs = Inff; });
			Bad(TEXT("PartDetachZoneDamage NaN"), TEXT("PartDetachZoneDamage"), [this](FDNDamageConfigRow& R) { R.PartDetachZoneDamage = NaNf; });
			Bad(TEXT("DestroyedTotalDamage NaN"), TEXT("DestroyedTotalDamage"), [this](FDNDamageConfigRow& R) { R.DestroyedTotalDamage = NaNf; });
		});

		It("lote 17: MorphCurveExponent em (0, 4] e MorphRisePerSecond em (0, 100]", [this]()
		{
			Good(TEXT("expoente 0,7 e subida 10"), [](FDNDamageConfigRow& R) { R.MorphCurveExponent = 0.7f; R.MorphRisePerSecond = 10.f; });
			Bad(TEXT("expoente 0"), TEXT("MorphCurveExponent"), [](FDNDamageConfigRow& R) { R.MorphCurveExponent = 0.f; });
			Good(TEXT("expoente 4"), [](FDNDamageConfigRow& R) { R.MorphCurveExponent = 4.f; });
			Bad(TEXT("expoente 4,01"), TEXT("MorphCurveExponent"), [](FDNDamageConfigRow& R) { R.MorphCurveExponent = 4.01f; });
			Bad(TEXT("expoente NaN"), TEXT("MorphCurveExponent"), [this](FDNDamageConfigRow& R) { R.MorphCurveExponent = NaNf; });
			Bad(TEXT("subida 0"), TEXT("MorphRisePerSecond"), [](FDNDamageConfigRow& R) { R.MorphRisePerSecond = 0.f; });
			Good(TEXT("subida 100"), [](FDNDamageConfigRow& R) { R.MorphRisePerSecond = 100.f; });
			Bad(TEXT("subida 101"), TEXT("MorphRisePerSecond"), [](FDNDamageConfigRow& R) { R.MorphRisePerSecond = 101.f; });
			Bad(TEXT("subida inf"), TEXT("MorphRisePerSecond"), [this](FDNDamageConfigRow& R) { R.MorphRisePerSecond = Inff; });
		});

		It("um erro nao esconde o outro: acumula todos", [this]()
		{
			FDNDamageConfigRow R = DNTest::DamageRow();
			R.DentMinDeltaVMs = -1.f;
			R.MaxActive = 0;
			R.DestroyedTotalDamage = 2.f;
			const TArray<FString> Errors = FDNDataValidation::ValidateDamage({ DNTest::Entry<FDNDamageConfigEntry>(R) });
			TestTrue("DentMinDeltaVMs", Cites(Errors, TEXT("DentMinDeltaVMs")));
			TestTrue("MaxActive", Cites(Errors, TEXT("MaxActive")));
			TestTrue("DestroyedTotalDamage", Cites(Errors, TEXT("DestroyedTotalDamage")));
		});
	});
}

// DERIVA: o CSV real, convertido por ToConfig(), precisa ser IGUAL, campo a campo, ao padrão da struct pura.
BEGIN_DEFINE_SPEC(FDNDataDamageDriftSpec, "Detonauto.Dados.DerivaDano",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataDamageDriftSpec)

void FDNDataDamageDriftSpec::Define()
{
	Describe("Damage.csv real x padrao da struct", [this]()
	{
		It("Damage.csv = FDNDamageConfig()", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			if (!TestEqual("uma linha em Damage.csv", Data.DamageConfig.Num(), 1)) { return; }
			const FDNDamageConfig Csv = Data.DamageConfig[0].Row.ToConfig();
			const FDNDamageConfig Def;
			auto Same = [this](const TCHAR* Field, double FromCsv, double FromStruct)
			{
				TestTrue(FString::Printf(TEXT("%s: o CSV tem %.9g e o padrao da struct tem %.9g (mudou so num dos dois lugares?)"), Field, FromCsv, FromStruct), FromCsv == FromStruct);
			};
			Same(TEXT("DentMinDeltaVMs"), Csv.DentMinDeltaVMs, Def.DentMinDeltaVMs);
			Same(TEXT("ZoneDestroyDeltaVMs"), Csv.ZoneDestroyDeltaVMs, Def.ZoneDestroyDeltaVMs);
			Same(TEXT("DentMaxDepthCm"), Csv.DentMaxDepthCm, Def.DentMaxDepthCm);
			Same(TEXT("DentDepthPerMs"), Csv.DentDepthPerMs, Def.DentDepthPerMs);
			Same(TEXT("DentBaseRadiusCm"), Csv.DentBaseRadiusCm, Def.DentBaseRadiusCm);
			Same(TEXT("MaxDents"), Csv.MaxDents, Def.MaxDents);
			Same(TEXT("PartDetachDeltaVMs"), Csv.PartDetachDeltaVMs, Def.PartDetachDeltaVMs);
			Same(TEXT("PartDetachZoneDamage"), Csv.PartDetachZoneDamage, Def.PartDetachZoneDamage);
			Same(TEXT("DestroyedTotalDamage"), Csv.DestroyedTotalDamage, Def.DestroyedTotalDamage);
			Same(TEXT("MaxActive"), Csv.MaxActive, Def.MaxActive);
			Same(TEXT("MorphCurveExponent"), Csv.MorphCurveExponent, Def.MorphCurveExponent);
			Same(TEXT("MorphRisePerSecond"), Csv.MorphRisePerSecond, Def.MorphRisePerSecond);
		});

		It("o padrao da struct tem os valores do contrato (docs/regras-dano.md)", [this]()
		{
			const FDNDamageConfig D;
			TestEqual("DentMinDeltaVMs", D.DentMinDeltaVMs, 2.0f);
			TestEqual("ZoneDestroyDeltaVMs", D.ZoneDestroyDeltaVMs, 30.0f);
			TestEqual("DentMaxDepthCm", D.DentMaxDepthCm, 25.0f);
			TestEqual("DentDepthPerMs", D.DentDepthPerMs, 1.5f);
			TestEqual("DentBaseRadiusCm", D.DentBaseRadiusCm, 30.0f);
			TestEqual("MaxDents", D.MaxDents, 8);
			TestEqual("PartDetachDeltaVMs", D.PartDetachDeltaVMs, 12.0f);
			TestEqual("PartDetachZoneDamage", D.PartDetachZoneDamage, 0.6f);
			TestEqual("DestroyedTotalDamage", D.DestroyedTotalDamage, 0.5f);
			TestEqual("MaxActive", D.MaxActive, 24);
			TestEqual("MorphCurveExponent (docs/regras-dano-visual.md)", D.MorphCurveExponent, 0.7f);
			TestEqual("MorphRisePerSecond", D.MorphRisePerSecond, 10.0f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
