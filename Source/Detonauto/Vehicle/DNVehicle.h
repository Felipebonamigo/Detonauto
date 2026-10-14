// O carro do jogo (lote 12): Chaos Vehicles configurado em C++, sem Blueprint, usando DIRETO o Core real.
//
//  - afinação: um preset de Data/Cars.csv (FDNCarConfig), lido pelo UDNGameDataSubsystem. Presets trocáveis jogando:
//    1 = arcade-1, 2 = arcade-2, 3 = arcade-3, 0 = Padrao (o carro do template);
//  - turbo: UDNTurboComponent / FDNTurbo do Core. Shift = botão do turbo. Por enquanto só consome: os ganhos reais (contramão,
//    raspão, derrapada, ar, takedown) entram quando houver os eventos. Em MODO DE TESTE (bTurboTestMode, ligado por padrão) a
//    barra começa cheia e enche de novo 2 s depois de esvaziar, para dar para testar sem fim;
//  - dano: FDNDamage do Core. A batida real vira amassado visual (material de deslocamento de vértices, docs/prototipo-amassado.md);
//  - freio de mão (Espaço) tira aderência da traseira, com rampa de ida e de volta (FDNHandbrakeRamp);
//  - HUD de debug na tela: preset, velocidade, turbo e amassados.
//
// O que vem de assets (Content/Detonauto/Vehicle/SportsCar, 22 assets do carro esportivo do template): a malha esquelética com o
// Physics Asset, as malhas estáticas da carroceria, dos vidros e da roda, materiais e texturas. As rodas visuais são malhas estáticas
// presas aos ossos Phys_Wheel_*; no template quem as move é o Anim Blueprint (um Blueprint), aqui o Tick copia para elas o que o
// Chaos calcula (giro, esterçamento e curso da suspensão), exatamente como o UVehicleAnimationInstance entrega ao Anim Blueprint.
//
// Parâmetros do Chaos que só valem na criação do veículo (RollbarScaling, curso/mola/amortecimento, FrictionForceMultiplier,
// MaxHandBrakeTorque, Mass, torque) são gravados nas classes de roda e no componente ANTES do registro (PreRegisterAllComponents).
// Trocar de preset jogando recria o estado físico (RecreatePhysicsState): a marcha e a rotação do motor reiniciam, a velocidade do
// corpo é mantida.
//
// Linha de comando (só para ferramentas de teste): -DNCarRow=<id> escolhe o preset ao nascer; -DNNoDents desliga o amassado;
// -DNDentMaterial liga só o material; -DNDents=N liga o material e aplica N amassados de demonstração.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Core/DNCar.h"
#include "Core/DNCamera.h"
#include "Core/DNGameDataSubsystem.h"
#include "Core/DNDamage.h"
#include "Core/DNDentParams.h"
#include "DNVehicle.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UChaosWheeledVehicleMovementComponent;
class UDNTurboComponent;
struct FInputActionValue;

UCLASS()
class DETONAUTO_API ADNVehicle : public AWheeledVehiclePawn
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDNTurboComponent> Turbo;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BrakeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SteeringAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HandbrakeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TurboAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CameraAction;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> PresetActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> DentActions;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	/** A malha estática da carroceria (a que recebe o material de amassado). */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BodyComp;

	/** As quatro rodas visuais, na ordem FL, FR, BL, BR. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WheelVisuals;

	/** O primeiro dos materiais de amassado (ligado = não nulo). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DentMaterial;

	/** Um material de amassado por slot da carroceria, cada um com o visual do slot (pintura; chassi e janelas). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DentMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> OriginalBodyMaterials;

	/** Atalho com o tipo certo do componente de movimento. */
	UChaosWheeledVehicleMovementComponent* Chaos() const;

