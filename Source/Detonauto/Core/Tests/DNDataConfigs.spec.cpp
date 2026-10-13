// Testes dos CSVs de ajuste de pontuação (Data/Turbo.csv, Crash.csv e Takedown.csv): validação por campo e
// DERIVA entre o CSV e o padrão da struct pura.
//
// Regras de validação por campo, derivadas de docs/regras-de-jogo.md (as duvidosas estão no relatório do lote 5):
//  - exatamente uma linha (id "padrao"); todos os valores finitos;
//  - limites, janelas e consumo: maiores que zero; velocidades, ganhos e bônus: maiores ou iguais a zero;
//  - ordem coerente: DriftMinAngleDeg < DriftMaxAngleDeg, TBoneMinAngleDegrees < TBoneMaxAngleDegrees;
//  - TakedownSetsBarTo em (0, MaxBar]; MaxMultiplier >= 1; ZoneHalfAngleDegrees em (0, 90).

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/Data/DNDataValidation.h"
#include "Core/Tests/DNTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	const float Inff = std::numeric_limits<float>::infinity();

	FString Join(const TArray<FString>& Errors) { return Errors.IsEmpty() ? FString(TEXT("(nenhum erro)")) : FString::Join(Errors, TEXT(" | ")); }

	bool HasField(const TArray<FString>& Errors, const TCHAR* Field)
	{
		for (const FString& E : Errors) { if (E.Contains(Field)) { return true; } }
		return false;
	}

	template <typename RowType>
	struct TCaseRunner
	{
		FAutomationTestBase& Test;
		TFunction<TArray<FString>(const RowType&)> Validate;

		/** Com o ajuste aplicado, esperava um erro que cite o campo. */
		void Bad(const TCHAR* What, const TCHAR* Field, const RowType& Base, TFunction<void(RowType&)> Tweak) const
		{
			RowType R = Base;
			Tweak(R);
			const TArray<FString> Errors = Validate(R);
			Test.TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Field, *Join(Errors)), HasField(Errors, Field));
		}

		/** Com o ajuste aplicado, esperava zero erros. */
		void Good(const TCHAR* What, const RowType& Base, TFunction<void(RowType&)> Tweak) const
		{
			RowType R = Base;
			Tweak(R);
			const TArray<FString> Errors = Validate(R);
			Test.TestTrue(FString::Printf(TEXT("%s: esperava zero erros; veio: %s"), What, *Join(Errors)), Errors.IsEmpty());
		}
	};

	void ExpectSame(FAutomationTestBase& T, const TCHAR* Field, double Csv, double Struct)
	{
		T.TestTrue(FString::Printf(TEXT("%s: o CSV tem %.9g e o padrão da struct tem %.9g (mudou só num dos dois lugares?)"), Field, Csv, Struct), Csv == Struct);
	}
}

