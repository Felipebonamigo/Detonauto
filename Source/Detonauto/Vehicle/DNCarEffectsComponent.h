// Efeitos contínuos de um carro (lote 19, docs/regras-efeitos.md): a fumaça de cada pneu (deslizamento e roda patinando), a fumaça do
// motor (clara, depois escura) e o fogo com uma luz pontual sem sombra e tremulante. QUANTO é regra pura (FDNEffects::TireSmokeRate e
// EngineSmoke); a vaga (até PoolMax carros por efeito) e a escala do teto de sprites vêm do UDNEffectsSubsystem.
//
// Parâmetros de usuário dos sistemas contínuos: Rate (0 a 1, já com a escala do teto), SpawnPerS (CountMax: sprites/s na taxa 1),
// LifeMin, LifeMax, SpeedMin, SpeedMax, Dark (0 ou 1, fumaça do motor), FadeNear e FadeFar (cm; a fumaça some perto da câmera).
// A fumaça e o fogo do motor só sobem (um dano maior não reinicia; E7); a tecla R zera tudo (ResetEffects).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DNEffects.h"
#include "DNCarEffectsComponent.generated.h"

class ADNVehicle;
class UNiagaraComponent;
class UPointLightComponent;
class UDNEffectsSubsystem;

UCLASS(ClassGroup=(Detonauto), meta=(BlueprintSpawnableComponent))
class DETONAUTO_API UDNCarEffectsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDNCarEffectsComponent();

	/** Apaga fumaça e fogo e devolve as vagas (tecla R, reinício da rodada). */
	void ResetEffects();

	/** O que o carro está soltando agora (log e testes no motor). */
	float TireRate(int32 Wheel) const { return TireRates.IsValidIndex(Wheel) ? TireRates[Wheel] : 0.0f; }
	const FDNEngineSmoke& Engine() const { return EngineState; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateTires(ADNVehicle& Car, UDNEffectsSubsystem& Fx, float Scale);
	void UpdateEngine(ADNVehicle& Car, UDNEffectsSubsystem& Fx, float Scale, float DeltaTime);
	/** Cria (uma vez) o componente Niagara do efeito preso ao carro, se o sistema existe. */
	UNiagaraComponent* Ensure(TObjectPtr<UNiagaraComponent>& Slot, UDNEffectsSubsystem& Fx, EDNEffect Effect, const FDNEffectSpec& Spec);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> TireFx;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SmokeFx;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> FireFx;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FireLight;

	TArray<float> TireRates;
	FDNEngineSmoke EngineState;
	bool bTireClaimed = false;
	bool bSmokeClaimed = false;
	bool bFireClaimed = false;
	float FlickerClock = 0.0f;
	/** Pneus soltando fumaça no quadro anterior (o log registra só quando liga e desliga). */
	bool bTiresWereSmoking = false;
};
