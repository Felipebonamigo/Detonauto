// Dano: regras puras. Sem UObject, sem Actor, sem mundo. Decide QUANTO dano um impacto causa e O QUE se solta; como
// isso aparece na tela (amassado no material, malha, peças físicas) é outro lote.
// Contrato: docs/regras-dano.md. Testes: Core/Tests/DNDamage.spec.cpp.
//
// Todos os números moram em FDNDamageConfig (valores iniciais; o CSV é Data/Damage.csv e um teste de deriva garante que
// os dois são iguais).
//
// Decisões de implementação (o contrato não cobre):
//  D1 no limiar exato (DeltaVMs == DentMinDeltaVMs) o impacto causa dano de valor 0 e um amassado de profundidade 0
//     (o contrato diz "gera um registro" e "2,0 causa");
//  D2 AddedZoneDamage é o aumento REAL da zona, depois da saturação em 1,0;
//  D3 a fusão compara a distância com a metade do raio do amassado EXISTENTE (inclusive); havendo vários ao alcance,
//     funde no mais próximo (empate: o mais antigo). Depois de fundir não se procura outra fusão;
//  D4 posição não finita: o amassado é ignorado, o dano segue;
//  D5 zona fora do enum: o impacto é ignorado;
//  D6 as peças são avaliadas em todas as zonas: a regra do dano acumulado vale para as peças de qualquer zona;
//  D7 configuração inválida não quebra: valores <= 0 que viram divisor são trocados por um mínimo minúsculo, profundidades
//     e raios ficam em >= 0 e as contagens em pelo menos 1 (quem recusa é o validador do CSV);
//  D8 FDNDamageResult::Dent é o amassado NOVO como calculado, antes de uma eventual fusão;
//  D12 destruído: total >= DestroyedTotalDamage OU qualquer zona entre Front, Rear, Left, Right e Roof em 1,0 (a Under não
//      conta). A fração 0,5 da fusão, o 1,0 da zona e as 5 zonas ficam fixos no código (DNDamage.cpp), comentados;
//  D9 com o limite cheio e sem fusão, o novo substitui o mais raso mesmo que seja mais raso que ele (o contrato é literal).

#pragma once

#include "CoreMinimal.h"

enum class EDNDamageZone : uint8
{
	Front,
	Rear,
	Left,
	Right,
	Roof,
	Under
};

enum class EDNDamagePart : uint8
{
	Hood,
	Trunk,
	DoorLeft,
	DoorRight,
	BumperFront,
	BumperRear,
	GlassFront
};

/** Todos os números do dano. Unidades: m/s (variação de velocidade), cm, e frações de 0 a 1 de dano. */
struct FDNDamageConfig
{
	/** Abaixo disto (estrito) o impacto não causa dano algum. Igual ou acima causa [m/s]. */
	float DentMinDeltaVMs = 2.0f;
	/** Variação de velocidade acima do limiar que satura sozinha uma zona em 1,0 [m/s]. */
	float ZoneDestroyDeltaVMs = 30.0f;
	/** Profundidade máxima de um amassado [cm]. */
	float DentMaxDepthCm = 25.0f;
	/** Profundidade por m/s acima do limiar [cm por m/s]. */
	float DentDepthPerMs = 1.5f;
	/** Raio de um amassado sem profundidade [cm]; o raio real soma a profundidade. */
	float DentBaseRadiusCm = 30.0f;
	/** Máximo de amassados guardados por veículo. */
	int32 MaxDents = 8;
	/** Variação de velocidade que solta as peças da zona atingida (inclusive) [m/s]. */
	float PartDetachDeltaVMs = 12.0f;
	/** Dano acumulado da zona que solta as peças dela (inclusive), de 0 a 1. */
	float PartDetachZoneDamage = 0.6f;
	/** Dano total (média das 5 zonas) que destrói o veículo (inclusive), de 0 a 1. */
	float DestroyedTotalDamage = 0.5f;
	/** Curva do peso dos alvos de deformação: peso = dano^expoente (lote 17; < 1 = o amassado aparece cedo e satura devagar). */
	float MorphCurveExponent = 0.7f;
	/** Subida máxima do peso dos alvos de deformação por segundo (10 = 0,1 s até o cheio; o metal não "cresce" devagar). */
	float MorphRisePerSecond = 10.0f;
	/** Desgaste da pintura (lote 17b, no material, pelo peso da zona): fração da superfície com tinta arrancada (primer e, no meio
	 *  das lascas, metal nu) onde a zona amassa mais, com ela totalmente amassada. 0,5 parecia camuflagem na captura do 17b. */
	float WearChipCoverage = 0.2f;
	/** Tamanho típico de uma lasca de tinta [cm]. */
	float WearChipSizeCm = 6.0f;
	/** Força dos riscos ao longo do carro (0 = nenhum, 1 = máximo). */
	float WearScratch = 0.6f;
	/** Escurecimento por fuligem e sujeira numa zona totalmente amassada (0 = nenhum, 1 = máximo). */
	float WearSoot = 0.5f;
	/** Máximo de peças soltas ativas ao mesmo tempo (corpos físicos). */
	int32 MaxActive = 24;
};

