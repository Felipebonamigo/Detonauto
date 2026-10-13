// Turbo: regras puras. Sem UObject, sem Actor, sem mundo.
// Contrato: docs/regras-de-jogo.md, seção 1, e a tabela "Decisões de implementação" (T1 a T7).
// Testes: Core/Tests/DNTurbo.spec.cpp.
//
// Todos os números moram em FDNTurboConfig (valores iniciais; depois migram para Data/*.csv).
// O tempo (DeltaTime) e as entradas do quadro entram como parâmetro.

#pragma once

#include "CoreMinimal.h"

/** Todos os números do turbo. Unidades: segundos, km/h, centímetros, graus e unidades de barra. */
struct FDNTurboConfig
{
	/** Capacidade da barra. */
	float MaxBar = 100.0f;

	// --- Ganho: contramão ---
	/** Velocidade mínima (estritamente acima) para ganhar na faixa contrária [km/h]. */
	float WrongWayMinSpeedKmh = 80.0f;
	float WrongWayGainPerSecond = 12.0f;

	// --- Ganho: passada raspando ---
	/** Distância lateral (estritamente abaixo) [cm]. */
	float NearMissMaxLateralCm = 150.0f;
	/** Velocidade mínima (estritamente acima) [km/h]. */
	float NearMissMinSpeedKmh = 60.0f;
	/** Ganho base por evento. */
	float NearMissGain = 6.0f;
	/** Janela da cadeia [s]: até este valor, inclusive, continua; acima reinicia. */
	float NearMissChainWindowSeconds = 2.0f;
	/** Multiplicador por posição na cadeia; além da última posição vale o último valor (T3). */
	TArray<float> NearMissChainMultipliers = { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

	// --- Ganho: derrapada ---
	/** Ângulo de deslizamento em valor absoluto, inclusivo nos dois extremos (T5) [graus]. */
	float DriftMinAngleDeg = 15.0f;
	float DriftMaxAngleDeg = 70.0f;
	/** Velocidade mínima (estritamente acima) [km/h]. */
	float DriftMinSpeedKmh = 60.0f;
	float DriftGainPerSecond = 8.0f;

	// --- Ganho: salto ---
	/** Tempo no ar antes de começar a ganhar [s]. */
	float AirTimeThresholdSeconds = 0.3f;
	float AirGainPerSecond = 10.0f;
	float AirMaxGainPerJump = 20.0f;

	// --- Takedown ---
	/** Valor a que a barra vai quando acontece um takedown. */
	float TakedownSetsBarTo = 100.0f;

	// --- Uso ---
	float ConsumptionPerSecond = 25.0f;

	// --- Turbo prolongado ---
	/** Unidades ganhas ao ligar com a barra cheia e segurar até 0 sem soltar. */
	float ExtendedBonus = 50.0f;
};

/** Passada raspando detectada no quadro. */
struct FDNNearMissEvent
{
	int32 VehicleId = 0;
	float LateralDistanceCm = 0.0f;
	bool bVehicleMoving = true;
};

/** Estado do turbo. Tem reinício explícito (Reset); T2: os veículos já contados valem até o Reset. */
struct FDNTurboState
{
	float Bar = 0.0f;
	bool bActive = false;

	/** T6: depois de zerar com o botão apertado, só um novo aperto liga de novo. */
	bool bNeedsNewPress = false;

	// cadeia de passadas raspando
	int32 NearMissChainLength = 0;
	float SecondsSinceNearMiss = 1.0e9f;
	TSet<int32> NearMissVehiclesCounted;

	// salto
	float AirTimeSeconds = 0.0f;
	float AirGainThisJump = 0.0f;

	// turbo prolongado
	bool bStartedAtFull = false;
	bool bExtendedAwarded = false;

	void Reset() { *this = FDNTurboState(); }
};

/** Entradas de um quadro. */
struct FDNTurboFrameInput
{
	float DeltaTime = 0.0f;
	bool bButtonHeld = false;
	float SpeedKmh = 0.0f;
	bool bWrongWay = false;
	/** Ângulo de deslizamento [graus]. O sinal é ignorado: vale o valor absoluto. */
	float DriftAngleDeg = 0.0f;
	bool bAirborne = false;
	bool bTakedown = false;
	TArray<FDNNearMissEvent> NearMisses;
};

struct FDNTurbo
{
	/** Ganho de contramão no quadro. */
	static float WrongWayGain(const FDNTurboConfig& Config, float SpeedKmh, bool bWrongWay, float DeltaTime);

	/** Ganho de derrapada no quadro. */
	static float DriftGain(const FDNTurboConfig& Config, float DriftAngleDeg, float SpeedKmh, float DeltaTime);

	/**
	 *  Ganho de uma passada raspando. Atualiza a cadeia e a lista de veículos já contados.
	 *  Devolve 0 (e não mexe na cadeia) se as condições não valem.
	 */
	static float NearMissGain(const FDNTurboConfig& Config, FDNTurboState& State, const FDNNearMissEvent& Event, float SpeedKmh);

	/** Ganho de salto no quadro. Atualiza o tempo no ar e o teto por salto. */
	static float AirGain(const FDNTurboConfig& Config, FDNTurboState& State, bool bAirborne, float DeltaTime);

	/** Takedown: a barra vai ao valor configurado. */
	static void ApplyTakedown(const FDNTurboConfig& Config, FDNTurboState& State);

	/** Um quadro: ganhos (só com o turbo desligado), takedown, ligar, consumo, desligar e turbo prolongado. */
	static void Tick(const FDNTurboConfig& Config, FDNTurboState& State, const FDNTurboFrameInput& Input);
};
