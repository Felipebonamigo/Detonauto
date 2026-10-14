// Testes dos modos de câmera (lote 14, J4): a ordem da tecla C e a leitura do tipo de âncora. Sem mundo.

#include "Misc/AutomationTest.h"
#include "Core/DNCamera.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNCameraSpec, "Detonauto.Camera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDNCameraSpec)

void FDNCameraSpec::Define()
{
	Describe("FDNCameraCycle::Next", [this]()
	{
		It("passa para o proximo e volta ao primeiro no fim", [this]()
		{
			TestEqual("0 -> 1", FDNCameraCycle::Next(0, 5), 1);
			TestEqual("3 -> 4", FDNCameraCycle::Next(3, 5), 4);
			TestEqual("4 -> 0", FDNCameraCycle::Next(4, 5), 0);
			TestEqual("um so modo", FDNCameraCycle::Next(0, 1), 0);
		});

		It("sem modos, indice negativo ou fora da lista: 0", [this]()
		{
			TestEqual("sem modos", FDNCameraCycle::Next(0, 0), 0);
			TestEqual("negativo", FDNCameraCycle::Next(-3, 5), 0);
			TestEqual("fora da lista", FDNCameraCycle::Next(9, 5), 0);
			TestEqual("igual ao total", FDNCameraCycle::Next(5, 5), 0);
			TestEqual("total negativo", FDNCameraCycle::Next(2, -1), 0);
		});

		It("percorrer Count vezes volta ao ponto de partida passando por todos", [this]()
		{
			int32 Index = 2;
			TSet<int32> Seen;
			for (int32 i = 0; i < 5; ++i) { Seen.Add(Index); Index = FDNCameraCycle::Next(Index, 5); }
			TestEqual("viu todos", Seen.Num(), 5);
			TestEqual("voltou", Index, 2);
		});
	});

	Describe("FDNCameraCycle::ParseAnchor", [this]()
	{
		It("Chase e Car, sem diferenciar a caixa", [this]()
		{
			EDNCameraAnchor A = EDNCameraAnchor::Car;
			TestTrue("Chase", FDNCameraCycle::ParseAnchor(TEXT("Chase"), A));
			TestEqual("e Chase", A, EDNCameraAnchor::Chase);
			TestTrue("car minusculo", FDNCameraCycle::ParseAnchor(TEXT("car"), A));
			TestEqual("e Car", A, EDNCameraAnchor::Car);
			TestTrue("CHASE", FDNCameraCycle::ParseAnchor(TEXT("CHASE"), A));
			TestEqual("e Chase de novo", A, EDNCameraAnchor::Chase);
		});

		It("qualquer outro texto e recusado e nao mexe na saida", [this]()
		{
			EDNCameraAnchor A = EDNCameraAnchor::Car;
			TestFalse("vazio", FDNCameraCycle::ParseAnchor(TEXT(""), A));
			TestFalse("outro", FDNCameraCycle::ParseAnchor(TEXT("Orbit"), A));
			TestFalse("com espaco", FDNCameraCycle::ParseAnchor(TEXT(" Chase"), A));
			TestEqual("saida intacta", A, EDNCameraAnchor::Car);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
