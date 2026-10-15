// Testes de Data/CarParts.csv e Data/CarGlass.csv (lote 18b, docs/regras-pecas.md): nomes em texto dos enums, cópia da linha para a
// regra, validação por campo, o CSV real igual à tabela do contrato e o subsistema. Faixas: Core/Data/DNDataValidation.h.

#include "Misc/AutomationTest.h"
#include "Core/Data/DNDataLoader.h"
#include "Core/Data/DNDataValidation.h"
#include "Core/DNGameDataSubsystem.h"
#include "Core/Tests/DNTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNDataCarPartsSpec, "Detonauto.Dados.PecasDoCarro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	static FString JoinErrors(const TArray<FString>& Errors) { return Errors.IsEmpty() ? FString(TEXT("(nenhum erro)")) : FString::Join(Errors, TEXT(" | ")); }

	static bool Cites(const TArray<FString>& Errors, const TCHAR* Needle)
	{
		for (const FString& E : Errors) { if (E.Contains(Needle)) { return true; } }
		return false;
	}

	void BadPart(const TCHAR* What, const TCHAR* Needle, TFunction<void(FDNCarPartRow&)> Tweak)
	{
		FDNCarPartRow R = DNTest::CarPartRow();
		Tweak(R);
		const TArray<FString> Errors = FDNDataValidation::ValidateCarParts({ DNTest::Entry<FDNCarPartEntry>(R, TEXT("peca")) });
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Needle, *JoinErrors(Errors)), Cites(Errors, Needle));
	}

	void BadGlass(const TCHAR* What, const TCHAR* Needle, TFunction<void(FDNCarGlassRow&)> Tweak)
	{
		FDNCarGlassRow R = DNTest::CarGlassRow();
		Tweak(R);
		const TArray<FString> Errors = FDNDataValidation::ValidateCarGlass({ DNTest::Entry<FDNCarGlassEntry>(R, TEXT("vidro")) });
		TestTrue(FString::Printf(TEXT("%s: esperava erro citando '%s'; veio: %s"), What, Needle, *JoinErrors(Errors)), Cites(Errors, Needle));
	}
END_DEFINE_SPEC(FDNDataCarPartsSpec)

