// GameMode de teste da rodada de Crash (lote 13b): liga o carro do jogador (ADNVehicle), o tráfego real (ADNTrafficManager) e as
// regras do Core numa estrada reta de 600 m, sem .umap e sem Blueprint. Só cola; nenhuma regra nova aqui.
//
//   tráfego  ADNTrafficManager (Rail -> Physics -> pool), números de Data/TrafficSettings.csv e TrafficVehicles.csv
//   pontos   UDNCrashRoundComponent (FDNCrash): RegisterImpact na 1ª batida no jogador; depois dela, RegisterDamage a cada batida
//            com dano (valor do CSV x fração do dano total adicionada; a destruição completa o valor; lote 13c); fim da rodada pelas
//            regras do Core (silêncio ou tempo)
//   medalha  FDNMedals sobre os limites do cruzamento CrossingId (padrão exemplo-rio-01, de Crossings.csv)
//   recorde  UDNRecordsSubsystem::RegisterResult, UMA vez por rodada
//   HUD      pontos, multiplicador, destruídos, tempo, medalha provisória e o turbo do carro
//
// PONTO DE EXTENSÃO (multiplicadores no chão): OnMultiplierPickup() chama CrashRound->CollectMultiplier(); ninguém o chama ainda.
// Hoje o multiplicador fica em 1. Quando houver os coletáveis no chão, eles chamam este ponto.
//
// SAVE: este GameMode NÃO usa o save real do jogador. Grava em Saved/SaveGames/DetonautoCrashTest.json (ou numa pasta temporária
// nos cenários automáticos); -DNRealSave volta ao arquivo real.
//
// Linha de comando (jogar e medir; Tools/Unreal/crash-measure.ps1):
//   -DNTraffic=N            carros de tráfego (padrão 20)
//   -DNTrafficMode=Rail|Physics   Physics = todos já nascem Physics (medição de custo)
//   -DNSeed=S               semente da estrada (padrão 12345)
//   -DNCrashScenario=Crash  o jogador nasce andando a -DNCrashKmh=K km/h e dirige sozinho; ao fim imprime "DNCrash: RESULTADO ..."
//   -DNCrashScenario=Traffic  o jogador fica parado ao lado da estrada; mede o tráfego (com -DNMeasure=S liga o CSV Profiler)
//   -DNWarmup=S -DNMeasure=S  aquecimento e janela de medição do cenário Traffic [s]

#pragma once

#include "CoreMinimal.h"
#include "Vehicle/DNVehicleTestGameMode.h"
#include "Core/DNCrash.h"
#include "Core/DNMedals.h"
#include "DNCrashTestGameMode.generated.h"

class UDNCrashRoundComponent;
class UDNRecordsSubsystem;
class ADNTrafficManager;
class ADNTrafficVehicle;

UCLASS()
class DETONAUTO_API ADNCrashTestGameMode : public ADNVehicleTestGameMode
{
	GENERATED_BODY()

public:
	ADNCrashTestGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Id do cruzamento (Crossings.csv) cujos limites dão a medalha e onde o recorde é gravado. */
	UPROPERTY(EditAnywhere, Category="DN Crash")
	FString CrossingId = TEXT("exemplo-rio-01");

	/** Ponto de extensão: um multiplicador coletado no chão. Ainda sem chamadores. */
	void OnMultiplierPickup();

protected:
	virtual void StartPlay() override;

private:
	enum class EScenario : uint8 { None, Crash, Traffic };

	void HandleImpact(ADNTrafficVehicle* Vehicle, AActor* Other);
	void HandleDestroyed(ADNTrafficVehicle* Vehicle, int32 ValueCash);
	void HandleDamaged(ADNTrafficVehicle* Vehicle, int32 ValueCash, float AddedTotalDamage, bool bDestroyed);
	void HandleRoundEnded(int32 FinalScore);
	void UpdateHud(const ADNVehicle& Vehicle) const;
	void DriveCrashScenario(ADNVehicle& Vehicle, float DeltaSeconds);
	void DriveTrafficScenario(ADNVehicle& Vehicle, float DeltaSeconds);
	void LogResult();
	float PlayerSpeedMs(const ADNVehicle& Vehicle) const;

	UPROPERTY(VisibleAnywhere, Category="DN Crash")
	TObjectPtr<UDNCrashRoundComponent> CrashRound;

	UPROPERTY(Transient)
	TObjectPtr<ADNTrafficManager> Traffic;

	EScenario Scenario = EScenario::None;
	int32 TrafficCount = 20;
	bool bAllPhysics = false;
	int32 Seed = 12345;
	float TargetKmh = 0.0f;
	float Warmup = 5.0f;
	float MeasureSeconds = 0.0f;

	bool bPlayerLinked = false;
	bool bLaunched = false;
	bool bEnded = false;
	bool bRecordRegistered = false;
	bool bNewRecord = false;
	bool bCsvStarted = false;
	bool bCsvStopped = false;
	int32 RecordRegistrations = 0;
	int32 ImpactEvents = 0;
	float Elapsed = 0.0f;
	float EndedAt = -1.0f;
	float FirstImpactAt = -1.0f;
	float RoundDuration = 0.0f;
	EDNMedal Medal = EDNMedal::None;
	FDNMedalThresholds Thresholds;
	bool bHasThresholds = false;
	FString SavePathUsed;
};
