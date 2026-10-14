// Carro de tráfego (lote 13a): ator com corpo físico simples, SEM Chaos Vehicle. Só cola: as regras estão no Core
// (FDNTraffic::AdvanceOnRail para andar na trilha, FDNDamage para o dano, FDNDentParams/FDNImpactGrouper para ler a batida).
// Quem decide o nível (Rail, Physics, Despawn) é o ADNTrafficManager com FDNTraffic::NextLevel; este ator só executa.
//
//  - Rail: cinemático. O corpo existe (colide e avisa batida) mas não simula; o gerente o move a cada quadro pela trilha, na
//    velocidade de FDNTraffic::FollowSpeed atrás do carro da frente (corpos cinemáticos não colidem entre si; lote 13b).
//  - Physics: simula física (massa do TrafficVehicles.csv), com a velocidade que tinha na trilha. Nunca volta a Rail. Enquanto
//    "dirige" (até a primeira batida que dá dano) rola com atrito baixo (RollingFriction, combinado pelo mínimo com o piso), para
//    não parar sozinho em poucos metros; depois da batida volta ao atrito normal do corpo (lote 13b).
//  - Pool: Deactivate esconde, tira a colisão e desliga o tick; Activate reaproveita o mesmo ator.
//
// Malha PROVISÓRIA: o cubo básico da engine escalado ao tamanho da classe do carro (arte final vem depois).
// Dano: ΔV medido no corpo como no lote 11 (velocidade no início do quadro menos a de depois do contato, maior valor da janela de
// 0,15 s). Carro destruído pelas regras do dano avisa uma vez (OnDestroyed) com o ValueCash do CSV.
// Limite conhecido: uma batida que chega com o carro ainda em Rail (cinemático) acorda o carro no quadro seguinte mas não gera
// dano nesse primeiro contato; o Physics a menos de WakeDistanceM de folga entre as caixas acorda o Rail antes (docs/regras-trafego.md).
// Contatos com a arena de teste (piso) são ignorados.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DNTraffic.h"
#include "Core/DNTrafficWorld.h"
#include "Core/DNDamage.h"
#include "Core/DNCrash.h"
#include "Core/DNDentParams.h"
#include "Core/Data/DNDataRows.h"
#include "DNTrafficVehicle.generated.h"

class UStaticMeshComponent;
class ADNTrafficVehicle;

DECLARE_MULTICAST_DELEGATE_TwoParams(FDNOnTrafficVehicleImpact, ADNTrafficVehicle* /*Vehicle*/, AActor* /*Other*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDNOnTrafficVehicleDestroyed, ADNTrafficVehicle* /*Vehicle*/, int32 /*ValueCash*/);
/** Uma batida com dano (lote 13c): o aumento do dano total (FDNDamage::TotalDamage, 0 a 1) e se ela destruiu o carro. */
DECLARE_MULTICAST_DELEGATE_FourParams(FDNOnTrafficVehicleDamaged, ADNTrafficVehicle* /*Vehicle*/, int32 /*ValueCash*/, float /*AddedTotalDamage*/, bool /*bDestroyed*/);

UCLASS()
class DETONAUTO_API ADNTrafficVehicle : public AActor
{
	GENERATED_BODY()

public:
	ADNTrafficVehicle();

	/**
	 *  Liga o carro na trilha. Direction é o sentido da trilha (unitário, no plano). Velocidade na trilha em km/h; RailLengthCm é
	 *  o quanto a trilha anda daqui até o fim. Reaproveita o ator do pool.
	 */
	void Activate(const FDNTrafficVehicleEntry& Spec, const FVector& StartCm, const FVector& Direction, float SpeedKmh, float RailLengthCm, const FDNDamageConfig& InDamageConfig);

