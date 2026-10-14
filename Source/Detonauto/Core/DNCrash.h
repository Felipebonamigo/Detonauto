// Pontuação do modo Crash: regras puras. Sem UObject, sem Actor, sem mundo.
// Contrato: docs/regras-de-jogo.md, seção 2, e a tabela "Decisões de implementação" (C1 a C5).
// Testes: Core/Tests/DNCrash.spec.cpp.
//
// O dinheiro é inteiro: a conta usa int64 sem ponto flutuante e trunca para baixo (C2).

#pragma once

#include "CoreMinimal.h"
#include "Core/DNScore.h"

/** Todos os números da pontuação e do fim da rodada. */
struct FDNCrashConfig
{
	/** O multiplicador vai de 1 até este valor. */
	int32 MaxMultiplier = 8;

	/** Janela da cadeia [s]: até este valor, inclusive, continua; acima reinicia. */
	float ChainWindowSeconds = 1.5f;
	/** Bônus por elo da cadeia, em porcentos do valor, e o teto do bônus. */
	int32 ChainBonusPercentPerLink = 10;
	int32 ChainBonusMaxPercent = 100;

	/** Um veículo "parado" tem velocidade menor ou igual a este valor [m/s]. */
	float QuietSpeedMetersPerSecond = 2.0f;
	/** Segundos seguidos sem nenhum veículo acima da velocidade acima para encerrar a rodada. */
	float QuietSecondsToEnd = 3.0f;
	/** A rodada encerra neste tempo depois do primeiro impacto [s]. */
	float MaxSecondsAfterFirstImpact = 30.0f;
};

struct FDNCrashState
{
	FDNScore Score;
	int32 Multiplier = 1;
	int32 DestroyedCount = 0;

	/** Elos da cadeia atual: o 1º veículo tem 0 (sem bônus), o 2º tem 1 etc. */
	int32 ChainLinks = 0;
	float SecondsSinceLastDestruction = 1.0e9f;

	/** Eventos da cadeia já registrados (impactos com dano e destruições, pela regra de pontos por dano). */
	int32 ChainEvents = 0;

	bool bFirstImpactHappened = false;
	float SecondsSinceFirstImpact = 0.0f;
	float QuietSeconds = 0.0f;
	bool bEnded = false;

	void Reset() { *this = FDNCrashState(); }
};

/** O livro de um veículo na pontuação por dano: quanto do valor já foi pago e se ele já foi destruído. Quem chama guarda (P5). */
struct FDNCrashVehicleLedger
{
	float PaidFraction = 0.0f;
	bool bDestroyed = false;
};

struct FDNCrash
{
	/**
	 *  Pontos de um veículo: valor com o bônus da cadeia, depois vezes o multiplicador (C1).
	 *  Inteiro de 64 bits, truncado para baixo (C2) e saturado em MAX_int32. Valor <= 0 vale 0.
	 */
	static int32 ComputePoints(const FDNCrashConfig& Config, int32 Value, int32 Multiplier, int32 ChainLinks);

	/** Registra um impacto. O primeiro impacto liga os relógios do fim da rodada (C3). */
	static void RegisterImpact(const FDNCrashConfig& Config, FDNCrashState& State);

	/**
	 *  Registra um veículo destruído e devolve os pontos somados.
	 *  Valor <= 0 é ignorado por inteiro (C5); depois do fim da rodada também (C4).
	 */
	static int32 RegisterDestruction(const FDNCrashConfig& Config, FDNCrashState& State, int32 Value);

	/**
	 *  Pontos por dano (revisão de 14 out 2026, lote 13c; decisões P1 a P5 em Core/Tests/DNCrash.spec.cpp). AddedDamageFraction é o
	 *  aumento do dano total do veículo (FDNDamage::TotalDamage, de 0 a 1) neste impacto; bDestroyed, se o impacto o destruiu.
	 *   - paga valor x fração (truncado), com a cadeia e o multiplicador do instante; destruir paga o que falta até 100%;
	 *   - a soma nunca passa de 100% e um veículo destruído não paga mais;
	 *   - impacto com dano (ou destruição) é elo da cadeia; sem dano, NaN ou negativo não faz nada;
	 *   - valor <= 0 e rodada encerrada são ignorados (C4, C5).
	 *  Devolve os pontos somados.
	 */
	static int32 RegisterDamage(const FDNCrashConfig& Config, FDNCrashState& State, FDNCrashVehicleLedger& Ledger, int32 Value,
		float AddedDamageFraction, bool bDestroyed);

	/** Multiplicador coletado no chão (+1, até o máximo). Ignorado depois do fim da rodada (C4). */
	static void CollectMultiplier(const FDNCrashConfig& Config, FDNCrashState& State);

	/** Avança o relógio. MaxVehicleSpeedMps é a maior velocidade entre os veículos no quadro. */
	static void Tick(const FDNCrashConfig& Config, FDNCrashState& State, float DeltaTime, float MaxVehicleSpeedMps);
};
