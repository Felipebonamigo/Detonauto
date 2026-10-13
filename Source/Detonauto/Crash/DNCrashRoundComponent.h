// Cola fina entre a rodada de Crash e a pontuação pura: repassa para FDNCrash e expõe estado e eventos.
// Nenhuma regra aqui. Testes: Core/Tests/DNComponents.spec.cpp. Decisões K1 a K3: ver Vehicle/DNTurboComponent.h.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DNCrash.h"
#include "DNCrashRoundComponent.generated.h"

class UDNGameDataSubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FDNOnCrashScoreChanged, int32 /*Total*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDNOnCrashMultiplierChanged, int32 /*Multiplier*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDNOnCrashRoundEnded, int32 /*FinalScore*/);

UCLASS()
class DETONAUTO_API UDNCrashRoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDNCrashRoundComponent();

	virtual void BeginPlay() override;

	void Configure(const FDNCrashConfig& NewConfig) { Config = NewConfig; }

	/** Pega os números do CSV (ou o padrão, se o subsistema é nulo ou a tabela é inválida). */
	void ConfigureFromData(const UDNGameDataSubsystem* DataSubsystem);

	/** Registra um impacto (liga os relógios do fim da rodada). */
	void RegisterImpact();

	/** Registra um veículo destruído; devolve os pontos somados. */
	int32 RegisterDestruction(int32 Value);

	/** Multiplicador coletado no chão. */
	void CollectMultiplier();

	/** Avança o relógio. MaxVehicleSpeedMps é a maior velocidade entre os veículos no quadro. */
	void Step(float DeltaTime, float MaxVehicleSpeedMps);

	/** Nova rodada: zera tudo e avisa o que mudou. */
	void ResetRound();

	const FDNCrashState& GetState() const { return State; }
	int32 GetScore() const { return State.Score.Total; }
	int32 GetMultiplier() const { return State.Multiplier; }
	bool IsEnded() const { return State.bEnded; }

	FDNOnCrashScoreChanged OnScoreChanged;
	FDNOnCrashMultiplierChanged OnMultiplierChanged;
	FDNOnCrashRoundEnded OnRoundEnded;

private:
	void Broadcast(int32 ScoreBefore, int32 MultiplierBefore, bool bEndedBefore);

	FDNCrashConfig Config;
	FDNCrashState State;
};
