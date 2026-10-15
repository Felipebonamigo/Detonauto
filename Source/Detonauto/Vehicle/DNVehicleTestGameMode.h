// GameMode de teste do carro (lote 12): spawna o ADNVehicle em um mapa vazio, com piso e parede criados em runtime
// (ADNTestArena). Não há .umap do projeto nem Blueprint: abra qualquer mapa vazio da engine com este GameMode, por exemplo
//
//     UnrealEditor.exe Detonauto.uproject /Engine/Maps/Entry?game=/Script/Detonauto.DNVehicleTestGameMode -game
//
// ou use Tools/Unreal/vehicle-probe.ps1. Num mapa com um PlayerStart marcado com a tag DNMapStart (a cena de aparência
// /Game/Detonauto/Maps/L_Lookdev, criada por Tools/Unreal/create_lookdev.py) não cria a arena: o carro nasce nesse PlayerStart. Teclas (as mesmas do protótipo): W/S acelera e freia, A/D esterça, Espaço = freio de mão,
// Shift = turbo, 1/2/3/0 = presets arcade-1/2/3/Padrao, T/Y/U = batida simulada na frente (pequena, média, máxima), G/H/J = no
// lado esquerdo, V = limpa os amassados.
//
// Também roda cenários sem intervenção para as ferramentas de teste (linha de comando; Tools/Unreal/vehicle-probe.ps1):
//   -DNCarProbe -DNCarScenario=Sin|Accel|Turbo|Handbrake|Turn|Switch|Wall [-DNWallKmh=N]   mede e imprime no log "DNCarProbe: ..."
//   -DNCarShot      acelera, liga o turbo e tira uma captura de tela com o HUD (dn-hud)
//   -DNShotDent     carro parado, 8 amassados de demonstração e quatro capturas da cena (dn-dent-*)
//   -DNSunYaw=G     gira o sol da cena G graus em volta do eixo vertical, só nesta execução (sol por trás da câmera)
// Cenários e capturas encerram o jogo sozinhos.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DNVehicleTestGameMode.generated.h"

class ADNVehicle;
class ADNTestArena;
class APlayerStart;
class ADNBreakableProp;
class ACameraActor;

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

	/** A arena criada em InitGame (a rodada de Crash desliga a parede). Nula num mapa de verdade (ver MapStartTag). */
	UPROPERTY(Transient)
	TObjectPtr<ADNTestArena> Arena;

	/** Tag do PlayerStart de um mapa de verdade (L_Lookdev): com ele, sem arena e o carro nasce ali. */
	inline static const FName MapStartTag = FName(TEXT("DNMapStart"));

	UPROPERTY(Transient)
	TObjectPtr<APlayerStart> MapStart;

	/** Medição de custo do dano visual (lote 17, -DNParkedCars=N): N carros do jogador, parados à frente e à direita, com as 5 zonas
	 *  amassadas (-DNParkedDamage=0 a 1, padrão 1; 0 = intactos). Até 12. -DNParkedDamageAtS=S: o dano entra S segundos depois (lote 18b,
	 *  as peças voando dentro da janela de medição). */
	void SpawnParkedCars(int32 Count);

	/** Batida real na cena de aparência (lote 17b, -DNWallAheadM=M): barreira de concreto atravessando a rua a M metros da largada. */
	void SpawnWallAhead(float DistanceM);

	/** Lote 17c: peças de rua na faixa do carro (-DNPropLine=poste,arvore,banco,lixeira; -DNPropStartM, padrão 60; -DNPropSpacingM,
	 *  padrão 45), cópias das peças daquele tipo que já estão no mapa. */
	void SpawnPropLine(const FString& Kinds, float StartM, float SpacingM);

	/** Resumo da fileira no QUIT, com veredito: todas quebradas, o carro nunca abaixo de 60% do cruzeiro depois da 1a quebra e
	 *  nunca a mais de 1,5 m da faixa ("bater e seguir"). */
	void LogPropLineSummary() const;
	TArray<TWeakObjectPtr<ADNBreakableProp>> LineProps;
	float LineMinKmh = TNumericLimits<float>::Max();
	float LineMaxOffsetM = 0.0f;

	/** -DNPropShots: captura antes (a 12 m), no contato e depois de cada peça quebrada (dn-prop-<n>-<tipo>-...). Depois: 0,8 s
	 *  ("depois"), ou os tempos de -DNPropShotDelays=0.1,0.25 em segundos ("t100ms", "t250ms"). */
	void HandlePropBroken(ADNBreakableProp& Prop);
	void PropShotTick(ADNVehicle& Vehicle, float DeltaSeconds);
	bool bPropShots = false;
	int32 PropShots = 0;
	struct FPendingShot
	{
		float Left = 0.0f;
		FString Name;
	};
	TArray<FPendingShot> PendingShots;
	TArray<float> PropShotDelays;
	TSet<TWeakObjectPtr<ADNBreakableProp>> PropsShotBefore;

	/** -DNPropCam=<i>,<frente m>,<lado m>,<altura m>: câmera parada perto da peça i da fileira (0 = a primeira), nos eixos da largada
	 *  (frente = para onde o carro anda, lado + = direita), olhando para a peça 1 m acima do chão. Frente positiva põe o sol por trás. */
	void SpawnPropCam(const FString& Spec);
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> PropCam;
	bool bPropCamInView = false;

	/** Lote 18, câmera de batida: -DNTrafficAheadM=M põe um carro de tráfego parado (já com física) na faixa, a M metros da largada;
	 *  -DNBlockSideM=S põe um bloco de 15 m de altura (um "prédio") ao lado da barreira de -DNWallAheadM, S metros para a direita
	 *  (negativo: esquerda), para conferir que a câmera não entra nele. */
	void SpawnTrafficAhead(float DistanceM);
	void SpawnBlockBeside(float WallAheadM, float SideM);

	/** -DNCrashShots=0,0.15,0.6: capturas com o HUD (a escala de tempo escrita) nesses segundos REAIS depois do disparo da câmera de
	 *  batida (dn-crash-t<ms>); -DNCrashSteer=-1|1 segura A ou D (o controle do destroço) enquanto ele vale. */
	void CrashCamTick(ADNVehicle& Vehicle);
	TArray<float> CrashShotDelays;
	int32 CrashShotNext = 0;
	float CrashSteer = 0.0f;
	bool bCrashSteerHeld = false;
	bool bCrashSeen = false;
	/** -DNSpeedShots=60,150,220: uma captura com o HUD quando o carro passa de cada velocidade [km/h] (dn-speed-<v>kmh). */
	TArray<float> SpeedShots;
	int32 SpeedShotNext = 0;

	/** -DNCruiseKmh=N: piloto automático segura N km/h na reta (o log diz que o acelerador vem dele). */
	float CruiseKmh = 0.0f;
	float CruiseLogLeft = 0.0f;
	/** O piloto parou na primeira batida real (peça de rua não conta): daí em diante, só o roteiro de teclas. */
	bool bCruiseStopped = false;

	/** Roteiro de teclas (-DNKeys): pressiona e solta teclas REAIS pelo PlayerController, o mesmo caminho do teclado do jogador. */
	void KeyScriptTick(ADNVehicle& Vehicle, float DeltaSeconds);

	struct FScriptEvent
	{
		FString What;       // nome da tecla (W, T, SpaceBar...), ou SHOT, CAM<yaw>, QUIT, DENT..., DEMO<n>, EXEC:<comando de console>
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
