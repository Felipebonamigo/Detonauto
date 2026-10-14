// Peso dos alvos de deformação por zona (lote 17, dano visual camada 1): regra pura, sem mundo e sem malha.
// Contrato: docs/regras-dano-visual.md, "Regra pura". Testes: Core/Tests/DNDamageMorph.spec.cpp (decisões M1 a M5 lá).
//
// Cada zona com alvo (Front, Rear, Left, Right, Roof; a Under não tem) mistura o seu alvo de deformação pelo dano da zona, com curva
// (MorphCurveExponent) e subida limitada (MorphRisePerSecond), sem nunca voltar. Quem chama guarda os pesos entre quadros e os aplica
// na malha (ADNVehicle).

#pragma once

#include "CoreMinimal.h"
#include "Core/DNDamage.h"

struct FDNDamageMorph
{
	/** Zonas com alvo: as 5 primeiras de EDNDamageZone, na mesma ordem. */
	static constexpr int32 ZoneCount = 5;

	/** Nome do alvo de deformação da zona (o mesmo do script do Blender, Tools/Blender/make_damage_morphs.py). */
	static const TCHAR* TargetName(int32 Zone);

	/** Peso novo de um alvo (M1 a M4): sobe até pow(dano, expoente), no máximo MorphRisePerSecond x Dt, e nunca desce. */
	static float Weight(const FDNDamageConfig& Config, float ZoneDamage, float Previous, float DeltaTime);

	/** Os 5 pesos de uma vez, a partir do estado do dano (M5). InOutWeights entra com os pesos do quadro anterior. */
	static void Weights(const FDNDamageConfig& Config, const FDNDamageState& State, float (&InOutWeights)[ZoneCount], float DeltaTime);
};
