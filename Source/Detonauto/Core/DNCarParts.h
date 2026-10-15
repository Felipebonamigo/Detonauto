// Peças que soltam e vidros que quebram (lote 18b, camada 2 do dano): regras puras. Sem UObject, sem mundo. Decidem QUANDO uma
// peça pendura e solta e QUANDO um vidro trinca e estoura; como isso aparece (malhas, física, material) é do motor.
// Contrato: docs/regras-pecas.md. Testes: Core/Tests/DNCarParts.spec.cpp. Dados: Data/CarParts.csv e Data/CarGlass.csv.
//
// As duas regras leem o dano acumulado DEPOIS do impacto (FDNDamage::ApplyImpact já aplicado) e o próprio impacto (para o Δv).
// O dano de uma peça ou vidro é o MAIOR entre a zona e a zona secundária. O Δv conta quando o impacto acerta uma das duas zonas.
// Limiares inclusivos, como no regras-dano.md.
//
// Decisões de implementação (o contrato não cobre):
//  P1 impacto nulo, Δv não finito ou negativo, ou zona fora do enum: o Δv não conta; o dano acumulado conta (a ferramenta DNZone leva
//     a zona a um valor sem batida);
//  P2 o vetor de estágios é paralelo ao de peças; tamanho diferente é refeito com todas presas (não quebra);
//  P3 uma peça com dobradiça que passa dos dois limiares no mesmo impacto vai direto para Solta: a mudança sai uma vez, de Presa
//     para Solta;
//  P4 vidro com trinca a partir de 1,0 ou mais: trinca 0 abaixo de 1,0 e 1 em 1,0 (sem divisão por zero).

#pragma once

#include "CoreMinimal.h"
#include "Core/DNDamage.h"

/** Presa → Pendurada → Solta, sem volta. Peça sem dobradiça pula de Presa para Solta. */
enum class EDNPartStage : uint8
{
	Attached,
	Hanging,
	Detached
};

/** Uma peça do carro: uma linha de Data/CarParts.csv. */
struct FDNCarPartSpec
{
	EDNDamagePart Part = EDNDamagePart::Hood;
	EDNDamageZone Zone = EDNDamageZone::Front;
	bool bHasSecondaryZone = false;
	EDNDamageZone SecondaryZone = EDNDamageZone::Front;
	float MassKg = 8.0f;
	/** Pendura com o dano da peça >= HangDamage ou Δv >= HangDeltaVMs. Só vale com dobradiça. */
	float HangDamage = 0.3f;
	float HangDeltaVMs = 8.0f;
	/** Solta com o dano da peça >= DetachDamage ou Δv >= DetachDeltaVMs. */
	float DetachDamage = 0.6f;
	float DetachDeltaVMs = 12.0f;
	/** Giro máximo da dobradiça [graus]; 0 = sem dobradiça. */
	float HingeMaxDeg = 0.0f;
	/** Amortecimento angular da peça pendurada. */
	float HingeDamping = 2.0f;
	/** Empurrão na direção da batida quando solta [m/s]. */
	float LaunchMs = 4.0f;

	bool HasHinge() const { return HingeMaxDeg > 0.0f; }
};

/** Uma peça que mudou de estágio neste impacto. Index é a posição dela na lista de peças. */
struct FDNPartChange
{
	int32 Index = INDEX_NONE;
	EDNDamagePart Part = EDNDamagePart::Hood;
	EDNPartStage From = EDNPartStage::Attached;
	EDNPartStage To = EDNPartStage::Attached;
};

struct FDNCarParts
{
	/** O maior dano entre a zona e a zona secundária (sem secundária: o da zona). Zona fora do enum vale 0. */
	static float ZoneDamage(EDNDamageZone Zone, bool bHasSecondary, EDNDamageZone Secondary, const FDNDamageState& Damage);

	/** O Δv do impacto se ele acerta a zona ou a secundária e é válido (P1); senão 0. */
	static float DeltaVOn(EDNDamageZone Zone, bool bHasSecondary, EDNDamageZone Secondary, const FDNImpact* Impact);

	/** Avalia todas as peças, em ordem, depois de um impacto já aplicado ao dano. Devolve as mudanças deste impacto. */
	static TArray<FDNPartChange> Evaluate(TConstArrayView<FDNCarPartSpec> Specs, TArray<EDNPartStage>& Stages, const FDNDamageState& Damage,
		const FDNImpact* Impact);
};

/** Vidros e lentes. O para-brisa nunca estoura (vidro laminado): trinca e afunda com o teto. */
enum class EDNGlass : uint8
{
	Windshield,
	SideLeft,
	SideRight,
	Rear,
	HeadlightLeft,
	HeadlightRight,
	TaillightLeft,
	TaillightRight
};

/** Um vidro: uma linha de Data/CarGlass.csv. */
struct FDNGlassSpec
{
	EDNGlass Glass = EDNGlass::Windshield;
	EDNDamageZone Zone = EDNDamageZone::Front;
	bool bHasSecondaryZone = false;
	EDNDamageZone SecondaryZone = EDNDamageZone::Front;
	/** A trinca começa neste dano e chega a 1 em 1,0. */
	float CrackFrom = 0.3f;
	/** Estoura com o dano >= isto; 0 = nunca pelo dano. Farol e lanterna: a luz apaga. */
	float BurstDamage = 0.6f;
	/** Estoura com Δv >= isto numa das zonas; 0 = nunca pelo Δv. */
	float BurstDeltaVMs = 0.0f;

	bool CanBurst() const { return BurstDamage > 0.0f || BurstDeltaVMs > 0.0f; }
};

struct FDNGlass
{
	/** Força da trinca, de 0 a 1 (P4). */
	static float Crack(const FDNGlassSpec& Spec, const FDNDamageState& Damage);

	/** Avalia os vidros depois de um impacto; Burst é paralelo a Specs (P2). Devolve as posições dos que estouraram agora. */
	static TArray<int32> Evaluate(TConstArrayView<FDNGlassSpec> Specs, TArray<bool>& Burst, const FDNDamageState& Damage, const FDNImpact* Impact);
};
