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

	/** Exatamente uma linha; finitos; limites e janelas > 0, ganhos e velocidades >= 0; Drift 0 <= min < max <= 90; TakedownSetsBarTo em (0, MaxBar]; multiplicadores da cadeia não vazios e > 0. */
	static TArray<FString> ValidateTurbo(const TArray<FDNTurboConfigEntry>& Rows);
	/** Exatamente uma linha; MaxMultiplier >= 1; percentuais >= 0; janelas e tempos > 0; QuietSecondsToEnd <= MaxSecondsAfterFirstImpact; velocidade de repouso >= 0; finitos. */
	static TArray<FString> ValidateCrash(const TArray<FDNCrashConfigEntry>& Rows);
	/** Exatamente uma linha; finitos; janela e altura > 0; ângulos em (0, 180]; SideMax <= TBoneMin; T: 0 <= min < max <= 180; ZoneHalfAngle em (0, 90). */
	static TArray<FString> ValidateTakedown(const TArray<FDNTakedownConfigEntry>& Rows);

	/** Exatamente uma linha; finitos; limites e fatores > 0; PartDetachZoneDamage e DestroyedTotalDamage em (0, 1]; MaxDents e MaxActive inteiros >= 1. */
	static TArray<FString> ValidateDamage(const TArray<FDNDamageConfigEntry>& Rows);

	/**
	 *  Uma ou mais linhas (presets); a linha "Padrao" é obrigatória. Cada campo tem uma faixa fechada (ver DNDataValidation.cpp);
	 *  RearGripFactor x HandbrakeRearGrip >= 0,295 (abaixo disso o carro roda com o freio de mão: docs/presets-carro.md).
	 */
	static TArray<FString> ValidateCars(const TArray<FDNCarConfigEntry>& Rows);

	/** Todas as tabelas, com as referências entre elas. */
	static TArray<FString> ValidateAll(const FDNData& Data);

	/** O id só tem letras sem acento, números, hífen e sublinhado (e não é vazio). */
	static bool IsSafeId(const FString& Id);
};
