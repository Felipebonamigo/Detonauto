// Testes do ajuste de carro (projeto descartável 2): tabela CSV, conversão linha -> parâmetros e limites.
// Nada aqui depende de mundo nem de física.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DNCarTuning.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNCarTuningSpec, "VeiculoTeste.Carro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNCarTuningSpec)

void FDNCarTuningSpec::Define()
{
	Describe("Tabela TestCars.csv", [this]()
	{
		It("tem todas as colunas esperadas no cabecalho", [this]()
		{
			FString Csv;
			const FString Path = FPaths::Combine(FPaths::ProjectDir(), FDNCarTuning::CsvRelativePath);
			if (!TestTrue(FString::Printf(TEXT("le %s"), *Path), FFileHelper::LoadFileToString(Csv, *Path)))
			{
				return;
			}

			TArray<FString> Lines;
			Csv.ParseIntoArrayLines(Lines);
			if (!TestTrue("tem cabecalho", Lines.Num() > 0))
			{
				return;
			}

			TArray<FString> Header;
			Lines[0].ParseIntoArray(Header, TEXT(","), false);
			for (const TCHAR* Column : { TEXT("MassKg"), TEXT("MaxTorque"), TEXT("LateralGrip"),
				TEXT("SuspensionStiffness"), TEXT("SteeringGain") })
			{
				TestTrue(FString::Printf(TEXT("coluna %s"), Column), Header.Contains(Column));
			}
		});

		It("importa como DataTable sem problemas e com linhas validas", [this]()
		{
			FString Csv;
			const FString Path = FPaths::Combine(FPaths::ProjectDir(), FDNCarTuning::CsvRelativePath);
			if (!TestTrue("le o CSV", FFileHelper::LoadFileToString(Csv, *Path)))
			{
				return;
			}

			UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
			Table->RowStruct = FDNTestCarRow::StaticStruct();
			const TArray<FString> Problems = Table->CreateTableFromCSVString(Csv);
			TestEqual("problemas de importacao", Problems.Num(), 0);

			TestTrue("tem a linha Padrao", Table->GetRowMap().Contains(FDNCarTuning::DefaultRowName));
			TestTrue("tem pelo menos uma linha", Table->GetRowMap().Num() > 0);

			for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
			{
				const FDNTestCarRow* Row = reinterpret_cast<const FDNTestCarRow*>(Pair.Value);
				FString Error;
				TestTrue(FString::Printf(TEXT("linha %s valida: %s"), *Pair.Key.ToString(), *Error),
					FDNCarTuning::ValidateRow(*Row, Error));
			}
		});
	});

	Describe("Conversao linha -> parametros", [this]()
	{
		It("copia os valores quando estao dentro dos limites", [this]()
		{
			FDNTestCarRow Row;
			Row.MassKg = 1400.f;
			Row.MaxTorque = 800.f;
			Row.LateralGrip = 1.5f;
			Row.SuspensionStiffness = 300.f;
			Row.SteeringGain = 1.2f;

			const FDNCarTuning T = FDNCarTuning::FromRow(Row);
			TestEqual("massa", T.MassKg, 1400.f);
			TestEqual("torque", T.MaxTorque, 800.f);
			TestEqual("aderencia", T.LateralGrip, 1.5f);
			TestEqual("suspensao", T.SpringRate, 300.f);
			TestEqual("volante", T.SteeringGain, 1.2f);
		});
	});

	Describe("Limites", [this]()
	{
		It("limita valores altos ao maximo", [this]()
		{
			FDNTestCarRow Row;
			Row.MassKg = 1e9f;
			Row.MaxTorque = 1e9f;
			Row.LateralGrip = 1e9f;
			Row.SuspensionStiffness = 1e9f;
			Row.SteeringGain = 1e9f;

			const FDNCarTuning T = FDNCarTuning::FromRow(Row);
			TestEqual("massa", T.MassKg, FDNCarTuning::MaxMassKg);
			TestEqual("torque", T.MaxTorque, FDNCarTuning::MaxMaxTorque);
			TestEqual("aderencia", T.LateralGrip, FDNCarTuning::MaxLateralGrip);
			TestEqual("suspensao", T.SpringRate, FDNCarTuning::MaxSpringRate);
			TestEqual("volante", T.SteeringGain, FDNCarTuning::MaxSteeringGain);
		});

		It("limita valores baixos ao minimo", [this]()
		{
			FDNTestCarRow Row;
			Row.MassKg = 0.001f;
			Row.MaxTorque = 0.001f;
			Row.LateralGrip = 0.001f;
			Row.SuspensionStiffness = 0.001f;
			Row.SteeringGain = 0.001f;

			const FDNCarTuning T = FDNCarTuning::FromRow(Row);
			TestEqual("massa", T.MassKg, FDNCarTuning::MinMassKg);
			TestEqual("torque", T.MaxTorque, FDNCarTuning::MinMaxTorque);
			TestEqual("aderencia", T.LateralGrip, FDNCarTuning::MinLateralGrip);
			TestEqual("suspensao", T.SpringRate, FDNCarTuning::MinSpringRate);
			TestEqual("volante", T.SteeringGain, FDNCarTuning::MinSteeringGain);
		});

		It("rejeita massa zero, negativa ou NaN na validacao", [this]()
		{
			FString Error;
			FDNTestCarRow Row;

			Row.MassKg = 0.f;
			TestFalse("zero", FDNCarTuning::ValidateRow(Row, Error));
			Row.MassKg = -5.f;
			TestFalse("negativa", FDNCarTuning::ValidateRow(Row, Error));
			Row.MassKg = std::numeric_limits<float>::quiet_NaN();
			TestFalse("NaN", FDNCarTuning::ValidateRow(Row, Error));
			Row.MassKg = 1500.f;
			TestTrue("valida", FDNCarTuning::ValidateRow(Row, Error));
		});

		It("troca NaN por um valor dentro dos limites na conversao", [this]()
		{
			FDNTestCarRow Row;
			Row.MaxTorque = std::numeric_limits<float>::quiet_NaN();
			const FDNCarTuning T = FDNCarTuning::FromRow(Row);
			TestTrue("torque finito e dentro do limite",
				FMath::IsFinite(T.MaxTorque) && T.MaxTorque >= FDNCarTuning::MinMaxTorque && T.MaxTorque <= FDNCarTuning::MaxMaxTorque);
		});
	});

	// ---------------- Lote 8: afinacao arcade ----------------

	Describe("Colunas novas (afinacao arcade)", [this]()
	{
		It("o cabecalho do CSV tem as colunas novas", [this]()
		{
			FString Csv;
			if (!TestTrue("le o CSV", FFileHelper::LoadFileToString(Csv, *FPaths::Combine(FPaths::ProjectDir(), FDNCarTuning::CsvRelativePath)))) { return; }
			TArray<FString> Lines;
			Csv.ParseIntoArrayLines(Lines);
			TArray<FString> Header;
			Lines[0].ParseIntoArray(Header, TEXT(","), false);
			for (const TCHAR* Column : { TEXT("CenterOfMassDropCm"), TEXT("SuspensionDampingRatio"), TEXT("SuspensionTravelCm"),
				TEXT("RollbarScaling"), TEXT("RearGripFactor"), TEXT("HandbrakeRearGrip"), TEXT("HandbrakeRampInSeconds"),
				TEXT("HandbrakeRampOutSeconds"), TEXT("HandbrakeTorque"), TEXT("CameraDistanceCm"), TEXT("CameraHeightCm"),
				TEXT("CameraFov"), TEXT("NitroAccelMps2"), TEXT("NitroDrainPerSecond"), TEXT("NitroRegenPerSecond") })
			{
				TestTrue(FString::Printf(TEXT("coluna %s"), Column), Header.Contains(Column));
			}
		});

		It("os padroes da linha sao os do carro do template (uma linha so com as colunas antigas nao muda o carro)", [this]()
		{
			const FDNTestCarRow Row;
			TestEqual("COM", Row.CenterOfMassDropCm, 0.f);
			TestEqual("amortecimento", Row.SuspensionDampingRatio, 0.5f);
			TestEqual("curso", Row.SuspensionTravelCm, 10.f);
			TestEqual("barra", Row.RollbarScaling, 0.15f);
			TestEqual("aderencia traseira", Row.RearGripFactor, 1.f);
			TestEqual("freio de mao: aderencia", Row.HandbrakeRearGrip, 1.f);
			TestEqual("freio de mao: torque", Row.HandbrakeTorque, 6000.f);
			TestEqual("camera: distancia", Row.CameraDistanceCm, 650.f);
			TestEqual("camera: altura", Row.CameraHeightCm, 150.f);
			TestEqual("camera: FOV", Row.CameraFov, 90.f);
		});

		It("FromRow copia os valores novos quando estao dentro dos limites", [this]()
		{
			FDNTestCarRow Row;
			Row.CenterOfMassDropCm = 25.f; Row.SuspensionDampingRatio = 0.6f; Row.SuspensionTravelCm = 18.f; Row.RollbarScaling = 0.05f;
			Row.RearGripFactor = 0.7f; Row.HandbrakeRearGrip = 0.35f; Row.HandbrakeRampInSeconds = 0.12f; Row.HandbrakeRampOutSeconds = 0.7f;
			Row.HandbrakeTorque = 3000.f; Row.CameraDistanceCm = 810.f; Row.CameraHeightCm = 185.f; Row.CameraFov = 95.f;
			Row.NitroAccelMps2 = 12.f; Row.NitroDrainPerSecond = 25.f; Row.NitroRegenPerSecond = 8.f;
			const FDNCarTuning T = FDNCarTuning::FromRow(Row);
			TestEqual("COM", T.CenterOfMassDropCm, 25.f);
			TestEqual("amortecimento", T.SuspensionDampingRatio, 0.6f);
			TestEqual("curso", T.SuspensionTravelCm, 18.f);
			TestEqual("barra", T.RollbarScaling, 0.05f);
			TestEqual("aderencia traseira", T.RearGripFactor, 0.7f);
			TestEqual("freio de mao: aderencia", T.HandbrakeRearGrip, 0.35f);
			TestEqual("freio de mao: sobe", T.HandbrakeRampInSeconds, 0.12f);
			TestEqual("freio de mao: desce", T.HandbrakeRampOutSeconds, 0.7f);
			TestEqual("freio de mao: torque", T.HandbrakeTorque, 3000.f);
			TestEqual("camera: distancia", T.CameraDistanceCm, 810.f);
			TestEqual("camera: altura", T.CameraHeightCm, 185.f);
			TestEqual("camera: FOV", T.CameraFov, 95.f);
			TestEqual("nitro: aceleracao", T.NitroAccelMps2, 12.f);
			TestEqual("nitro: gasto", T.NitroDrainPerSecond, 25.f);
			TestEqual("nitro: recarga", T.NitroRegenPerSecond, 8.f);
		});

		It("FromRow limita os valores novos nos extremos", [this]()
		{
			FDNTestCarRow Hi;
			Hi.CenterOfMassDropCm = 1e9f; Hi.SuspensionDampingRatio = 1e9f; Hi.SuspensionTravelCm = 1e9f; Hi.RollbarScaling = 1e9f;
			Hi.RearGripFactor = 1e9f; Hi.HandbrakeRearGrip = 1e9f; Hi.HandbrakeRampInSeconds = 1e9f; Hi.HandbrakeRampOutSeconds = 1e9f;
			Hi.HandbrakeTorque = 1e9f; Hi.CameraDistanceCm = 1e9f; Hi.CameraHeightCm = 1e9f; Hi.CameraFov = 1e9f;
			Hi.NitroAccelMps2 = 1e9f; Hi.NitroDrainPerSecond = 1e9f; Hi.NitroRegenPerSecond = 1e9f;
			const FDNCarTuning H = FDNCarTuning::FromRow(Hi);
			TestEqual("COM", H.CenterOfMassDropCm, FDNCarTuning::MaxCenterOfMassDropCm);
			TestEqual("amortecimento", H.SuspensionDampingRatio, FDNCarTuning::MaxDampingRatio);
			TestEqual("curso", H.SuspensionTravelCm, FDNCarTuning::MaxTravelCm);
			TestEqual("barra", H.RollbarScaling, FDNCarTuning::MaxRollbarScaling);
			TestEqual("aderencia traseira", H.RearGripFactor, FDNCarTuning::MaxRearGripFactor);
			TestEqual("freio de mao: aderencia", H.HandbrakeRearGrip, FDNCarTuning::MaxHandbrakeRearGrip);
			TestEqual("freio de mao: sobe", H.HandbrakeRampInSeconds, FDNCarTuning::MaxRampSeconds);
			TestEqual("freio de mao: desce", H.HandbrakeRampOutSeconds, FDNCarTuning::MaxRampSeconds);
			TestEqual("freio de mao: torque", H.HandbrakeTorque, FDNCarTuning::MaxHandbrakeTorque);
			TestEqual("camera: distancia", H.CameraDistanceCm, FDNCarTuning::MaxCameraDistanceCm);
			TestEqual("camera: altura", H.CameraHeightCm, FDNCarTuning::MaxCameraHeightCm);
			TestEqual("camera: FOV", H.CameraFov, FDNCarTuning::MaxCameraFov);
			TestEqual("nitro: aceleracao", H.NitroAccelMps2, FDNCarTuning::MaxNitroAccel);
			TestEqual("nitro: gasto", H.NitroDrainPerSecond, FDNCarTuning::MaxNitroDrain);
			TestEqual("nitro: recarga", H.NitroRegenPerSecond, FDNCarTuning::MaxNitroRegen);

			FDNTestCarRow Lo;
			Lo.CenterOfMassDropCm = -1.f; Lo.SuspensionDampingRatio = 0.0001f; Lo.SuspensionTravelCm = 0.0001f; Lo.RollbarScaling = -1.f;
			Lo.RearGripFactor = 0.0001f; Lo.HandbrakeRearGrip = 0.0001f; Lo.HandbrakeRampInSeconds = 0.0001f; Lo.HandbrakeRampOutSeconds = 0.0001f;
			Lo.HandbrakeTorque = -1.f; Lo.CameraDistanceCm = 0.0001f; Lo.CameraHeightCm = -1.f; Lo.CameraFov = 0.0001f;
			Lo.NitroAccelMps2 = -1.f; Lo.NitroDrainPerSecond = 0.0001f; Lo.NitroRegenPerSecond = -1.f;
			const FDNCarTuning L = FDNCarTuning::FromRow(Lo);
			TestEqual("COM", L.CenterOfMassDropCm, 0.f);
			TestEqual("amortecimento", L.SuspensionDampingRatio, FDNCarTuning::MinDampingRatio);
			TestEqual("curso", L.SuspensionTravelCm, FDNCarTuning::MinTravelCm);
			TestEqual("barra", L.RollbarScaling, 0.f);
			TestEqual("aderencia traseira", L.RearGripFactor, FDNCarTuning::MinRearGripFactor);
			TestEqual("freio de mao: aderencia", L.HandbrakeRearGrip, FDNCarTuning::MinHandbrakeRearGrip);
			TestEqual("freio de mao: sobe", L.HandbrakeRampInSeconds, FDNCarTuning::MinRampSeconds);
			TestEqual("freio de mao: desce", L.HandbrakeRampOutSeconds, FDNCarTuning::MinRampSeconds);
			TestEqual("freio de mao: torque", L.HandbrakeTorque, 0.f);
			TestEqual("camera: distancia", L.CameraDistanceCm, FDNCarTuning::MinCameraDistanceCm);
			TestEqual("camera: altura", L.CameraHeightCm, 0.f);
			TestEqual("camera: FOV", L.CameraFov, FDNCarTuning::MinCameraFov);
			TestEqual("nitro: aceleracao", L.NitroAccelMps2, 0.f);
			TestEqual("nitro: gasto", L.NitroDrainPerSecond, FDNCarTuning::MinNitroDrain);
			TestEqual("nitro: recarga", L.NitroRegenPerSecond, 0.f);
		});

		It("NaN em campo novo vira o valor padrao dentro dos limites", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			FDNTestCarRow Row;
			Row.CameraDistanceCm = NaN; Row.RearGripFactor = NaN; Row.NitroAccelMps2 = NaN; Row.HandbrakeRampInSeconds = NaN;
			const FDNCarTuning T = FDNCarTuning::FromRow(Row);
			TestEqual("camera", T.CameraDistanceCm, 650.f);
			TestEqual("aderencia traseira", T.RearGripFactor, 1.f);
			TestEqual("nitro", T.NitroAccelMps2, 10.f);
			TestTrue("rampa finita e dentro do limite", FMath::IsFinite(T.HandbrakeRampInSeconds) && T.HandbrakeRampInSeconds >= FDNCarTuning::MinRampSeconds);
		});

		It("ValidateRow recusa NaN, infinito e negativo em cada campo novo; zero so onde faz sentido", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			const float Inf = std::numeric_limits<float>::infinity();
			struct FCase { const TCHAR* Name; TFunction<void(FDNTestCarRow&, float)> Set; bool bZeroOk; };
			const FCase Cases[] = {
				{ TEXT("CenterOfMassDropCm"), [](FDNTestCarRow& R, float V) { R.CenterOfMassDropCm = V; }, true },
				{ TEXT("SuspensionDampingRatio"), [](FDNTestCarRow& R, float V) { R.SuspensionDampingRatio = V; }, false },
				{ TEXT("SuspensionTravelCm"), [](FDNTestCarRow& R, float V) { R.SuspensionTravelCm = V; }, false },
				{ TEXT("RollbarScaling"), [](FDNTestCarRow& R, float V) { R.RollbarScaling = V; }, true },
				{ TEXT("RearGripFactor"), [](FDNTestCarRow& R, float V) { R.RearGripFactor = V; }, false },
				{ TEXT("HandbrakeRearGrip"), [](FDNTestCarRow& R, float V) { R.HandbrakeRearGrip = V; }, false },
				{ TEXT("HandbrakeRampInSeconds"), [](FDNTestCarRow& R, float V) { R.HandbrakeRampInSeconds = V; }, false },
				{ TEXT("HandbrakeRampOutSeconds"), [](FDNTestCarRow& R, float V) { R.HandbrakeRampOutSeconds = V; }, false },
				{ TEXT("HandbrakeTorque"), [](FDNTestCarRow& R, float V) { R.HandbrakeTorque = V; }, true },
				{ TEXT("CameraDistanceCm"), [](FDNTestCarRow& R, float V) { R.CameraDistanceCm = V; }, false },
				{ TEXT("CameraHeightCm"), [](FDNTestCarRow& R, float V) { R.CameraHeightCm = V; }, true },
				{ TEXT("CameraFov"), [](FDNTestCarRow& R, float V) { R.CameraFov = V; }, false },
				{ TEXT("NitroAccelMps2"), [](FDNTestCarRow& R, float V) { R.NitroAccelMps2 = V; }, true },
				{ TEXT("NitroDrainPerSecond"), [](FDNTestCarRow& R, float V) { R.NitroDrainPerSecond = V; }, false },
				{ TEXT("NitroRegenPerSecond"), [](FDNTestCarRow& R, float V) { R.NitroRegenPerSecond = V; }, true }
			};
			for (const FCase& C : Cases)
			{
				FString Error;
				FDNTestCarRow R;
				C.Set(R, NaN);  TestFalse(FString::Printf(TEXT("%s NaN"), C.Name), FDNCarTuning::ValidateRow(R, Error));
				TestTrue(FString::Printf(TEXT("%s: o erro cita o campo (%s)"), C.Name, *Error), Error.Contains(C.Name));
				C.Set(R, Inf);  TestFalse(FString::Printf(TEXT("%s infinito"), C.Name), FDNCarTuning::ValidateRow(R, Error));
				C.Set(R, -1.f); TestFalse(FString::Printf(TEXT("%s negativo"), C.Name), FDNCarTuning::ValidateRow(R, Error));
				C.Set(R, 0.f);
				TestEqual(FString::Printf(TEXT("%s zero"), C.Name), FDNCarTuning::ValidateRow(R, Error), C.bZeroOk);
			}
			FString Error;
			TestTrue("a linha padrao e valida", FDNCarTuning::ValidateRow(FDNTestCarRow(), Error));
		});
	});

	Describe("Rampa do freio de mao", [this]()
	{
		It("sobe linearmente ate 1 em InSeconds e desce ate 0 em OutSeconds", [this]()
		{
			float Ramp = 0.f;
			Ramp = FDNHandbrakeRamp::Step(Ramp, true, 0.05f, 0.1f, 0.5f);
			TestEqual("meio caminho", Ramp, 0.5f);
			Ramp = FDNHandbrakeRamp::Step(Ramp, true, 0.05f, 0.1f, 0.5f);
			TestEqual("chegou", Ramp, 1.f);
			Ramp = FDNHandbrakeRamp::Step(Ramp, true, 1.f, 0.1f, 0.5f);
			TestEqual("nao passa de 1", Ramp, 1.f);
			Ramp = FDNHandbrakeRamp::Step(Ramp, false, 0.25f, 0.1f, 0.5f);
			TestEqual("solta: meio caminho", Ramp, 0.5f);
			Ramp = FDNHandbrakeRamp::Step(Ramp, false, 0.25f, 0.1f, 0.5f);
			TestEqual("solta: zero", Ramp, 0.f);
			Ramp = FDNHandbrakeRamp::Step(Ramp, false, 1.f, 0.1f, 0.5f);
			TestEqual("nao passa de 0", Ramp, 0.f);
		});

		It("tempo invalido (zero, negativo, NaN) nao mexe; duracao zero vira salto", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			TestEqual("dt 0", FDNHandbrakeRamp::Step(0.3f, true, 0.f, 0.1f, 0.5f), 0.3f);
			TestEqual("dt negativo", FDNHandbrakeRamp::Step(0.3f, true, -1.f, 0.1f, 0.5f), 0.3f);
			TestEqual("dt NaN", FDNHandbrakeRamp::Step(0.3f, true, NaN, 0.1f, 0.5f), 0.3f);
			TestEqual("atual NaN vira 0", FDNHandbrakeRamp::Step(NaN, false, 0.1f, 0.1f, 0.5f), 0.f);
			TestEqual("sobe instantaneo com duracao 0", FDNHandbrakeRamp::Step(0.f, true, 0.001f, 0.f, 0.5f), 1.f);
			TestEqual("desce instantaneo com duracao 0", FDNHandbrakeRamp::Step(1.f, false, 0.001f, 0.1f, 0.f), 0.f);
		});

		It("multiplicador da aderencia traseira: 1 sem freio de mao, o valor do preset com a rampa cheia, e no meio a media", [this]()
		{
			TestEqual("rampa 0", FDNHandbrakeRamp::RearGripMultiplier(0.35f, 0.f), 1.f);
			TestEqual("rampa 1", FDNHandbrakeRamp::RearGripMultiplier(0.35f, 1.f), 0.35f);
			TestEqual("rampa 0,5", FDNHandbrakeRamp::RearGripMultiplier(0.35f, 0.5f), 0.675f);
			TestEqual("preset 1 nao muda nada", FDNHandbrakeRamp::RearGripMultiplier(1.f, 1.f), 1.f);
			TestEqual("rampa fora de 0..1 e limitada", FDNHandbrakeRamp::RearGripMultiplier(0.35f, 7.f), 0.35f);
		});
	});

	Describe("Nitro de teste", [this]()
	{
		It("segurando com barra: ativo e gasta; solto: recarrega ate o maximo", [this]()
		{
			FDNTestNitro N;
			TestEqual("comeca cheio", N.Bar, FDNTestNitro::MaxBar);
			N.Step(true, 1.f, 25.f, 10.f);
			TestTrue("ativo", N.bActive);
			TestEqual("gastou", N.Bar, 75.f);
			N.Step(false, 1.f, 25.f, 10.f);
			TestFalse("solto: inativo", N.bActive);
			TestEqual("recarregou", N.Bar, 85.f);
			N.Step(false, 100.f, 25.f, 10.f);
			TestEqual("nao passa do maximo", N.Bar, FDNTestNitro::MaxBar);
		});

		It("a barra acaba: desliga sozinho, e segurar vazio nao liga nem recarrega", [this]()
		{
			FDNTestNitro N;
			N.Bar = 10.f;
			N.Step(true, 1.f, 25.f, 10.f);
			TestEqual("zerou", N.Bar, 0.f);
			TestFalse("desligou ao zerar", N.bActive);
			N.Step(true, 1.f, 25.f, 10.f);
			TestFalse("vazio segurando: inativo", N.bActive);
			TestEqual("vazio segurando: nao recarrega", N.Bar, 0.f);
			N.Step(false, 1.f, 25.f, 10.f);
			TestEqual("solto: recarrega", N.Bar, 10.f);
			N.Step(true, 0.1f, 25.f, 10.f);
			TestTrue("liga de novo com barra", N.bActive);
		});

		It("tempo invalido nao mexe; gasto e recarga invalidos ou negativos nao quebram a barra", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			FDNTestNitro N;
			N.Bar = 50.f;
			N.Step(true, NaN, 25.f, 10.f);
			TestEqual("dt NaN", N.Bar, 50.f);
			N.Step(true, -1.f, 25.f, 10.f);
			TestEqual("dt negativo", N.Bar, 50.f);
			N.Step(true, 1.f, -25.f, 10.f);
			TestEqual("gasto negativo nao enche", N.Bar, 50.f);
			N.Step(false, 1.f, 25.f, -10.f);
			TestEqual("recarga negativa nao esvazia", N.Bar, 50.f);
			N.Step(true, 1.f, NaN, 10.f);
			TestTrue("gasto NaN: barra segue finita e dentro de 0..max", FMath::IsFinite(N.Bar) && N.Bar >= 0.f && N.Bar <= FDNTestNitro::MaxBar);
		});
	});

	Describe("Presets de Data/TestCars.csv", [this]()
	{
		auto LoadRows = [this](TMap<FName, FDNTestCarRow>& Out) -> bool
		{
			FString Csv;
			if (!TestTrue("le o CSV", FFileHelper::LoadFileToString(Csv, *FPaths::Combine(FPaths::ProjectDir(), FDNCarTuning::CsvRelativePath)))) { return false; }
			UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
			Table->RowStruct = FDNTestCarRow::StaticStruct();
			if (!TestEqual("problemas de importacao", Table->CreateTableFromCSVString(Csv).Num(), 0)) { return false; }
			for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
			{
				Out.Add(Pair.Key, *reinterpret_cast<const FDNTestCarRow*>(Pair.Value));
			}
			return true;
		};

		It("existem Padrao, arcade-1, arcade-2 e arcade-3, todos validos", [this, LoadRows]()
		{
			TMap<FName, FDNTestCarRow> Rows;
			if (!LoadRows(Rows)) { return; }
			for (const TCHAR* Name : { TEXT("Padrao"), TEXT("arcade-1"), TEXT("arcade-2"), TEXT("arcade-3") })
			{
				const FDNTestCarRow* Row = Rows.Find(FName(Name));
				if (TestNotNull(FString::Printf(TEXT("linha %s"), Name), Row))
				{
					FString Error;
					TestTrue(FString::Printf(TEXT("%s valida: %s"), Name, *Error), FDNCarTuning::ValidateRow(*Row, Error));
				}
			}
		});

		It("a linha Padrao e o carro do template: nada das colunas novas muda o comportamento", [this, LoadRows]()
		{
			TMap<FName, FDNTestCarRow> Rows;
			if (!LoadRows(Rows) || !Rows.Contains(FName("Padrao"))) { return; }
			const FDNTestCarRow& P = Rows[FName("Padrao")];
			const FDNTestCarRow Def;
			TestEqual("massa", P.MassKg, 1500.f);
			TestEqual("COM", P.CenterOfMassDropCm, Def.CenterOfMassDropCm);
			TestEqual("curso", P.SuspensionTravelCm, Def.SuspensionTravelCm);
			TestEqual("barra", P.RollbarScaling, Def.RollbarScaling);
			TestEqual("aderencia traseira", P.RearGripFactor, 1.f);
			TestEqual("freio de mao: aderencia", P.HandbrakeRearGrip, 1.f);
			TestEqual("freio de mao: torque", P.HandbrakeTorque, Def.HandbrakeTorque);
			TestEqual("camera: distancia", P.CameraDistanceCm, Def.CameraDistanceCm);
		});

		It("os tres presets preservam a resposta de direcao (volante e aderencia lateral iguais ao Padrao)", [this, LoadRows]()
		{
			TMap<FName, FDNTestCarRow> Rows;
			if (!LoadRows(Rows) || !Rows.Contains(FName("Padrao"))) { return; }
			const FDNTestCarRow& P = Rows[FName("Padrao")];
			for (const TCHAR* Name : { TEXT("arcade-1"), TEXT("arcade-2"), TEXT("arcade-3") })
			{
				if (!Rows.Contains(FName(Name))) { AddError(FString::Printf(TEXT("falta %s"), Name)); continue; }
				const FDNTestCarRow& R = Rows[FName(Name)];
				TestEqual(FString::Printf(TEXT("%s: ganho do volante"), Name), R.SteeringGain, P.SteeringGain);
				TestEqual(FString::Printf(TEXT("%s: aderencia lateral"), Name), R.LateralGrip, P.LateralGrip);
			}
		});

		It("arcade-1 e mais leve, de centro de massa mais baixo, suspensao mais macia com mais curso e barra mais fraca que o Padrao", [this, LoadRows]()
		{
			TMap<FName, FDNTestCarRow> Rows;
			if (!LoadRows(Rows) || !Rows.Contains(FName("Padrao")) || !Rows.Contains(FName("arcade-1"))) { return; }
			const FDNTestCarRow& P = Rows[FName("Padrao")];
			const FDNTestCarRow& A = Rows[FName("arcade-1")];
			TestTrue("massa menor", A.MassKg < P.MassKg);
			TestTrue("centro de massa mais baixo", A.CenterOfMassDropCm > P.CenterOfMassDropCm);
			TestTrue("mola mais macia", A.SuspensionStiffness < P.SuspensionStiffness);
			TestTrue("mais curso", A.SuspensionTravelCm > P.SuspensionTravelCm);
			TestTrue("barra estabilizadora mais fraca", A.RollbarScaling < P.RollbarScaling);
			TestEqual("a traseira nao e mexida", A.RearGripFactor, 1.f);
		});

		It("arcade-2 e o arcade-1 com traseira que escorrega e freio de mao que solta a traseira (com rampa de volta)", [this, LoadRows]()
		{
			TMap<FName, FDNTestCarRow> Rows;
			if (!LoadRows(Rows) || !Rows.Contains(FName("arcade-1")) || !Rows.Contains(FName("arcade-2"))) { return; }
			const FDNTestCarRow& A1 = Rows[FName("arcade-1")];
			const FDNTestCarRow& A2 = Rows[FName("arcade-2")];
			TestEqual("mesma massa", A2.MassKg, A1.MassKg);
			TestEqual("mesmo centro de massa", A2.CenterOfMassDropCm, A1.CenterOfMassDropCm);
			TestEqual("mesma mola", A2.SuspensionStiffness, A1.SuspensionStiffness);
			TestEqual("mesmo curso", A2.SuspensionTravelCm, A1.SuspensionTravelCm);
			TestEqual("mesma barra", A2.RollbarScaling, A1.RollbarScaling);
			TestTrue("aderencia traseira menor", A2.RearGripFactor < A1.RearGripFactor);
			TestTrue("o freio de mao tira aderencia da traseira", A2.HandbrakeRearGrip < 1.f);
			TestTrue("a rampa de volta e mais lenta que a de ida (recupera suave)", A2.HandbrakeRampOutSeconds > A2.HandbrakeRampInSeconds);
		});

		It("arcade-3 fica entre o arcade-1 e o arcade-2 na aderencia traseira, tem o mesmo freio de mao do arcade-2, e fica entre o Padrao e o arcade-1 na massa", [this, LoadRows]()
		{
			TMap<FName, FDNTestCarRow> Rows;
			if (!LoadRows(Rows) || !Rows.Contains(FName("Padrao")) || !Rows.Contains(FName("arcade-1")) || !Rows.Contains(FName("arcade-2")) || !Rows.Contains(FName("arcade-3"))) { return; }
			const FDNTestCarRow& P = Rows[FName("Padrao")];
			const FDNTestCarRow& A1 = Rows[FName("arcade-1")];
			const FDNTestCarRow& A2 = Rows[FName("arcade-2")];
			const FDNTestCarRow& A3 = Rows[FName("arcade-3")];
			TestTrue("massa entre Padrao e arcade-1", A3.MassKg > A1.MassKg && A3.MassKg < P.MassKg);
			TestTrue("aderencia traseira entre arcade-2 e arcade-1", A3.RearGripFactor > A2.RearGripFactor && A3.RearGripFactor < A1.RearGripFactor);
			TestEqual("freio de mao igual ao do arcade-2 (a diferenca esta na traseira)", A3.HandbrakeRearGrip, A2.HandbrakeRearGrip);
			TestTrue("freio de mao solta a traseira", A3.HandbrakeRearGrip < 1.f);
			TestTrue("mola entre arcade-1 e Padrao", A3.SuspensionStiffness > A1.SuspensionStiffness && A3.SuspensionStiffness < P.SuspensionStiffness);
		});

		It("a camera dos presets e pelo menos 20% mais longe e mais alta que a do Padrao", [this, LoadRows]()
		{
			TMap<FName, FDNTestCarRow> Rows;
			if (!LoadRows(Rows) || !Rows.Contains(FName("Padrao"))) { return; }
			const FDNTestCarRow& P = Rows[FName("Padrao")];
			for (const TCHAR* Name : { TEXT("arcade-1"), TEXT("arcade-2"), TEXT("arcade-3") })
			{
				if (!Rows.Contains(FName(Name))) { AddError(FString::Printf(TEXT("falta %s"), Name)); continue; }
				const FDNTestCarRow& R = Rows[FName(Name)];
				TestTrue(FString::Printf(TEXT("%s: distancia"), Name), R.CameraDistanceCm >= P.CameraDistanceCm * 1.2f);
				TestTrue(FString::Printf(TEXT("%s: altura"), Name), R.CameraHeightCm > P.CameraHeightCm);
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