/** Um impacto. DeltaVMs: variação de velocidade causada pelo impacto (impulso normal / massa), >= 0. */
struct FDNImpact
{
	float DeltaVMs = 0.0f;
	EDNDamageZone Zone = EDNDamageZone::Front;
	/** Ponto no espaço do carro [cm]. */
	FVector LocalPositionCm = FVector::ZeroVector;
};

/** Um amassado guardado. */
struct FDNDent
{
	FVector LocalPositionCm = FVector::ZeroVector;
	float DepthCm = 0.0f;
	float RadiusCm = 0.0f;
};

/** Estado de dano de um veículo. Tem reinício explícito (Reset). */
struct FDNDamageState
{
	static constexpr int32 ZoneCount = 6;
	static constexpr int32 PartCount = 7;
	/** As 5 primeiras zonas do enum (Front a Roof) entram no total; a Under não. */
	static constexpr int32 TotalZoneCount = 5;

	/** Dano acumulado por zona, de 0 a 1, indexado por EDNDamageZone. */
	float ZoneDamage[ZoneCount] = {};
	/** Peças já soltas, indexado por EDNDamagePart. */
	bool bPartDetached[PartCount] = {};
	/** Amassados na ordem de criação (o mais antigo primeiro). */
	TArray<FDNDent> Dents;
	/** Permanente depois de verdadeiro. */
	bool bDestroyed = false;

	void Reset() { *this = FDNDamageState(); }
};

/** O que um impacto causou. */
struct FDNDamageResult
{
	/** Aumento real do dano da zona atingida (depois da saturação em 1,0). */
	float AddedZoneDamage = 0.0f;
	bool bDentAdded = false;
	/** O amassado novo, como calculado (D8). Só vale com bDentAdded. */
	FDNDent Dent;
	/** Peças soltas por ESTE impacto, em ordem do enum. */
	TArray<EDNDamagePart> DetachedParts;
	/** Verdadeiro só no impacto que destruiu o veículo (uma vez). */
	bool bJustDestroyed = false;
};

struct FDNDamage
{
	/** Zona a que cada peça pertence. */
	static EDNDamageZone ZoneOfPart(EDNDamagePart Part);

	/** Média do dano das 5 zonas Front, Rear, Left, Right e Roof (Under não conta). */
	static float TotalDamage(const FDNDamageState& State);

	/** Aplica um impacto ao estado. Entrada inválida (NaN, infinito, negativo, zona fora do enum) é ignorada. */
	static FDNDamageResult ApplyImpact(const FDNDamageConfig& Config, FDNDamageState& State, const FDNImpact& Impact);
};

/** Quantas peças novas viram corpo físico e quantas das mais antigas são recicladas. */
struct FDNPartPoolPlan
{
	int32 Accepted = 0;
	int32 Recycle = 0;
};

struct FDNPartPool
{
	/**
	 *  Se Active + Requested > MaxActive, recicla Active + Requested - MaxActive (as mais antigas), nunca mais do que
	 *  Active; se Requested > MaxActive aceita só MaxActive. Entradas negativas valem 0. Sem estouro de inteiro.
	 */
	static FDNPartPoolPlan Plan(int32 MaxActive, int32 Active, int32 Requested);
};
