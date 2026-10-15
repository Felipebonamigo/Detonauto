// Efeitos de batida (lote 19, docs/regras-efeitos.md): regras puras. Decidem SE um efeito dispara e QUANTO (partículas, taxa,
// intensidade); o Niagara só desenha. Sem UObject, sem mundo. Testes: Core/Tests/DNEffects.spec.cpp. Dados: Data/Effects.csv (uma
// linha por efeito) e Data/EffectsSettings.csv (os tetos globais).
//
// Limiares inclusivos (">= limiar dispara"), como no regras-dano.md. Entrada não finita (NaN, infinito) não dispara nada.
//
// Decisões de implementação (o contrato não cobre):
//  E1 colunas genéricas por efeito (TriggerMin, TriggerFull, MinSpeedKmh, CountMin, CountMax, CountPerUnit, Extra); o que cada uma
//     significa em cada efeito está na tabela de FDNEffectSpec. Os tetos globais (2000 partículas de GPU, 400 sprites de fumaça, a
//     distância da câmera) ficam numa linha própria, EffectsSettings.csv, como o BreakableSettings.csv do mobiliário;
//  E2 faísca de impacto: SparkPerMs = 6 partículas por m/s de Δv (10 a 120 entre ~1,7 e 20 m/s; o limiar de 4 m/s já dá 24);
//  E3 faísca de raspão: 15 partículas/s por m/s de velocidade relativa, no máximo 300/s;
//  E4 cacos: 250 por m² de vidro, entre 60 e 150 (lente de farol ~0,05 m² -> 60; lateral ~0,35 -> 88; traseiro ~0,5 -> 125); área não
//     finita ou <= 0 -> o mínimo;
//  E5 lascas de plástico: de 10 em Δv 12 a 30 em Δv 30, linear;
//  E6 fumaça de pneu: taxa = deslizamento / 25 graus (até 1) a partir de 8 graus; roda patinando garante 0,6; abaixo de 20 km/h, nada;
//  E7 fumaça do motor: intensidade de 0 (dano 0,6) a 1 (dano 0,8) e fica 1 (escura) daí em diante. Fogo em 0,9, ou com o carro
//     destruído e a frente em 0,7 ou mais;
//  E8 orçamento: um disparo pede N partículas; recebe o que cabe até o teto (FitBudget); nunca negativo.

#pragma once

#include "CoreMinimal.h"

/** Os efeitos (uma linha de Effects.csv cada). */
enum class EDNEffect : uint8
{
	SparkImpact,   // faísca de impacto
	SparkScrape,   // faísca de raspão (OnPartScrape, carroceria raspando)
	GlassShards,   // cacos de vidro (substitui o ADNGlassShards)
	PlasticChips,  // lascas de plástico
	TireSmoke,     // fumaça de pneu, por roda
	EngineSmoke,   // fumaça do motor
	EngineFire,    // fogo no vão do motor
	PropDust,      // pó do mobiliário quebrando
	PropLeaves     // folhas da árvore quebrando
};

/**
 * Um efeito: uma linha de Data/Effects.csv. O que cada coluna quer dizer depende do efeito:
 *
 *  | efeito       | TriggerMin          | TriggerFull          | MinSpeedKmh | CountMin..CountMax       | CountPerUnit         | Extra              |
 *  |--------------|---------------------|----------------------|-------------|--------------------------|----------------------|--------------------|
 *  | SparkImpact  | Δv mínimo [m/s]     | -                    | -           | partículas               | partículas por m/s   | -                  |
 *  | SparkScrape  | velocidade mín [m/s]| -                    | -           | - .. taxa máxima [1/s]   | (1/s) por m/s        | -                  |
 *  | GlassShards  | -                   | -                    | -           | cubos                    | cubos por m² de vidro| brilhos            |
 *  | PlasticChips | Δv mínimo [m/s]     | Δv do máximo [m/s]   | -           | pedaços                  | -                    | -                  |
 *  | TireSmoke    | deslizamento [graus]| deslizamento da taxa 1| velocidade | -                        | -                    | taxa da roda patinando |
 *  | EngineSmoke  | dano da clara       | dano da escura       | -           | -                        | -                    | -                  |
 *  | EngineFire   | dano do fogo        | -                    | -           | -                        | -                    | dano com o carro destruído |
 *  | PropDust/Leaves | -                | -                    | -           | partículas               | -                    | -                  |
 *
 * Vida, velocidade e o teto de sistemas ativos (PoolMax) vão para o Niagara e para o pool do motor.
 */