void FDNDataCarPartsSpec::Define()
{
	Describe("Nomes em texto", [this]()
	{
		It("cada peca, zona, borda de dobradica e vidro volta ao enum pelo proprio nome, sem diferenciar a caixa", [this]()
		{
			for (int32 i = 0; i < FDNDamageState::PartCount; ++i)
			{
				const EDNDamagePart P = static_cast<EDNDamagePart>(i);
				EDNDamagePart Back = P == EDNDamagePart::Hood ? EDNDamagePart::Trunk : EDNDamagePart::Hood;
				TestTrue(FString::Printf(TEXT("peca %d (%s)"), i, FDNCarParts::PartName(P)), FDNCarParts::ParsePart(FString(FDNCarParts::PartName(P)).ToLower(), Back) && Back == P);
			}
			for (int32 i = 0; i < FDNDamageState::ZoneCount; ++i)
			{
				const EDNDamageZone Z = static_cast<EDNDamageZone>(i);
				EDNDamageZone Back = Z == EDNDamageZone::Front ? EDNDamageZone::Rear : EDNDamageZone::Front;
				TestTrue(FString::Printf(TEXT("zona %d (%s)"), i, FDNCarParts::ZoneName(Z)), FDNCarParts::ParseZone(FDNCarParts::ZoneName(Z), Back) && Back == Z);
			}
			for (const EDNHingeEdge E : { EDNHingeEdge::None, EDNHingeEdge::TopRear, EDNHingeEdge::TopFront, EDNHingeEdge::SideFront, EDNHingeEdge::Lateral })
			{
				EDNHingeEdge Back = E == EDNHingeEdge::None ? EDNHingeEdge::Lateral : EDNHingeEdge::None;
				TestTrue(FString::Printf(TEXT("borda %s"), FDNCarParts::HingeEdgeName(E)), FDNCarParts::ParseHingeEdge(FDNCarParts::HingeEdgeName(E), Back) && Back == E);
			}
			for (int32 i = 0; i <= static_cast<int32>(EDNGlass::TaillightRight); ++i)
			{
				const EDNGlass G = static_cast<EDNGlass>(i);
				EDNGlass Back = G == EDNGlass::Windshield ? EDNGlass::Rear : EDNGlass::Windshield;
				TestTrue(FString::Printf(TEXT("vidro %d (%s)"), i, FDNGlass::GlassName(G)), FDNGlass::ParseGlass(FString(FDNGlass::GlassName(G)).ToUpper(), Back) && Back == G);
			}
			EDNDamagePart Untouched = EDNDamagePart::Spoiler;
			TestFalse("texto desconhecido", FDNCarParts::ParsePart(TEXT("Retrovisor"), Untouched));
			TestTrue("nao mexe em Out", Untouched == EDNDamagePart::Spoiler);
			TestEqual("nome do para-lama", FString(FDNCarParts::PartName(EDNDamagePart::FenderFrontLeft)), FString(TEXT("FenderFrontLeft")));
			TestEqual("nome do vidro", FString(FDNGlass::GlassName(EDNGlass::SideLeft)), FString(TEXT("SideLeft")));
		});
	});

	Describe("CarParts.csv", [this]()
	{
		It("ToSpec copia todos os campos e le os textos", [this]()
		{
			FDNCarPartRow R = DNTest::CarPartRow(TEXT("hood"), TEXT("Front"), TEXT("roof"), TEXT("TopRear"));
			R.MassKg = 12.f;
			R.HangDamage = 0.35f;
			R.HangDeltaVMs = 9.f;
			R.DetachDamage = 0.75f;
			R.DetachDeltaVMs = 16.f;
			R.HingeMaxDeg = 55.f;
			R.HingeDamping = 3.f;
			R.LaunchMs = 5.f;
			FDNCarPartSpec S;
			if (!TestTrue("le", R.ToSpec(S)))
			{
				return;
			}
			TestTrue("Part", S.Part == EDNDamagePart::Hood);
			TestTrue("Zone", S.Zone == EDNDamageZone::Front);
			TestTrue("secundaria", S.bHasSecondaryZone && S.SecondaryZone == EDNDamageZone::Roof);
			TestTrue("HingeEdge", S.HingeEdge == EDNHingeEdge::TopRear);
			TestEqual("MassKg", S.MassKg, 12.f);
			TestEqual("HangDamage", S.HangDamage, 0.35f);
			TestEqual("HangDeltaVMs", S.HangDeltaVMs, 9.f);
			TestEqual("DetachDamage", S.DetachDamage, 0.75f);
			TestEqual("DetachDeltaVMs", S.DetachDeltaVMs, 16.f);
			TestEqual("HingeMaxDeg", S.HingeMaxDeg, 55.f);
			TestEqual("HingeDamping", S.HingeDamping, 3.f);
			TestEqual("LaunchMs", S.LaunchMs, 5.f);
			FDNCarPartRow NoSecondary = DNTest::CarPartRow();
			FDNCarPartSpec S2;
			TestTrue("sem secundaria", NoSecondary.ToSpec(S2) && !S2.bHasSecondaryZone);
			FDNCarPartRow Bad = DNTest::CarPartRow(TEXT("Retrovisor"));
			FDNCarPartSpec S3;
			TestFalse("texto desconhecido nao le", Bad.ToSpec(S3));
		});

		It("valida cada campo", [this]()
		{
			TestTrue(FString::Printf(TEXT("linha padrao valida: %s"), *JoinErrors(FDNDataValidation::ValidateCarParts({ DNTest::Entry<FDNCarPartEntry>(DNTest::CarPartRow(), TEXT("peca")) }))),
				FDNDataValidation::ValidateCarParts({ DNTest::Entry<FDNCarPartEntry>(DNTest::CarPartRow(), TEXT("peca")) }).IsEmpty());
			TestTrue("sem dobradica valida", FDNDataValidation::ValidateCarParts({ DNTest::Entry<FDNCarPartEntry>([]() {
				FDNCarPartRow R = DNTest::CarPartRow(TEXT("Spoiler"), TEXT("Rear"), TEXT("Roof"), TEXT("None"));
				R.HangDamage = 0.f; R.HangDeltaVMs = 0.f; R.HingeMaxDeg = 0.f; return R; }(), TEXT("aerofolio")) }).IsEmpty());
			TestFalse("nenhuma linha", FDNDataValidation::ValidateCarParts({}).IsEmpty());
			BadPart(TEXT("carro vazio"), TEXT("CarId"), [](FDNCarPartRow& R) { R.CarId = TEXT(""); });
			BadPart(TEXT("peca desconhecida"), TEXT("Part"), [](FDNCarPartRow& R) { R.Part = TEXT("Retrovisor"); });
			BadPart(TEXT("zona desconhecida"), TEXT("Zone"), [](FDNCarPartRow& R) { R.Zone = TEXT("Meio"); });
			BadPart(TEXT("secundaria desconhecida"), TEXT("SecondaryZone"), [](FDNCarPartRow& R) { R.SecondaryZone = TEXT("Meio"); });
			BadPart(TEXT("secundaria igual a zona"), TEXT("SecondaryZone"), [](FDNCarPartRow& R) { R.SecondaryZone = TEXT("Front"); });
			BadPart(TEXT("borda desconhecida"), TEXT("HingeEdge"), [](FDNCarPartRow& R) { R.HingeEdge = TEXT("Meio"); });
			BadPart(TEXT("massa 0"), TEXT("MassKg"), [](FDNCarPartRow& R) { R.MassKg = 0.f; });
			BadPart(TEXT("massa 501"), TEXT("MassKg"), [](FDNCarPartRow& R) { R.MassKg = 501.f; });
			BadPart(TEXT("solta com dano 0"), TEXT("DetachDamage"), [](FDNCarPartRow& R) { R.DetachDamage = 0.f; });
			BadPart(TEXT("solta com dano 1,1"), TEXT("DetachDamage"), [](FDNCarPartRow& R) { R.DetachDamage = 1.1f; });
			BadPart(TEXT("solta com Dv 0"), TEXT("DetachDeltaVMs"), [](FDNCarPartRow& R) { R.DetachDeltaVMs = 0.f; });
			BadPart(TEXT("pendura depois de soltar (dano)"), TEXT("HangDamage"), [](FDNCarPartRow& R) { R.HangDamage = R.DetachDamage; });
			BadPart(TEXT("pendura depois de soltar (Dv)"), TEXT("HangDeltaVMs"), [](FDNCarPartRow& R) { R.HangDeltaVMs = R.DetachDeltaVMs; });
			BadPart(TEXT("dobradica sem limiar de pendurar"), TEXT("HangDamage"), [](FDNCarPartRow& R) { R.HangDamage = 0.f; });
			BadPart(TEXT("dobradica de 0 graus"), TEXT("HingeMaxDeg"), [](FDNCarPartRow& R) { R.HingeMaxDeg = 0.f; });
			BadPart(TEXT("dobradica de 181 graus"), TEXT("HingeMaxDeg"), [](FDNCarPartRow& R) { R.HingeMaxDeg = 181.f; });
			BadPart(TEXT("sem dobradica com angulo"), TEXT("HingeMaxDeg"), [](FDNCarPartRow& R) { R.HingeEdge = TEXT("None"); R.HangDamage = 0.f; R.HangDeltaVMs = 0.f; });
			BadPart(TEXT("sem dobradica com limiar de pendurar"), TEXT("HangDamage"), [](FDNCarPartRow& R) { R.HingeEdge = TEXT("None"); R.HingeMaxDeg = 0.f; R.HangDeltaVMs = 0.f; });
			BadPart(TEXT("amortecimento negativo"), TEXT("HingeDamping"), [](FDNCarPartRow& R) { R.HingeDamping = -1.f; });
			BadPart(TEXT("empurrao 31"), TEXT("LaunchMs"), [](FDNCarPartRow& R) { R.LaunchMs = 31.f; });
			const TArray<FString> Twice = FDNDataValidation::ValidateCarParts({ DNTest::Entry<FDNCarPartEntry>(DNTest::CarPartRow(), TEXT("a")),
				DNTest::Entry<FDNCarPartEntry>(DNTest::CarPartRow(TEXT("bumperfront")), TEXT("b")) });
			TestTrue(FString::Printf(TEXT("a mesma peca duas vezes no mesmo carro: %s"), *JoinErrors(Twice)), Cites(Twice, TEXT("BumperFront")) || Cites(Twice, TEXT("bumperfront")));
			TestTrue("a mesma peca em outro carro pode", FDNDataValidation::ValidateCarParts({ DNTest::Entry<FDNCarPartEntry>(DNTest::CarPartRow(), TEXT("a")),
				DNTest::Entry<FDNCarPartEntry>(DNTest::CarPartRow(TEXT("BumperFront"), TEXT("Front"), TEXT(""), TEXT("Lateral"), TEXT("OutroCarro")), TEXT("b")) }).IsEmpty());
		});

		It("o CSV real carrega, e valido e tem a tabela do contrato, na ordem", [this]()
		{
			TArray<FDNCarPartEntry> Rows;
			const TArray<FString> LoadErrors = FDNDataLoader::LoadEntries<FDNCarPartRow>(TEXT("Data/CarParts.csv"), Rows);
			TestTrue(FString::Printf(TEXT("carga: %s"), *JoinErrors(LoadErrors)), LoadErrors.IsEmpty());
			const TArray<FString> Errors = FDNDataValidation::ValidateCarParts(Rows);
			TestTrue(FString::Printf(TEXT("validacao: %s"), *JoinErrors(Errors)), Errors.IsEmpty());
			// a tabela de docs/regras-pecas.md: peça, zona, secundária, massa, pendura (dano, Δv), solta (dano, Δv), borda, ângulo
			struct FRowSpec { EDNDamagePart Part; EDNDamageZone Zone; bool bSec; EDNDamageZone Sec; float Mass; float Hd; float Hv; float Dd; float Dv; EDNHingeEdge Edge; float Deg; };
			const FRowSpec Contract[] = {
				{ EDNDamagePart::BumperFront, EDNDamageZone::Front, false, EDNDamageZone::Front, 8.f, 0.30f, 8.f, 0.60f, 12.f, EDNHingeEdge::Lateral, 60.f },
				{ EDNDamagePart::BumperRear, EDNDamageZone::Rear, false, EDNDamageZone::Front, 8.f, 0.30f, 8.f, 0.60f, 12.f, EDNHingeEdge::Lateral, 60.f },
				{ EDNDamagePart::Hood, EDNDamageZone::Front, true, EDNDamageZone::Roof, 12.f, 0.35f, 9.f, 0.75f, 16.f, EDNHingeEdge::TopRear, 55.f },
				{ EDNDamagePart::Trunk, EDNDamageZone::Rear, false, EDNDamageZone::Front, 10.f, 0.35f, 9.f, 0.75f, 16.f, EDNHingeEdge::TopFront, 55.f },
				{ EDNDamagePart::DoorLeft, EDNDamageZone::Left, false, EDNDamageZone::Front, 20.f, 0.35f, 9.f, 0.80f, 16.f, EDNHingeEdge::SideFront, 70.f },
				{ EDNDamagePart::DoorRight, EDNDamageZone::Right, false, EDNDamageZone::Front, 20.f, 0.35f, 9.f, 0.80f, 16.f, EDNHingeEdge::SideFront, 70.f },
				{ EDNDamagePart::FenderFrontLeft, EDNDamageZone::Front, true, EDNDamageZone::Left, 5.f, 0.f, 0.f, 0.70f, 14.f, EDNHingeEdge::None, 0.f },
				{ EDNDamagePart::FenderFrontRight, EDNDamageZone::Front, true, EDNDamageZone::Right, 5.f, 0.f, 0.f, 0.70f, 14.f, EDNHingeEdge::None, 0.f },
				{ EDNDamagePart::FenderRearLeft, EDNDamageZone::Rear, true, EDNDamageZone::Left, 5.f, 0.f, 0.f, 0.70f, 14.f, EDNHingeEdge::None, 0.f },
				{ EDNDamagePart::FenderRearRight, EDNDamageZone::Rear, true, EDNDamageZone::Right, 5.f, 0.f, 0.f, 0.70f, 14.f, EDNHingeEdge::None, 0.f },
				{ EDNDamagePart::Spoiler, EDNDamageZone::Rear, true, EDNDamageZone::Roof, 6.f, 0.f, 0.f, 0.50f, 10.f, EDNHingeEdge::None, 0.f } };
			if (!TestEqual("11 pecas", Rows.Num(), static_cast<int32>(UE_ARRAY_COUNT(Contract))))
			{
				return;
			}
			for (int32 i = 0; i < Rows.Num(); ++i)
			{
				const FRowSpec& C = Contract[i];
				FDNCarPartSpec S;
				const FString Label = FString::Printf(TEXT("linha %d (%s)"), i, *Rows[i].Id);
				if (!TestTrue(Label + TEXT(": le"), Rows[i].Row.ToSpec(S)))
				{
					continue;
				}
				TestEqual(Label + TEXT(": carro"), Rows[i].Row.CarId, FString(TEXT("SportsCar")));
				TestTrue(Label + TEXT(": peca"), S.Part == C.Part);
				TestTrue(Label + TEXT(": zona"), S.Zone == C.Zone);
				TestTrue(Label + TEXT(": secundaria"), S.bHasSecondaryZone == C.bSec && (!C.bSec || S.SecondaryZone == C.Sec));
				TestEqual(Label + TEXT(": massa"), S.MassKg, C.Mass);
				TestEqual(Label + TEXT(": pendura dano"), S.HangDamage, C.Hd);
				TestEqual(Label + TEXT(": pendura Dv"), S.HangDeltaVMs, C.Hv);
				TestEqual(Label + TEXT(": solta dano"), S.DetachDamage, C.Dd);
				TestEqual(Label + TEXT(": solta Dv"), S.DetachDeltaVMs, C.Dv);
				TestTrue(Label + TEXT(": borda"), S.HingeEdge == C.Edge);
				TestEqual(Label + TEXT(": angulo"), S.HingeMaxDeg, C.Deg);
				TestEqual(Label + TEXT(": amortecimento padrao"), S.HingeDamping, 2.f);
				TestEqual(Label + TEXT(": empurrao padrao"), S.LaunchMs, 4.f);
			}
		});
	});

	Describe("CarGlass.csv", [this]()
	{
		It("ToSpec copia todos os campos e le os textos", [this]()
		{
			FDNCarGlassRow R = DNTest::CarGlassRow(TEXT("rear"), TEXT("Rear"), TEXT("Roof"));
			R.CrackFrom = 0.25f;
			R.BurstDamage = 0.65f;
			R.BurstDeltaVMs = 11.f;
			FDNGlassSpec S;
			if (!TestTrue("le", R.ToSpec(S)))
			{
				return;
			}
			TestTrue("Glass", S.Glass == EDNGlass::Rear);
			TestTrue("Zone", S.Zone == EDNDamageZone::Rear);
			TestTrue("secundaria", S.bHasSecondaryZone && S.SecondaryZone == EDNDamageZone::Roof);
			TestEqual("CrackFrom", S.CrackFrom, 0.25f);
			TestEqual("BurstDamage", S.BurstDamage, 0.65f);
			TestEqual("BurstDeltaVMs", S.BurstDeltaVMs, 11.f);
			FDNGlassSpec S2;
			TestFalse("texto desconhecido nao le", DNTest::CarGlassRow(TEXT("Teto")).ToSpec(S2));
		});

		It("valida cada campo", [this]()
		{
			TestTrue("linha padrao valida", FDNDataValidation::ValidateCarGlass({ DNTest::Entry<FDNCarGlassEntry>(DNTest::CarGlassRow(), TEXT("vidro")) }).IsEmpty());
			TestFalse("nenhuma linha", FDNDataValidation::ValidateCarGlass({}).IsEmpty());
			BadGlass(TEXT("carro vazio"), TEXT("CarId"), [](FDNCarGlassRow& R) { R.CarId = TEXT(""); });
			BadGlass(TEXT("vidro desconhecido"), TEXT("Glass"), [](FDNCarGlassRow& R) { R.Glass = TEXT("Teto"); });
			BadGlass(TEXT("zona desconhecida"), TEXT("Zone"), [](FDNCarGlassRow& R) { R.Zone = TEXT("Meio"); });
			BadGlass(TEXT("secundaria igual a zona"), TEXT("SecondaryZone"), [](FDNCarGlassRow& R) { R.SecondaryZone = TEXT("Left"); });
			BadGlass(TEXT("trinca 1,1"), TEXT("CrackFrom"), [](FDNCarGlassRow& R) { R.CrackFrom = 1.1f; });
			BadGlass(TEXT("trinca negativa"), TEXT("CrackFrom"), [](FDNCarGlassRow& R) { R.CrackFrom = -0.1f; });
			BadGlass(TEXT("estoura em 1,1"), TEXT("BurstDamage"), [](FDNCarGlassRow& R) { R.BurstDamage = 1.1f; });
			BadGlass(TEXT("estoura com Dv negativo"), TEXT("BurstDeltaVMs"), [](FDNCarGlassRow& R) { R.BurstDeltaVMs = -1.f; });
			const TArray<FString> Twice = FDNDataValidation::ValidateCarGlass({ DNTest::Entry<FDNCarGlassEntry>(DNTest::CarGlassRow(), TEXT("a")),
				DNTest::Entry<FDNCarGlassEntry>(DNTest::CarGlassRow(), TEXT("b")) });
			TestTrue(FString::Printf(TEXT("o mesmo vidro duas vezes no mesmo carro: %s"), *JoinErrors(Twice)), Cites(Twice, TEXT("SideLeft")));
		});

		It("o CSV real carrega, e valido e tem a tabela do contrato, na ordem", [this]()
		{
			TArray<FDNCarGlassEntry> Rows;
			const TArray<FString> LoadErrors = FDNDataLoader::LoadEntries<FDNCarGlassRow>(TEXT("Data/CarGlass.csv"), Rows);
			TestTrue(FString::Printf(TEXT("carga: %s"), *JoinErrors(LoadErrors)), LoadErrors.IsEmpty());
			const TArray<FString> Errors = FDNDataValidation::ValidateCarGlass(Rows);
			TestTrue(FString::Printf(TEXT("validacao: %s"), *JoinErrors(Errors)), Errors.IsEmpty());
			struct FRowSpec { EDNGlass Glass; EDNDamageZone Zone; bool bSec; EDNDamageZone Sec; float Crack; float Burst; float BurstDv; };
			const FRowSpec Contract[] = {
				{ EDNGlass::Windshield, EDNDamageZone::Front, true, EDNDamageZone::Roof, 0.40f, 0.f, 0.f },
				{ EDNGlass::SideLeft, EDNDamageZone::Left, false, EDNDamageZone::Front, 0.30f, 0.60f, 12.f },
				{ EDNGlass::SideRight, EDNDamageZone::Right, false, EDNDamageZone::Front, 0.30f, 0.60f, 12.f },
				{ EDNGlass::Rear, EDNDamageZone::Rear, true, EDNDamageZone::Roof, 0.30f, 0.65f, 12.f },
				{ EDNGlass::HeadlightLeft, EDNDamageZone::Front, false, EDNDamageZone::Front, 0.30f, 0.60f, 0.f },
				{ EDNGlass::HeadlightRight, EDNDamageZone::Front, false, EDNDamageZone::Front, 0.30f, 0.60f, 0.f },
				{ EDNGlass::TaillightLeft, EDNDamageZone::Rear, false, EDNDamageZone::Front, 0.30f, 0.60f, 0.f },
				{ EDNGlass::TaillightRight, EDNDamageZone::Rear, false, EDNDamageZone::Front, 0.30f, 0.60f, 0.f } };
			if (!TestEqual("8 vidros", Rows.Num(), static_cast<int32>(UE_ARRAY_COUNT(Contract))))
			{
				return;
			}
			for (int32 i = 0; i < Rows.Num(); ++i)
			{
				const FRowSpec& C = Contract[i];
				FDNGlassSpec S;
				const FString Label = FString::Printf(TEXT("linha %d (%s)"), i, *Rows[i].Id);
				if (!TestTrue(Label + TEXT(": le"), Rows[i].Row.ToSpec(S)))
				{
					continue;
				}
				TestTrue(Label + TEXT(": vidro"), S.Glass == C.Glass);
				TestTrue(Label + TEXT(": zona"), S.Zone == C.Zone);
				TestTrue(Label + TEXT(": secundaria"), S.bHasSecondaryZone == C.bSec && (!C.bSec || S.SecondaryZone == C.Sec));
				TestEqual(Label + TEXT(": trinca"), S.CrackFrom, C.Crack);
				TestEqual(Label + TEXT(": estoura"), S.BurstDamage, C.Burst);
				TestEqual(Label + TEXT(": estoura Dv"), S.BurstDeltaVMs, C.BurstDv);
			}
		});
	});

	Describe("UDNGameDataSubsystem", [this]()
	{
		It("entrega as pecas e os vidros de um carro, na ordem; carro sem linhas ou tabela invalida: vazio", [this]()
		{
			UDNGameDataSubsystem* Sub = DNTest::NewSubsystem<UDNGameDataSubsystem>();
			AddExpectedError(TEXT("Dados do jogo:"), EAutomationExpectedErrorFlags::Contains, 0); // as outras tabelas vazias dão erro
			FDNData Data;
			Data.CarParts = { DNTest::Entry<FDNCarPartEntry>(DNTest::CarPartRow(), TEXT("a")),
				DNTest::Entry<FDNCarPartEntry>(DNTest::CarPartRow(TEXT("Hood"), TEXT("Front"), TEXT("Roof"), TEXT("TopRear")), TEXT("b")),
				DNTest::Entry<FDNCarPartEntry>(DNTest::CarPartRow(TEXT("Hood"), TEXT("Front"), TEXT(""), TEXT("TopRear"), TEXT("Outro")), TEXT("c")) };
			Data.CarGlass = { DNTest::Entry<FDNCarGlassEntry>(DNTest::CarGlassRow(), TEXT("v")) };
			Sub->ApplyData(Data, {});
			const TArray<FDNCarPartSpec> Parts = Sub->GetCarParts(TEXT("sportscar"));
			TestTrue("duas pecas do SportsCar, na ordem", Parts.Num() == 2 && Parts[0].Part == EDNDamagePart::BumperFront && Parts[1].Part == EDNDamagePart::Hood);
			TestEqual("um vidro", Sub->GetCarGlass(TEXT("SportsCar")).Num(), 1);
			TestEqual("carro sem linhas", Sub->GetCarParts(TEXT("Nenhum")).Num(), 0);
			Data.CarParts[0].Row.MassKg = 0.f;
			Sub->ApplyData(Data, {});
			TestEqual("tabela invalida: nenhuma peca", Sub->GetCarParts(TEXT("SportsCar")).Num(), 0);
			TestEqual("os vidros seguem", Sub->GetCarGlass(TEXT("SportsCar")).Num(), 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
