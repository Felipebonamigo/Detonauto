// Gerente do tráfego (lote 13a): cola entre as regras puras do tráfego (Core/DNTraffic, Core/DNTrafficWorld) e os carros de tráfego.
// Nenhuma regra aqui: PlanSpawns diz quantos criar, PlanSlots onde, NextLevel o que cada carro vira, BuildInput monta a entrada.
//
//  - Os números vêm de Data/TrafficSettings.csv pelo UDNGameDataSubsystem; os carros, de Data/TrafficVehicles.csv.
//  - Pool: carros despachados voltam ao pool e são reaproveitados; o pool tem tamanho máximo (PoolMax).
//  - NextLevel é avaliado a cada quadro SÓ nos carros Rail (custo baixo). Os Physics são avaliados a cada
//    PhysicsCheckSeconds apenas para o despawn (a regra manda: "Rail ou Physics -> Despawn"; um Physics nunca volta a Rail).
//  - A estrada de teste começa em RoadOriginCm e corre em +X (ADNTestArena). Faixa contrária anda em -X.
//  - bAllPhysics (medição): todo carro já nasce Physics, para comparar o custo com o tráfego em trilhos.
//  - Os carros de tráfego só nascem uma vez (os slots de PlanSlots acabam): quem passou do jogador e some não é reposto, a estrada de
//    teste não tem fim para repor. Sem jogador ligado, nada some (BuildInput sem jogador), então nenhuma vaga se perde no começo.
//  - Lote 13b: cada carro em trilho segue o da frente na mesma faixa (LeaderAhead + FollowSpeed; o jogador também conta como carro da
//    frente); acordar usa a folga entre as caixas (NearestGapM), não a distância entre centros.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DNTraffic.h"
#include "Core/DNTrafficWorld.h"
#include "Core/DNDamage.h"
#include "Traffic/DNTrafficVehicle.h"
#include "DNTrafficManager.generated.h"

class UDNGameDataSubsystem;

/** Contadores para as medições e a HUD de debug. */
struct FDNTrafficStats
{
	int32 Spawned = 0;
	int32 Despawned = 0;
	/** Quantos carros foram de Rail a Physics (os que "acordaram"). */
	int32 Woken = 0;
	int32 ActiveRail = 0;
	int32 ActivePhysics = 0;
	int32 Destroyed = 0;
	/** Batidas que chegaram a um carro ainda em Rail (sem dano nesse primeiro contato). */
	int32 RailHitsBeforeWake = 0;
	/** Menor folga já vista entre um carro em trilho e o da frente [m] (lote 13b: perto da folga mínima ou mais; negativa = atravessou). */
	float MinRailGapM = TNumericLimits<float>::Max();
	/** Carros Physics que ainda dirigem (acordados sem batida com dano) e a velocidade média deles [m/s], neste quadro. */
	int32 DrivingPhysics = 0;
	float DrivingPhysicsMeanSpeedMs = 0.0f;
};

UCLASS()
class DETONAUTO_API ADNTrafficManager : public AActor
{
	GENERATED_BODY()

public:
	ADNTrafficManager();

	/** Liga o tráfego: quantos carros, semente, tamanho do pool e a estrada. Os slots de nascimento ficam planejados aqui. */
	void Configure(const FDNTrafficRoadConfig& InRoad, int32 InTargetCount, int32 InSeed, int32 InPoolMax, bool bInAllPhysics);

	/** O jogador (ou rival): alvo das regras de distância. Guarda as meias medidas da caixa dele (espaço local). */
	void SetPlayer(AActor* InPlayer);

	/** A rodada de Crash está ativa: Physics em movimento não some (docs/regras-trafego.md). */
	void SetCrashRoundActive(bool bActive) { bCrashRoundActive = bActive; }

	/** Início da estrada no mundo [cm]. */
	FVector RoadOriginCm = FVector::ZeroVector;

	/** O número de carros planejados (pode ser menor que o pedido se a estrada lota). */
	int32 PlannedCount() const { return Slots.Num(); }

	const FDNTrafficStats& GetStats() const { return Stats; }

	/**
	 *  A maior velocidade [m/s] entre os DESTROÇOS: carros Physics que já levaram batida com dano (para o fim da rodada de Crash).
	 *  Carro acordado que ainda dirige (rolando, ou mantendo a velocidade na medição) não conta: ele é trânsito, não destroço, e com
	 *  ele a rodada nunca silenciava (lote 13b).
	 */
	float MaxWreckSpeedMs() const;

	/** O carro de tráfego em batida: o jogador bateu em alguém (um aviso por contato). */
	FDNOnTrafficVehicleImpact OnVehicleImpact;
	/** Um carro de tráfego destruído pelas regras do dano, com o valor do CSV. */
	FDNOnTrafficVehicleDestroyed OnVehicleDestroyed;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void SpawnFromPlan();
	ADNTrafficVehicle* Acquire();
	void Release(ADNTrafficVehicle* Vehicle);
	void UpdateRailVehicle(ADNTrafficVehicle* Vehicle, float DeltaTime, const TArray<FDNTrafficBody>& AllBodies, const TArray<FDNTrafficBody>& PhysicsBodies);
	FDNTrafficBody PlayerBody() const;
	void CheckPhysicsVehicle(ADNTrafficVehicle* Vehicle);
	FDNTrafficGeometryInput MakeGeometry(const ADNTrafficVehicle* Vehicle) const;
	void HandleImpact(ADNTrafficVehicle* Vehicle, AActor* Other);
	void HandleDestroyed(ADNTrafficVehicle* Vehicle, int32 ValueCash);

	UPROPERTY(Transient)
	TArray<TObjectPtr<ADNTrafficVehicle>> Pool;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Player;

	/** Meias medidas da caixa do jogador no espaço local [cm]. */
	FVector PlayerHalfSizeCm = FVector(235.0f, 115.0f, 60.0f);

	/** Atrito de "rolando" para os carros acordados que ainda dirigem (RollingFriction, combinado pelo mínimo). */
	UPROPERTY(Transient)
	TObjectPtr<class UPhysicalMaterial> RollingMaterial;

	FDNTrafficSettings Settings;
	FDNDamageConfig DamageConfig;
	FDNTrafficRoadConfig Road;
	TArray<FDNTrafficVehicleEntry> Specs;
	TArray<FDNTrafficSlot> Slots;
	int32 NextSlot = 0;
	FDNTrafficPool PoolState{ 64 };
	bool bAllPhysics = false;
	bool bCrashRoundActive = false;
	bool bConfigured = false;
	float PhysicsCheckTimer = 0.0f;
	FDNTrafficStats Stats;

	/** Quanto a trilha contrária continua além do início da estrada [m]. */
	static constexpr float OncomingOvershootM = 150.0f;

	/** Segundos entre as avaliações de despawn dos carros Physics. */
	static constexpr float PhysicsCheckSeconds = 0.5f;
};
