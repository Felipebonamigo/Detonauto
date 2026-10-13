// Validadores dos CSVs de Data/. Funções puras: recebem as linhas já lidas e devolvem a lista de erros em português
// (vazia = tudo certo). Cada validador acumula todos os erros em vez de parar no primeiro.
// Testes: Core/Tests/DNDataValidation.spec.cpp (dados em memória) e DNDataFiles.spec.cpp (os arquivos reais).
//
// Regras:
//  - ids não vazios, só com letras sem acento, números, hífen e sublinhado, e únicos sem diferenciar maiúsculas de
//    minúsculas (dois ids que só diferem na caixa são o mesmo id, como no FName);
//  - integridade das referências (cidade para região; ponto turístico e cruzamento para cidade), ignorando a caixa;
//  - latitude em [-90, 90] e longitude em [-180, 180];
//  - valores maiores que zero onde faz sentido (ordem, valor em dinheiro, massa, limites das medalhas);
//  - limites de medalha válidos (FDNMedals::Validate);
//  - Order sem repetição dentro do grupo (global nas regiões, por região nas cidades, por cidade nos cruzamentos);
//  - cidade jogável precisa de distrito;
//  - ponto turístico: texto só sem revisão pendente e sempre com fonte (regra do CLAUDE.md sobre textos de cidades).

#pragma once

#include "CoreMinimal.h"
#include "Core/Data/DNDataRows.h"
#include "Core/Data/DNDataLoader.h"

struct FDNDataValidation
{
	static TArray<FString> ValidateRegions(const TArray<FDNRegionEntry>& Regions);
	static TArray<FString> ValidateCities(const TArray<FDNCityEntry>& Cities, const TArray<FDNRegionEntry>& Regions);
	static TArray<FString> ValidateLandmarks(const TArray<FDNLandmarkEntry>& Landmarks, const TArray<FDNCityEntry>& Cities);
	static TArray<FString> ValidateCrossings(const TArray<FDNCrossingEntry>& Crossings, const TArray<FDNCityEntry>& Cities);
	static TArray<FString> ValidateTrafficVehicles(const TArray<FDNTrafficVehicleEntry>& Vehicles);
	/** Exatamente uma linha; todos os valores finitos e maiores que zero; WakeDistanceM < WakeAheadM < DespawnDistanceM. */
	static TArray<FString> ValidateTrafficSettings(const TArray<FDNTrafficSettingsEntry>& Settings);

	/** Todas as tabelas, com as referências entre elas. */
	static TArray<FString> ValidateAll(const FDNData& Data);

	/** O id só tem letras sem acento, números, hífen e sublinhado (e não é vazio). */
	static bool IsSafeId(const FString& Id);
};
