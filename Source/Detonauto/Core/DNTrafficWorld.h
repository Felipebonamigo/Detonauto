// Geometria do tráfego (lote 13a): a estrada reta de teste e a montagem da entrada de FDNTraffic::NextLevel a partir de posições e
// velocidades do mundo. Regras puras: sem UObject, sem Actor, sem mundo, só CoreMinimal. Os atores (ADNTrafficVehicle,
// ADNTrafficManager) só chamam estas funções. Testes: Core/Tests/DNTrafficWorld.spec.cpp (decisões W1 a W4 lá).
//
// Eixos: a estrada corre ao longo de +X (cm no mundo); a posição de um carro é a distância em metros desde o início da estrada.
// O jogador anda em +X; as faixas "contrárias" levam o tráfego em -X.

#pragma once

#include "CoreMinimal.h"
#include "Core/DNTraffic.h"

/** A estrada de teste. Valores iniciais: 600 m, 3 faixas de 3,5 m, a última no sentido contrário. */
struct FDNTrafficRoadConfig
{
	float LengthM = 600.0f;
	int32 LaneCount = 3;
	float LaneWidthM = 3.5f;
	/** Quantas das últimas faixas vão no sentido contrário. Limitado a 0..LaneCount-1 (sempre sobra uma a favor). */
	int32 OncomingLanes = 1;
	float MinSpeedKmh = 40.0f;
	float MaxSpeedKmh = 70.0f;
	/** Espaço mínimo entre dois carros da mesma faixa [m]. */
	float MinGapM = 30.0f;
	/** Ninguém nasce nesta distância do início nem do fim da estrada [m]. */
	float EdgeMarginM = 60.0f;
};

/** Onde e como um carro de tráfego nasce. */
struct FDNTrafficSlot
{
	int32 Lane = 0;
	/** Distância desde o início da estrada, ao longo de +X [m]. */
	float PositionM = 0.0f;
	/** Velocidade na trilha [km/h], sempre positiva; o sentido vem de bOncoming. */
	float SpeedKmh = 0.0f;
	bool bOncoming = false;
};

struct FDNTrafficRoad
{
	/** Deslocamento lateral do centro da faixa [m] (W1): (Lane - (LaneCount-1)/2) x LaneWidthM. */
	static float LaneOffsetM(const FDNTrafficRoadConfig& Config, int32 Lane);

	/** A faixa vai no sentido contrário ao do jogador. Faixa inexistente: falso. */
	static bool IsOncoming(const FDNTrafficRoadConfig& Config, int32 Lane);

	/**
	 *  Até Count carros, ordenados do mais perto para o mais longe (W2), sem sobrepor (MinGapM entre carros da mesma faixa),
	 *  repartidos por igual entre as faixas, com velocidade entre MinSpeedKmh e MaxSpeedKmh. A mesma semente dá a mesma estrada
	 *  (W3). Faixas lotadas cortam o excesso. Estrada inválida (comprimento, faixas, espaço, margens ou velocidades
	 *  não finitos ou sem sentido) ou Count <= 0: lista vazia.
	 */
	static TArray<FDNTrafficSlot> PlanSlots(const FDNTrafficRoadConfig& Config, int32 Count, int32 Seed);
};

/** O que o mundo sabe de um veículo de tráfego e do jogador neste quadro. Posições em cm, velocidades em cm/s. */
struct FDNTrafficGeometryInput
{
	EDNTrafficLevel Level = EDNTrafficLevel::Rail;
	float ImpactForce = 0.0f;
	/** Distância [m] ao veículo Physics mais próximo (FDNTrafficGeometry::NearestDistanceM). Máximo = nenhum. */
	float NearestPhysicsDistanceM = TNumericLimits<float>::Max();
	bool bCrashRoundActive = false;

	FVector VehiclePositionCm = FVector::ZeroVector;
	FVector VehicleForward = FVector::ForwardVector;
	FVector VehicleVelocityCmS = FVector::ZeroVector;

	FVector PlayerPositionCm = FVector::ZeroVector;
	FVector PlayerForward = FVector::ForwardVector;
	FVector PlayerVelocityCmS = FVector::ZeroVector;
};

struct FDNTrafficGeometry
{
	/**
	 *  Monta a entrada de NextLevel (W4): distância em linha reta [m], projeções nos eixos de avanço [m] e velocidade de aproximação
	 *  [km/h] (positiva = a distância entre os centros está diminuindo), mais a velocidade escalar do veículo [m/s]. Avanço de
	 *  tamanho zero ou NaN dá projeção 0. Mesmo ponto: aproximação 0. Posição NaN: distância NaN (NextLevel trata NaN como "não").
	 */
	static FDNTrafficVehicleInput BuildInput(const FDNTrafficGeometryInput& Input);

	/** Menor distância entre centros [m] de Position às posições de Others [cm]. Lista vazia ou só NaN: máximo (nenhum). */
	static float NearestDistanceM(const FVector& PositionCm, const TArray<FVector>& OthersCm);
};