struct FDNEffectSpec
{
	EDNEffect Effect = EDNEffect::SparkImpact;
	float TriggerMin = 0.0f;
	float TriggerFull = 0.0f;
	float MinSpeedKmh = 0.0f;
	int32 CountMin = 0;
	int32 CountMax = 0;
	float CountPerUnit = 0.0f;
	float Extra = 0.0f;
	float LifeMinS = 0.5f;
	float LifeMaxS = 1.0f;
	float SpeedMinMs = 0.0f;
	float SpeedMaxMs = 0.0f;
	/** Teto de sistemas deste efeito ativos ao mesmo tempo (o pool do motor reaproveita o mais antigo). */
	int32 PoolMax = 4;
	/** Mobiliário: os tipos de Breakables.csv que disparam este efeito (vazio nos outros). */
	TArray<FString> Props;
};

/** Os tetos globais (a linha de EffectsSettings.csv). */
struct FDNEffectsSettings
{
	/** Faísca, cacos, lascas, pó e folhas somados, em GPU. */
	int32 MaxGpuParticles = 2000;
	/** Fumaça (pneu e motor) somada, em CPU. */
	int32 MaxSmokeSprites = 400;
	/** A fumaça some suave de FadeFar até FadeNear da câmera [cm]: a câmera de batida passa por dentro sem encher a tela. */
	float SmokeFadeNearCm = 150.0f;
	float SmokeFadeFarCm = 300.0f;
};

/** Fumaça e fogo do motor num instante. */
struct FDNEngineSmoke
{
	/** 0 sem fumaça; sobe de 0 a 1 entre a clara e a escura. */
	float Intensity = 0.0f;
	bool bSmoke = false;
	/** Fumaça escura e grossa (dano da escura ou mais). */
	bool bDark = false;
	bool bFire = false;
};

struct FDNEffects
{
	/** Nome em texto (o do enum, igual ao do CSV) e a leitura, sem diferenciar a caixa. Falso, sem mexer em Out, se não é nenhum. */
	static const TCHAR* EffectName(EDNEffect Effect);
	static bool ParseEffect(const FString& Text, EDNEffect& Out);

	/** E2: partículas da faísca de impacto; 0 abaixo do limiar. */
	static int32 ImpactSparks(const FDNEffectSpec& Spec, float DeltaVMs);

	/** E3: taxa da faísca de raspão [partículas/s]; 0 abaixo da velocidade mínima. */
	static float ScrapeSparkRate(const FDNEffectSpec& Spec, float SpeedMs);

	/** E4: cubos de um vidro que estoura, pela área dele [m²]. */
	static int32 GlassShards(const FDNEffectSpec& Spec, float AreaM2);

	/** E5: lascas de plástico; 0 abaixo do limiar. */
	static int32 PlasticChips(const FDNEffectSpec& Spec, float DeltaVMs);

	/** E6: taxa da fumaça de uma roda, de 0 a 1. SlipDeg com sinal (vale o módulo). */
	static float TireSmokeRate(const FDNEffectSpec& Spec, float SlipDeg, float SpeedKmh, bool bWheelSpin);

	/** E7: fumaça e fogo do motor pelo dano da frente (0 a 1). */
	static FDNEngineSmoke EngineSmoke(const FDNEffectSpec& Smoke, const FDNEffectSpec& Fire, float FrontDamage, bool bDestroyed);

	/** E8: quantas das Requested partículas cabem no teto, com Active já em uso. */
	static int32 FitBudget(int32 Requested, int32 Active, int32 Max);
};
