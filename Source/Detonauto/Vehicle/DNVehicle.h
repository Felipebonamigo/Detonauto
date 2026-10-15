// O carro do jogo (lote 12): Chaos Vehicles configurado em C++, sem Blueprint, usando DIRETO o Core real.
//
//  - afinação: um preset de Data/Cars.csv (FDNCarConfig), lido pelo UDNGameDataSubsystem. Presets trocáveis jogando:
//    1 = arcade-1, 2 = arcade-2, 3 = arcade-3, 0 = Padrao (o carro do template);
//  - turbo: UDNTurboComponent / FDNTurbo do Core. Shift = botão do turbo. Por enquanto só consome: os ganhos reais (contramão,
//    raspão, derrapada, ar, takedown) entram quando houver os eventos. Em MODO DE TESTE (bTurboTestMode, ligado por padrão) a
//    barra começa cheia e enche de novo 2 s depois de esvaziar, para dar para testar sem fim;
//  - dano: FDNDamage do Core. A batida real vira amassado visual em duas camadas: a deformação grande de cada zona (alvos de
//    deformação da carroceria, FDNDamageMorph, docs/regras-dano-visual.md) e, por cima, as mossas locais (material de deslocamento
//    de vértices, docs/prototipo-amassado.md);
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
//
// Câmera de velocidade e de batida (lote 18, docs/regras-camera.md, regras em Core/DNCameraFx): campo de visão pela velocidade e pelo
// turbo, tremor, desfoque nas bordas (material M_DNEdgeBlur) e a câmera de batida com câmera lenta e o controle do destroço. Tudo anda
// em tempo real (FApp::GetDeltaTime), porque a câmera lenta muda a escala de tempo do jogo. -DNNoEdgeBlur, -DNNoShake e -DNNoCrashCam
// desligam cada parte; no console, DNEdgeBlur 0/1 e DNShake 0/1. -DNEdgeBlurForce=S fixa a força do desfoque (medição de custo).

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Core/DNCar.h"
#include "Core/DNCamera.h"
#include "Core/DNCameraFx.h"
#include "Core/DNGameDataSubsystem.h"
#include "Core/DNDamage.h"
#include "Core/DNDamageMorph.h"
#include "Core/DNDentParams.h"
#include "Core/DNCarParts.h"
#include "DNVehicle.generated.h"

class UMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UChaosWheeledVehicleMovementComponent;
class UDNTurboComponent;
class ADNCarPart;
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
	TObjectPtr<UInputAction> RestartAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	/** A carroceria (a que recebe o material de amassado): BodySkin com SK_DNSportsCarBody, os faróis e os 5 alvos de deformação do
	 *  dano por zona (lote 17, Tools/Unreal/damage-body.ps1); sem ela, BodyStatic com a malha estática SM_SportsCar do template. */
	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> BodyComp;

	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> BodySkin;

	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BodyStatic;

	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> GlassStatic;

	/** Malhas por caminho: carregadas em LoadCarMeshes (antes do registro), nunca no construtor (ver o .cpp). A pasta do carro está
	 *  na lista de cozimento (Config/DefaultGame.ini), porque o cozimento não segue caminho guardado no padrão de classe nativa. */
	UPROPERTY(EditDefaultsOnly, Category="DN Carro|Malhas")
	TSoftObjectPtr<USkeletalMesh> ChassisMesh;

	UPROPERTY(EditDefaultsOnly, Category="DN Carro|Malhas")
	TSoftObjectPtr<USkeletalMesh> DamageBodyMesh;

	UPROPERTY(EditDefaultsOnly, Category="DN Carro|Malhas")
	TSoftObjectPtr<UStaticMesh> BodyMesh;

	UPROPERTY(EditDefaultsOnly, Category="DN Carro|Malhas")
	TSoftObjectPtr<UStaticMesh> GlassMesh;

	UPROPERTY(EditDefaultsOnly, Category="DN Carro|Malhas")
	TSoftObjectPtr<UStaticMesh> WheelMesh;

	/** Carrega as malhas e escolhe a carroceria (com alvos ou a estática). */
	void LoadCarMeshes();

	/** As quatro rodas visuais, na ordem FL, FR, BL, BR. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WheelVisuals;

	/** Lote 18b: as peças que soltam (uma por linha de Data/CarParts.csv do carro), na ordem do CSV. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADNCarPart>> Parts;

	/** Lote 18b: os vidros e lentes (Data/CarGlass.csv), presos ao chassi como a carroceria, na ordem do CSV. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> GlassComps;

	/** O desfoque nas bordas (lote 18), nos blendables da câmera; nulo se o material não existe. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EdgeBlurMaterial;

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

	/** Tira todos os amassados (e desamassa a carroceria: os alvos de deformação voltam a 0). */
	void ClearDents();

	/** Tecla R (playtest 2): reinício completo, como numa rodada nova. O carro volta para onde nasceu, parado, sem dano e com as peças,
	 *  os vidros e o turbo repostos; sai da câmera de batida; o mobiliário quebrado do mundo volta inteiro. Também pelo console. */
	UFUNCTION(Exec)
	void DNRestart();

	/** Batida em peça de rua que quebrou (lote 17c): o dano de sempre com a variação de velocidade da regra, sem contar como batida
	 *  do jogador na rodada de Crash nem para a câmera de batida. */
	void ApplyPropImpact(float DeltaVMs, const FVector& WorldPoint);

	/** B9 (lote 17c): o destroço de peça de rua dentro da caixa da carroceria sai da frente (FDNBreakable::Plow); o carro não muda. */
	void PlowDebris();

	// ---- peças que soltam e vidros (lote 18b, docs/regras-pecas.md) ----

	/** O modelo do carro nas tabelas de peças e vidros e nos nomes das malhas (SK_DN<Modelo>_<Peça>). */
	static constexpr const TCHAR* CarModelId = TEXT("SportsCar");
	const TArray<TObjectPtr<ADNCarPart>>& GetParts() const { return Parts; }
	/** Estágio da peça (Presa se o carro não tem a peça). */
	EDNPartStage PartStage(EDNDamagePart Part) const;
	/** Força da trinca e se estourou, por vidro (0 e falso se o carro não tem o vidro). */
	float GlassCrack(EDNGlass Glass) const;
	bool IsGlassBurst(EDNGlass Glass) const;

	/** Ferramenta de captura (console ou EXEC: do -DNKeys): leva a zona (0 frente, 1 traseira, 2 esquerda, 3 direita, 4 teto) a
	 *  pelo menos Damage (0 a 1), sem batida. Os alvos de deformação seguem a regra de sempre (FDNDamageMorph). */
	UFUNCTION(Exec)
	void DNZone(int32 Zone, float Damage);

	/** Peso atual do alvo de deformação da zona (0 a 1); 0 sem a carroceria com alvos. */
	float MorphWeight(int32 Zone) const { return Zone >= 0 && Zone < FDNDamageMorph::ZoneCount ? MorphWeights[Zone] : 0.0f; }

	int32 DentCount() const { return DamageState.Dents.Num(); }
	int32 MaxDentCount() const { return DamageConfig.MaxDents; }
	bool IsDentMaterialOn() const { return DentMaterial != nullptr; }
	bool IsDestroyed() const { return DamageState.bDestroyed; }
	float MaxRealImpactMs() const { return MaxRealDeltaVMs; }
	int32 RealImpactTotal() const { return RealImpactCount; }

	/** Câmera parada olhando o carro de um ângulo (YawDegrees em relação à frente), para as capturas de tela, com FOV próprio. */
	void SetShotCamera(float YawDegrees);
	bool bShotView = false;
	static constexpr float ShotFovDegrees = 60.0f;

	/** Volta a câmera ao modo escolhido (Data/Cameras.csv). */
	void RestoreChaseCamera();

	/** Próximo modo de câmera (tecla C), na ordem de Cameras.csv. */
	void CycleCamera();
	/** Muda para o modo de câmera Index (fora da lista: o primeiro). */
	void SetCameraMode(int32 Index);
	FString CameraName() const;

	/** Assistência de derrapagem ligada (padrão sim); -DNNoAssist desliga para as medições de comparação. */
	bool bDriftAssist = true;

	/** -DNNoHud: sem o HUD de depuração (capturas para o Felipe). */
	bool bHudHidden = false;

	// ---- câmera de velocidade e de batida (lote 18) ----

	/** Liga (1) ou desliga (0) o desfoque nas bordas; para as capturas com e sem. */
	UFUNCTION(Exec)
	void DNEdgeBlur(int32 bOn);

	/** Liga (1) ou desliga (0) o tremor de velocidade: o multiplicador global que o menu de acessibilidade vai guardar no save. */
	UFUNCTION(Exec)
	void DNShake(int32 bOn);

	/** Ferramenta de captura (lote 18b): mostra (1) ou esconde (0) os vidros, para conferir o que fica atrás deles. */
	UFUNCTION(Exec)
	void DNShowGlass(int32 bOn);

	EDNCrashCamPhase CrashCamPhase() const { return CrashCam.Phase; }
	/** Escala de tempo que a câmera de batida pediu neste quadro (1 fora dela). */
	float CrashTimeScale() const { return AppliedTimeScale; }
	/** Campo de visão mostrado agora (com velocidade e turbo) e a força do desfoque nas bordas. */
	float DisplayedFov() const { return DisplayedFovDeg; }
	float EdgeBlurStrength() const { return AppliedEdgeBlur; }
	/** Tempo real desde o último disparo da câmera de batida [s]. */
	float CrashCamSeconds() const { return CrashCamClockS; }

	/** A vista da câmera de batida por cima da câmera do modo (CalcCamera do ator dá a do componente Camera). */
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

	/** Ângulo de deslize agora [graus] (positivo = velocidade à direita do nariz). */
	float SlipAngleDegNow() const;

	/** Linhas do HUD de debug: preset, velocidade, turbo e amassados. */
	TArray<FString> HudLines() const;

