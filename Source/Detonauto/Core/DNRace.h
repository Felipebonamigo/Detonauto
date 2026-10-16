// Corrida ponto a ponto (docs/fatia-jogavel.md, modo 1): regras puras de progresso, chegada e posição. Sem UObject, sem mundo: o
// percurso (a linha central da rua) é do mapa; aqui só entram os números. Testes: Core/Tests/DNRace.spec.cpp.
//
// Decisões (o contrato da fatia não cobre):
//  R1 o progresso de cada corredor é a distância andada ao longo do percurso [cm] e nunca diminui (ré e giro não contam contra);
//  R2 progresso não finito ou negativo é ignorado (o anterior vale);
//  R3 chegou quando o progresso alcança o comprimento do percurso (inclusivo); o tempo da chegada é o do quadro em que passou, gravado uma
//     vez; comprimento <= 0 ou não finito: ninguém chega;
//  R4 a ordem: quem chegou vem antes de quem não chegou, por tempo de chegada (menor primeiro); quem não chegou, por progresso (maior
//     primeiro); empate fica com o índice menor (ordem estável);
//  R5 posição = lugar na ordem, começando em 1.

#pragma once

#include "CoreMinimal.h"

/** Um corredor (o jogador ou um rival). */
struct FDNRacerState
{
	float ProgressCm = 0.0f;
	bool bFinished = false;
	/** Tempo de corrida em que chegou [s]; só vale com bFinished. */
	float FinishTimeS = 0.0f;
};

struct FDNRace
{
	/** R1 a R3: leva o corredor ao novo progresso, no tempo TimeS de corrida; devolve verdadeiro se ele acabou de chegar. */
	static bool Step(FDNRacerState& Racer, float NewProgressCm, float TimeS, float CourseLengthCm);

	/** R4: os índices dos corredores, do primeiro ao último. */
	static TArray<int32> Order(TConstArrayView<FDNRacerState> Racers);

	/** R5: a posição (1 a N) do corredor Index; 0 se o índice não existe. */
	static int32 Position(TConstArrayView<FDNRacerState> Racers, int32 Index);
};
