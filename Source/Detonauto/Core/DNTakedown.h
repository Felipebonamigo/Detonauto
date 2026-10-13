// Takedown: qualificação e classificação. Regras puras, sem UObject, sem Actor, sem mundo.
// Contrato: docs/regras-de-jogo.md, seção 4 e a "Convenção de contato"; decisões K1 a K3.
// Testes: Core/Tests/DNTakedown.spec.cpp.
//
// Convenção (K1): a normal do contato é um vetor unitário no mundo que aponta do JOGADOR para o RIVAL.
// Com V a frente do rival: zona traseira = dot(normal, V) >= cos 45; frontal = dot(normal, V) <= -cos 45;
// lateral = o resto. O ângulo entre A (frente do jogador) e V é acos(dot(A, V)), de 0 a 180 graus.
//
// Os limites de ângulo são comparados pelo produto escalar, que é equivalente a comparar o ângulo (o
// cosseno é decrescente de 0 a 180 graus) e dá limites exatos: "ângulo menor que 30" é "dot maior que cos 30".

#pragma once

#include "CoreMinimal.h"

/** Todos os números do takedown. Unidades: km/h, segundos, centímetros e graus. */
struct FDNTakedownConfig
{
	// --- Qualificação ---
	/** Condição 1: velocidade relativa no contato, de pelo menos [km/h] (inclusivo). */
	float MinRelativeSpeedKmh = 20.0f;
	/** Condições 2 e 3: janela depois do contato [s] (inclusiva). */
	float RivalCrashWindowSeconds = 1.5f;

	// --- Tipo ---
	/** Vertical: o jogador está pelo menos esta altura acima do rival [cm] (inclusivo) e caindo. */
	float VerticalMinHeightCm = 100.0f;
	/** Por trás: ângulo menor que este [graus] (estrito) e zona traseira. */
	float BehindMaxAngleDegrees = 30.0f;
	/** Em T: ângulo entre estes valores [graus] (inclusivo) e zona lateral. */
	float TBoneMinAngleDegrees = 60.0f;
	float TBoneMaxAngleDegrees = 120.0f;
	/** Lateral: zona lateral e ângulo menor que este [graus] (estrito). */
	float SideMaxAngleDegrees = 60.0f;

	/** Zonas: traseira se dot >= cos deste ângulo; frontal se dot <= -cos dele [graus]. */
	float ZoneHalfAngleDegrees = 45.0f;
};

enum class EDNTakedownType : uint8
{
	Vertical,
	FromBehind,
	TBone,
	Side,
	General
};

enum class EDNContactZone : uint8
{
	Rear,
	Front,
	Side
};

struct FDNTakedownClassifyInput
{
	/** Frente do jogador (A). */
	FVector PlayerForward = FVector::ForwardVector;
	/** Frente do rival (V). */
	FVector RivalForward = FVector::ForwardVector;
	/** Normal do contato, do jogador para o rival. */
	FVector ContactNormal = FVector::ForwardVector;
	/** Altura do jogador acima do rival [cm]. */
	float PlayerHeightAboveRivalCm = 0.0f;
	bool bPlayerFalling = false;
};

/** O que aconteceu depois do toque do jogador no rival. */
struct FDNTakedownContact
{
	/** Velocidade relativa no contato [km/h]. */
	float RelativeSpeedKmh = 0.0f;

	bool bRivalCrashed = false;
	/** Segundos entre o contato e a batida do rival. */
	float RivalCrashSecondsAfterContact = 0.0f;

	bool bPlayerDestroyed = false;
	/** Segundos entre o contato e a destruição do próprio jogador. */
	float PlayerDestroyedSecondsAfterContact = 0.0f;
};

struct FDNTakedown
{
	/** Cosseno de um ângulo em graus, calculado em precisão dupla e arredondado para float (limites limpos). */
	static float CosineOf(float Degrees);

	/** Zona do contato no rival. Vetor inválido (zero ou NaN) cai em lateral, o "resto" do contrato. */
	static EDNContactZone DeriveZone(const FDNTakedownConfig& Config, const FVector& ContactNormal, const FVector& RivalForward);

	/** Tipo do takedown, na ordem do contrato: vertical, por trás, em T, lateral, geral. */
	static EDNTakedownType Classify(const FDNTakedownConfig& Config, const FDNTakedownClassifyInput& Input);

	/** As condições 1 a 3. */
	static bool Qualifies(const FDNTakedownConfig& Config, const FDNTakedownContact& Contact);

	static const TCHAR* ToString(EDNTakedownType Type);
	static const TCHAR* ToString(EDNContactZone Zone);
};
