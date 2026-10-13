// Testes do takedown (docs/regras-de-jogo.md, seção 4 e a "Convenção de contato"; decisões K1 a K3).
// Funções puras: nada de mundo, Actor ou física.
//
// Convenção: a normal do contato é um vetor unitário no mundo que aponta do JOGADOR para o RIVAL.
// V é a frente do rival. Zona traseira: dot(normal, V) >= cos 45; frontal: dot <= -cos 45; lateral: o resto.
// O ângulo entre A e V é acos(dot(A, V)), de 0 a 180 graus.
//
// Nos limites exatos os testes montam os vetores com o mesmo valor de cosseno que o código usa, para o produto
// escalar cair exatamente no limite (frente do rival = eixo X, então dot = componente X).

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNTakedown.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FVector RivalForward(1.0, 0.0, 0.0);

	/** Vetor unitário no plano XY cujo produto escalar com o eixo X é exatamente Cos. */
	FVector WithCosine(float Cos)
	{
		return FVector(Cos, FMath::Sqrt(FMath::Max(0.f, 1.f - Cos * Cos)), 0.0);
	}

	void ExpectType(FAutomationTestBase& Test, const TCHAR* What, EDNTakedownType Actual, EDNTakedownType Expected)
	{
		Test.TestTrue(FString::Printf(TEXT("%s: esperado %s, veio %s"), What, FDNTakedown::ToString(Expected), FDNTakedown::ToString(Actual)),
			Actual == Expected);
	}

	void ExpectZone(FAutomationTestBase& Test, const TCHAR* What, EDNContactZone Actual, EDNContactZone Expected)
	{
		Test.TestTrue(FString::Printf(TEXT("%s: esperada zona %s, veio %s"), What, FDNTakedown::ToString(Expected), FDNTakedown::ToString(Actual)),
			Actual == Expected);
	}

	FDNTakedownClassifyInput MakeInput(const FVector& A, const FVector& Normal, float HeightCm = 0.f, bool bFalling = false)
	{
		FDNTakedownClassifyInput In;
		In.PlayerForward = A;
		In.RivalForward = RivalForward;
		In.ContactNormal = Normal;
		In.PlayerHeightAboveRivalCm = HeightCm;
		In.bPlayerFalling = bFalling;
		return In;
	}
}

