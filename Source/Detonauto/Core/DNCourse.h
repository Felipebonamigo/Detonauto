// Percurso da corrida ponto a ponto (docs/fatia-jogavel.md): a linha central da rua como lista de pontos. Regras puras, sem UObject:
// comprimento e progresso de um ponto do mundo ao longo da linha. Testes: Core/Tests/DNCourse.spec.cpp.
//
// Decisões:
//  C1 o comprimento é a soma dos trechos [cm]; menos de 2 pontos ou ponto não finito: comprimento 0;
//  C2 o progresso de uma posição é o arco até a projeção dela no trecho mais próximo, dentro de uma janela em volta do progresso anterior
//     (Hint - Back .. Hint + Ahead): impede atalho entre ruas que se cruzam ou se aproximam;
//  C3 posição não finita, ou percurso inválido: devolve o Hint (o progresso anterior vale);
//  C5 as marcas (largada, pontos de verificação, chegada) ficam a cada SpacingCm a partir da largada, e a última é sempre a chegada (o
//     comprimento); espaçamento <= 0 ou não finito, ou comprimento <= 0: só a chegada se o comprimento é válido, senão nada;
//  C6 o erro de rumo é o ângulo [graus, -180 a 180] entre a frente do carro e a direção ao alvo, no plano horizontal; positivo = o
//     alvo está do lado +Y da frente, que no Unreal (X à frente, Y à direita) é a DIREITA, o mesmo sinal do esterço; vetor ou alvo
//     degenerado: 0.
//  C4 a projeção fica presa nas pontas: antes do início é 0, depois do fim é o comprimento.

#pragma once

#include "CoreMinimal.h"

struct FDNCourse
{
	/** C1: comprimento total da linha [cm]. */
	static float Length(TConstArrayView<FVector> Points);

	/** C2 a C4: progresso [cm] de Position, procurando só entre Hint - BackCm e Hint + AheadCm. */
	static float Progress(TConstArrayView<FVector> Points, const FVector& Position, float HintCm, float BackCm, float AheadCm);

	/** C5: os progressos das marcas [cm], em ordem crescente; a última é a chegada. */
	static TArray<float> Marks(float LengthCm, float SpacingCm);

	/** C6: o quanto o carro precisa virar para apontar para Target [graus]; positivo = para a direita (+Y), como o esterço. */
	static float HeadingErrorDeg(const FVector& Position, const FVector& Forward, const FVector& Target);

	/** O ponto da linha no progresso dado (preso em 0 .. comprimento); ZeroVector se o percurso é inválido. */
	static FVector LocationAt(TConstArrayView<FVector> Points, float ProgressCm);
};
