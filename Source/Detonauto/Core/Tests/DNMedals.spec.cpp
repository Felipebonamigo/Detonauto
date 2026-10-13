// Testes das medalhas (docs/regras-de-jogo.md, seção 3, e a decisão M1).
// Funções puras, sem mundo.

#include "Misc/AutomationTest.h"
#include "Core/DNMedals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	void ExpectMedal(FAutomationTestBase& Test, const TCHAR* What, EDNMedal Actual, EDNMedal Expected)
	{
		Test.TestTrue(FString::Printf(TEXT("%s: esperado %s, veio %s"), What, FDNMedals::ToString(Expected), FDNMedals::ToString(Actual)),
			Actual == Expected);
	}
}

BEGIN_DEFINE_SPEC(FDNMedalsSpec, "Detonauto.Medalhas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDNMedalThresholds Limits;
END_DEFINE_SPEC(FDNMedalsSpec)

void FDNMedalsSpec::Define()
{
	BeforeEach([this]()
	{
		Limits.Bronze = 1000;
		Limits.Silver = 5000;
		Limits.Gold = 12000;
	});

	Describe("Avaliacao", [this]()
	{
		It("abaixo do bronze nao tem medalha", [this]()
		{
			ExpectMedal(*this, TEXT("zero"), FDNMedals::Evaluate(Limits, 0), EDNMedal::None);
			ExpectMedal(*this, TEXT("999"), FDNMedals::Evaluate(Limits, 999), EDNMedal::None);
		});

		// M1: alcançar = maior ou igual.
		It("exatamente o limite alcanca a medalha", [this]()
		{
			ExpectMedal(*this, TEXT("1000 e bronze"), FDNMedals::Evaluate(Limits, 1000), EDNMedal::Bronze);
			ExpectMedal(*this, TEXT("5000 e prata"), FDNMedals::Evaluate(Limits, 5000), EDNMedal::Silver);
			ExpectMedal(*this, TEXT("12000 e ouro"), FDNMedals::Evaluate(Limits, 12000), EDNMedal::Gold);
		});

		It("um ponto abaixo do limite fica na medalha de baixo", [this]()
		{
			ExpectMedal(*this, TEXT("4999 e bronze"), FDNMedals::Evaluate(Limits, 4999), EDNMedal::Bronze);
			ExpectMedal(*this, TEXT("11999 e prata"), FDNMedals::Evaluate(Limits, 11999), EDNMedal::Silver);
		});

		It("acima do ouro continua ouro, inclusive no teto do int32", [this]()
		{
			ExpectMedal(*this, TEXT("acima"), FDNMedals::Evaluate(Limits, 99999), EDNMedal::Gold);
			ExpectMedal(*this, TEXT("MAX_int32"), FDNMedals::Evaluate(Limits, MAX_int32), EDNMedal::Gold);
		});

		It("pontuacao negativa nao tem medalha", [this]()
		{
			ExpectMedal(*this, TEXT("negativa"), FDNMedals::Evaluate(Limits, -1), EDNMedal::None);
			ExpectMedal(*this, TEXT("menor int32"), FDNMedals::Evaluate(Limits, MIN_int32), EDNMedal::None);
		});

		It("limites invalidos nao dao medalha nenhuma", [this]()
		{
			Limits.Bronze = 0; // invalido: bronze precisa ser maior que zero
			ExpectMedal(*this, TEXT("bronze zero"), FDNMedals::Evaluate(Limits, 5000), EDNMedal::None);
			Limits.Bronze = 1000;
			Limits.Silver = 1000; // invalido: nao estritamente crescente
			ExpectMedal(*this, TEXT("prata igual ao bronze"), FDNMedals::Evaluate(Limits, 5000), EDNMedal::None);
		});

		It("o menor limite valido (bronze 1) e alcancado com 1 ponto", [this]()
		{
			Limits.Bronze = 1;
			Limits.Silver = 2;
			Limits.Gold = 3;
			ExpectMedal(*this, TEXT("1 ponto"), FDNMedals::Evaluate(Limits, 1), EDNMedal::Bronze);
			ExpectMedal(*this, TEXT("0 pontos"), FDNMedals::Evaluate(Limits, 0), EDNMedal::None);
		});
	});

	Describe("Validacao dos limites", [this]()
	{
		It("aceita limites maiores que zero e estritamente crescentes", [this]()
		{
			FString Error;
			TestTrue("valido", FDNMedals::Validate(Limits, &Error));
		});

		It("aceita ouro no teto do int32", [this]()
		{
			Limits.Gold = MAX_int32;
			TestTrue("valido", FDNMedals::Validate(Limits));
		});

		It("recusa prata igual ao bronze", [this]()
		{
			Limits.Silver = Limits.Bronze;
			TestFalse("recusado", FDNMedals::Validate(Limits));
		});

		It("recusa prata menor que o bronze", [this]()
		{
			Limits.Silver = Limits.Bronze - 1;
			TestFalse("recusado", FDNMedals::Validate(Limits));
		});

		It("recusa ouro igual a prata", [this]()
		{
			Limits.Gold = Limits.Silver;
			TestFalse("recusado", FDNMedals::Validate(Limits));
		});

		It("recusa ouro menor que a prata", [this]()
		{
			Limits.Gold = Limits.Silver - 1;
			TestFalse("recusado", FDNMedals::Validate(Limits));
		});

		// M1: além de crescentes, os limites precisam ser maiores que zero.
		It("recusa bronze zero ou negativo", [this]()
		{
			Limits.Bronze = 0;
			TestFalse("zero", FDNMedals::Validate(Limits));
			Limits.Bronze = -5;
			TestFalse("negativo", FDNMedals::Validate(Limits));
		});

		It("recusa tudo igual e tudo zero", [this]()
		{
			Limits.Bronze = Limits.Silver = Limits.Gold = 100;
			TestFalse("tudo igual", FDNMedals::Validate(Limits));
			Limits.Bronze = Limits.Silver = Limits.Gold = 0;
			TestFalse("tudo zero", FDNMedals::Validate(Limits));
		});

		It("a mensagem de erro diz qual limite falhou", [this]()
		{
			FString Error;
			Limits.Silver = Limits.Bronze;
			TestFalse("recusado", FDNMedals::Validate(Limits, &Error));
			TestTrue("fala da prata", Error.Contains(TEXT("prata"), ESearchCase::IgnoreCase));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
