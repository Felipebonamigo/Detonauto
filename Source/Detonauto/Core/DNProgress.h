// Progresso do menu de cidades. Regra pura, sem UObject, sem Actor, sem mundo.
// Contrato: docs/menu-mapa.md, "Regras de progresso". Testes: Core/Tests/DNProgress.spec.cpp.
//
// Regras do contrato:
//  - um cruzamento está FEITO com medalha de bronze ou melhor;
//  - os contadores são feitos sobre o total, com o número de ouros, por cidade, por região e no geral;
//  - uma cidade com Playable = false conta no mapa, mas não entra no total jogável.
//
// Decisões de implementação (o documento não cobre):
//  P1 a lista de entrada traz TODOS os cruzamentos do jogo, cada um com a medalha do jogador (None se ainda não
//     jogou); o total de uma cidade é o número de cruzamentos dela;
//  P2 id de cruzamento repetido: vale o primeiro, o resto vira aviso;
//  P3 cruzamento de cidade desconhecida: ignorado, com aviso. Nunca quebra;
//  P4 cidade não jogável: aparece com 0/0 e 0 ouros; os cruzamentos dela ficam fora do total, sem aviso;
//  P5 id de cidade repetido na tabela: vale o primeiro, o resto vira aviso. Id de cidade ou de região vazio:
//     a linha é ignorada, com aviso.

#pragma once

#include "CoreMinimal.h"
#include "Core/DNMedals.h"

/** Um cruzamento do jogo com a melhor medalha do jogador nele. */
struct FDNCrossingProgress
{
	FName CrossingId;
	FName CityId;
	EDNMedal Medal = EDNMedal::None;
};

/** Uma linha da tabela cidade para região. */
struct FDNCityRegion
{
	FName CityId;
	FName RegionId;
	bool bPlayable = true;
};

struct FDNProgressCount
{
	/** Cruzamentos feitos (bronze ou melhor). */
	int32 Done = 0;
	/** Cruzamentos jogáveis. */
	int32 Total = 0;
	/** Cruzamentos com medalha de ouro. */
	int32 Gold = 0;
};

struct FDNCityProgress
{
	FName RegionId;
	bool bPlayable = false;
	FDNProgressCount Count;
};

struct FDNProgressResult
{
	TMap<FName, FDNCityProgress> ByCity;
	TMap<FName, FDNProgressCount> ByRegion;
	FDNProgressCount Overall;
	/** Dados inconsistentes que foram ignorados, em português, na ordem da entrada. */
	TArray<FString> Warnings;
};

struct FDNProgress
{
	/** Feito = bronze ou melhor. */
	static bool IsDone(EDNMedal Medal) { return Medal != EDNMedal::None; }

	static FDNProgressResult Compute(const TArray<FDNCrossingProgress>& Crossings, const TArray<FDNCityRegion>& Cities);
};
