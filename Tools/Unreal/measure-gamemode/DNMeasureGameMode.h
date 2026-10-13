// Modo de medição de desempenho (Sprint 6). Usado por Tools/Unreal/measure.ps1.
//
// Roda um cenário sem intervenção: coloca N carros "substitutos", aquece, liga o CSV Profiler,
// mede, desliga e sai. Os carros extras servem só para estimar custo; NÃO são o tráfego real
// nem o dano real do jogo.
//
// Argumentos de linha de comando (todos opcionais):
//   -DNCarClass=Template|DN   carro do jogador e dos extras (Template = BP_SportsCar_Pawn do template;
//                             DN = ADNTestCar). Padrão: Template.
//   -DNExtra=N                carros extras (padrão 0)
//   -DNExtraMove              extras se movem (acelerador + volante em vai-e-vem); sem isto ficam parados
//   -DNWarmup=S -DNMeasure=S  segundos de aquecimento e de medição (padrão 30 e 60)
//   -DNSeed=N                 semente do que for aleatório (padrão 12345)
//   -DNStill                  o jogador fica parado (freio de mão): a câmera enxerga os extras, que ficam à frente

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DNMeasureGameMode.generated.h"

class AWheeledVehiclePawn;

UCLASS()
class ADNMeasureGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	ADNMeasureGameMode();

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:

	enum class EPhase : uint8 { Warmup, Measuring, Finishing };

	TSubclassOf<APawn> CarClass;
	int32 ExtraRequested = 0;
	bool bExtraMove = false;
	bool bPlayerStill = false;
	float WarmupSeconds = 30.0f;
	float MeasureSeconds = 60.0f;
	int32 Seed = 12345;

	EPhase Phase = EPhase::Warmup;
	float Elapsed = 0.0f;
	float FinishTimer = 0.0f;
	float WakeTimer = 0.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AWheeledVehiclePawn>> Extras;

	TArray<float> ExtraPhase;
	TArray<float> FrameMs;

	void SpawnExtras();
	void DriveCar(AWheeledVehiclePawn* Car, float Time, float PhaseOffset) const;
	void LogSummary() const;
};
