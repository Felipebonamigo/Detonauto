// Testes de Data/Cameras.csv (modos de câmera): validação por campo, a linha obrigatória "alta-longe", e DERIVA entre essa linha do
// CSV e o padrão da struct pura FDNCameraConfig. Mesmo padrão dos outros CSVs de configuração.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/Data/DNDataValidation.h"
#include "Core/Data/DNDataLoader.h"
#include "Core/Tests/DNTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDataCamerasSpec, "Detonauto.Dados.ConfigCameras",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	const float Inff = std::numeric_limits<float>::infinity();

	static FString JoinErrors(const TArray<FString>& Errors) { return Errors.IsEmpty() ? FString(TEXT("(nenhum erro)")) : FString::Join(Errors, TEXT(" | ")); }
	static bool Cites(const TArray<FString>& Errors, const TCHAR* Needle)
	{
		for (const FString& E : Errors) { if (E.Contains(Needle)) { return true; } }
		return false;
	}

	TArray<FString> ValidateWith(TFunction<void(FDNCameraConfigRow&)> Tweak)
	{
		FDNCameraConfigRow R = DNTest::CameraRow();
		Tweak(R);
		return FDNDataValidation::ValidateCameras({ DNTest::Entry<FDNCameraConfigEntry>(R, TEXT("alta-longe")) });
	}
	void Bad(const TCHAR* What, const TCHAR* Field, TFunction<void(FDNCameraConfigRow&)> Tweak)
	{
		const TArray<FString> Errors = ValidateWith(Tweak);
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Field, *JoinErrors(Errors)), Cites(Errors, Field));
	}
	void Good(const TCHAR* What, TFunction<void(FDNCameraConfigRow&)> Tweak)
	{
		const TArray<FString> Errors = ValidateWith(Tweak);
		TestTrue(FString::Printf(TEXT("%s: esperava zero erros; veio: %s"), What, *JoinErrors(Errors)), Errors.IsEmpty());
	}
	void Range(const TCHAR* Field, float Min, float Max, TFunction<void(FDNCameraConfigRow&, float)> Set)
	{
		const float Margin = FMath::Max(1.0e-3f, (Max - Min) * 1.0e-3f);
		Good(*FString::Printf(TEXT("%s no minimo"), Field), [&](FDNCameraConfigRow& R) { Set(R, Min); });
		Good(*FString::Printf(TEXT("%s no maximo"), Field), [&](FDNCameraConfigRow& R) { Set(R, Max); });
		Bad(*FString::Printf(TEXT("%s abaixo do minimo"), Field), Field, [&](FDNCameraConfigRow& R) { Set(R, Min - Margin); });
		Bad(*FString::Printf(TEXT("%s acima do maximo"), Field), Field, [&](FDNCameraConfigRow& R) { Set(R, Max + Margin); });
		Bad(*FString::Printf(TEXT("%s NaN"), Field), Field, [&](FDNCameraConfigRow& R) { Set(R, NaNf); });
		Bad(*FString::Printf(TEXT("%s infinito"), Field), Field, [&](FDNCameraConfigRow& R) { Set(R, Inff); });
	}
END_DEFINE_SPEC(FDNDataCamerasSpec)

