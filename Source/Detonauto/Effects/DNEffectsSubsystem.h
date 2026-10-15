// Efeitos de batida no mundo (lote 19, docs/regras-efeitos.md): os disparos curtos (faísca de impacto e de raspão, cacos, lascas de
// plástico, pó e folhas do mobiliário) com o pool por efeito, o teto de partículas e os números de Data/Effects.csv. QUANDO e QUANTO é
// regra pura (FDNEffects, Core/DNEffects.h); aqui só se cria o sistema Niagara e se preenchem os parâmetros de usuário dele.
//
// Assets: /Game/Detonauto/Effects/NS_DN<Efeito> (NS_DNSparkImpact, NS_DNGlassShards...). Sem o asset, o efeito passa pelo pool e pelo
// orçamento e fica só no log (DNFx), até a montagem pelo MCP. Parâmetros de usuário que cada sistema lê (os que não usar, ignora):
//   Count (int), LifeMin, LifeMax, SpeedMin, SpeedMax, Extra (float), Direction (vetor unitário: o eixo do cone), Velocity (cm/s,
//   herdada do carro), Extent (cm, a meia caixa de onde nascem os cacos).
// Os contínuos (fumaça de pneu e do motor, fogo) ficam no carro (Vehicle/UDNCarEffectsComponent), que pede vaga e escala aqui.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DNEffects.h"
#include "DNEffectsSubsystem.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class ADNBreakableProp;

UCLASS()
class DETONAUTO_API UDNEffectsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Batida em superfície dura: faísca (Δv >= 4) e lascas de plástico (Δv >= 12). Normal: para fora da superfície batida. Devolve
	 *  as faíscas concedidas (0: nada saiu). */
	int32 OnImpact(const FVector& Point, const FVector& Normal, const FVector& CarVelocityCmS, float DeltaVMs);

	/** Peça raspando no chão (OnPartScrape): uma rajada com as faíscas de IntervalS segundos na taxa da velocidade. */
	void OnScrape(const FVector& Point, const FVector& CarVelocityCmS, float SpeedMs, float IntervalS);

	/** Vidro que estoura: os cacos pela área. Falso se não há o sistema (o carro usa os cacos antigos, ADNGlassShards). */
	bool OnGlassBurst(const FVector& Center, const FVector& Extent, const FVector& VelocityCmS, float AreaM2);

	/** Peça de rua que quebrou: o pó e as folhas dos efeitos que citam o tipo dela (Props). */
	void OnPropBroken(ADNBreakableProp& Prop);

	/** O número do efeito; falso se Effects.csv é inválido (nenhum efeito sai com número inventado). */
	bool Spec(EDNEffect Effect, FDNEffectSpec& Out) const;
	const FDNEffectsSettings& Settings() const { return EffectsSettings; }
	/** O sistema do efeito, ou nulo sem o asset (avisa uma vez). */
	UNiagaraSystem* System(EDNEffect Effect);

	/** Contínuos: uma vaga para Owner (até PoolMax donos por efeito); falso se cheio. Release devolve. */
	bool ClaimContinuous(EDNEffect Effect, const UObject* Owner);
	void ReleaseContinuous(EDNEffect Effect, const UObject* Owner);
	/** E11: cada dono informa os sprites de fumaça que pede neste quadro e recebe a escala do quadro anterior (todos juntos). */
	float ReportSmoke(const UObject* Owner, float RequestedSprites);

	/** Reinício (tecla R): apaga os disparos vivos e zera pools e orçamento. Os contínuos o carro zera. */
	void ResetAll();

	/** Disparos de cada efeito até agora (log e testes no motor). */
	int32 BurstCount(EDNEffect Effect) const { return Bursts[static_cast<int32>(Effect)]; }

private:
	static constexpr int32 EffectCount = static_cast<int32>(EDNEffect::PropLeaves) + 1;

	/** Um disparo curto: orçamento, pool, sistema e parâmetros. Devolve as partículas concedidas. */
	int32 Burst(EDNEffect Effect, int32 Count, const FVector& Location, const FVector& Direction, const FVector& VelocityCmS, const FVector& Extent);
	void LoadData();

	TArray<FDNEffectSpec> Specs;
	TArray<bool> SpecValid;
	FDNEffectsSettings EffectsSettings;
	bool bDataLoaded = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraSystem>> Systems;
	TArray<bool> SystemLooked;

	FDNEffectPool Pools[EffectCount];
	TArray<TWeakObjectPtr<UNiagaraComponent>> Slots[EffectCount];
	FDNParticleBudget GpuBudget;
	int32 Bursts[EffectCount] = {};

	TArray<TWeakObjectPtr<const UObject>> ContinuousOwners[EffectCount];
	TMap<TWeakObjectPtr<const UObject>, float> SmokeRequests;
	float SmokeScaleNow = 1.0f;
	uint64 SmokeFrame = 0;
};