BEGIN_DEFINE_SPEC(FDNDataTurboConfigSpec, "Detonauto.Dados.ConfigTurbo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataTurboConfigSpec)

void FDNDataTurboConfigSpec::Define()
{
	Describe("Turbo.csv", [this]()
	{
		const FDNTurboConfigRow Base = DNTest::TurboRow();
		const TCaseRunner<FDNTurboConfigRow> Check{ *this, [](const FDNTurboConfigRow& R) { return FDNDataValidation::ValidateTurbo({ DNTest::Entry<FDNTurboConfigEntry>(R) }); } };

		It("a configuracao inicial e valida", [this]()
		{
			TestEqual("erros", FDNDataValidation::ValidateTurbo({ DNTest::TurboEntry() }).Num(), 0);
		});

		It("exige EXATAMENTE uma linha", [this]()
		{
			const TArray<FString> None = FDNDataValidation::ValidateTurbo({});
			TestTrue(FString::Printf(TEXT("zero linhas: %s"), *Join(None)), HasField(None, TEXT("uma linha")));
			const TArray<FString> Two = FDNDataValidation::ValidateTurbo({ DNTest::TurboEntry(TEXT("a")), DNTest::TurboEntry(TEXT("b")) });
			TestTrue(FString::Printf(TEXT("duas linhas: %s"), *Join(Two)), HasField(Two, TEXT("uma linha")));
		});

		It("id com caractere invalido ou so diferente na caixa e recusado", [this]()
		{
			TestTrue("espaco", HasField(FDNDataValidation::ValidateTurbo({ DNTest::TurboEntry(TEXT("pa drao")) }), TEXT("caractere")));
			TestTrue("id vazio", HasField(FDNDataValidation::ValidateTurbo({ DNTest::TurboEntry(TEXT("")) }), TEXT("id vazio")));
		});

		It("campos que precisam ser MAIORES que zero", [this, Base, Check]()
		{
			Check.Bad(TEXT("MaxBar 0"), TEXT("MaxBar"), Base, [](FDNTurboConfigRow& R) { R.MaxBar = 0.f; });
			Check.Bad(TEXT("MaxBar -1"), TEXT("MaxBar"), Base, [](FDNTurboConfigRow& R) { R.MaxBar = -1.f; });
			Check.Good(TEXT("MaxBar 0,001 (e o resto continua coerente)"), Base, [](FDNTurboConfigRow& R) { R.MaxBar = 0.001f; R.TakedownSetsBarTo = 0.001f; });
			Check.Bad(TEXT("NearMissMaxLateralCm 0"), TEXT("NearMissMaxLateralCm"), Base, [](FDNTurboConfigRow& R) { R.NearMissMaxLateralCm = 0.f; });
			Check.Bad(TEXT("NearMissChainWindowSeconds 0"), TEXT("NearMissChainWindowSeconds"), Base, [](FDNTurboConfigRow& R) { R.NearMissChainWindowSeconds = 0.f; });
			Check.Bad(TEXT("ConsumptionPerSecond 0"), TEXT("ConsumptionPerSecond"), Base, [](FDNTurboConfigRow& R) { R.ConsumptionPerSecond = 0.f; });
			Check.Bad(TEXT("ConsumptionPerSecond -5"), TEXT("ConsumptionPerSecond"), Base, [](FDNTurboConfigRow& R) { R.ConsumptionPerSecond = -5.f; });
		});

		It("campos que precisam ser MAIORES OU IGUAIS a zero (zero vale)", [this, Base, Check]()
		{
			Check.Bad(TEXT("WrongWayMinSpeedKmh -1"), TEXT("WrongWayMinSpeedKmh"), Base, [](FDNTurboConfigRow& R) { R.WrongWayMinSpeedKmh = -0.001f; });
			Check.Good(TEXT("WrongWayMinSpeedKmh 0"), Base, [](FDNTurboConfigRow& R) { R.WrongWayMinSpeedKmh = 0.f; });
			Check.Bad(TEXT("WrongWayGainPerSecond -1"), TEXT("WrongWayGainPerSecond"), Base, [](FDNTurboConfigRow& R) { R.WrongWayGainPerSecond = -1.f; });
			Check.Good(TEXT("WrongWayGainPerSecond 0"), Base, [](FDNTurboConfigRow& R) { R.WrongWayGainPerSecond = 0.f; });
			Check.Bad(TEXT("NearMissMinSpeedKmh -1"), TEXT("NearMissMinSpeedKmh"), Base, [](FDNTurboConfigRow& R) { R.NearMissMinSpeedKmh = -1.f; });
			Check.Bad(TEXT("NearMissGain -1"), TEXT("NearMissGain"), Base, [](FDNTurboConfigRow& R) { R.NearMissGain = -1.f; });
			Check.Good(TEXT("NearMissGain 0"), Base, [](FDNTurboConfigRow& R) { R.NearMissGain = 0.f; });
			Check.Bad(TEXT("DriftMinSpeedKmh -1"), TEXT("DriftMinSpeedKmh"), Base, [](FDNTurboConfigRow& R) { R.DriftMinSpeedKmh = -1.f; });
			Check.Bad(TEXT("DriftGainPerSecond -1"), TEXT("DriftGainPerSecond"), Base, [](FDNTurboConfigRow& R) { R.DriftGainPerSecond = -1.f; });
			Check.Bad(TEXT("AirTimeThresholdSeconds -1"), TEXT("AirTimeThresholdSeconds"), Base, [](FDNTurboConfigRow& R) { R.AirTimeThresholdSeconds = -1.f; });
			Check.Good(TEXT("AirTimeThresholdSeconds 0"), Base, [](FDNTurboConfigRow& R) { R.AirTimeThresholdSeconds = 0.f; });
			Check.Bad(TEXT("AirGainPerSecond -1"), TEXT("AirGainPerSecond"), Base, [](FDNTurboConfigRow& R) { R.AirGainPerSecond = -1.f; });
			Check.Bad(TEXT("AirMaxGainPerJump -1"), TEXT("AirMaxGainPerJump"), Base, [](FDNTurboConfigRow& R) { R.AirMaxGainPerJump = -1.f; });
			Check.Bad(TEXT("ExtendedBonus -1"), TEXT("ExtendedBonus"), Base, [](FDNTurboConfigRow& R) { R.ExtendedBonus = -1.f; });
			Check.Good(TEXT("ExtendedBonus 0"), Base, [](FDNTurboConfigRow& R) { R.ExtendedBonus = 0.f; });
		});

		It("os angulos da derrapada: 0 <= minimo < maximo <= 180", [this, Base, Check]()
		{
			Check.Bad(TEXT("min = max"), TEXT("DriftMinAngleDeg"), Base, [](FDNTurboConfigRow& R) { R.DriftMinAngleDeg = 70.f; R.DriftMaxAngleDeg = 70.f; });
			Check.Bad(TEXT("min > max"), TEXT("DriftMinAngleDeg"), Base, [](FDNTurboConfigRow& R) { R.DriftMinAngleDeg = 80.f; R.DriftMaxAngleDeg = 70.f; });
			Check.Bad(TEXT("min negativo"), TEXT("DriftMinAngleDeg"), Base, [](FDNTurboConfigRow& R) { R.DriftMinAngleDeg = -1.f; });
			Check.Good(TEXT("min 0"), Base, [](FDNTurboConfigRow& R) { R.DriftMinAngleDeg = 0.f; });
			Check.Bad(TEXT("max 90,001 (acima de 90 o carro anda de re)"), TEXT("DriftMaxAngleDeg"), Base, [](FDNTurboConfigRow& R) { R.DriftMaxAngleDeg = 90.001f; });
			Check.Bad(TEXT("max 180"), TEXT("DriftMaxAngleDeg"), Base, [](FDNTurboConfigRow& R) { R.DriftMaxAngleDeg = 180.f; });
			Check.Good(TEXT("max 90"), Base, [](FDNTurboConfigRow& R) { R.DriftMaxAngleDeg = 90.f; });
		});

		It("TakedownSetsBarTo precisa estar em (0, MaxBar]", [this, Base, Check]()
		{
			Check.Bad(TEXT("0"), TEXT("TakedownSetsBarTo"), Base, [](FDNTurboConfigRow& R) { R.TakedownSetsBarTo = 0.f; });
			Check.Bad(TEXT("negativo"), TEXT("TakedownSetsBarTo"), Base, [](FDNTurboConfigRow& R) { R.TakedownSetsBarTo = -5.f; });
			Check.Good(TEXT("0,001"), Base, [](FDNTurboConfigRow& R) { R.TakedownSetsBarTo = 0.001f; });
			Check.Good(TEXT("exatamente MaxBar"), Base, [](FDNTurboConfigRow& R) { R.TakedownSetsBarTo = R.MaxBar; });
			Check.Bad(TEXT("MaxBar + 0,001"), TEXT("TakedownSetsBarTo"), Base, [](FDNTurboConfigRow& R) { R.TakedownSetsBarTo = R.MaxBar + 0.001f; });
			Check.Bad(TEXT("acima de um MaxBar menor"), TEXT("TakedownSetsBarTo"), Base, [](FDNTurboConfigRow& R) { R.MaxBar = 50.f; R.TakedownSetsBarTo = 60.f; });
		});

		It("a lista de multiplicadores da cadeia: nao vazia e cada um finito e maior que zero", [this, Base, Check]()
		{
			Check.Bad(TEXT("vazia"), TEXT("NearMissChainMultipliers"), Base, [](FDNTurboConfigRow& R) { R.NearMissChainMultipliers.Reset(); });
			Check.Bad(TEXT("com zero"), TEXT("NearMissChainMultipliers"), Base, [](FDNTurboConfigRow& R) { R.NearMissChainMultipliers = { 1.f, 0.f, 2.f }; });
			Check.Bad(TEXT("negativo"), TEXT("NearMissChainMultipliers"), Base, [](FDNTurboConfigRow& R) { R.NearMissChainMultipliers = { 1.f, -1.f }; });
			Check.Bad(TEXT("NaN"), TEXT("NearMissChainMultipliers"), Base, [](FDNTurboConfigRow& R) { R.NearMissChainMultipliers = { 1.f, NaNf }; });
			Check.Good(TEXT("um so"), Base, [](FDNTurboConfigRow& R) { R.NearMissChainMultipliers = { 1.f }; });
		});

		It("NaN e infinito sao recusados nos campos decimais", [this, Base, Check]()
		{
			Check.Bad(TEXT("MaxBar NaN"), TEXT("MaxBar"), Base, [](FDNTurboConfigRow& R) { R.MaxBar = NaNf; });
			Check.Bad(TEXT("WrongWayGainPerSecond inf"), TEXT("WrongWayGainPerSecond"), Base, [](FDNTurboConfigRow& R) { R.WrongWayGainPerSecond = Inff; });
			Check.Bad(TEXT("NearMissGain NaN"), TEXT("NearMissGain"), Base, [](FDNTurboConfigRow& R) { R.NearMissGain = NaNf; });
			Check.Bad(TEXT("DriftMaxAngleDeg inf"), TEXT("DriftMaxAngleDeg"), Base, [](FDNTurboConfigRow& R) { R.DriftMaxAngleDeg = Inff; });
			Check.Bad(TEXT("AirGainPerSecond NaN"), TEXT("AirGainPerSecond"), Base, [](FDNTurboConfigRow& R) { R.AirGainPerSecond = NaNf; });
			Check.Bad(TEXT("TakedownSetsBarTo NaN"), TEXT("TakedownSetsBarTo"), Base, [](FDNTurboConfigRow& R) { R.TakedownSetsBarTo = NaNf; });
			Check.Bad(TEXT("ConsumptionPerSecond inf"), TEXT("ConsumptionPerSecond"), Base, [](FDNTurboConfigRow& R) { R.ConsumptionPerSecond = Inff; });
			Check.Bad(TEXT("ExtendedBonus NaN"), TEXT("ExtendedBonus"), Base, [](FDNTurboConfigRow& R) { R.ExtendedBonus = NaNf; });
		});
	});
}

BEGIN_DEFINE_SPEC(FDNDataCrashConfigSpec, "Detonauto.Dados.ConfigCrash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataCrashConfigSpec)

void FDNDataCrashConfigSpec::Define()
{
	Describe("Crash.csv", [this]()
	{
		const FDNCrashConfigRow Base = DNTest::CrashRow();
		const TCaseRunner<FDNCrashConfigRow> Check{ *this, [](const FDNCrashConfigRow& R) { return FDNDataValidation::ValidateCrash({ DNTest::Entry<FDNCrashConfigEntry>(R) }); } };

		It("a configuracao inicial e valida", [this]()
		{
			TestEqual("erros", FDNDataValidation::ValidateCrash({ DNTest::CrashEntry() }).Num(), 0);
		});

		It("exige EXATAMENTE uma linha", [this]()
		{
			TestTrue("zero", HasField(FDNDataValidation::ValidateCrash({}), TEXT("uma linha")));
			TestTrue("duas", HasField(FDNDataValidation::ValidateCrash({ DNTest::CrashEntry(TEXT("a")), DNTest::CrashEntry(TEXT("b")) }), TEXT("uma linha")));
		});

		It("MaxMultiplier >= 1 (inteiro)", [this, Base, Check]()
		{
			Check.Bad(TEXT("0"), TEXT("MaxMultiplier"), Base, [](FDNCrashConfigRow& R) { R.MaxMultiplier = 0; });
			Check.Bad(TEXT("negativo"), TEXT("MaxMultiplier"), Base, [](FDNCrashConfigRow& R) { R.MaxMultiplier = -3; });
			Check.Good(TEXT("1"), Base, [](FDNCrashConfigRow& R) { R.MaxMultiplier = 1; });
		});

		It("percentuais do bonus: maiores ou iguais a zero", [this, Base, Check]()
		{
			Check.Bad(TEXT("por elo -1"), TEXT("ChainBonusPercentPerLink"), Base, [](FDNCrashConfigRow& R) { R.ChainBonusPercentPerLink = -1; });
			Check.Good(TEXT("por elo 0"), Base, [](FDNCrashConfigRow& R) { R.ChainBonusPercentPerLink = 0; });
			Check.Bad(TEXT("teto -1"), TEXT("ChainBonusMaxPercent"), Base, [](FDNCrashConfigRow& R) { R.ChainBonusMaxPercent = -1; });
			Check.Good(TEXT("teto 0"), Base, [](FDNCrashConfigRow& R) { R.ChainBonusMaxPercent = 0; });
		});

		It("janelas e tempos: maiores que zero; velocidade de repouso: maior ou igual a zero", [this, Base, Check]()
		{
			Check.Bad(TEXT("ChainWindowSeconds 0"), TEXT("ChainWindowSeconds"), Base, [](FDNCrashConfigRow& R) { R.ChainWindowSeconds = 0.f; });
			Check.Bad(TEXT("QuietSecondsToEnd 0"), TEXT("QuietSecondsToEnd"), Base, [](FDNCrashConfigRow& R) { R.QuietSecondsToEnd = 0.f; });
			Check.Bad(TEXT("MaxSecondsAfterFirstImpact 0"), TEXT("MaxSecondsAfterFirstImpact"), Base, [](FDNCrashConfigRow& R) { R.MaxSecondsAfterFirstImpact = 0.f; });
			Check.Bad(TEXT("QuietSpeedMetersPerSecond -1"), TEXT("QuietSpeedMetersPerSecond"), Base, [](FDNCrashConfigRow& R) { R.QuietSpeedMetersPerSecond = -0.1f; });
			Check.Good(TEXT("QuietSpeedMetersPerSecond 0"), Base, [](FDNCrashConfigRow& R) { R.QuietSpeedMetersPerSecond = 0.f; });
		});

		It("o silencio precisa caber antes do limite da rodada: QuietSecondsToEnd <= MaxSecondsAfterFirstImpact", [this, Base, Check]()
		{
			Check.Bad(TEXT("silencio maior que o limite"), TEXT("QuietSecondsToEnd"), Base, [](FDNCrashConfigRow& R) { R.QuietSecondsToEnd = 31.f; R.MaxSecondsAfterFirstImpact = 30.f; });
			Check.Bad(TEXT("silencio 0,001 acima"), TEXT("QuietSecondsToEnd"), Base, [](FDNCrashConfigRow& R) { R.QuietSecondsToEnd = 30.001f; R.MaxSecondsAfterFirstImpact = 30.f; });
			Check.Good(TEXT("silencio igual ao limite"), Base, [](FDNCrashConfigRow& R) { R.QuietSecondsToEnd = 30.f; R.MaxSecondsAfterFirstImpact = 30.f; });
		});

		It("NaN e infinito sao recusados", [this, Base, Check]()
		{
			Check.Bad(TEXT("ChainWindowSeconds NaN"), TEXT("ChainWindowSeconds"), Base, [](FDNCrashConfigRow& R) { R.ChainWindowSeconds = NaNf; });
			Check.Bad(TEXT("QuietSpeedMetersPerSecond inf"), TEXT("QuietSpeedMetersPerSecond"), Base, [](FDNCrashConfigRow& R) { R.QuietSpeedMetersPerSecond = Inff; });
			Check.Bad(TEXT("QuietSecondsToEnd NaN"), TEXT("QuietSecondsToEnd"), Base, [](FDNCrashConfigRow& R) { R.QuietSecondsToEnd = NaNf; });
			Check.Bad(TEXT("MaxSecondsAfterFirstImpact inf"), TEXT("MaxSecondsAfterFirstImpact"), Base, [](FDNCrashConfigRow& R) { R.MaxSecondsAfterFirstImpact = Inff; });
		});
	});
}

BEGIN_DEFINE_SPEC(FDNDataTakedownConfigSpec, "Detonauto.Dados.ConfigTakedown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataTakedownConfigSpec)

void FDNDataTakedownConfigSpec::Define()
{
	Describe("Takedown.csv", [this]()
	{
		const FDNTakedownConfigRow Base = DNTest::TakedownRow();
		const TCaseRunner<FDNTakedownConfigRow> Check{ *this, [](const FDNTakedownConfigRow& R) { return FDNDataValidation::ValidateTakedown({ DNTest::Entry<FDNTakedownConfigEntry>(R) }); } };

		It("a configuracao inicial e valida", [this]()
		{
			TestEqual("erros", FDNDataValidation::ValidateTakedown({ DNTest::TakedownEntry() }).Num(), 0);
		});

		It("exige EXATAMENTE uma linha", [this]()
		{
			TestTrue("zero", HasField(FDNDataValidation::ValidateTakedown({}), TEXT("uma linha")));
			TestTrue("duas", HasField(FDNDataValidation::ValidateTakedown({ DNTest::TakedownEntry(TEXT("a")), DNTest::TakedownEntry(TEXT("b")) }), TEXT("uma linha")));
		});

		It("velocidade minima: maior ou igual a zero; janela e altura: maiores que zero", [this, Base, Check]()
		{
			Check.Bad(TEXT("MinRelativeSpeedKmh -1"), TEXT("MinRelativeSpeedKmh"), Base, [](FDNTakedownConfigRow& R) { R.MinRelativeSpeedKmh = -1.f; });
			Check.Good(TEXT("MinRelativeSpeedKmh 0"), Base, [](FDNTakedownConfigRow& R) { R.MinRelativeSpeedKmh = 0.f; });
			Check.Bad(TEXT("RivalCrashWindowSeconds 0"), TEXT("RivalCrashWindowSeconds"), Base, [](FDNTakedownConfigRow& R) { R.RivalCrashWindowSeconds = 0.f; });
			Check.Bad(TEXT("VerticalMinHeightCm 0"), TEXT("VerticalMinHeightCm"), Base, [](FDNTakedownConfigRow& R) { R.VerticalMinHeightCm = 0.f; });
			Check.Bad(TEXT("VerticalMinHeightCm -1"), TEXT("VerticalMinHeightCm"), Base, [](FDNTakedownConfigRow& R) { R.VerticalMinHeightCm = -1.f; });
		});

		It("angulos: por tras e lateral em (0, 180]", [this, Base, Check]()
		{
			Check.Bad(TEXT("BehindMax 0"), TEXT("BehindMaxAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.BehindMaxAngleDegrees = 0.f; });
			Check.Good(TEXT("BehindMax 180"), Base, [](FDNTakedownConfigRow& R) { R.BehindMaxAngleDegrees = 180.f; });
			Check.Bad(TEXT("BehindMax 180,001"), TEXT("BehindMaxAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.BehindMaxAngleDegrees = 180.001f; });
			Check.Bad(TEXT("SideMax 0"), TEXT("SideMaxAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.SideMaxAngleDegrees = 0.f; });
			Check.Bad(TEXT("SideMax 181"), TEXT("SideMaxAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.SideMaxAngleDegrees = 181.f; });
			Check.Bad(TEXT("SideMax 180 (passa do inicio do T)"), TEXT("SideMaxAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.SideMaxAngleDegrees = 180.f; });
		});

		It("o angulo do T: 0 <= minimo < maximo <= 180", [this, Base, Check]()
		{
			Check.Bad(TEXT("min = max"), TEXT("TBoneMinAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.TBoneMinAngleDegrees = 120.f; R.TBoneMaxAngleDegrees = 120.f; });
			Check.Bad(TEXT("min > max"), TEXT("TBoneMinAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.TBoneMinAngleDegrees = 130.f; R.TBoneMaxAngleDegrees = 120.f; });
			Check.Bad(TEXT("min negativo"), TEXT("TBoneMinAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.TBoneMinAngleDegrees = -1.f; });
			Check.Bad(TEXT("min 0 (o T comeca antes do fim da lateral)"), TEXT("SideMaxAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.TBoneMinAngleDegrees = 0.f; });
			Check.Bad(TEXT("max 180,001"), TEXT("TBoneMaxAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.TBoneMaxAngleDegrees = 180.001f; });
			Check.Good(TEXT("max 180"), Base, [](FDNTakedownConfigRow& R) { R.TBoneMaxAngleDegrees = 180.f; });
		});

		It("a lateral nao pode invadir o T: SideMaxAngleDegrees <= TBoneMinAngleDegrees (vao e permitido)", [this, Base, Check]()
		{
			Check.Bad(TEXT("lateral 0,001 acima do T"), TEXT("SideMaxAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.SideMaxAngleDegrees = 60.001f; R.TBoneMinAngleDegrees = 60.f; });
			Check.Bad(TEXT("lateral 70, T comeca em 60"), TEXT("SideMaxAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.SideMaxAngleDegrees = 70.f; });
			Check.Good(TEXT("lateral igual ao inicio do T"), Base, [](FDNTakedownConfigRow& R) { R.SideMaxAngleDegrees = 60.f; R.TBoneMinAngleDegrees = 60.f; });
			Check.Good(TEXT("vao entre a lateral e o T"), Base, [](FDNTakedownConfigRow& R) { R.SideMaxAngleDegrees = 50.f; R.TBoneMinAngleDegrees = 60.f; });
		});

		It("o meio angulo das zonas: estritamente entre 0 e 90", [this, Base, Check]()
		{
			Check.Bad(TEXT("0"), TEXT("ZoneHalfAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.ZoneHalfAngleDegrees = 0.f; });
			Check.Bad(TEXT("90 (zonas se sobrepoem em 0)"), TEXT("ZoneHalfAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.ZoneHalfAngleDegrees = 90.f; });
			Check.Bad(TEXT("120"), TEXT("ZoneHalfAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.ZoneHalfAngleDegrees = 120.f; });
			Check.Good(TEXT("89,999"), Base, [](FDNTakedownConfigRow& R) { R.ZoneHalfAngleDegrees = 89.999f; });
			Check.Good(TEXT("0,001"), Base, [](FDNTakedownConfigRow& R) { R.ZoneHalfAngleDegrees = 0.001f; });
		});

		It("NaN e infinito sao recusados", [this, Base, Check]()
		{
			Check.Bad(TEXT("MinRelativeSpeedKmh NaN"), TEXT("MinRelativeSpeedKmh"), Base, [](FDNTakedownConfigRow& R) { R.MinRelativeSpeedKmh = NaNf; });
			Check.Bad(TEXT("RivalCrashWindowSeconds inf"), TEXT("RivalCrashWindowSeconds"), Base, [](FDNTakedownConfigRow& R) { R.RivalCrashWindowSeconds = Inff; });
			Check.Bad(TEXT("VerticalMinHeightCm NaN"), TEXT("VerticalMinHeightCm"), Base, [](FDNTakedownConfigRow& R) { R.VerticalMinHeightCm = NaNf; });
			Check.Bad(TEXT("BehindMaxAngleDegrees NaN"), TEXT("BehindMaxAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.BehindMaxAngleDegrees = NaNf; });
			Check.Bad(TEXT("SideMaxAngleDegrees inf"), TEXT("SideMaxAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.SideMaxAngleDegrees = Inff; });
			Check.Bad(TEXT("ZoneHalfAngleDegrees NaN"), TEXT("ZoneHalfAngleDegrees"), Base, [](FDNTakedownConfigRow& R) { R.ZoneHalfAngleDegrees = NaNf; });
		});
	});
}

// DERIVA: o CSV real, convertido por ToConfig(), precisa ser IGUAL, campo a campo, ao padrão da struct pura.
// Quem mudar um número só num dos dois lugares faz este teste reprovar.
BEGIN_DEFINE_SPEC(FDNDataDriftSpec, "Detonauto.Dados.Deriva",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataDriftSpec)

void FDNDataDriftSpec::Define()
{
	Describe("CSV real x padrao da struct", [this]()
	{
		It("Turbo.csv = FDNTurboConfig()", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			if (!TestEqual("uma linha em Turbo.csv", Data.TurboConfig.Num(), 1)) { return; }
			const FDNTurboConfig Csv = Data.TurboConfig[0].Row.ToConfig();
			const FDNTurboConfig Def;
			ExpectSame(*this, TEXT("MaxBar"), Csv.MaxBar, Def.MaxBar);
			ExpectSame(*this, TEXT("WrongWayMinSpeedKmh"), Csv.WrongWayMinSpeedKmh, Def.WrongWayMinSpeedKmh);
			ExpectSame(*this, TEXT("WrongWayGainPerSecond"), Csv.WrongWayGainPerSecond, Def.WrongWayGainPerSecond);
			ExpectSame(*this, TEXT("NearMissMaxLateralCm"), Csv.NearMissMaxLateralCm, Def.NearMissMaxLateralCm);
			ExpectSame(*this, TEXT("NearMissMinSpeedKmh"), Csv.NearMissMinSpeedKmh, Def.NearMissMinSpeedKmh);
			ExpectSame(*this, TEXT("NearMissGain"), Csv.NearMissGain, Def.NearMissGain);
			ExpectSame(*this, TEXT("NearMissChainWindowSeconds"), Csv.NearMissChainWindowSeconds, Def.NearMissChainWindowSeconds);
			if (TestEqual("tamanho da lista NearMissChainMultipliers", Csv.NearMissChainMultipliers.Num(), Def.NearMissChainMultipliers.Num()))
			{
				for (int32 i = 0; i < Def.NearMissChainMultipliers.Num(); ++i)
				{
					ExpectSame(*this, *FString::Printf(TEXT("NearMissChainMultipliers[%d]"), i), Csv.NearMissChainMultipliers[i], Def.NearMissChainMultipliers[i]);
				}
			}
			ExpectSame(*this, TEXT("DriftMinAngleDeg"), Csv.DriftMinAngleDeg, Def.DriftMinAngleDeg);
			ExpectSame(*this, TEXT("DriftMaxAngleDeg"), Csv.DriftMaxAngleDeg, Def.DriftMaxAngleDeg);
			ExpectSame(*this, TEXT("DriftMinSpeedKmh"), Csv.DriftMinSpeedKmh, Def.DriftMinSpeedKmh);
			ExpectSame(*this, TEXT("DriftGainPerSecond"), Csv.DriftGainPerSecond, Def.DriftGainPerSecond);
			ExpectSame(*this, TEXT("AirTimeThresholdSeconds"), Csv.AirTimeThresholdSeconds, Def.AirTimeThresholdSeconds);
			ExpectSame(*this, TEXT("AirGainPerSecond"), Csv.AirGainPerSecond, Def.AirGainPerSecond);
			ExpectSame(*this, TEXT("AirMaxGainPerJump"), Csv.AirMaxGainPerJump, Def.AirMaxGainPerJump);
			ExpectSame(*this, TEXT("TakedownSetsBarTo"), Csv.TakedownSetsBarTo, Def.TakedownSetsBarTo);
			ExpectSame(*this, TEXT("ConsumptionPerSecond"), Csv.ConsumptionPerSecond, Def.ConsumptionPerSecond);
			ExpectSame(*this, TEXT("ExtendedBonus"), Csv.ExtendedBonus, Def.ExtendedBonus);
		});

		It("Crash.csv = FDNCrashConfig()", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			if (!TestEqual("uma linha em Crash.csv", Data.CrashConfig.Num(), 1)) { return; }
			const FDNCrashConfig Csv = Data.CrashConfig[0].Row.ToConfig();
			const FDNCrashConfig Def;
			ExpectSame(*this, TEXT("MaxMultiplier"), Csv.MaxMultiplier, Def.MaxMultiplier);
			ExpectSame(*this, TEXT("ChainWindowSeconds"), Csv.ChainWindowSeconds, Def.ChainWindowSeconds);
			ExpectSame(*this, TEXT("ChainBonusPercentPerLink"), Csv.ChainBonusPercentPerLink, Def.ChainBonusPercentPerLink);
			ExpectSame(*this, TEXT("ChainBonusMaxPercent"), Csv.ChainBonusMaxPercent, Def.ChainBonusMaxPercent);
			ExpectSame(*this, TEXT("QuietSpeedMetersPerSecond"), Csv.QuietSpeedMetersPerSecond, Def.QuietSpeedMetersPerSecond);
			ExpectSame(*this, TEXT("QuietSecondsToEnd"), Csv.QuietSecondsToEnd, Def.QuietSecondsToEnd);
			ExpectSame(*this, TEXT("MaxSecondsAfterFirstImpact"), Csv.MaxSecondsAfterFirstImpact, Def.MaxSecondsAfterFirstImpact);
		});

		It("Takedown.csv = FDNTakedownConfig()", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			if (!TestEqual("uma linha em Takedown.csv", Data.TakedownConfig.Num(), 1)) { return; }
			const FDNTakedownConfig Csv = Data.TakedownConfig[0].Row.ToConfig();
			const FDNTakedownConfig Def;
			ExpectSame(*this, TEXT("MinRelativeSpeedKmh"), Csv.MinRelativeSpeedKmh, Def.MinRelativeSpeedKmh);
			ExpectSame(*this, TEXT("RivalCrashWindowSeconds"), Csv.RivalCrashWindowSeconds, Def.RivalCrashWindowSeconds);
			ExpectSame(*this, TEXT("VerticalMinHeightCm"), Csv.VerticalMinHeightCm, Def.VerticalMinHeightCm);
			ExpectSame(*this, TEXT("BehindMaxAngleDegrees"), Csv.BehindMaxAngleDegrees, Def.BehindMaxAngleDegrees);
			ExpectSame(*this, TEXT("TBoneMinAngleDegrees"), Csv.TBoneMinAngleDegrees, Def.TBoneMinAngleDegrees);
			ExpectSame(*this, TEXT("TBoneMaxAngleDegrees"), Csv.TBoneMaxAngleDegrees, Def.TBoneMaxAngleDegrees);
			ExpectSame(*this, TEXT("SideMaxAngleDegrees"), Csv.SideMaxAngleDegrees, Def.SideMaxAngleDegrees);
			ExpectSame(*this, TEXT("ZoneHalfAngleDegrees"), Csv.ZoneHalfAngleDegrees, Def.ZoneHalfAngleDegrees);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