void FDNDataCamerasSpec::Define()
{
	Describe("Cameras.csv", [this]()
	{
		It("a configuracao inicial (alta-longe) e valida", [this]()
		{
			TestEqual("erros", FDNDataValidation::ValidateCameras({ DNTest::CameraEntry() }).Num(), 0);
		});

		It("exige a linha alta-longe, mesmo escrita com outra caixa, e pelo menos uma linha", [this]()
		{
			TestTrue("tabela vazia", Cites(FDNDataValidation::ValidateCameras({}), TEXT("alta-longe")));
			TestTrue("sem alta-longe", Cites(FDNDataValidation::ValidateCameras({ DNTest::CameraEntry(TEXT("capo")) }), TEXT("alta-longe")));
			TestEqual("Alta-Longe vale", FDNDataValidation::ValidateCameras({ DNTest::CameraEntry(TEXT("Alta-Longe")) }).Num(), 0);
		});

		It("ids: vazio, repetido (so na caixa) e caractere invalido sao recusados", [this]()
		{
			TestTrue("vazio", Cites(FDNDataValidation::ValidateCameras({ DNTest::CameraEntry(), DNTest::CameraEntry(TEXT("")) }), TEXT("id vazio")));
			TestTrue("repetido na caixa", Cites(FDNDataValidation::ValidateCameras({ DNTest::CameraEntry(), DNTest::CameraEntry(TEXT("ALTA-LONGE")) }), TEXT("mesmo id")));
			TestTrue("espaco", Cites(FDNDataValidation::ValidateCameras({ DNTest::CameraEntry(), DNTest::CameraEntry(TEXT("capo frente")) }), TEXT("caractere")));
		});

		It("varias linhas: cada uma e validada e o erro cita a linha", [this]()
		{
			FDNCameraConfigRow Ruim = DNTest::CameraRow();
			Ruim.Fov = 5.f;
			const TArray<FString> Errors = FDNDataValidation::ValidateCameras({ DNTest::CameraEntry(), DNTest::Entry<FDNCameraConfigEntry>(Ruim, TEXT("capo")) });
			TestTrue("cita a linha", Cites(Errors, TEXT("'capo'")));
			TestTrue("cita o campo", Cites(Errors, TEXT("Fov")));
			TestFalse("a linha boa nao e acusada", Cites(Errors, TEXT("'alta-longe'")));
		});

		It("Anchor: so Chase ou Car (sem diferenciar a caixa)", [this]()
		{
			Good(TEXT("Car"), [](FDNCameraConfigRow& R) { R.Anchor = TEXT("Car"); R.DistanceCm = 0.f; });
			Good(TEXT("chase minusculo"), [](FDNCameraConfigRow& R) { R.Anchor = TEXT("chase"); });
			Bad(TEXT("outro"), TEXT("Anchor"), [](FDNCameraConfigRow& R) { R.Anchor = TEXT("Orbit"); });
			Bad(TEXT("vazio"), TEXT("Anchor"), [](FDNCameraConfigRow& R) { R.Anchor = TEXT(""); });
		});

		It("DistanceCm: Chase 150 a 3000; Car precisa ser 0", [this]()
		{
			Range(TEXT("DistanceCm"), 150.f, 3000.f, [](FDNCameraConfigRow& R, float V) { R.DistanceCm = V; });
			Bad(TEXT("Car com distancia"), TEXT("DistanceCm"), [](FDNCameraConfigRow& R) { R.Anchor = TEXT("Car"); R.DistanceCm = 100.f; });
		});

		It("HeightCm, ForwardCm, SideCm, PitchDeg", [this]()
		{
			Range(TEXT("HeightCm"), -50.f, 800.f, [](FDNCameraConfigRow& R, float V) { R.HeightCm = V; });
			Range(TEXT("ForwardCm"), -300.f, 400.f, [](FDNCameraConfigRow& R, float V) { R.ForwardCm = V; });
			Range(TEXT("SideCm"), -150.f, 150.f, [](FDNCameraConfigRow& R, float V) { R.SideCm = V; });
			Range(TEXT("PitchDeg"), -45.f, 45.f, [](FDNCameraConfigRow& R, float V) { R.PitchDeg = V; });
		});

		It("Fov e Smoothing", [this]()
		{
			Range(TEXT("Fov"), 40.f, 120.f, [](FDNCameraConfigRow& R, float V) { R.Fov = V; });
			Range(TEXT("Smoothing"), 0.f, 30.f, [](FDNCameraConfigRow& R, float V) { R.Smoothing = V; });
		});

		It("campo fora da faixa da exatamente um erro (a regra do tipo nao repete o aviso)", [this]()
		{
			const TArray<FString> E = ValidateWith([](FDNCameraConfigRow& R) { R.DistanceCm = 20.f; });
			TestEqual("um erro", E.Num(), 1);
			TestTrue(*JoinErrors(E), Cites(E, TEXT("DistanceCm")));
		});

		It("um erro nao esconde o outro: acumula todos", [this]()
		{
			const TArray<FString> E = ValidateWith([](FDNCameraConfigRow& R) { R.Fov = 5.f; R.PitchDeg = 90.f; R.Smoothing = -1.f; });
			TestTrue("Fov", Cites(E, TEXT("Fov")));
			TestTrue("PitchDeg", Cites(E, TEXT("PitchDeg")));
			TestTrue("Smoothing", Cites(E, TEXT("Smoothing")));
		});
	});
}

