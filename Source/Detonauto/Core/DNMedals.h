// Medalhas: regras puras. Sem UObject, sem Actor, sem mundo.
// Contrato: docs/regras-de-jogo.md, seção 3, e a decisão M1. Testes: Core/Tests/DNMedals.spec.cpp.

#pragma once

#include "CoreMinimal.h"

enum class EDNMedal : uint8
{
	None,
	Bronze,
	Silver,
	Gold
};

/** Limites de um cruzamento. Precisam ser maiores que zero e estritamente crescentes (M1). */
struct FDNMedalThresholds
{
	int32 Bronze = 0;
	int32 Silver = 0;
	int32 Gold = 0;
};

struct FDNMedals
{
	/**
	 *  A medalha mais alta cujo limite a pontuação alcançou (maior ou igual). Abaixo do bronze, nenhuma.
	 *  Limites inválidos não dão medalha nenhuma.
	 */
	static EDNMedal Evaluate(const FDNMedalThresholds& Thresholds, int32 Score);

	/** Recusa limites que não sejam maiores que zero e estritamente crescentes. */
	static bool Validate(const FDNMedalThresholds& Thresholds, FString* OutError = nullptr);

	static const TCHAR* ToString(EDNMedal Medal);
};
