// Testes da pontuação saturante (FDNScore). Não dependem de mundo nem de física.
//
// Regra (docs/regras-de-jogo.md, seção 2): a pontuação só sobe; valores menores ou iguais a zero
// são ignorados; o total satura em MAX_int32 em vez de dar a volta.

#include "Misc/AutomationTest.h"
#include "Core/DNScore.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNScoreSpec, "Detonauto.Pontuacao",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNScoreSpec)

void FDNScoreSpec::Define()
{
	Describe("Add", [this]()
	{
		It("comeca em zero", [this]()
		{
			FDNScore Score;
			TestEqual("total inicial", Score.Total, 0);
		});

		It("soma valores positivos e devolve o novo total", [this]()
		{
			FDNScore Score;
			Score.Add(10);
			TestEqual("devolve o novo total", Score.Add(5), 15);
			TestEqual("total guardado", Score.Total, 15);
		});

		It("soma o menor valor positivo possivel", [this]()
		{
			FDNScore Score;
			TestEqual("um ponto", Score.Add(1), 1);
		});

		It("ignora zero", [this]()
		{
			FDNScore Score;
			Score.Add(7);
			TestEqual("zero nao muda", Score.Add(0), 7);
		});

		It("ignora valores negativos", [this]()
		{
			FDNScore Score;
			Score.Add(7);
			TestEqual("negativo nao muda", Score.Add(-100), 7);
			TestEqual("o menor int32 nao muda", Score.Add(MIN_int32), 7);
			TestEqual("total guardado", Score.Total, 7);
		});

		It("satura em MAX_int32 sem dar a volta", [this]()
		{
			FDNScore Score;
			Score.Add(MAX_int32 - 1);
			TestEqual("quase no teto", Score.Total, MAX_int32 - 1);
			TestEqual("estoura para o teto", Score.Add(10), MAX_int32);
			TestEqual("continua no teto", Score.Add(MAX_int32), MAX_int32);
		});

		It("chega exatamente ao teto sem passar", [this]()
		{
			FDNScore Score;
			Score.Add(MAX_int32 - 5);
			TestEqual("soma exata ate o teto", Score.Add(5), MAX_int32);
			TestEqual("mais um ponto continua no teto", Score.Add(1), MAX_int32);
		});
	});

	Describe("Reset", [this]()
	{
		It("volta a zero", [this]()
		{
			FDNScore Score;
			Score.Add(50);
			Score.Reset();
			TestEqual("total apos reset", Score.Total, 0);
		});

		It("permite pontuar de novo depois de chegar ao teto", [this]()
		{
			FDNScore Score;
			Score.Add(MAX_int32);
			Score.Reset();
			TestEqual("recomeca", Score.Add(3), 3);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