public:

	ADNVehicle();

	/** Id do preset de Data/Cars.csv aplicado ao nascer (Padrao, arcade-1, arcade-2, arcade-3...). O arcade-3 é o que o Felipe mais
	 *  gostou no playtest 1 (curva boa); Padrao e arcade-1 ficam como referência. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DN Carro")
	FName CarId = FName(TEXT("arcade-3"));

	/** Modo de teste do turbo: a barra começa cheia e enche de novo 2 s depois de esvaziar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DN Carro")
	bool bTurboTestMode = true;

	/** Ganho do volante efetivamente aplicado (somente leitura). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="DN Carro")
	float AppliedSteeringGain = 1.0f;

	/** Troca de preset jogando (tecla 1, 2, 3 ou 0). Devolve falso (e não muda nada) se o preset não existe em Cars.csv. */
	bool SwitchPreset(FName Id);

	/** Os ids das teclas 1, 2, 3 e 0, nessa ordem. */
	static const TArray<FName>& HotkeyRows();

	UDNTurboComponent* GetTurbo() const { return Turbo; }
	const FDNCarConfig& GetCarConfig() const { return Car; }
	float SpeedKmh() const;

	/** Botão do freio de mão e do turbo (as teclas Espaço e Shift chamam estas; as ferramentas de teste também). */
	void SetHandbrake(bool bHeld);
	void SetTurboButton(bool bHeld);

	// ---- amassado visual (ver docs/prototipo-amassado.md) ----

	/** Liga o material de amassado na carroceria. Falso (com erro no log) se o material-pai não existe. */
	bool EnableDentMaterial(bool bBindRealHits = true);

	/** Uma batida simulada: variação de velocidade [m/s], zona e ponto local na carroceria [cm]. */
	void SimulateImpact(float DeltaVMs, EDNDamageZone Zone, const FVector& LocalPositionCm);

	/** Batida na caixa da carroceria na fração Fraction (-1..1 por eixo) da extensão, em torno do centro: para achar a superfície. */
	void SimulateImpactAtBoxFraction(float DeltaVMs, const FVector& Fraction);

	/** Até 8 amassados de profundidade máxima espalhados pela carroceria (para tirar foto). */
	void ApplyDemoDents(int32 Count);

	/** Tira todos os amassados. */
	void ClearDents();

	int32 DentCount() const { return DamageState.Dents.Num(); }
	int32 MaxDentCount() const { return DamageConfig.MaxDents; }
	bool IsDentMaterialOn() const { return DentMaterial != nullptr; }
	bool IsDestroyed() const { return DamageState.bDestroyed; }
	float MaxRealImpactMs() const { return MaxRealDeltaVMs; }
	int32 RealImpactTotal() const { return RealImpactCount; }

	/** Câmera parada olhando o carro de um ângulo (YawDegrees em relação à frente), para as capturas de tela. */
	void SetShotCamera(float YawDegrees);

	/** Volta a câmera ao modo escolhido (Data/Cameras.csv). */
	void RestoreChaseCamera();

	/** Próximo modo de câmera (tecla C), na ordem de Cameras.csv. */
	void CycleCamera();
	/** Muda para o modo de câmera Index (fora da lista: o primeiro). */
	void SetCameraMode(int32 Index);
	FString CameraName() const;

	/** Assistência de derrapagem ligada (padrão sim); -DNNoAssist desliga para as medições de comparação. */
	bool bDriftAssist = true;

	/** Ângulo de deslize agora [graus] (positivo = velocidade à direita do nariz). */
	float SlipAngleDegNow() const;

	/** Linhas do HUD de debug: preset, velocidade, turbo e amassados. */
	TArray<FString> HudLines() const;

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
	void TurboStart(const FInputActionValue& Value);
	void TurboStop(const FInputActionValue& Value);
	void CameraKey(const FInputActionValue& Value);
	void PresetKey(const FInputActionValue& Value, int32 HotkeyIndex);
	void DentKey(const FInputActionValue& Value, int32 DentIndex);

	/** Batida real do corpo físico (colisão com a parede, com outros carros...). */
	UFUNCTION()
	void OnCarHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void PushDentsToMaterial();

	/** Fecha a janela de contatos vencida e aplica a batida (o maior valor da janela). */
	void FlushImpacts(double Now);
	void GetBodyBox(FVector& OutCenter, FVector& OutExtent) const;

	void CreateInputAssets();

	/** Copia para as rodas visuais o que o Chaos calculou: giro, esterçamento e curso da suspensão. */
	void UpdateWheelVisuals();

	/** Lê o preset de Cars.csv pelo subsistema de dados. Falso (e registra no log) se não existe ou a tabela é inválida. */
	bool LoadCar(FName Id, FDNCarConfig& OutCar) const;

	/** Grava o que só vale na criação do veículo (massa, torque, rodas, suspensão, barra, freio de mão). */
	void WriteCreationParameters(const FDNCarConfig& Config);

	/** Aplica o que vale com o veículo já criado (centro de massa, aderência, câmera). */
	void ApplyRuntimeParameters(const FDNCarConfig& Config);

	/** Aderência lateral/longitudinal por roda, com o efeito atual do freio de mão. Só escreve no Chaos se mudou. */
	void ApplyWheelGrip(bool bForce);

	/** Desenha o HUD de debug na tela. */
	void UpdateHud() const;
	FString TurboText() const;

	FDNCarConfig Car;
	bool bCarLoaded = false;

	FDNDamageConfig DamageConfig;
	FDNDamageState DamageState;
	float MaxRealDeltaVMs = 0.0f;
	int32 RealImpactCount = 0;
	int32 LoggedImpacts = 0;

	// batida real: velocidade do corpo no início do quadro (antes do passo de física) e a janela de contatos
	FVector VelocityBeforeStepCmS = FVector::ZeroVector;
	FVector GroupVelocityBeforeCmS = FVector::ZeroVector;
	FDNImpactGrouper ImpactGrouper;
	int32 GroupContacts = 0;

	bool bHandbrakeHeld = false;
	bool bTurboHeld = false;
	float HandbrakeRamp = 0.0f;
	float AppliedRearMultiplier = -1.0f;
	/** Segundos com a barra do turbo vazia e desligado (modo de teste). */
	float TurboEmptySeconds = 0.0f;
	/** Segundos que faltam da vista de dano (as teclas de batida simulada giram a câmera para o amassado aparecer). */
	float DentViewSecondsLeft = 0.0f;

	TArray<FDNCameraMode> CameraModes;
	int32 CameraIndex = 0;

	/** Abertura do campo de visão com o turbo ativo [graus], e a velocidade com que sobe e desce. */
	static constexpr float TurboFovKickDegrees = 12.0f;
	static constexpr float TurboFovBlendSpeed = 5.0f;
	float TurboFovBlend = 0.0f;

	/** Rotação do osso de cada roda no pose de referência (espaço do componente): converte o curso da suspensão para o espaço do osso. */
	TArray<FQuat> WheelBoneRotations;
};
