// Textos da corrida (marco 1d de docs/fatia-jogavel.md): tempo e posição como o HUD e a tela de resultado mostram. Regras puras, sem
// UObject. Testes: Core/Tests/DNRaceFormat.spec.cpp.
//
// Decisões:
//  F1 tempo em "m:ss.d" (minutos sem zero à esquerda, segundos com dois dígitos, uma casa decimal), truncado, não arredondado: o décimo
//     que o relógio ainda não completou não aparece;
//  F2 tempo negativo ou não finito vira "--:--.-"; a partir de 100 minutos o tempo fica em "99:59.9";
//  F3 posição em "1º", "2º"...; zero, negativa ou acima de 99 vira "-".

#pragma once

#include "CoreMinimal.h"

struct FDNRaceFormat
{
	/** F1 e F2. */
	static FString Time(float Seconds);

	/** F3. */
	static FString Place(int32 Position);
};
