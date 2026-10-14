// GameMode de teste do carro (lote 12): spawna o ADNVehicle em um mapa vazio, com piso e parede criados em runtime
// (ADNTestArena). Não há .umap do projeto nem Blueprint: abra qualquer mapa vazio da engine com este GameMode, por exemplo
//
//     UnrealEditor.exe Detonauto.uproject /Engine/Maps/Entry?game=/Script/Detonauto.DNVehicleTestGameMode -game
//
// ou use Tools/Unreal/play-vehicle.ps1. Teclas (as mesmas do protótipo): W/S acelera e freia, A/D esterça, Espaço = freio de mão,
// Shift = turbo, 1/2/3/0 = presets arcade-1/2/3/Padrao, T/Y/U = batida simulada na frente (pequena, média, máxima), G/H/J = no
// lado esquerdo, V = limpa os amassados.
//
// Também roda cenários sem intervenção para as ferramentas de teste (linha de comando; Tools/Unreal/vehicle-probe.ps1):
//   -DNCarProbe -DNCarScenario=Sin|Accel|Turbo|Handbrake|Turn|Switch|Wall [-DNWallKmh=N]   mede e imprime no log "DNCarProbe: ..."
//   -DNCarShot      acelera, liga o turbo e tira uma captura de tela com o HUD (dn-hud)
//   -DNShotDent     carro parado, 8 amassados de demonstração e quatro capturas da cena (dn-dent-*)
// Cenários e capturas encerram o jogo sozinhos.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DNVehicleTestGameMode.generated.h"

class ADNVehicle;
class APlayerStart;

UCLASS()
class DETONAUTO_API ADNVehicleTestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADNVehicleTestGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void StartPlay() override;

private:
	ADNVehicle* FindVehicle() const;
	void ScenarioTick(ADNVehicle& Vehicle, float DeltaSeconds);
	void ShotTick(ADNVehicle& Vehicle, float DeltaSeconds);
	void ProbeFinish(ADNVehicle& Vehicle);

	UPROPERTY(Transient)
	TObjectPtr<APlayerStart> Start;

	bool bProbe = false;
	bool bShot = false;
	bool bShotDent = false;
	bool bPrepared = false;
	FString Scenario;
	float WallTargetKmh = 0.0f;
	float ScenarioSeconds = 10.0f;
	float ScenarioTime = 0.0f;
	float ShotTime = 0.0f;

	// medidas do cenário Handbrake/Turn
	float MaxSlipDeg = 0.0f;
	float MaxYawRateDegS = 0.0f;
	float MinSpeedKmh = 1.0e9f;
	float SpeedAtManeuver = 0.0f;
	float ManeuverT0 = -1.0f; // instante em que a manobra começou (-1 = ainda acelerando)
};
