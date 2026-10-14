// Testes de Data/Cars.csv (presets do carro): validação por campo, a faixa do "precipício" da aderência traseira e DERIVA entre a
// linha Padrao do CSV e o padrão da struct pura FDNCarConfig.
//
// Faixas (inclusivas) vêm dos limites que o protótipo do lote 8 já usava e foram medidas no Chaos 5.8 (docs/presets-carro.md):
//  - a validação REJEITA fora da faixa (o protótipo limitava em silêncio);
//  - precipício: RearGripFactor x HandbrakeRearGrip >= 0,295. Medido com o freio de mão a 3000: produto 0,32 = 12 graus de
//    deslizamento, 0,30 = 37 a 41 graus, 0,294 = 77 graus, 0,28 = 90 graus (o carro roda). O mínimo 0,295 deixa passar o
//    arcade-2 (0,85 x 0,35 = 0,2975) e recusa o que rodopia. Ajustes de RearGripFactor ou HandbrakeRearGrip: passos de 0,02.
//  - o CSV precisa ter a linha "Padrao" (é o carro do template e o que o jogo usa quando o preset pedido não existe).

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/Data/DNDataValidation.h"
#include "Core/Tests/DNTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDataCarsSpec, "Detonauto.Dados.ConfigCarros",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	const float Inff = std::numeric_limits<float>::infinity();

	static FString JoinErrors(const TArray<FString>& Errors) { return Errors.IsEmpty() ? FString(TEXT("(nenhum erro)")) : FString::Join(Errors, TEXT(" | ")); }

	static bool Cites(const TArray<FString>& Errors, const TCHAR* Needle)
	{
		for (const FString& E : Errors) { if (E.Contains(Needle)) { return true; } }
		return false;
	}

	static TArray<FDNCarConfigEntry> Table(std::initializer_list<FDNCarConfigEntry> Rows) { return TArray<FDNCarConfigEntry>(Rows); }

	/** A tabela com a linha Padrao e uma segunda linha ajustada. */
	TArray<FString> ValidateWith(TFunction<void(FDNCarConfigRow&)> Tweak)
	{
		FDNCarConfigRow R = DNTest::CarRow();
		Tweak(R);
		return FDNDataValidation::ValidateCars({ DNTest::Entry<FDNCarConfigEntry>(R, TEXT("Padrao")) });
	}

	void Bad(const TCHAR* What, const TCHAR* Field, TFunction<void(FDNCarConfigRow&)> Tweak)
	{
		const TArray<FString> Errors = ValidateWith(Tweak);
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Field, *JoinErrors(Errors)), Cites(Errors, Field));
	}

	void Good(const TCHAR* What, TFunction<void(FDNCarConfigRow&)> Tweak)
	{
		const TArray<FString> Errors = ValidateWith(Tweak);
		TestTrue(FString::Printf(TEXT("%s: esperava zero erros; veio: %s"), What, *JoinErrors(Errors)), Errors.IsEmpty());
	}

	/** Os dois extremos da faixa valem; um pouco fora, de cada lado, e NaN, são recusados. */
	void Range(const TCHAR* Field, float Min, float Max, TFunction<void(FDNCarConfigRow&, float)> Set)
	{
		const float Margin = FMath::Max(1.0e-3f, (Max - Min) * 1.0e-3f);
		Good(*FString::Printf(TEXT("%s no minimo"), Field), [&](FDNCarConfigRow& R) { Set(R, Min); });
		Good(*FString::Printf(TEXT("%s no maximo"), Field), [&](FDNCarConfigRow& R) { Set(R, Max); });
		Bad(*FString::Printf(TEXT("%s abaixo do minimo"), Field), Field, [&](FDNCarConfigRow& R) { Set(R, Min - Margin); });
		Bad(*FString::Printf(TEXT("%s acima do maximo"), Field), Field, [&](FDNCarConfigRow& R) { Set(R, Max + Margin); });
		Bad(*FString::Printf(TEXT("%s NaN"), Field), Field, [&](FDNCarConfigRow& R) { Set(R, NaNf); });
		Bad(*FString::Printf(TEXT("%s infinito"), Field), Field, [&](FDNCarConfigRow& R) { Set(R, Inff); });
	}
END_DEFINE_SPEC(FDNDataCarsSpec)