BEGIN_DEFINE_SPEC(FDNTakedownZoneSpec, "Detonauto.Takedown.Zona",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNTakedownConfig Config;
END_DEFINE_SPEC(FDNTakedownZoneSpec)

void FDNTakedownZoneSpec::Define()
{
	Describe("Derivacao da zona a partir da normal (K1)", [this]()
	{
		It("normal na direcao da frente do rival: zona traseira; oposta: frontal; perpendicular: lateral", [this]()
		{
			ExpectZone(*this, TEXT("d = 1"), FDNTakedown::DeriveZone(Config, FVector(1, 0, 0), RivalForward), EDNContactZone::Rear);
			ExpectZone(*this, TEXT("d = -1"), FDNTakedown::DeriveZone(Config, FVector(-1, 0, 0), RivalForward), EDNContactZone::Front);
			ExpectZone(*this, TEXT("d = 0"), FDNTakedown::DeriveZone(Config, FVector(0, 1, 0), RivalForward), EDNContactZone::Side);
			ExpectZone(*this, TEXT("d = 0 do outro lado"), FDNTakedown::DeriveZone(Config, FVector(0, -1, 0), RivalForward), EDNContactZone::Side);
		});

		It("exatamente cos 45: traseira (maior ou igual); um pouco abaixo: lateral", [this]()
		{
			const float Cos45 = FDNTakedown::CosineOf(Config.ZoneHalfAngleDegrees);
			ExpectZone(*this, TEXT("d = cos 45"), FDNTakedown::DeriveZone(Config, WithCosine(Cos45), RivalForward), EDNContactZone::Rear);
			ExpectZone(*this, TEXT("d = cos 45 - 1e-4"), FDNTakedown::DeriveZone(Config, WithCosine(Cos45 - 1e-4f), RivalForward), EDNContactZone::Side);
			ExpectZone(*this, TEXT("d = cos 45 + 1e-4"), FDNTakedown::DeriveZone(Config, WithCosine(Cos45 + 1e-4f), RivalForward), EDNContactZone::Rear);
		});

		It("exatamente -cos 45: frontal (menor ou igual); um pouco acima: lateral", [this]()
		{
			const float Cos45 = FDNTakedown::CosineOf(Config.ZoneHalfAngleDegrees);
			ExpectZone(*this, TEXT("d = -cos 45"), FDNTakedown::DeriveZone(Config, WithCosine(-Cos45), RivalForward), EDNContactZone::Front);
			ExpectZone(*this, TEXT("d = -cos 45 + 1e-4"), FDNTakedown::DeriveZone(Config, WithCosine(-Cos45 + 1e-4f), RivalForward), EDNContactZone::Side);
			ExpectZone(*this, TEXT("d = -cos 45 - 1e-4"), FDNTakedown::DeriveZone(Config, WithCosine(-Cos45 - 1e-4f), RivalForward), EDNContactZone::Front);
		});

		It("vetores sem comprimento 1 dao o mesmo resultado", [this]()
		{
			ExpectZone(*this, TEXT("normal x5"), FDNTakedown::DeriveZone(Config, FVector(5, 0, 0), RivalForward), EDNContactZone::Rear);
			ExpectZone(*this, TEXT("normal x5 lateral"), FDNTakedown::DeriveZone(Config, FVector(0, 5, 0), RivalForward), EDNContactZone::Side);
			ExpectZone(*this, TEXT("frente do rival x3"), FDNTakedown::DeriveZone(Config, FVector(-1, 0, 0), FVector(3, 0, 0)), EDNContactZone::Front);
		});

		It("vetor invalido (zero ou NaN) cai em lateral, o 'resto' do contrato", [this]()
		{
			ExpectZone(*this, TEXT("normal zero"), FDNTakedown::DeriveZone(Config, FVector::ZeroVector, RivalForward), EDNContactZone::Side);
			ExpectZone(*this, TEXT("frente zero"), FDNTakedown::DeriveZone(Config, FVector(1, 0, 0), FVector::ZeroVector), EDNContactZone::Side);
			const double NaN = std::numeric_limits<double>::quiet_NaN();
			ExpectZone(*this, TEXT("NaN"), FDNTakedown::DeriveZone(Config, FVector(NaN, 0, 0), RivalForward), EDNContactZone::Side);
		});
	});
}

BEGIN_DEFINE_SPEC(FDNTakedownTypeSpec, "Detonauto.Takedown.Tipo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNTakedownConfig Config;
	FVector RearNormal = FVector(1, 0, 0);   // zona traseira
	FVector SideNormal = FVector(0, 1, 0);   // zona lateral
	FVector FrontNormal = FVector(-1, 0, 0); // zona frontal
END_DEFINE_SPEC(FDNTakedownTypeSpec)

void FDNTakedownTypeSpec::Define()
{
	Describe("1. Vertical", [this]()
	{
		It("100 cm acima e caindo: vertical (inclusive em 100)", [this]()
		{
			ExpectType(*this, TEXT("100 cm"), FDNTakedown::Classify(Config, MakeInput(FVector(1, 0, 0), SideNormal, 100.f, true)), EDNTakedownType::Vertical);
			ExpectType(*this, TEXT("250 cm"), FDNTakedown::Classify(Config, MakeInput(FVector(1, 0, 0), SideNormal, 250.f, true)), EDNTakedownType::Vertical);
		});

		It("99,9 cm ou sem estar caindo nao e vertical", [this]()
		{
			ExpectType(*this, TEXT("99,9 cm caindo"), FDNTakedown::Classify(Config, MakeInput(FVector(0, 1, 0), SideNormal, 99.9f, true)), EDNTakedownType::TBone);
			ExpectType(*this, TEXT("alto mas subindo"), FDNTakedown::Classify(Config, MakeInput(FVector(0, 1, 0), SideNormal, 300.f, false)), EDNTakedownType::TBone);
		});

		It("vem antes de todos os outros, mesmo com a geometria de 'por tras'", [this]()
		{
			ExpectType(*this, TEXT("por tras + vertical"), FDNTakedown::Classify(Config, MakeInput(FVector(1, 0, 0), RearNormal, 150.f, true)), EDNTakedownType::Vertical);
		});

		It("nao depende dos vetores: vale mesmo com vetores invalidos", [this]()
		{
			ExpectType(*this, TEXT("vetores zero"), FDNTakedown::Classify(Config, MakeInput(FVector::ZeroVector, FVector::ZeroVector, 120.f, true)), EDNTakedownType::Vertical);
		});
	});

	Describe("2. Por tras", [this]()
	{
		It("angulo menor que 30 e zona traseira", [this]()
		{
			ExpectType(*this, TEXT("0 graus"), FDNTakedown::Classify(Config, MakeInput(FVector(1, 0, 0), RearNormal)), EDNTakedownType::FromBehind);
			ExpectType(*this, TEXT("20 graus"), FDNTakedown::Classify(Config, MakeInput(WithCosine(FDNTakedown::CosineOf(20.f)), RearNormal)), EDNTakedownType::FromBehind);
		});

		It("exatamente 30 graus nao e por tras (so menor que 30)", [this]()
		{
			const float Cos30 = FDNTakedown::CosineOf(30.f);
			ExpectType(*this, TEXT("30 graus"), FDNTakedown::Classify(Config, MakeInput(WithCosine(Cos30), RearNormal)), EDNTakedownType::General);
			ExpectType(*this, TEXT("29,9 graus"), FDNTakedown::Classify(Config, MakeInput(WithCosine(Cos30 + 1e-3f), RearNormal)), EDNTakedownType::FromBehind);
		});

		It("angulo pequeno mas zona frontal nao e por tras", [this]()
		{
			ExpectType(*this, TEXT("zona frontal"), FDNTakedown::Classify(Config, MakeInput(FVector(1, 0, 0), FrontNormal)), EDNTakedownType::General);
		});
	});

	Describe("3. Em T", [this]()
	{
		It("angulo entre 60 e 120 e zona lateral", [this]()
		{
			ExpectType(*this, TEXT("90 graus"), FDNTakedown::Classify(Config, MakeInput(FVector(0, 1, 0), SideNormal)), EDNTakedownType::TBone);
		});

		It("60 e 120 graus exatos contam (inclusivo)", [this]()
		{
			ExpectType(*this, TEXT("60 graus"), FDNTakedown::Classify(Config, MakeInput(WithCosine(FDNTakedown::CosineOf(60.f)), SideNormal)), EDNTakedownType::TBone);
			ExpectType(*this, TEXT("120 graus"), FDNTakedown::Classify(Config, MakeInput(WithCosine(FDNTakedown::CosineOf(120.f)), SideNormal)), EDNTakedownType::TBone);
		});

		It("logo abaixo de 60 e logo acima de 120 saem do T", [this]()
		{
			ExpectType(*this, TEXT("59,9 graus"), FDNTakedown::Classify(Config, MakeInput(WithCosine(FDNTakedown::CosineOf(60.f) + 1e-3f), SideNormal)), EDNTakedownType::Side);
			ExpectType(*this, TEXT("120,1 graus"), FDNTakedown::Classify(Config, MakeInput(WithCosine(FDNTakedown::CosineOf(120.f) - 1e-3f), SideNormal)), EDNTakedownType::General);
		});

		It("angulo de T com zona traseira ou frontal nao e T", [this]()
		{
			ExpectType(*this, TEXT("zona traseira"), FDNTakedown::Classify(Config, MakeInput(FVector(0, 1, 0), RearNormal)), EDNTakedownType::General);
			ExpectType(*this, TEXT("zona frontal"), FDNTakedown::Classify(Config, MakeInput(FVector(0, 1, 0), FrontNormal)), EDNTakedownType::General);
		});
	});

	Describe("4. Lateral", [this]()
	{
		It("zona lateral e angulo menor que 60", [this]()
		{
			ExpectType(*this, TEXT("0 graus"), FDNTakedown::Classify(Config, MakeInput(FVector(1, 0, 0), SideNormal)), EDNTakedownType::Side);
			ExpectType(*this, TEXT("45 graus"), FDNTakedown::Classify(Config, MakeInput(WithCosine(FDNTakedown::CosineOf(45.f)), SideNormal)), EDNTakedownType::Side);
		});

		It("por tras tem prioridade sobre lateral, e a zona decide entre eles", [this]()
		{
			// mesmo angulo pequeno: zona traseira vira "por tras"; zona lateral vira "lateral"
			ExpectType(*this, TEXT("zona traseira"), FDNTakedown::Classify(Config, MakeInput(FVector(1, 0, 0), RearNormal)), EDNTakedownType::FromBehind);
			ExpectType(*this, TEXT("zona lateral"), FDNTakedown::Classify(Config, MakeInput(FVector(1, 0, 0), SideNormal)), EDNTakedownType::Side);
		});
	});

	Describe("5. Geral", [this]()
	{
		It("qualquer outro caso", [this]()
		{
			ExpectType(*this, TEXT("zona lateral, angulo 150"), FDNTakedown::Classify(Config, MakeInput(WithCosine(FDNTakedown::CosineOf(150.f)), SideNormal)), EDNTakedownType::General);
			ExpectType(*this, TEXT("zona traseira, angulo 45"), FDNTakedown::Classify(Config, MakeInput(WithCosine(FDNTakedown::CosineOf(45.f)), RearNormal)), EDNTakedownType::General);
			ExpectType(*this, TEXT("zona frontal, angulo 180"), FDNTakedown::Classify(Config, MakeInput(FVector(-1, 0, 0), FrontNormal)), EDNTakedownType::General);
		});
	});

	Describe("Entradas invalidas e escala dos vetores", [this]()
	{
		It("vetores sem comprimento 1 classificam como os unitarios", [this]()
		{
			ExpectType(*this, TEXT("A x7, normal x3"), FDNTakedown::Classify(Config, MakeInput(FVector(7, 0, 0), FVector(3, 0, 0))), EDNTakedownType::FromBehind);
		});

		It("vetor zero ou NaN (fora do vertical) vira geral", [this]()
		{
			ExpectType(*this, TEXT("A zero"), FDNTakedown::Classify(Config, MakeInput(FVector::ZeroVector, SideNormal)), EDNTakedownType::General);
			const double NaN = std::numeric_limits<double>::quiet_NaN();
			ExpectType(*this, TEXT("A NaN"), FDNTakedown::Classify(Config, MakeInput(FVector(NaN, 0, 0), SideNormal)), EDNTakedownType::General);
			FDNTakedownClassifyInput In = MakeInput(FVector(1, 0, 0), SideNormal);
			In.RivalForward = FVector::ZeroVector;
			ExpectType(*this, TEXT("frente do rival zero"), FDNTakedown::Classify(Config, In), EDNTakedownType::General);
		});

		It("altura NaN nao vira vertical", [this]()
		{
			ExpectType(*this, TEXT("altura NaN"),
				FDNTakedown::Classify(Config, MakeInput(FVector(0, 1, 0), SideNormal, std::numeric_limits<float>::quiet_NaN(), true)), EDNTakedownType::TBone);
		});
	});
}

BEGIN_DEFINE_SPEC(FDNTakedownQualifySpec, "Detonauto.Takedown.Qualificacao",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNTakedownConfig Config;
	FDNTakedownContact Contact;
END_DEFINE_SPEC(FDNTakedownQualifySpec)

void FDNTakedownQualifySpec::Define()
{
	BeforeEach([this]()
	{
		// um contato que cumpre as tres condicoes
		Contact = FDNTakedownContact();
		Contact.RelativeSpeedKmh = 50.f;
		Contact.bRivalCrashed = true;
		Contact.RivalCrashSecondsAfterContact = 0.8f;
		Contact.bPlayerDestroyed = false;
	});

	Describe("Condicoes 1 a 3", [this]()
	{
		It("cumprindo as tres, qualifica", [this]()
		{
			TestTrue("qualifica", FDNTakedown::Qualifies(Config, Contact));
		});

		It("1. velocidade relativa de pelo menos 20 km/h (inclusivo)", [this]()
		{
			Contact.RelativeSpeedKmh = 20.f;
			TestTrue("20 km/h", FDNTakedown::Qualifies(Config, Contact));
			Contact.RelativeSpeedKmh = 19.99f;
			TestFalse("19,99 km/h", FDNTakedown::Qualifies(Config, Contact));
			Contact.RelativeSpeedKmh = 0.f;
			TestFalse("zero", FDNTakedown::Qualifies(Config, Contact));
		});

		It("2. o rival bate em ate 1,5 s depois do contato (inclusivo)", [this]()
		{
			Contact.RivalCrashSecondsAfterContact = 1.5f;
			TestTrue("1,5 s", FDNTakedown::Qualifies(Config, Contact));
			Contact.RivalCrashSecondsAfterContact = 1.51f;
			TestFalse("1,51 s", FDNTakedown::Qualifies(Config, Contact));
			Contact.RivalCrashSecondsAfterContact = 0.f;
			TestTrue("no mesmo instante", FDNTakedown::Qualifies(Config, Contact));
		});

		It("2. sem batida do rival nao qualifica", [this]()
		{
			Contact.bRivalCrashed = false;
			TestFalse("rival nao bateu", FDNTakedown::Qualifies(Config, Contact));
		});

		It("3. o jogador se destruir no mesmo intervalo desqualifica", [this]()
		{
			Contact.bPlayerDestroyed = true;
			Contact.PlayerDestroyedSecondsAfterContact = 1.0f;
			TestFalse("dentro do intervalo", FDNTakedown::Qualifies(Config, Contact));
			Contact.PlayerDestroyedSecondsAfterContact = 1.5f;
			TestFalse("exatamente 1,5 s", FDNTakedown::Qualifies(Config, Contact));
		});

		It("3. se destruir depois do intervalo ou antes do contato nao desqualifica", [this]()
		{
			Contact.bPlayerDestroyed = true;
			Contact.PlayerDestroyedSecondsAfterContact = 1.51f;
			TestTrue("depois do intervalo", FDNTakedown::Qualifies(Config, Contact));
			Contact.PlayerDestroyedSecondsAfterContact = -0.1f;
			TestTrue("antes do contato", FDNTakedown::Qualifies(Config, Contact));
		});
	});

	Describe("Entradas invalidas", [this]()
	{
		It("tempo negativo ou NaN do rival nao qualifica", [this]()
		{
			Contact.RivalCrashSecondsAfterContact = -0.1f;
			TestFalse("negativo", FDNTakedown::Qualifies(Config, Contact));
			Contact.RivalCrashSecondsAfterContact = std::numeric_limits<float>::quiet_NaN();
			TestFalse("NaN", FDNTakedown::Qualifies(Config, Contact));
		});

		It("velocidade NaN nao qualifica", [this]()
		{
			Contact.RelativeSpeedKmh = std::numeric_limits<float>::quiet_NaN();
			TestFalse("NaN", FDNTakedown::Qualifies(Config, Contact));
		});

		It("tempo NaN da destruicao do jogador nao o desqualifica", [this]()
		{
			Contact.bPlayerDestroyed = true;
			Contact.PlayerDestroyedSecondsAfterContact = std::numeric_limits<float>::quiet_NaN();
			TestTrue("NaN nao esta no intervalo", FDNTakedown::Qualifies(Config, Contact));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
