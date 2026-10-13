// Pontuação saturante. Regra pura, sem dependência do mundo do jogo.
// Contrato: docs/regras-de-jogo.md, seção 2. Testes: Core/Tests/DNScore.spec.cpp.

#pragma once

#include "CoreMinimal.h"

/**
 *  Pontuação acumulada.
 *  A pontuação só sobe: valores menores ou iguais a zero são ignorados. O total satura em
 *  MAX_int32 em vez de dar a volta.
 */
struct FDNScore
{
	int32 Total = 0;

	/** Soma Amount ao total e devolve o novo total. */
	int32 Add(int32 Amount)
	{
		if (Amount <= 0)
		{
			return Total;
		}

		Total = (Amount > MAX_int32 - Total) ? MAX_int32 : Total + Amount;
		return Total;
	}

	void Reset()
	{
		Total = 0;
	}
};