protected:

	virtual void PreRegisterAllComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	/** Devolve a escala de tempo a 1 se o carro sair no meio da câmera lenta. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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
	void RestartKey(const FInputActionValue& Value);

	/** Batida real do corpo físico (colisão com a parede, com outros carros...). */
	UFUNCTION()
	void OnCarHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void PushDentsToMaterial();

	/** Leva os pesos dos alvos de deformação na direção do dano de cada zona (FDNDamageMorph) e aplica na carroceria. */
	void UpdateDamageMorphs(float DeltaSeconds);

	/** Manda os pesos das zonas ao desgaste da pintura (lascas, metal nu, riscos e fuligem; lote 17b). */
	void PushWearToMaterial();

	/** Lote 18b: cria as peças e os vidros do carro (malhas em /Game/Detonauto/Vehicle/<Modelo>/Parts). */
	void SpawnPartsAndGlass();
	/** Põe os materiais das peças presas e dos vidros na lista dos amassados (recebem amassado e desgaste como a carroceria). */
	void RegisterPartMaterials();
	/** Depois de um impacto já aplicado ao dano (nulo: só o dano acumulado, como na ferramenta DNZone): pendura e solta as peças e trinca
	 *  e estoura os vidros pelas regras do Core (FDNCarParts, FDNGlass). */
	void EvaluatePartsAndGlass(const FDNImpact* Impact);
	/** Manda a trinca de cada vidro ao material dele. */
	void PushGlassCrack();
	/** Carro novo: peças presas, vidros inteiros. */
	void ResetPartsAndGlass();

	/** Fecha a janela de contatos vencida e aplica a batida (o maior valor da janela). */
	void FlushImpacts(double Now);
	void GetBodyBox(FVector& OutCenter, FVector& OutExtent) const;
	/** Limites locais da malha da carroceria (estática ou com esqueleto); falso sem malha. */
	bool BodyAssetBounds(FBoxSphereBounds& OutBounds) const;

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
	/** Peso de cada alvo de deformação, na ordem de EDNDamageZone (sobe com o dano, nunca desce). */
	float MorphWeights[FDNDamageMorph::ZoneCount] = {};
	float MaxRealDeltaVMs = 0.0f;
	int32 RealImpactCount = 0;
	int32 LoggedImpacts = 0;

	// batida real: velocidade do corpo no início do quadro (antes do passo de física) e a janela de contatos
	FVector VelocityBeforeStepCmS = FVector::ZeroVector;
	/** Giro do corpo no início do quadro: a peça de rua devolve o carro com o giro de antes do contato (sem guinada). */
	FVector AngularBeforeStepDegS = FVector::ZeroVector;
	/** Batida real vira dano (EnableDentMaterial com bBindRealHits). O aviso de contato fica sempre ligado, por causa das peças. */
	bool bRealHitDamage = false;
	/** Acelerador apertado pela entrada do jogador (para registrar no log quando liga). */
	bool bThrottleHeld = false;
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

	// ---- peças que soltam e vidros (lote 18b) ----
	TArray<FDNCarPartSpec> PartSpecs;
	TArray<EDNPartStage> PartStages;
	/** Os materiais dinâmicos de cada peça (saem da lista dos amassados quando ela solta: voa já amassada). */
	TArray<TArray<TObjectPtr<UMaterialInstanceDynamic>>> PartMaterials;
	TArray<FDNGlassSpec> GlassSpecs;
	TArray<bool> GlassBurst;
	TArray<TArray<TObjectPtr<UMaterialInstanceDynamic>>> GlassMaterials;
	/** Onde cada vidro trinca, no espaço do carro: o ponto da batida mais perto dele quando a trinca começa [cm]. */
	TArray<FVector> GlassCrackCenter;
	/** Ponto da última batida no espaço do carro (para o centro da trinca e o lado do para-choque) [cm]. */
	FVector LastImpactLocal = FVector::ZeroVector;
	/** Onde o carro nasceu (a largada do DNRestart). */
	FTransform StartTransform = FTransform::Identity;
	/** Quadros sem o atraso de rotação da câmera depois do DNRestart (a câmera chega junto com o carro). */
	int32 CameraSnapFrames = 0;

	/** Velocidade com que o turbo entra e sai do campo de visão (o quanto ele soma é TurboFovAddDeg de Data/CameraFx.csv). */
	static constexpr float TurboFovBlendSpeed = 5.0f;
	float TurboFovBlend = 0.0f;

	// ---- câmera de velocidade e de batida (lote 18) ----

	/** Campo de visão, tremor e desfoque deste quadro, em tempo real. */
	void UpdateSpeedCamera(float RealDt);
	/** Avança a câmera de batida, aplica a escala de tempo e o controle do destroço, em tempo real. */
	void UpdateCrashCam(float RealDt);
	/** Contato real do jogador: dispara a câmera de batida se a regra deixar. */
	void TryCrashCam(float DeltaVMs, EDNImpactSource Source, const FVector& WorldPoint);

	FDNCameraFxSettings CameraFx;
	float DisplayedFovDeg = 95.0f;
	double ShakeTimeS = 0.0;
	float ShakeGlobalScale = 1.0f;
	bool bEdgeBlurOn = true;
	/** -DNEdgeBlurForce=S: desfoque com força fixa S em qualquer velocidade, só para medir o custo (negativo = desligado). */
	float EdgeBlurForce = -1.0f;
	bool bCrashCamOn = true;
	float AppliedEdgeBlur = 0.0f;
	float AppliedTimeScale = 1.0f;
	/** Entrada de esterço do jogador (-1 a 1), para o controle do destroço. */
	float SteerInputRaw = 0.0f;

	FDNCrashCamState CrashCam;
	/** Ângulo da órbita em volta do carro [graus, no mundo] e o sentido do giro (+1 ou -1, para o lado da batida). */
	float OrbitYawDeg = 0.0f;
	float OrbitSign = 1.0f;
	/** A câmera de batida acabou de disparar: a órbita começa onde a câmera do modo está (sem corte). */
	bool bOrbitPending = false;
	/** Para onde fica a batida, visto do carro [graus de guinada, no mundo]: a órbita gira para esse lado. */
	float CrashImpactYawDeg = 0.0f;
	/** Tempo real desde o disparo [s], para o log e as capturas. */
	float CrashCamClockS = 0.0f;
	/** Fração do caminho do carro até a órbita que a câmera usa (a esfera de parede encurta na hora e volta devagar). */
	float ProbeFraction = 1.0f;
	int32 ProbeLogs = 0;
	/** Quanto o controle do destroço já mudou a velocidade nesta batida [m/s], e a posição do carro no disparo (para o log). */
	float WreckSteerUsedMs = 0.0f;
	FVector CrashStartLocation = FVector::ZeroVector;
	/** Vista da última CalcCamera. */
	FRotator LastViewRotation = FRotator::ZeroRotator;
	/** A direita da tela na câmera do jogador no instante da batida: o lado do controle do destroço. A da órbita não serve: ela fica ao
	 *  lado do carro, e a direita dela corre ao longo da velocidade (o lado da perpendicular ficaria indefinido). */
	FVector CrashScreenRight = FVector::RightVector;

	/** Rotação do osso de cada roda no pose de referência (espaço do componente): converte o curso da suspensão para o espaço do osso. */
	TArray<FQuat> WheelBoneRotations;
};
