// Percurso da corrida ponto a ponto (docs/fatia-jogavel.md): a linha central da rua como lista de pontos. Regras puras, sem UObject:
// comprimento e progresso de um ponto do mundo ao longo da linha. Testes: Core/Tests/DNCourse.spec.cpp.
//
// Decisões:
//  C1 o comprimento é a soma dos trechos [cm]; menos de 2 pontos ou ponto não finito: comprimento 0;
//  C2 o progresso de uma posição é o arco até a projeção dela no trecho mais próximo, dentro de uma janela em volta do progresso anterior
//     (Hint - Back .. Hint + Ahead): impede atalho entre ruas que se cruzam ou se aproximam;
//  C3 posição não finita, ou percurso inválido: devolve o Hint (o progresso anterior vale);
//  C4 a projeção fica presa nas pontas: antes do início é 0, depois do fim é o comprimento.

#pragma once

#include "CoreMinimal.h"

struct FDNCourse
{
	/** C1: comprimento total da linha [cm]. */
	static float Length(TConstArrayView<FVector> Points);

	/** C2 a C4: progresso [cm] de Position, procurando só entre Hint - BackCm e Hint + AheadCm. */
	static float Progress(TConstArrayView<FVector> Points, const FVector& Position, float HintCm, float BackCm, float AheadCm);

	/** O ponto da linha no progresso dado (preso em 0 .. comprimento); ZeroVector se o percurso é inválido. */
	static FVector LocationAt(TConstArrayView<FVector> Points, float ProgressCm);
};