// DERIVA: a linha alta-longe do CSV real, convertida por ToConfig(), precisa ser IGUAL, campo a campo, ao padrão da struct pura.
BEGIN_DEFINE_SPEC(FDNDataCamerasDriftSpec, "Detonauto.Dados.DerivaCameras",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataCamerasDriftSpec)

void FDNDataCamerasDriftSpec::Define()
{
	Describe("Cameras.csv real x padrao da struct", [this]()
	{
		It("a linha alta-longe de Cameras.csv = FDNCameraConfig()", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			const FDNCameraConfigEntry* Row = Data.CameraModes.FindByPredicate([](const FDNCameraConfigEntry& E) { return E.Id.Equals(TEXT("alta-longe"), ESearchCase::IgnoreCase); });
			if (!TestNotNull("linha alta-longe", Row)) { return; }
			const FDNCameraConfig Csv = Row->Row.ToConfig();
			const FDNCameraConfig Def;
			TestEqual("Anchor", Csv.Anchor, Def.Anchor);
			auto Same = [this](const TCHAR* Field, float FromCsv, float FromStruct)
			{
				TestTrue(FString::Printf(TEXT("%s: o CSV tem %.6g e o padrao da struct tem %.6g (mudou so num dos dois lugares?)"), Field, FromCsv, FromStruct), FromCsv == FromStruct);
			};
			Same(TEXT("DistanceCm"), Csv.DistanceCm, Def.DistanceCm);
			Same(TEXT("HeightCm"), Csv.HeightCm, Def.HeightCm);
			Same(TEXT("ForwardCm"), Csv.ForwardCm, Def.ForwardCm);
			Same(TEXT("SideCm"), Csv.SideCm, Def.SideCm);
			Same(TEXT("PitchDeg"), Csv.PitchDeg, Def.PitchDeg);
			Same(TEXT("Fov"), Csv.Fov, Def.Fov);
			Same(TEXT("Smoothing"), Csv.Smoothing, Def.Smoothing);
		});

		It("os cinco modos pedidos existem, na ordem da tecla C, e o primeiro e o padrao", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			const TCHAR* Expected[] = { TEXT("alta-longe"), TEXT("alta-muito-longe"), TEXT("capo"), TEXT("frente-para-choque"), TEXT("motorista") };
			if (!TestEqual("quantidade", Data.CameraModes.Num(), 5)) { return; }
			for (int32 i = 0; i < 5; ++i)
			{
				TestEqual(FString::Printf(TEXT("modo %d"), i), Data.CameraModes[i].Id, FString(Expected[i]));
			}
		});

		It("as tres de dentro do carro (capo, para-choque, motorista) sao Car; as duas de fora sao Chase, e a muito longe e mais longe e mais alta", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			if (Data.CameraModes.Num() != 5) { return; }
			TestEqual("alta-longe", Data.CameraModes[0].Row.Anchor, FString(TEXT("Chase")));
			TestEqual("alta-muito-longe", Data.CameraModes[1].Row.Anchor, FString(TEXT("Chase")));
			TestTrue("muito longe: mais distante", Data.CameraModes[1].Row.DistanceCm > Data.CameraModes[0].Row.DistanceCm);
			TestTrue("muito longe: mais alta", Data.CameraModes[1].Row.HeightCm > Data.CameraModes[0].Row.HeightCm);
			for (int32 i = 2; i < 5; ++i)
			{
				TestEqual(FString::Printf(TEXT("%s: Car"), *Data.CameraModes[i].Id), Data.CameraModes[i].Row.Anchor, FString(TEXT("Car")));
			}
			TestTrue("para-choque mais a frente que o capo", Data.CameraModes[3].Row.ForwardCm > Data.CameraModes[2].Row.ForwardCm);
			TestTrue("motorista a esquerda (volante)", Data.CameraModes[4].Row.SideCm < 0.f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
