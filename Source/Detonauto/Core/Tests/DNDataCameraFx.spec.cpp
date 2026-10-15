// Testes de Data/CameraFx.csv e das colunas SpeedFovScale e ShakeScale de Data/Cameras.csv (lote 18, docs/regras-camera.md):
// validação por campo, cópia de todos os campos, o CSV real igual ao padrão da struct (os valores do contrato) e o subsistema.
//
// Faixas: as rampas de velocidade (campo de visão, tremor, desfoque) com o começo em [0, 400] e o fim maior que o começo, até 500;
// SpeedFovMaxAddDeg e TurboFovAddDeg [0, 30]; MaxFovDeg (60, 140]; FovBlendDegPerS (0, 360]; ShakeMaxDeg [0, 5]; ShakeHz (0, 60];
// EdgeBlurMax [0, 1]; EdgeBlurInnerRadius [0, 1); CrashCamMinDeltaVMs (0, 100]; CrashCamTimeScale (0, 1]; CrashCamEnterS e
// CrashCamExitS (0, 5]; CrashCamHoldS [0, 10]; CrashCamCooldownS [0, 30]; CrashCamOrbitRadiusM (0, 50]; CrashCamOrbitHeightM [0, 50];
// CrashCamOrbitDegPerS [0, 360]; CrashCamProbeCm (0, 200]; CrashCamLeadDeg [0, 180]; WreckSteerAccelMs2 e WreckSteerMaxDeltaVMs [0, 50].
// Cameras.csv: SpeedFovScale [0, 2] e ShakeScale [0, 3].

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/Data/DNDataLoader.h"
#include "Core/Data/DNDataValidation.h"
#include "Core/DNGameDataSubsystem.h"
#include "Core/Tests/DNTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDataCameraFxSpec, "Detonauto.Dados.CameraFx",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();

	static FString JoinErrors(const TArray<FString>& Errors) { return Errors.IsEmpty() ? FString(TEXT("(nenhum erro)")) : FString::Join(Errors, TEXT(" | ")); }

	static bool Cites(const TArray<FString>& Errors, const TCHAR* Needle)
	{
		for (const FString& E : Errors) { if (E.Contains(Needle)) { return true; } }
		return false;
	}

	void Bad(const TCHAR* What, const TCHAR* Field, TFunction<void(FDNCameraFxRow&)> Tweak)
	{
		FDNCameraFxRow R = DNTest::CameraFxRow();
		Tweak(R);
		const TArray<FString> Errors = FDNDataValidation::ValidateCameraFx({ DNTest::Entry<FDNCameraFxEntry>(R) });
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Field, *JoinErrors(Errors)), Cites(Errors, Field));
	}

	void BadCamera(const TCHAR* What, const TCHAR* Field, TFunction<void(FDNCameraConfigRow&)> Tweak)
	{
		FDNCameraConfigRow R = DNTest::CameraRow();
		Tweak(R);
		const TArray<FString> Errors = FDNDataValidation::ValidateCameras({ DNTest::Entry<FDNCameraConfigEntry>(R, TEXT("alta-longe")) });
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Field, *JoinErrors(Errors)), Cites(Errors, Field));
	}
END_DEFINE_SPEC(FDNDataCameraFxSpec)