	/**
	 *  Só para a medição "todos em Physics": o carro Physics mantém a velocidade da trilha no plano até o primeiro contato que não
	 *  seja o piso. Antes valia também para carro batido e empurrando destroço, e a rodada nunca silenciava (lote 13b).
	 */
	void SetMaintainSpeed(bool bMaintain) { bMaintainSpeed = bMaintain; }

	/** Material físico de "rolando" (atrito baixo), dado pelo gerente; nulo = sem troca de atrito. */
	void SetRollingMaterial(class UPhysicalMaterial* InMaterial) { RollingMaterial = InMaterial; }

	/** Carro em trilho atrás do carro da frente: ajusta a velocidade na trilha (FDNTraffic::FollowSpeed). */
	void FollowLeader(const FDNTrafficSettings& Settings, const FDNTrafficLeader& Leader);

	/** A caixa do carro para as contas de folga (posição, avanço real, meias medidas da classe, velocidade). */
	FDNTrafficBody AsBody() const;

	/** Ainda dirige: não sofreu batida com dano desde que nasceu. */
	bool IsDriving() const { return bDriving; }

	/** Devolve ao pool: escondido, sem colisão, sem tick. */
	void Deactivate();

	bool IsActive() const { return bActive; }
	EDNTrafficLevel GetLevel() const { return Level; }

	/** Anda DeltaTime na trilha (só no nível Rail). */
	void AdvanceRail(float DeltaTime);

	/** Rail para Physics: passa a simular, com a velocidade da trilha. */
	void WakeUp();

	FVector GetPositionCm() const;
	/** Eixo de avanço do carro (o sentido da trilha). */
	FVector GetForward() const { return RailDirection; }
	FVector GetVelocityCmS() const;
	float GetSpeedMs() const { return static_cast<float>(GetVelocityCmS().Size() / 100.0); }

	/** Força do último impacto desde a última chamada (e zera). Acima de 0 acorda o Rail. */
	float TakePendingImpact();

	int32 GetValueCash() const { return ValueCash; }
	const FDNDamageState& GetDamageState() const { return DamageState; }
	int32 GetRailHitsBeforeWake() const { return RailHitsBeforeWake; }

	FDNOnTrafficVehicleImpact OnImpact;
	FDNOnTrafficVehicleDestroyed OnDestroyed;
	FDNOnTrafficVehicleDamaged OnDamaged;

	/** O livro deste carro na pontuação por dano (quanto do valor já rendeu); zera ao sair do pool. */
	FDNCrashVehicleLedger& GetLedger() { return Ledger; }

protected:
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnBodyHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	void FlushImpacts(double Now);

	UPROPERTY(VisibleAnywhere, Category="Traffic")
	TObjectPtr<UStaticMeshComponent> Body;

	bool bActive = false;
	EDNTrafficLevel Level = EDNTrafficLevel::Despawn;

	FVector RailStart = FVector::ZeroVector;
	FVector RailDirection = FVector::ForwardVector;
	float RailDistanceCm = 0.0f;
	float RailLengthCm = 0.0f;
	float RailSpeedCmS = 0.0f;
	/** Velocidade sorteada para o carro (a da trilha sem ninguém na frente); zera no fim da trilha. */
	float DesiredSpeedCmS = 0.0f;
	/** Meias medidas da caixa da classe [cm]. */
	FVector HalfSizeCm = FVector(225.0f, 92.5f, 75.0f);
	bool bDriving = false;

	UPROPERTY(Transient)
	TObjectPtr<class UPhysicalMaterial> RollingMaterial;

	float MassKg = 1000.0f;
	int32 ValueCash = 0;

	float PendingImpact = 0.0f;
	int32 RailHitsBeforeWake = 0;
	bool bMaintainSpeed = false;

	FDNDamageConfig DamageConfig;
	FDNDamageState DamageState;
	FDNCrashVehicleLedger Ledger;
	FDNImpactGrouper ImpactGrouper;
	FVector VelocityBeforeStepCmS = FVector::ZeroVector;
};
