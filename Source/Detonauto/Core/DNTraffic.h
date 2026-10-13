// Tráfego simplificado: regras puras. Sem UObject, sem Actor, sem mundo, só CoreMinimal.
// Contrato: docs/regras-trafego.md e a seção "Decisões do lote 4". Testes: Core/Tests/DNTraffic.spec.cpp.
//
// Ideia: o carro de tráfego anda "nos trilhos" (Rail, sem física) e só vira físico (Physics) quando algo o atinge ou
// chega perto. Physics nunca volta a Rail. Despawn devolve o veículo ao pool e é final.

#pragma once

#include "CoreMinimal.h"

enum class EDNTrafficLevel : uint8
{
	Rail,
	Physics,
	Despawn
};

/** Os números do tráfego. Valores iniciais; moram em Data/TrafficSettings.csv. */
struct FDNTrafficSettings
{
	/** Veículo Physics a esta distância (inclusive) acorda o Rail [m]. */
	float WakeDistanceM = 6.0f;
	/** Jogador a menos desta distância (estrito), na frente, acorda o Rail [m]. */
	float WakeAheadM = 25.0f;
	/** Velocidade de aproximação mínima (inclusiva) para acordar pela regra da frente [km/h]. */
	float WakeAheadMinClosingKmh = 20.0f;
	/** Mais longe que isto (estrito) e atrás do jogador, o veículo some [m]. */
	float DespawnDistanceM = 200.0f;
	/** Physics acima desta velocidade (estrito) não some durante a rodada de Crash [m/s]. */
	float StillSpeedMs = 2.0f;
	/** Máximo de veículos criados por quadro. */
	int32 MaxSpawnPerFrame = 2;
};

/** O que NextLevel precisa saber de um veículo, calculado por quem chama. Os padrões são "nada acontecendo". */
struct FDNTrafficVehicleInput
{
	EDNTrafficLevel Level = EDNTrafficLevel::Rail;

	/** Força do impacto registrado no veículo neste quadro, de qualquer origem. Acima de 0 acorda. */
	float ImpactForce = 0.0f;

	/** Distância entre centros ao veículo Physics mais próximo (jogador, rival ou outro tráfego). Máximo = nenhum. */
	float NearestPhysicsDistanceM = TNumericLimits<float>::Max();

	/** Distância em linha reta entre centros até o jogador [m]. */
	float PlayerDistanceM = TNumericLimits<float>::Max();

	/** (posição do jogador - posição do veículo) projetada no eixo de avanço do VEÍCULO [m]. Maior que 0 = jogador na frente. */
	float PlayerProjectionOnVehicleForwardM = 0.0f;

	/** Velocidade de aproximação entre o veículo e o jogador [km/h]; positiva = se aproximando. Calculada por quem chama. */
	float ClosingSpeedKmh = 0.0f;

	/** (posição do veículo - posição do jogador) projetada no eixo de avanço do JOGADOR [m]. Menor que 0 = veículo atrás do jogador. */
	float VehicleProjectionOnPlayerForwardM = 0.0f;

	/** Velocidade escalar atual do veículo [m/s]. */
	float SpeedMs = 0.0f;

	bool bCrashRoundActive = false;
};

/** Resultado de um passo na trilha. */
struct FDNRailAdvance
{
	/** Nova distância ao longo da trilha. Em erro, a distância recebida. */
	float Distance = 0.0f;
	/** Sem loop: houve avanço e a distância chegou ao comprimento. Sempre falso com loop. */
	bool bReachedEnd = false;
	/** Entrada inválida: não moveu. */
	bool bError = false;
};

struct FDNTraffic
{
	/**
	 *  Próximo nível do veículo. Ordem (Q3): primeiro acorda (Rail para Physics), depois avalia o despawn sobre o nível
	 *  resultante.
	 *   - Despawn é final; Physics nunca volta a Rail.
	 *   - Rail acorda com: impacto > 0; veículo Physics a <= WakeDistanceM; ou jogador a < WakeAheadM em linha reta, na
	 *     frente (projeção > 0) e aproximando-se a >= WakeAheadMinClosingKmh.
	 *   - Some quem está a > DespawnDistanceM do jogador e atrás dele (projeção < 0), exceto Physics com velocidade
	 *     > StillSpeedMs (estrito) enquanto a rodada de Crash estiver ativa. Rail nunca é protegido.
	 *  NaN em qualquer comparação conta como "não" (não acorda, não some); velocidade NaN conta como em movimento.
	 */
	static EDNTrafficLevel NextLevel(const FDNTrafficSettings& Settings, const FDNTrafficVehicleInput& Vehicle);

	/**
	 *  Anda na trilha: Distance += Speed * Dt.
	 *   - Dt <= 0, Speed < 0 ou Speed == 0 não movem e não são erro (bReachedEnd falso);
	 *   - valores não finitos ou TrackLength <= 0: não move e informa erro;
	 *   - sem loop: a distância inicial precisa estar em [0, TrackLength] (senão é erro); satura em TrackLength e informa
	 *     bReachedEnd;
	 *   - com loop: aceita qualquer distância finita e normaliza por módulo; o resultado fica sempre em [0, TrackLength).
	 */
	static FDNRailAdvance AdvanceOnRail(float Distance, float Speed, float DeltaTime, float TrackLength, bool bLoop);

	/** Quantos veículos criar neste quadro: clamp(Target - Active, 0, MaxPerFrame), em int64, sem estourar. Negativos valem 0. */
	static int32 PlanSpawns(int32 Target, int32 Active, int32 MaxPerFrame);
};
