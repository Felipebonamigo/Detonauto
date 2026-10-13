// Projeto descartável 2 (Sprint 4) e lote 8 (afinação arcade): carro com Chaos Vehicles configurado em C++.
//
// O que está em C++ aqui: rodas, motor, transmissão, direção, câmera, entrada, e os parâmetros de ajuste lidos de
// Data/TestCars.csv (uma linha = um preset).
// O que vem de assets: malha esquelética, Physics Asset e Anim Blueprint do carro
// esportivo do template (caminhos explícitos no .cpp). O Chaos não funciona sem
// uma malha esquelética com ossos de roda e um Physics Asset; criar esses assets
// exige o editor visual.
//
// Lote 8:
//  - presets trocáveis jogando: 1 = arcade-1, 2 = arcade-2, 3 = arcade-3, 0 = Padrao (o carro do template);
//  - Shift = nitro de teste (aceleração extra para frente; barra fictícia);
//  - freio de mão (Espaço) tira aderência da traseira com rampa de ida e de volta (HandbrakeRearGrip no CSV);
//  - HUD de debug na tela: preset, velocidade em km/h e estado do nitro.
//
// Parâmetros do Chaos que só valem na criação do veículo (RollbarScaling, curso/mola/amortecimento da suspensão,
// FrictionForceMultiplier, MaxHandBrakeTorque, Mass, torque) são gravados nas classes de roda e no componente ANTES do
// registro (PreRegisterAllComponents). Trocar de preset jogando recria o estado físico do veículo (RecreatePhysicsState):
// a marcha e a rotação do motor reiniciam, a velocidade do corpo é mantida.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "DNCarTuning.h"
#include "DNTestCar.generated.h"

class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UChaosWheeledVehicleMovementComponent;
struct FInputActionValue;

UCLASS()
class ADNTestCar : public AWheeledVehiclePawn
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BrakeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SteeringAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HandbrakeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NitroAction;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> PresetActions;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Atalho com o tipo certo do componente de movimento. */
	UChaosWheeledVehicleMovementComponent* Chaos() const;

public:

	ADNTestCar();

	/** Linha de Data/TestCars.csv aplicada ao nascer (Padrao, arcade-1, arcade-2, arcade-3...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DN Carro")
	FName TuningRow = FName(TEXT("Padrao"));

	/** Parâmetros efetivamente aplicados (somente leitura). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="DN Carro")
	float AppliedSteeringGain = 1.0f;

	/** Troca de preset jogando (tecla 1, 2, 3 ou 0). Devolve falso se a linha não existe ou é inválida. */
	bool SwitchPreset(FName Row);

	/** As linhas das teclas 1, 2, 3 e 0, nessa ordem. */
	static const TArray<FName>& HotkeyRows();

protected:

	virtual void PreRegisterAllComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Throttle(const FInputActionValue& Value);
	void Brake(const FInputActionValue& Value);
	void Steering(const FInputActionValue& Value);
	void HandbrakeStart(const FInputActionValue& Value);
	void HandbrakeStop(const FInputActionValue& Value);
	void NitroStart(const FInputActionValue& Value);
	void NitroStop(const FInputActionValue& Value);
	void PresetKey(const FInputActionValue& Value, int32 HotkeyIndex);

	void CreateInputAssets();

	/** Lê a linha do CSV. Devolve false (e registra no log) se o arquivo ou a linha faltam ou são inválidos. */
	bool LoadTuning(FDNCarTuning& OutTuning) const;

	/** Grava o que só vale na criação do veículo (massa, torque, rodas, suspensão, barra, freio de mão). */
	void WriteCreationParameters(const FDNCarTuning& Tuning);

	/** Aplica o que vale com o veículo já criado (centro de massa, aderência, câmera, nitro). */
	void ApplyRuntimeParameters(const FDNCarTuning& Tuning);

	/** Aderência lateral/longitudinal por roda, com o efeito atual do freio de mão. Só escreve no Chaos se mudou. */
	void ApplyWheelGrip(bool bForce);

	/** Linhas do HUD de debug: preset, velocidade e nitro. */
	TArray<FString> HudLines() const;

	/** Desenha o HUD de debug na tela. */
	void UpdateHud() const;
	FString NitroText() const;
	float SpeedKmh() const;

	FDNCarTuning Tuning;
	bool bTuningLoaded = false;

	bool bHandbrakeHeld = false;
	bool bNitroHeld = false;
	float HandbrakeRamp = 0.0f;
	float AppliedRearMultiplier = -1.0f;
	FDNTestNitro Nitro;

	// --- modo de medição sem janela (linha de comando: -DNCarProbe [-DNCarScenario=Sin|Handbrake|Switch]) ---
	bool bProbe = false;
	FString ProbeScenario;
	float ProbeSeconds = 10.0f;
	float ProbeTime = 0.0f;
	double LastTickSeconds = 0.0;
	TArray<float> ProbeFrameMs;
	TArray<float> ProbeGameThreadMs;

	// medidas do cenário Handbrake
	float ProbeMaxSlipDeg = 0.0f;
	float ProbeMaxYawRateDegS = 0.0f;
	float ProbeMinSpeedKmh = 1.0e9f;
	float ProbeSpeedAtManeuver = 0.0f;
	float ProbeT0 = -1.0f; // instante em que a manobra começou (-1 = ainda acelerando)
	float ProbeRearLoss = 0.0f;

	// captura de tela para conferir o HUD e a câmera (linha de comando: -DNCarShot): acelera, liga o nitro e tira a foto
	bool bShot = false;
	float ShotTime = 0.0f;

	void ShotTick(float DeltaSeconds);
	void ProbeTick(float DeltaSeconds);
	void ProbeFinish();
};
