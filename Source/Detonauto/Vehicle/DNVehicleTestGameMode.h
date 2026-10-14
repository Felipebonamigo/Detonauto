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
class ADNTestArena;
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

protected:
	ADNVehicle* FindVehicle() const;

	/** A arena criada em InitGame (a rodada de Crash desliga a parede). */
	UPROPERTY(Transient)
	TObjectPtr<ADNTestArena> Arena;

	/** Roteiro de teclas (-DNKeys): pressiona e solta teclas REAIS pelo PlayerController, o mesmo caminho do teclado do jogador. */
	void KeyScriptTick(ADNVehicle& Vehicle, float DeltaSeconds);

	struct FScriptEvent
	{
		FString What;       // nome da tecla (W, T, SpaceBar...), ou SHOT, CAM<yaw>, QUIT
		float Start = 0.0f;
		float End = -1.0f;  // soltar; teclas sem fim soltam 0,15 s depois
		bool bPressed = false;
		bool bDone = false;
	};
	TArray<FScriptEvent> ScriptEvents;
	float ScriptTime = 0.0f;
	int32 ScriptShots = 0;

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
	/** Cenário Steer: entrada de esterço mantida (-DNSteer, 0 a 1) e as medidas da curva em regime. */
	float SteerInput = 1.0f;
	float SteerT0 = -1.0f;
	float SteerAtSpeedSince = -1.0f;
	double SteerRadiusSum = 0.0;
	double SteerYawSum = 0.0;
	double SteerFrontAngleSum = 0.0;
	int32 SteerSamples = 0;
	float ScenarioSeconds = 10.0f;
	float ScenarioTime = 0.0f;
	float ShotTime = 0.0f;

	// medidas do cenário Handbrake/Turn
	float MaxSlipDeg = 0.0f;
	float MaxYawRateDegS = 0.0f;
	float MinSpeedKmh = 1.0e9f;
	float SpeedAtManeuver = 0.0f;
	/** Cenários Handbrake/Turn: velocidade de entrada (-DNHbKmh, padrão 55) e esterço (-DNSteer, padrão 0,5). */
	float HandbrakeEntryKmh = 55.0f;
	float HandbrakeSteer = 0.5f;
	/** Acelerador durante a curva (-DNThrottle, padrão 0,3): 1,0 mede a saída de traseira com o pé no fundo. */
	float HandbrakeThrottle = 0.3f;
	/** Cenário Boost: instante em que o turbo (ou a aceleração sem turbo) começou e se o turbo liga (-DNTurboOn). */
	float BoostT0 = -1.0f;
	float BoostAtSpeedSince = -1.0f;
	bool bBoostTurbo = false;
	float RecoverySeconds = -1.0f; // segundos depois de soltar o freio de mão até o carro voltar a ser controlável (-1 = não voltou)
	float MaxSlipFullDeg = 0.0f;   // maior ângulo de deslize em toda a janela, de -180 a 180 (o de MaxSlipDeg ignora a ré)
	float ManeuverT0 = -1.0f; // instante em que a manobra começou (-1 = ainda acelerando)
};