void FDNDataCarsSpec::Define()
{
	Describe("Cars.csv", [this]()
	{
		It("a configuracao inicial (Padrao) e valida", [this]()
		{
			TestEqual("erros", FDNDataValidation::ValidateCars({ DNTest::CarEntry() }).Num(), 0);
		});

		It("exige a linha Padrao, mesmo escrita com outra caixa", [this]()
		{
			TestTrue("tabela vazia", Cites(FDNDataValidation::ValidateCars({}), TEXT("Padrao")));
			TestTrue("sem Padrao", Cites(FDNDataValidation::ValidateCars({ DNTest::CarEntry(TEXT("arcade-1")) }), TEXT("Padrao")));
			TestEqual("padrao minusculo vale", FDNDataValidation::ValidateCars({ DNTest::CarEntry(TEXT("padrao")) }).Num(), 0);
		});

		It("varias linhas: cada uma e validada e o erro cita a linha", [this]()
		{
			FDNCarConfigRow Ruim = DNTest::CarRow();
			Ruim.MassKg = 5.f;
			const TArray<FString> Errors = FDNDataValidation::ValidateCars({ DNTest::CarEntry(TEXT("Padrao")), DNTest::Entry<FDNCarConfigEntry>(Ruim, TEXT("arcade-9")) });
			TestTrue("cita a linha", Cites(Errors, TEXT("arcade-9")));
			TestTrue("cita o campo", Cites(Errors, TEXT("MassKg")));
			TestFalse("a linha boa nao e acusada", Cites(Errors, TEXT("'Padrao'")));
		});

		It("id vazio, repetido (so na caixa) ou com caractere invalido e recusado", [this]()
		{
			TestTrue("vazio", Cites(FDNDataValidation::ValidateCars({ DNTest::CarEntry(TEXT("Padrao")), DNTest::CarEntry(TEXT("")) }), TEXT("id vazio")));
			TestTrue("repetido na caixa", Cites(FDNDataValidation::ValidateCars({ DNTest::CarEntry(TEXT("Padrao")), DNTest::CarEntry(TEXT("PADRAO")) }), TEXT("mesmo id")));
			TestTrue("espaco", Cites(FDNDataValidation::ValidateCars({ DNTest::CarEntry(TEXT("Padrao")), DNTest::CarEntry(TEXT("arcade 1")) }), TEXT("caractere")));
		});

		It("MassKg, MaxTorque, LateralGrip, SuspensionStiffness, SteeringGain", [this]()
		{
			Range(TEXT("MassKg"), 100.f, 10000.f, [](FDNCarConfigRow& R, float V) { R.MassKg = V; });
			Range(TEXT("MaxTorque"), 50.f, 5000.f, [](FDNCarConfigRow& R, float V) { R.MaxTorque = V; });
			Range(TEXT("LateralGrip"), 0.1f, 5.f, [](FDNCarConfigRow& R, float V) { R.LateralGrip = V; });
			Range(TEXT("SuspensionStiffness"), 10.f, 2000.f, [](FDNCarConfigRow& R, float V) { R.SuspensionStiffness = V; });
			Range(TEXT("SteeringGain"), 0.1f, 3.f, [](FDNCarConfigRow& R, float V) { R.SteeringGain = V; });
		});

		It("CenterOfMassDropCm, SuspensionDampingRatio, SuspensionTravelCm, RollbarScaling", [this]()
		{
			Range(TEXT("CenterOfMassDropCm"), 0.f, 60.f, [](FDNCarConfigRow& R, float V) { R.CenterOfMassDropCm = V; });
			Range(TEXT("SuspensionDampingRatio"), 0.05f, 1.f, [](FDNCarConfigRow& R, float V) { R.SuspensionDampingRatio = V; });
			Range(TEXT("SuspensionTravelCm"), 2.f, 40.f, [](FDNCarConfigRow& R, float V) { R.SuspensionTravelCm = V; });
			Range(TEXT("RollbarScaling"), 0.f, 1.f, [](FDNCarConfigRow& R, float V) { R.RollbarScaling = V; });
		});

		It("HandbrakeRampInSeconds, HandbrakeRampOutSeconds, HandbrakeTorque", [this]()
		{
			Range(TEXT("HandbrakeRampInSeconds"), 0.01f, 3.f, [](FDNCarConfigRow& R, float V) { R.HandbrakeRampInSeconds = V; });
			Range(TEXT("HandbrakeRampOutSeconds"), 0.01f, 3.f, [](FDNCarConfigRow& R, float V) { R.HandbrakeRampOutSeconds = V; });
			Range(TEXT("HandbrakeTorque"), 0.f, 20000.f, [](FDNCarConfigRow& R, float V) { R.HandbrakeTorque = V; });
		});

		It("NitroAccelMps2 e a assistencia de derrapagem (inicio, ganho, teto)", [this]()
		{
			Range(TEXT("DriftAssistStartDeg"), 0.f, 80.f, [](FDNCarConfigRow& R, float V) { R.DriftAssistStartDeg = V; });
			// ganho e teto sao um par: os dois zero (desligada) ou os dois maiores que zero; por isso o extremo de baixo vale no par
			Range(TEXT("DriftAssistGain"), 0.f, 500.f, [](FDNCarConfigRow& R, float V) { R.DriftAssistGain = V; R.DriftAssistMax = V > 0.f ? 100.f : 0.f; });
			Range(TEXT("DriftAssistMax"), 0.f, 2000.f, [](FDNCarConfigRow& R, float V) { R.DriftAssistMax = V; R.DriftAssistGain = V > 0.f ? 10.f : 0.f; });
			Range(TEXT("NitroAccelMps2"), 0.f, 40.f, [](FDNCarConfigRow& R, float V) { R.NitroAccelMps2 = V; });
		});

		It("RearGripFactor e HandbrakeRearGrip: cada um na sua faixa (com o freio de mao sem efeito)", [this]()
		{
			// com o outro em 1,0 o produto fica igual ao campo: os limites 0,295 do produto e 0,2/0,05 de cada campo se sobrepoem;
			// por isso aqui a faixa de cada campo e testada pelo teto e por NaN, e o minimo no teste do precipicio abaixo.
			Good(TEXT("RearGripFactor no maximo"), [](FDNCarConfigRow& R) { R.RearGripFactor = 2.f; });
			Bad(TEXT("RearGripFactor acima do maximo"), TEXT("RearGripFactor"), [](FDNCarConfigRow& R) { R.RearGripFactor = 2.001f; });
			Bad(TEXT("RearGripFactor NaN"), TEXT("RearGripFactor"), [this](FDNCarConfigRow& R) { R.RearGripFactor = NaNf; });
			Bad(TEXT("RearGripFactor abaixo do minimo"), TEXT("RearGripFactor"), [](FDNCarConfigRow& R) { R.RearGripFactor = 0.199f; });
			Good(TEXT("HandbrakeRearGrip no maximo"), [](FDNCarConfigRow& R) { R.HandbrakeRearGrip = 1.5f; });
			Bad(TEXT("HandbrakeRearGrip acima do maximo"), TEXT("HandbrakeRearGrip"), [](FDNCarConfigRow& R) { R.HandbrakeRearGrip = 1.501f; });
			Bad(TEXT("HandbrakeRearGrip NaN"), TEXT("HandbrakeRearGrip"), [this](FDNCarConfigRow& R) { R.HandbrakeRearGrip = NaNf; });
			Bad(TEXT("HandbrakeRearGrip abaixo do minimo"), TEXT("HandbrakeRearGrip"), [](FDNCarConfigRow& R) { R.HandbrakeRearGrip = 0.049f; });
		});

		It("assistencia de derrapagem: ganho e teto sao um par (os dois zero ou os dois maiores que zero)", [this]()
		{
			Good(TEXT("os dois zero (desligada)"), [](FDNCarConfigRow& R) { R.DriftAssistGain = 0.f; R.DriftAssistMax = 0.f; });
			Good(TEXT("os dois ligados"), [](FDNCarConfigRow& R) { R.DriftAssistGain = 10.f; R.DriftAssistMax = 300.f; });
			Bad(TEXT("ganho sem teto"), TEXT("DriftAssist"), [](FDNCarConfigRow& R) { R.DriftAssistGain = 10.f; R.DriftAssistMax = 0.f; });
			Bad(TEXT("teto sem ganho"), TEXT("DriftAssist"), [](FDNCarConfigRow& R) { R.DriftAssistGain = 0.f; R.DriftAssistMax = 300.f; });
		});

		It("campo fora da propria faixa da exatamente um erro, o da faixa (o produto nao repete o aviso)", [this]()
		{
			for (const float V : { 0.199f, 2.001f })
			{
				const TArray<FString> E = ValidateWith([V](FDNCarConfigRow& R) { R.RearGripFactor = V; });
				TestEqual(FString::Printf(TEXT("RearGripFactor %f: um erro"), V), E.Num(), 1);
				TestTrue(FString::Printf(TEXT("RearGripFactor %f: e o da faixa; veio: %s"), V, *JoinErrors(E)), Cites(E, TEXT("número finito em")));
			}
			for (const float V : { 0.049f, 1.501f })
			{
				const TArray<FString> E = ValidateWith([V](FDNCarConfigRow& R) { R.HandbrakeRearGrip = V; });
				TestEqual(FString::Printf(TEXT("HandbrakeRearGrip %f: um erro"), V), E.Num(), 1);
				TestTrue(FString::Printf(TEXT("HandbrakeRearGrip %f: e o da faixa; veio: %s"), V, *JoinErrors(E)), Cites(E, TEXT("número finito em")));
			}
		});

		It("PRECIPICIO: RearGripFactor x HandbrakeRearGrip abaixo de 0,295 faz o carro rodar e e recusado", [this]()
		{
			// medidos (docs/presets-carro.md): 0,28 = 90 graus, 0,294 = 77 graus (rodopio); 0,2975 = 37 a 41 graus (derrapa e volta)
			Bad(TEXT("0,28 (rodopio, 90 graus)"), TEXT("HandbrakeRearGrip"), [](FDNCarConfigRow& R) { R.RearGripFactor = 0.7f; R.HandbrakeRearGrip = 0.4f; });
			Bad(TEXT("0,294 (rodopio, 77 graus)"), TEXT("RearGripFactor"), [](FDNCarConfigRow& R) { R.RearGripFactor = 0.7f; R.HandbrakeRearGrip = 0.42f; });
			Good(TEXT("0,2975 (o arcade-2: derrapa 37 a 41 graus)"), [](FDNCarConfigRow& R) { R.RearGripFactor = 0.85f; R.HandbrakeRearGrip = 0.35f; });
			Good(TEXT("0,322 (o arcade-3: derrapada leve)"), [](FDNCarConfigRow& R) { R.RearGripFactor = 0.92f; R.HandbrakeRearGrip = 0.35f; });
			Good(TEXT("0,3 exato"), [](FDNCarConfigRow& R) { R.RearGripFactor = 0.5f; R.HandbrakeRearGrip = 0.6f; });
			Bad(TEXT("0,25"), TEXT("0,295"), [](FDNCarConfigRow& R) { R.RearGripFactor = 0.5f; R.HandbrakeRearGrip = 0.5f; });
		});

		It("a mensagem do precipicio cita os dois campos e a linha", [this]()
		{
			FDNCarConfigRow R = DNTest::CarRow();
			R.RearGripFactor = 0.5f;
			R.HandbrakeRearGrip = 0.5f;
			const TArray<FString> Errors = FDNDataValidation::ValidateCars({ DNTest::Entry<FDNCarConfigEntry>(R, TEXT("Padrao")) });
			if (!TestEqual("um erro so", Errors.Num(), 1)) { return; }
			TestTrue("RearGripFactor", Errors[0].Contains(TEXT("RearGripFactor")));
			TestTrue("HandbrakeRearGrip", Errors[0].Contains(TEXT("HandbrakeRearGrip")));
			TestTrue("a linha", Errors[0].Contains(TEXT("'Padrao'")));
		});

		It("um erro nao esconde o outro: acumula todos", [this]()
		{
			FDNCarConfigRow R = DNTest::CarRow();
			R.MassKg = 1.f;
			R.NitroAccelMps2 = 999.f;
			R.NitroAccelMps2 = -3.f;
			const TArray<FString> Errors = FDNDataValidation::ValidateCars({ DNTest::Entry<FDNCarConfigEntry>(R, TEXT("Padrao")) });
			TestTrue("MassKg", Cites(Errors, TEXT("MassKg")));
			TestTrue("NitroAccelMps2", Cites(Errors, TEXT("NitroAccelMps2")));
		});
	});
}