void FDNDataCameraFxSpec::Define()
{
	Describe("CameraFx.csv", [this]()
	{
		It("exatamente uma linha e as faixas de cada campo", [this]()
		{
			TestTrue("padrao da struct", FDNDataValidation::ValidateCameraFx({ DNTest::Entry<FDNCameraFxEntry>(DNTest::CameraFxRow()) }).IsEmpty());
			TestFalse("nenhuma", FDNDataValidation::ValidateCameraFx({}).IsEmpty());
			TestFalse("duas", FDNDataValidation::ValidateCameraFx({ DNTest::Entry<FDNCameraFxEntry>(DNTest::CameraFxRow()),
				DNTest::Entry<FDNCameraFxEntry>(DNTest::CameraFxRow(), TEXT("outra")) }).IsEmpty());
			Bad(TEXT("fim do campo de visao no comeco"), TEXT("SpeedFovFullKmh"), [](FDNCameraFxRow& R) { R.SpeedFovFullKmh = R.SpeedFovStartKmh; });
			Bad(TEXT("campo de visao +31"), TEXT("SpeedFovMaxAddDeg"), [](FDNCameraFxRow& R) { R.SpeedFovMaxAddDeg = 31.f; });
			Bad(TEXT("turbo negativo"), TEXT("TurboFovAddDeg"), [](FDNCameraFxRow& R) { R.TurboFovAddDeg = -1.f; });
			Bad(TEXT("teto 60"), TEXT("MaxFovDeg"), [](FDNCameraFxRow& R) { R.MaxFovDeg = 60.f; });
			Bad(TEXT("teto 141"), TEXT("MaxFovDeg"), [](FDNCameraFxRow& R) { R.MaxFovDeg = 141.f; });
			Bad(TEXT("mistura 0"), TEXT("FovBlendDegPerS"), [](FDNCameraFxRow& R) { R.FovBlendDegPerS = 0.f; });
			Bad(TEXT("fim do tremor antes do comeco"), TEXT("ShakeFullKmh"), [](FDNCameraFxRow& R) { R.ShakeFullKmh = R.ShakeStartKmh - 1.f; });
			Bad(TEXT("tremor de 6 graus"), TEXT("ShakeMaxDeg"), [](FDNCameraFxRow& R) { R.ShakeMaxDeg = 6.f; });
			Bad(TEXT("tremor a 0 Hz"), TEXT("ShakeHz"), [](FDNCameraFxRow& R) { R.ShakeHz = 0.f; });
			Bad(TEXT("fim do desfoque no comeco"), TEXT("EdgeBlurFullKmh"), [](FDNCameraFxRow& R) { R.EdgeBlurFullKmh = R.EdgeBlurStartKmh; });
			Bad(TEXT("desfoque 1,1"), TEXT("EdgeBlurMax"), [](FDNCameraFxRow& R) { R.EdgeBlurMax = 1.1f; });
			Bad(TEXT("raio sem desfoque 1"), TEXT("EdgeBlurInnerRadius"), [](FDNCameraFxRow& R) { R.EdgeBlurInnerRadius = 1.f; });
			Bad(TEXT("disparo 0"), TEXT("CrashCamMinDeltaVMs"), [](FDNCameraFxRow& R) { R.CrashCamMinDeltaVMs = 0.f; });
			Bad(TEXT("escala de tempo 0"), TEXT("CrashCamTimeScale"), [](FDNCameraFxRow& R) { R.CrashCamTimeScale = 0.f; });
			Bad(TEXT("escala de tempo 1,1"), TEXT("CrashCamTimeScale"), [](FDNCameraFxRow& R) { R.CrashCamTimeScale = 1.1f; });
			Bad(TEXT("entrada 0"), TEXT("CrashCamEnterS"), [](FDNCameraFxRow& R) { R.CrashCamEnterS = 0.f; });
			Bad(TEXT("segura NaN"), TEXT("CrashCamHoldS"), [this](FDNCameraFxRow& R) { R.CrashCamHoldS = NaNf; });
			Bad(TEXT("saida 0"), TEXT("CrashCamExitS"), [](FDNCameraFxRow& R) { R.CrashCamExitS = 0.f; });
			Bad(TEXT("espera negativa"), TEXT("CrashCamCooldownS"), [](FDNCameraFxRow& R) { R.CrashCamCooldownS = -1.f; });
			Bad(TEXT("orbita de raio 0"), TEXT("CrashCamOrbitRadiusM"), [](FDNCameraFxRow& R) { R.CrashCamOrbitRadiusM = 0.f; });
			Bad(TEXT("orbita abaixo do chao"), TEXT("CrashCamOrbitHeightM"), [](FDNCameraFxRow& R) { R.CrashCamOrbitHeightM = -1.f; });
			Bad(TEXT("orbita a 361 graus/s"), TEXT("CrashCamOrbitDegPerS"), [](FDNCameraFxRow& R) { R.CrashCamOrbitDegPerS = 361.f; });
			Bad(TEXT("esfera de 0 cm"), TEXT("CrashCamProbeCm"), [](FDNCameraFxRow& R) { R.CrashCamProbeCm = 0.f; });
			Bad(TEXT("orbita comeca 181 graus antes"), TEXT("CrashCamLeadDeg"), [](FDNCameraFxRow& R) { R.CrashCamLeadDeg = 181.f; });
			Bad(TEXT("orbita comeca depois da batida"), TEXT("CrashCamLeadDeg"), [](FDNCameraFxRow& R) { R.CrashCamLeadDeg = -1.f; });
			Bad(TEXT("controle negativo"), TEXT("WreckSteerAccelMs2"), [](FDNCameraFxRow& R) { R.WreckSteerAccelMs2 = -1.f; });
			Bad(TEXT("teto do controle 51"), TEXT("WreckSteerMaxDeltaVMs"), [](FDNCameraFxRow& R) { R.WreckSteerMaxDeltaVMs = 51.f; });
		});

		It("ToSettings copia os 27 campos", [this]()
		{
			FDNCameraFxSettings In;
			float V = 1.f;
			for (float* F : { &In.SpeedFovStartKmh, &In.SpeedFovFullKmh, &In.SpeedFovMaxAddDeg, &In.TurboFovAddDeg, &In.MaxFovDeg, &In.FovBlendDegPerS,
				&In.ShakeStartKmh, &In.ShakeFullKmh, &In.ShakeMaxDeg, &In.ShakeHz, &In.EdgeBlurStartKmh, &In.EdgeBlurFullKmh, &In.EdgeBlurMax,
				&In.EdgeBlurInnerRadius, &In.CrashCamMinDeltaVMs, &In.CrashCamTimeScale, &In.CrashCamEnterS, &In.CrashCamHoldS, &In.CrashCamExitS,
				&In.CrashCamCooldownS, &In.CrashCamOrbitRadiusM, &In.CrashCamOrbitHeightM, &In.CrashCamOrbitDegPerS, &In.CrashCamProbeCm,
				&In.CrashCamLeadDeg, &In.WreckSteerAccelMs2, &In.WreckSteerMaxDeltaVMs })
			{
				*F = 1000.f + V++; // um valor diferente por campo: troca de campos aparece
			}
			const FDNCameraFxSettings Out = DNTest::CameraFxRow(In).ToSettings();
			TestEqual("SpeedFovStartKmh", Out.SpeedFovStartKmh, In.SpeedFovStartKmh);
			TestEqual("SpeedFovFullKmh", Out.SpeedFovFullKmh, In.SpeedFovFullKmh);
			TestEqual("SpeedFovMaxAddDeg", Out.SpeedFovMaxAddDeg, In.SpeedFovMaxAddDeg);
			TestEqual("TurboFovAddDeg", Out.TurboFovAddDeg, In.TurboFovAddDeg);
			TestEqual("MaxFovDeg", Out.MaxFovDeg, In.MaxFovDeg);
			TestEqual("FovBlendDegPerS", Out.FovBlendDegPerS, In.FovBlendDegPerS);
			TestEqual("ShakeStartKmh", Out.ShakeStartKmh, In.ShakeStartKmh);
			TestEqual("ShakeFullKmh", Out.ShakeFullKmh, In.ShakeFullKmh);
			TestEqual("ShakeMaxDeg", Out.ShakeMaxDeg, In.ShakeMaxDeg);
			TestEqual("ShakeHz", Out.ShakeHz, In.ShakeHz);
			TestEqual("EdgeBlurStartKmh", Out.EdgeBlurStartKmh, In.EdgeBlurStartKmh);
			TestEqual("EdgeBlurFullKmh", Out.EdgeBlurFullKmh, In.EdgeBlurFullKmh);
			TestEqual("EdgeBlurMax", Out.EdgeBlurMax, In.EdgeBlurMax);
			TestEqual("EdgeBlurInnerRadius", Out.EdgeBlurInnerRadius, In.EdgeBlurInnerRadius);
			TestEqual("CrashCamMinDeltaVMs", Out.CrashCamMinDeltaVMs, In.CrashCamMinDeltaVMs);
			TestEqual("CrashCamTimeScale", Out.CrashCamTimeScale, In.CrashCamTimeScale);
			TestEqual("CrashCamEnterS", Out.CrashCamEnterS, In.CrashCamEnterS);
			TestEqual("CrashCamHoldS", Out.CrashCamHoldS, In.CrashCamHoldS);
			TestEqual("CrashCamExitS", Out.CrashCamExitS, In.CrashCamExitS);
			TestEqual("CrashCamCooldownS", Out.CrashCamCooldownS, In.CrashCamCooldownS);
			TestEqual("CrashCamOrbitRadiusM", Out.CrashCamOrbitRadiusM, In.CrashCamOrbitRadiusM);
			TestEqual("CrashCamOrbitHeightM", Out.CrashCamOrbitHeightM, In.CrashCamOrbitHeightM);
			TestEqual("CrashCamOrbitDegPerS", Out.CrashCamOrbitDegPerS, In.CrashCamOrbitDegPerS);
			TestEqual("CrashCamProbeCm", Out.CrashCamProbeCm, In.CrashCamProbeCm);
			TestEqual("CrashCamLeadDeg", Out.CrashCamLeadDeg, In.CrashCamLeadDeg);
			TestEqual("WreckSteerAccelMs2", Out.WreckSteerAccelMs2, In.WreckSteerAccelMs2);
			TestEqual("WreckSteerMaxDeltaVMs", Out.WreckSteerMaxDeltaVMs, In.WreckSteerMaxDeltaVMs);
		});

		It("o CSV real carrega, e valido e e igual ao padrao da struct (os valores do contrato)", [this]()
		{
			TArray<FDNCameraFxEntry> Rows;
			const TArray<FString> LoadErrors = FDNDataLoader::LoadEntries<FDNCameraFxRow>(TEXT("Data/CameraFx.csv"), Rows);
			TestTrue(FString::Printf(TEXT("carga: %s"), *JoinErrors(LoadErrors)), LoadErrors.IsEmpty());
			TestTrue(FString::Printf(TEXT("validacao: %s"), *JoinErrors(FDNDataValidation::ValidateCameraFx(Rows))), FDNDataValidation::ValidateCameraFx(Rows).IsEmpty());
			if (!TestEqual("uma linha", Rows.Num(), 1))
			{
				return;
			}
			const FDNCameraFxSettings Csv = Rows[0].Row.ToSettings();
			const FDNCameraFxSettings Def;
			TestEqual("igual ao padrao: SpeedFovMaxAddDeg", Csv.SpeedFovMaxAddDeg, Def.SpeedFovMaxAddDeg);
			TestEqual("igual ao padrao: TurboFovAddDeg", Csv.TurboFovAddDeg, Def.TurboFovAddDeg);
			TestEqual("igual ao padrao: MaxFovDeg", Csv.MaxFovDeg, Def.MaxFovDeg);
			TestEqual("igual ao padrao: ShakeMaxDeg", Csv.ShakeMaxDeg, Def.ShakeMaxDeg);
			TestEqual("igual ao padrao: EdgeBlurMax", Csv.EdgeBlurMax, Def.EdgeBlurMax);
			TestEqual("igual ao padrao: CrashCamTimeScale", Csv.CrashCamTimeScale, Def.CrashCamTimeScale);
			TestEqual("igual ao padrao: CrashCamHoldS", Csv.CrashCamHoldS, Def.CrashCamHoldS);
			TestEqual("igual ao padrao: CrashCamOrbitRadiusM", Csv.CrashCamOrbitRadiusM, Def.CrashCamOrbitRadiusM);
			TestEqual("igual ao padrao: WreckSteerMaxDeltaVMs", Csv.WreckSteerMaxDeltaVMs, Def.WreckSteerMaxDeltaVMs);
			const FDNCameraFxRow DefRow = DNTest::CameraFxRow(Def);
			const FDNCameraFxRow& CsvRow = Rows[0].Row;
			for (TFieldIterator<FFloatProperty> It(FDNCameraFxRow::StaticStruct()); It; ++It)
			{
				TestEqual(FString::Printf(TEXT("CSV = padrao em %s"), *It->GetName()), It->GetPropertyValue_InContainer(&CsvRow), It->GetPropertyValue_InContainer(&DefRow));
			}
			TestEqual("contrato: comeca a abrir a 60 km/h", Def.SpeedFovStartKmh, 60.f);
			TestEqual("contrato: tremor a partir de 150 km/h", Def.ShakeStartKmh, 150.f);
			TestEqual("contrato: dispara com 15 m/s", Def.CrashCamMinDeltaVMs, 15.f);
			TestEqual("contrato: camera lenta 0,25", Def.CrashCamTimeScale, 0.25f);
			TestEqual("contrato: espera de 2 s", Def.CrashCamCooldownS, 2.f);
			TestEqual("captura aprovada: orbita comeca 90 graus antes do lado da batida", Def.CrashCamLeadDeg, 90.f);
		});
	});

	Describe("Cameras.csv (lote 18)", [this]()
	{
		It("SpeedFovScale e ShakeScale: copia e faixas", [this]()
		{
			FDNCameraConfigRow R = DNTest::CameraRow();
			R.SpeedFovScale = 0.5f;
			R.ShakeScale = 1.2f;
			const FDNCameraConfig C = R.ToConfig();
			TestEqual("SpeedFovScale", C.SpeedFovScale, 0.5f);
			TestEqual("ShakeScale", C.ShakeScale, 1.2f);
			BadCamera(TEXT("campo de visao 2,1"), TEXT("SpeedFovScale"), [](FDNCameraConfigRow& X) { X.SpeedFovScale = 2.1f; });
			BadCamera(TEXT("campo de visao negativo"), TEXT("SpeedFovScale"), [](FDNCameraConfigRow& X) { X.SpeedFovScale = -0.1f; });
			BadCamera(TEXT("tremor 3,1"), TEXT("ShakeScale"), [](FDNCameraConfigRow& X) { X.ShakeScale = 3.1f; });
		});

		It("o CSV real tem os valores do contrato por modo", [this]()
		{
			TArray<FDNCameraConfigEntry> Rows;
			const TArray<FString> LoadErrors = FDNDataLoader::LoadEntries<FDNCameraConfigRow>(TEXT("Data/Cameras.csv"), Rows);
			TestTrue(FString::Printf(TEXT("carga: %s"), *JoinErrors(LoadErrors)), LoadErrors.IsEmpty());
			const struct { const TCHAR* Id; float Fov; float Shake; } Contract[] = {
				{ TEXT("alta-longe"), 1.0f, 1.0f }, { TEXT("alta-muito-longe"), 1.0f, 0.7f }, { TEXT("capo"), 1.0f, 1.2f },
				{ TEXT("frente-para-choque"), 1.0f, 1.2f }, { TEXT("motorista"), 0.5f, 0.8f } };
			for (const auto& C : Contract)
			{
				const FDNCameraConfigEntry* E = Rows.FindByPredicate([&C](const FDNCameraConfigEntry& X) { return X.Id == C.Id; });
				if (!TestNotNull(C.Id, E))
				{
					continue;
				}
				TestEqual(FString::Printf(TEXT("%s SpeedFovScale"), C.Id), E->Row.SpeedFovScale, C.Fov);
				TestEqual(FString::Printf(TEXT("%s ShakeScale"), C.Id), E->Row.ShakeScale, C.Shake);
			}
		});
	});

	Describe("UDNGameDataSubsystem", [this]()
	{
		It("le a configuracao da camera do CSV; tabela invalida vale o padrao", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			AddExpectedError(TEXT("Dados do jogo:"), EAutomationExpectedErrorFlags::Contains, 0); // as outras tabelas vazias dão erro
			FDNData Data;
			FDNCameraFxSettings S;
			S.CrashCamTimeScale = 0.4f;
			Data.CameraFx = { DNTest::Entry<FDNCameraFxEntry>(DNTest::CameraFxRow(S)) };
			Sub->ApplyData(Data, {});
			TestEqual("configuracao do CSV", Sub->GetCameraFxSettings().CrashCamTimeScale, 0.4f);
			Data.CameraFx[0].Row.CrashCamTimeScale = 0.f;
			Sub->ApplyData(Data, {});
			TestEqual("configuracao invalida: padrao", Sub->GetCameraFxSettings().CrashCamTimeScale, FDNCameraFxSettings().CrashCamTimeScale);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
