// Testes do percurso (C1 a C4): comprimento, progresso com janela, pontas presas, entradas inválidas.

#include <limits>
#include "Misc/AutomationTest.h"
#include "Core/DNCourse.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDNCourseSpec, "Detonauto.Percurso",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const float NaNf = std::numeric_limits<float>::quiet_NaN();
	// em L: 1000 cm para +X, depois 500 cm para +Y; total 1500
	TArray<FVector> L = { FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 500, 0) };
END_DEFINE_SPEC(FDNCourseSpec)

void FDNCourseSpec::Define()
{
	Describe("Comprimento (C1)", [this]()
	{
		It("soma os trechos", [this]()
		{
			TestEqual("L", FDNCourse::Length(L), 1500.f);
		});

		It("menos de 2 pontos ou ponto invalido: 0", [this]()
		{
			TestEqual("vazio", FDNCourse::Length(TArray<FVector>()), 0.f);
			TestEqual("um", FDNCourse::Length(TArray<FVector>{ FVector(1, 2, 3) }), 0.f);
			TestEqual("NaN", FDNCourse::Length(TArray<FVector>{ FVector(0, 0, 0), FVector(NaNf, 0, 0) }), 0.f);
		});
	});

	Describe("Progresso (C2 a C4)", [this]()
	{
		It("projeta no trecho: ponto ao lado da reta conta o arco ate a projecao", [this]()
		{
			TestEqual("no primeiro trecho", FDNCourse::Progress(L, FVector(400, 300, 0), 0.f, 1000.f, 5000.f), 400.f);
			TestEqual("no segundo trecho", FDNCourse::Progress(L, FVector(1200, 200, 0), 1000.f, 1000.f, 5000.f), 1200.f);
		});

		It("presa nas pontas", [this]()
		{
			TestEqual("antes do inicio", FDNCourse::Progress(L, FVector(-500, 0, 0), 0.f, 1000.f, 5000.f), 0.f);
			TestEqual("depois do fim", FDNCourse::Progress(L, FVector(1000, 900, 0), 1400.f, 1000.f, 5000.f), 1500.f);
		});

		It("a janela impede atalho entre trechos proximos", [this]()
		{
			// percurso em U: vai 1000 para +X, sobe 100, volta 1000 para -X; o ponto (500, 90) esta perto do 3o trecho (arco ~1600),
			// mas com o progresso em 500 a janela de 300 a frente so enxerga o primeiro trecho
			const TArray<FVector> U = { FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 100, 0), FVector(0, 100, 0) };
			TestEqual("fica no primeiro trecho", FDNCourse::Progress(U, FVector(500, 90, 0), 500.f, 100.f, 300.f), 500.f);
			TestEqual("janela larga acha o 3o trecho", FDNCourse::Progress(U, FVector(500, 90, 0), 500.f, 100.f, 2000.f), 1600.f);
		});

		It("o resultado nunca sai da janela", [this]()
		{
			const TArray<FVector> U = { FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 100, 0), FVector(0, 100, 0) };
			TestEqual("corta na frente", FDNCourse::Progress(U, FVector(900, 10, 0), 500.f, 100.f, 300.f), 800.f);
			TestEqual("corta atras", FDNCourse::Progress(U, FVector(100, 10, 0), 500.f, 100.f, 300.f), 400.f);
			// janela [1200, 1400] so enxerga o 2o trecho de L, mesmo com o 1o mais perto
			TestEqual("trecho fora da janela nao entra", FDNCourse::Progress(L, FVector(900, 0, 0), 1300.f, 100.f, 100.f), 1200.f);
		});

		It("entrada invalida devolve o hint", [this]()
		{
			TestEqual("posicao NaN", FDNCourse::Progress(L, FVector(NaNf, 0, 0), 700.f, 1000.f, 5000.f), 700.f);
			TestEqual("percurso vazio", FDNCourse::Progress(TArray<FVector>(), FVector(1, 1, 1), 700.f, 1000.f, 5000.f), 700.f);
		});
	});

	Describe("Marcas (C5)", [this]()
	{
		It("a cada espacamento, e a ultima e a chegada", [this]()
		{
			const TArray<float> M = FDNCourse::Marks(1000.f, 300.f);
			TestTrue("300, 600, 900, 1000", M.Num() == 4 && M[0] == 300.f && M[1] == 600.f && M[2] == 900.f && M[3] == 1000.f);
		});

		It("comprimento multiplo: a chegada nao repete", [this]()
		{
			const TArray<float> M = FDNCourse::Marks(900.f, 300.f);
			TestTrue("300, 600, 900", M.Num() == 3 && M[2] == 900.f);
		});

		It("espacamento maior que o percurso: so a chegada", [this]()
		{
			const TArray<float> M = FDNCourse::Marks(500.f, 800.f);
			TestTrue("500", M.Num() == 1 && M[0] == 500.f);
		});

		It("entradas invalidas", [this]()
		{
			TestEqual("espacamento 0: so a chegada", FDNCourse::Marks(500.f, 0.f).Num(), 1);
			TestEqual("espacamento NaN: so a chegada", FDNCourse::Marks(500.f, NaNf).Num(), 1);
			TestEqual("comprimento 0: nada", FDNCourse::Marks(0.f, 100.f).Num(), 0);
			TestEqual("comprimento NaN: nada", FDNCourse::Marks(NaNf, 100.f).Num(), 0);
		});
	});

	Describe("Erro de rumo (C6)", [this]()
	{
		It("alvo para +Y (direita no Unreal) e positivo, para -Y negativo, em frente zero, atras 180", [this]()
		{
			const FVector P(0, 0, 0), F(1, 0, 0);
			TestEqual("frente", FDNCourse::HeadingErrorDeg(P, F, FVector(100, 0, 0)), 0.f, 0.01f);
			TestEqual("direita (+Y)", FDNCourse::HeadingErrorDeg(P, F, FVector(0, 100, 0)), 90.f, 0.01f);
			TestEqual("esquerda (-Y)", FDNCourse::HeadingErrorDeg(P, F, FVector(0, -100, 0)), -90.f, 0.01f);
			TestEqual("45 para +Y", FDNCourse::HeadingErrorDeg(P, F, FVector(100, 100, 0)), 45.f, 0.01f);
			TestEqual("atras", FMath::Abs(FDNCourse::HeadingErrorDeg(P, F, FVector(-100, 0, 0))), 180.f, 0.01f);
		});

		It("ignora a altura e vale com a frente nao normalizada e a posicao fora da origem", [this]()
		{
			TestEqual("Z ignorado", FDNCourse::HeadingErrorDeg(FVector(10, 10, 5), FVector(0, 7, 3), FVector(10, 10, 900)), 0.f, 0.01f);
			TestEqual("frente +Y, alvo -X: +Y gira para -X", FDNCourse::HeadingErrorDeg(FVector(10, 10, 0), FVector(0, 7, 0), FVector(-90, 10, 0)), 90.f, 0.01f);
		});

		It("degenerado: zero", [this]()
		{
			TestEqual("frente zero", FDNCourse::HeadingErrorDeg(FVector::ZeroVector, FVector::ZeroVector, FVector(1, 0, 0)), 0.f);
			TestEqual("alvo na posicao", FDNCourse::HeadingErrorDeg(FVector(5, 5, 0), FVector(1, 0, 0), FVector(5, 5, 9)), 0.f);
			TestEqual("NaN", FDNCourse::HeadingErrorDeg(FVector::ZeroVector, FVector(1, 0, 0), FVector(NaNf, 0, 0)), 0.f);
		});
	});

	Describe("Ponto no progresso", [this]()
	{
		It("interpola e prende", [this]()
		{
			TestTrue("700", FDNCourse::LocationAt(L, 700.f).Equals(FVector(700, 0, 0), 0.01f));
			TestTrue("1250", FDNCourse::LocationAt(L, 1250.f).Equals(FVector(1000, 250, 0), 0.01f));
			TestTrue("negativo", FDNCourse::LocationAt(L, -9.f).Equals(FVector(0, 0, 0), 0.01f));
			TestTrue("alem do fim", FDNCourse::LocationAt(L, 9999.f).Equals(FVector(1000, 500, 0), 0.01f));
		});
	});
}

#endif