// DERIVA: a linha Padrao do CSV real, convertida por ToConfig(), precisa ser IGUAL, campo a campo, ao padrão da struct pura.
BEGIN_DEFINE_SPEC(FDNDataCarsDriftSpec, "Detonauto.Dados.DerivaCarros",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNDataCarsDriftSpec)

void FDNDataCarsDriftSpec::Define()
{
	Describe("Cars.csv real x padrao da struct", [this]()
	{
		It("a linha Padrao de Cars.csv = FDNCarConfig()", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			const FDNCarConfigEntry* Padrao = Data.CarConfigs.FindByPredicate([](const FDNCarConfigEntry& E) { return E.Id.Equals(TEXT("Padrao"), ESearchCase::IgnoreCase); });
			if (!TestNotNull("linha Padrao em Cars.csv", Padrao)) { return; }
			const FDNCarConfig Csv = Padrao->Row.ToConfig();
			const FDNCarConfig Def;
			auto Same = [this](const TCHAR* Field, double FromCsv, double FromStruct)
			{
				TestTrue(FString::Printf(TEXT("%s: o CSV tem %.9g e o padrao da struct tem %.9g (mudou so num dos dois lugares?)"), Field, FromCsv, FromStruct), FromCsv == FromStruct);
			};
			Same(TEXT("MassKg"), Csv.MassKg, Def.MassKg);
			Same(TEXT("MaxTorque"), Csv.MaxTorque, Def.MaxTorque);
			Same(TEXT("LateralGrip"), Csv.LateralGrip, Def.LateralGrip);
			Same(TEXT("SuspensionStiffness"), Csv.SuspensionStiffness, Def.SuspensionStiffness);
			Same(TEXT("SteeringGain"), Csv.SteeringGain, Def.SteeringGain);
			Same(TEXT("CenterOfMassDropCm"), Csv.CenterOfMassDropCm, Def.CenterOfMassDropCm);
			Same(TEXT("SuspensionDampingRatio"), Csv.SuspensionDampingRatio, Def.SuspensionDampingRatio);
			Same(TEXT("SuspensionTravelCm"), Csv.SuspensionTravelCm, Def.SuspensionTravelCm);
			Same(TEXT("RollbarScaling"), Csv.RollbarScaling, Def.RollbarScaling);
			Same(TEXT("RearGripFactor"), Csv.RearGripFactor, Def.RearGripFactor);
			Same(TEXT("HandbrakeRearGrip"), Csv.HandbrakeRearGrip, Def.HandbrakeRearGrip);
			Same(TEXT("HandbrakeRampInSeconds"), Csv.HandbrakeRampInSeconds, Def.HandbrakeRampInSeconds);
			Same(TEXT("HandbrakeRampOutSeconds"), Csv.HandbrakeRampOutSeconds, Def.HandbrakeRampOutSeconds);
			Same(TEXT("HandbrakeTorque"), Csv.HandbrakeTorque, Def.HandbrakeTorque);
			Same(TEXT("DriftAssistStartDeg"), Csv.DriftAssistStartDeg, Def.DriftAssistStartDeg);
			Same(TEXT("DriftAssistGain"), Csv.DriftAssistGain, Def.DriftAssistGain);
			Same(TEXT("DriftAssistMax"), Csv.DriftAssistMax, Def.DriftAssistMax);
			Same(TEXT("NitroAccelMps2"), Csv.NitroAccelMps2, Def.NitroAccelMps2);
		});

		It("o padrao da struct e o carro do template (valores do Chaos do template Vehicle)", [this]()
		{
			const FDNCarConfig D;
			TestEqual("massa", D.MassKg, 1500.f);
			TestEqual("torque", D.MaxTorque, 750.f);
			TestEqual("mola", D.SuspensionStiffness, 250.f);
			TestEqual("amortecimento", D.SuspensionDampingRatio, 0.5f);
			TestEqual("curso", D.SuspensionTravelCm, 10.f);
			TestEqual("barra", D.RollbarScaling, 0.15f);
			TestEqual("freio de mao: torque", D.HandbrakeTorque, 6000.f);
			TestEqual("freio de mao nao solta a traseira", D.HandbrakeRearGrip, 1.f);
			TestEqual("assistencia de derrapagem desligada no template", D.DriftAssistGain, 0.f);
			TestEqual("assistencia: teto zero", D.DriftAssistMax, 0.f);
		});

		It("os presets das teclas existem no CSV: Padrao, arcade-1, arcade-2, arcade-3", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			for (const TCHAR* Name : { TEXT("Padrao"), TEXT("arcade-1"), TEXT("arcade-2"), TEXT("arcade-3") })
			{
				TestTrue(FString::Printf(TEXT("linha %s"), Name), Data.CarConfigs.ContainsByPredicate([Name](const FDNCarConfigEntry& E) { return E.Id.Equals(Name, ESearchCase::IgnoreCase); }));
			}
		});

		It("os tres presets arcade preservam a direcao do Padrao (volante e aderencia lateral)", [this]()
		{
			FDNData Data;
			FDNDataLoader::LoadAll(Data);
			const FDNCarConfigEntry* P = Data.CarConfigs.FindByPredicate([](const FDNCarConfigEntry& E) { return E.Id == TEXT("Padrao"); });
			if (!TestNotNull("Padrao", P)) { return; }
			for (const TCHAR* Name : { TEXT("arcade-1"), TEXT("arcade-2"), TEXT("arcade-3") })
			{
				const FDNCarConfigEntry* A = Data.CarConfigs.FindByPredicate([Name](const FDNCarConfigEntry& E) { return E.Id.Equals(Name, ESearchCase::IgnoreCase); });
				if (!TestNotNull(Name, A)) { continue; }
				TestEqual(FString::Printf(TEXT("%s: SteeringGain"), Name), A->Row.SteeringGain, P->Row.SteeringGain);
				TestEqual(FString::Printf(TEXT("%s: LateralGrip"), Name), A->Row.LateralGrip, P->Row.LateralGrip);
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
