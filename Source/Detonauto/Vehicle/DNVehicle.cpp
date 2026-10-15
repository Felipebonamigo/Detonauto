#include "Vehicle/DNVehicle.h"
#include "Vehicle/DNCarPart.h"
#include "Vehicle/DNGlassShards.h"
#include "Vehicle/DNVehicleWheels.h"
#include "Vehicle/DNTurboComponent.h"
#include "Vehicle/DNCarEffectsComponent.h"
#include "Effects/DNEffectsSubsystem.h"
#include "Crash/DNBreakableProp.h"
#include "Traffic/DNTrafficVehicle.h"
#include "Core/DNBreakable.h"
#include "Core/DNGameDataSubsystem.h"
#include "Core/DNWheelVisual.h"
#include "Detonauto.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "AnimationRuntime.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/OverlapResult.h"
#include "Misc/App.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraTypes.h"

namespace
{
	// Assets do carro esportivo do template, migrados para Content/Detonauto (Tools/Unreal/migrate_sports_car.ps1). Caminho completo
	// (Pacote.Objeto): o FSoftObjectPath não completa o nome do objeto como o ConstructorHelpers fazia (sem ele, a carga volta nula)
	const TCHAR* SkeletalMeshPath = TEXT("/Game/Detonauto/Vehicle/SportsCar/SK_SportsCar.SK_SportsCar");
	const TCHAR* DamageBodyPath = TEXT("/Game/Detonauto/Vehicle/SportsCar/SK_DNSportsCarBody.SK_DNSportsCarBody");
	const TCHAR* BodyMeshPath = TEXT("/Game/Detonauto/Vehicle/SportsCar/SM_SportsCar.SM_SportsCar");
	const TCHAR* GlassMeshPath = TEXT("/Game/Detonauto/Vehicle/SportsCar/SM_SportsCar_Glass.SM_SportsCar_Glass");
	const TCHAR* WheelMeshPath = TEXT("/Game/Detonauto/Vehicle/SportsCar/SM_SportsCar_Wheel.SM_SportsCar_Wheel");
	const TCHAR* DentParentPath = TEXT("/Game/Detonauto/Vehicle/M_DNDentParent.M_DNDentParent");
	/** Peças que soltam e vidros (lote 18b): Tools/Unreal/damage-body.ps1 -Parts. Nome: SK_DN<Modelo>_<Peça> e SK_DN<Modelo>_Glass_<Vidro>. */
	const TCHAR* PartsDir = TEXT("/Game/Detonauto/Vehicle/SportsCar/Parts");
	/** Cacos por vidro que estoura. */
	constexpr int32 ShardsPerGlass = 40;
	/** Lente de farol e lanterna (material translúcido do template, sem luz própria): a trinca deixa a lente fosca. */
	constexpr float LensOpacity = 0.03f;
	constexpr float LensCrackedOpacity = 0.55f;
	/** Quanto o centro da trinca entra na caixa do vidro, em fração da meia caixa (0: na borda; 1: no meio). */
	constexpr float GlassCrackInset = 0.7f;
	/** Lote 19: um choque dá vários contatos em quadros seguidos; as faíscas de impacto saem uma vez a cada tanto [s do jogo]. */
	constexpr double ImpactFxGapS = 0.15;
	/** Raspão: uma rajada de faíscas a cada tanto por peça [s do jogo]; a primeira cobre este intervalo. 0,08 s com vida de 0,5 s: ~6
	 *  rajadas vivas por peça, abaixo do pool de 8 (a 0,05 s o pool reciclava rajada viva) */
	constexpr double ScrapeFxGapS = 0.08;
	constexpr float FirstScrapeIntervalS = 0.1f;

	/** Desfoque nas bordas (lote 18): Tools/Unreal/create_edge_blur_material.ps1. */
	const TCHAR* EdgeBlurPath = TEXT("/Game/Detonauto/Camera/M_DNEdgeBlur.M_DNEdgeBlur");

	const TCHAR* CrashPhaseName(EDNCrashCamPhase Phase)
	{
		switch (Phase)
		{
		case EDNCrashCamPhase::Entering: return TEXT("entrando");
		case EDNCrashCamPhase::Holding: return TEXT("segurando");
		case EDNCrashCamPhase::Exiting: return TEXT("saindo");
		case EDNCrashCamPhase::Cooldown: return TEXT("espera");
		default: return TEXT("parada");
		}
	}

	/** Altura do ponto que a câmera de batida olha, sobre a origem do carro [cm]. */
	constexpr float CrashCamAimUpCm = 60.0f;
	/** Quanto do caminho a câmera de batida recupera por segundo real depois que a esfera deixa de bater (ver CalcCamera). Detalhe do
	 *  suavizado da colisão, não ajuste de jogo: fica aqui. Se precisar de ajuste por cena, vai para CameraFx.csv. */
	constexpr float CrashCamProbeRecoverPerS = 1.5f;

	const FName WheelBones[4] = { FName("Phys_Wheel_FL"), FName("Phys_Wheel_FR"), FName("Phys_Wheel_BL"), FName("Phys_Wheel_BR") };

	/** Base da malha da roda em cada osso [graus em Z]: a malha tem o eixo do cubo no Y local; 180 vira a face do aro para o outro lado
	 *  (as rodas da esquerda olham para -Y). Conferido na captura lateral (lote 16). */
	const float WheelMeshBaseYaw[4] = { 180.0f, 0.0f, 180.0f, 0.0f };

	/** Variações de velocidade das batidas simuladas: dão profundidade pequena (4,5 cm), média (12 cm) e máxima (25 cm). */
	const float SimulatedDeltaV[3] = { 5.0f, 10.0f, 20.0f };

	/** Modo de teste: depois de tantos segundos com a barra vazia e o turbo desligado, a barra enche de novo. */
	constexpr float TurboTestRefillSeconds = 2.0f;

	const UDNGameDataSubsystem* DataSubsystemOf(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UDNGameDataSubsystem>() : nullptr;
	}
}

ADNVehicle::ADNVehicle()
{
	PrimaryActorTick.bCanEverTick = true;

	// --- malhas: o construtor só guarda os caminhos; quem carrega é LoadCarMeshes, antes do registro dos componentes. Carregar aqui
	// (ConstructorHelpers) fazia a partida de todo commandlet carregar as malhas, as instâncias dos slots e o M_DNDentParent, que
	// ficavam presos como raiz: refazer o material quebrava com "Assertion failed: !IsRooted()" (lote 17b, provado tirando as malhas)
	ChassisMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(SkeletalMeshPath));
	DamageBodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(DamageBodyPath));
	BodyMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(BodyMeshPath));
	GlassMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(GlassMeshPath));
	WheelMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(WheelMeshPath));

	// Physics Asset vem junto com a malha do chassi. Sem Anim Blueprint: as rodas visuais são movidas em UpdateWheelVisuals
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(FName("Vehicle"));

	// --- partes visuais, presas ao esqueleto, ainda sem malha ---
	auto MakeVisual = [this](const TCHAR* Name, FName Bone) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(GetMesh(), Bone);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Comp;
	};
	// lote 17: a carroceria com os 5 alvos de deformação e os faróis (SM_SportsCar_Glass) unidos; sem ela, a malha estática e o vidro
	BodySkin = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BodySkin"));
	BodySkin->SetupAttachment(GetMesh());
	BodySkin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyStatic = MakeVisual(TEXT("Body"), NAME_None);
	GlassStatic = MakeVisual(TEXT("Glass"), NAME_None);
	const TCHAR* Names[4] = { TEXT("WheelFL"), TEXT("WheelFR"), TEXT("WheelBL"), TEXT("WheelBR") };
	for (int32 i = 0; i < 4; ++i)
	{
		WheelVisuals.Add(MakeVisual(Names[i], WheelBones[i]));
	}

	// --- câmera de perseguição ---
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 650.0f;
	CameraBoom->SocketOffset.Z = 150.0f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 2.0f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);
	// Sem desfoque de movimento: a roda girando e o chão ficavam borrados (playtest 1: "embaçadas"); o Felipe não quer o efeito.
	Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
	Camera->PostProcessSettings.MotionBlurAmount = 0.0f;
	Camera->PostProcessSettings.bOverride_MotionBlurMax = true;
	Camera->PostProcessSettings.MotionBlurMax = 0.0f;
	Camera->PostProcessBlendWeight = 1.0f;

	// --- turbo: cola fina sobre o FDNTurbo do Core; quem tem as entradas (este carro) chama Step() a cada quadro ---
	Turbo = CreateDefaultSubobject<UDNTurboComponent>(TEXT("Turbo"));
	Effects = CreateDefaultSubobject<UDNCarEffectsComponent>(TEXT("Effects"));

	// --- Chaos: chassi, rodas, motor, transmissão e direção, tudo em C++ ---
	UChaosWheeledVehicleMovementComponent* Move = Chaos();

	// as rodas não sobem em destroço de peça de rua (o traço da suspensão corre no canal WorldDynamic; ver ECC_DNDebris)
	Move->SetWheelTraceResponseToChannel(ECC_DNDebris, ECR_Ignore);
	Move->ChassisHeight = 144.0f;
	Move->DragCoefficient = 0.31f;
	Move->bLegacyWheelFrictionPosition = true;

	Move->WheelSetups.SetNum(4);
	Move->WheelSetups[0].WheelClass = UDNVehicleWheelFront::StaticClass();
	Move->WheelSetups[1].WheelClass = UDNVehicleWheelFront::StaticClass();
	Move->WheelSetups[2].WheelClass = UDNVehicleWheelRear::StaticClass();
	Move->WheelSetups[3].WheelClass = UDNVehicleWheelRear::StaticClass();
	for (int32 i = 0; i < 4; ++i)
	{
		Move->WheelSetups[i].BoneName = WheelBones[i];
		Move->WheelSetups[i].AdditionalOffset = FVector::ZeroVector;
	}

	Move->Mass = 1500.0f;
	Move->EngineSetup.MaxTorque = 750.0f;
	Move->EngineSetup.MaxRPM = 7000.0f;
	Move->EngineSetup.EngineIdleRPM = 900.0f;
	Move->EngineSetup.EngineBrakeEffect = 0.2f;
	Move->EngineSetup.EngineRevUpMOI = 5.0f;
	Move->EngineSetup.EngineRevDownRate = 600.0f;
	// curva de torque: normalizada pelo máximo (X = RPM)
	FRichCurve* Torque = Move->EngineSetup.TorqueCurve.GetRichCurve();
	Torque->Reset();
	// a curva do template (BP_SportsCar_Pawn, lida do Blueprint no lote 14): a do protótipo era outra (0,0.6)(2000,0.9)(4500,1)(7000,0.7)
	Torque->AddKey(1000.0f, 0.5f);
	Torque->AddKey(1800.0f, 0.9f);
	Torque->AddKey(3500.0f, 1.0f);
	Torque->AddKey(6000.0f, 0.8f);
	Torque->AddKey(7000.0f, 0.0f);

	Move->TransmissionSetup.bUseAutomaticGears = true;
	Move->TransmissionSetup.bUseAutoReverse = true;
	Move->TransmissionSetup.FinalRatio = 2.81f;
	Move->TransmissionSetup.ChangeUpRPM = 6000.0f;
	Move->TransmissionSetup.ChangeDownRPM = 2000.0f;
	Move->TransmissionSetup.GearChangeTime = 0.2f;
	Move->TransmissionSetup.TransmissionEfficiency = 0.9f;
	Move->TransmissionSetup.ForwardGearRatios = { 4.25f, 2.52f, 1.66f, 1.22f, 1.0f };
	Move->TransmissionSetup.ReverseGearRatios = { 4.04f };

	Move->SteeringSetup.SteeringType = ESteeringType::Ackermann;
	Move->SteeringSetup.AngleRatio = 0.7f;
}

UChaosWheeledVehicleMovementComponent* ADNVehicle::Chaos() const
{
	return CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
}

bool ADNVehicle::LoadCar(FName Id, FDNCarConfig& OutCar) const
{
	const UDNGameDataSubsystem* Data = DataSubsystemOf(this);
	if (!Data)
	{
		UE_LOG(LogDetonauto, Error, TEXT("ADNVehicle: sem subsistema de dados (sem mundo de jogo); preset '%s' não carregado"), *Id.ToString());
		return false;
	}
	if (!Data->TryGetCarConfig(Id.ToString(), OutCar))
	{
		UE_LOG(LogDetonauto, Error, TEXT("ADNVehicle: o preset '%s' não existe em Data/Cars.csv ou a tabela é inválida"), *Id.ToString());
		return false;
	}
	return true;
}

const TArray<FName>& ADNVehicle::HotkeyRows()
{
	static const TArray<FName> Rows = { FName(TEXT("arcade-1")), FName(TEXT("arcade-2")), FName(TEXT("arcade-3")), FName(TEXT("Padrao")) };
	return Rows;
}

void ADNVehicle::WriteCreationParameters(const FDNCarConfig& C)
{
	UChaosWheeledVehicleMovementComponent* Move = Chaos();

	// Massa e torque entram na criação do veículo (UpdateMassProperties usa Mass; SetMassOverrideInKg não vale aqui).
	Move->Mass = C.MassKg;
	Move->EngineSetup.MaxTorque = C.MaxTorque;

	// O Chaos lê as classes de roda (CDO) ao criar o veículo; a barra estabilizadora (RollbarScaling) só existe aí:
	// não há função para mudá-la depois. Por isso gravamos nas classes e, ao trocar de preset, recriamos o estado físico.
	UDNVehicleWheelFront* Front = GetMutableDefault<UDNVehicleWheelFront>();
	UDNVehicleWheelRear* Rear = GetMutableDefault<UDNVehicleWheelRear>();
	for (UChaosVehicleWheel* Wheel : { static_cast<UChaosVehicleWheel*>(Front), static_cast<UChaosVehicleWheel*>(Rear) })
	{
		Wheel->SpringRate = C.SuspensionStiffness;
		Wheel->SuspensionDampingRatio = C.SuspensionDampingRatio;
		Wheel->SuspensionMaxRaise = C.SuspensionTravelCm;
		Wheel->SuspensionMaxDrop = C.SuspensionTravelCm;
		Wheel->RollbarScaling = C.RollbarScaling;
	}
	Rear->FrictionForceMultiplier = UDNVehicleWheelRear::TemplateFriction * C.RearGripFactor;
	Rear->MaxHandBrakeTorque = C.HandbrakeTorque;

	AppliedSteeringGain = C.SteeringGain;
}

void ADNVehicle::ApplyRuntimeParameters(const FDNCarConfig& C)
{
	// centro de massa: deslocamento em relação ao calculado pela malha (COMNudge do corpo)
	GetMesh()->SetCenterOfMass(FVector(0.0f, 0.0f, -C.CenterOfMassDropCm), NAME_None);

	// aderência: a lateral (curva de deslizamento) vale nas 4 rodas; a traseira também leva o fator do eixo e o freio de mão
	HandbrakeRamp = 0.0f;
	AppliedRearMultiplier = -1.0f;
	ApplyWheelGrip(true);

	if (const FBodyInstance* Body = GetMesh()->GetBodyInstance())
	{
		UE_LOG(LogDetonauto, Display,
			TEXT("ADNVehicle: preset '%s' aplicado: massa pedida=%.0f kg (no corpo=%.1f kg), COM local=%s (queda pedida=%.0f cm), torque=%.0f, "
				"aderência lateral=%.2f, traseira x%.2f, freio de mão traseira x%.2f (rampa %.2f/%.2f s, torque %.0f), mola=%.0f, amortecimento=%.2f, "
				"curso=%.0f cm, barra=%.2f, ganho do volante=%.2f, turbo=%.0f m/s², assistência de derrapagem: início %.0f graus, ganho %.1f, teto %.0f"),
			*CarId.ToString(), C.MassKg, Body->GetBodyMass(), *Body->GetMassSpaceLocal().GetTranslation().ToCompactString(), C.CenterOfMassDropCm,
			C.MaxTorque, C.LateralGrip, C.RearGripFactor, C.HandbrakeRearGrip, C.HandbrakeRampInSeconds, C.HandbrakeRampOutSeconds, C.HandbrakeTorque,
			C.SuspensionStiffness, C.SuspensionDampingRatio, C.SuspensionTravelCm, C.RollbarScaling, C.SteeringGain,
			C.NitroAccelMps2, C.DriftAssistStartDeg, C.DriftAssistGain, C.DriftAssistMax);
	}
}

void ADNVehicle::ApplyWheelGrip(bool bForce)
{	const float HandbrakeMultiplier = FDNHandbrakeRamp::RearGripMultiplier(Car.HandbrakeRearGrip, HandbrakeRamp);
	if (!bForce && FMath::IsNearlyEqual(HandbrakeMultiplier, AppliedRearMultiplier, 0.002f))
	{
		return;
	}
	AppliedRearMultiplier = HandbrakeMultiplier;

	UChaosWheeledVehicleMovementComponent* Move = Chaos();
	for (int32 i = 0; i < Move->Wheels.Num(); ++i)
	{
		const UChaosVehicleWheel* Wheel = Move->Wheels[i];
		const bool bRear = Wheel && Wheel->AxleType == EAxleType::Rear;
		const float AxleFactor = bRear ? Car.RearGripFactor * HandbrakeMultiplier : 1.0f;
		Move->SetWheelSlipGraphMultiplier(i, Car.LateralGrip * AxleFactor);
		if (bRear)
		{
			Move->SetWheelFrictionMultiplier(i, UDNVehicleWheelRear::TemplateFriction * AxleFactor);
		}
	}
}

bool ADNVehicle::SwitchPreset(FName Id)
{
	FDNCarConfig New;
	if (!LoadCar(Id, New))
	{
		return false;
	}
	CarId = Id;
	Car = New;
	bCarLoaded = true;

	WriteCreationParameters(Car);
	if (Chaos()->IsRegistered())
	{
		// Recriar o estado físico zera a velocidade do corpo e deixa a transmissão em ponto morto: guardamos a velocidade
		// e deixamos o câmbio automático engatar de novo com o acelerador.
		const FVector LinearVelocity = GetMesh()->GetPhysicsLinearVelocity();
		const FVector AngularVelocity = GetMesh()->GetPhysicsAngularVelocityInDegrees();

		Chaos()->RecreatePhysicsState();

		GetMesh()->SetPhysicsLinearVelocity(LinearVelocity);
		GetMesh()->SetPhysicsAngularVelocityInDegrees(AngularVelocity);
		// A cópia local da marcha alvo precisa voltar a 0, igual ao ResetVehicleState() da engine: com ela diferente
		// da marcha real (ponto morto), o câmbio automático nunca engata a 1ª.
		Chaos()->SetTargetGear(0, true);
	}
	ApplyRuntimeParameters(Car);
	return true;
}

void ADNVehicle::LoadCarMeshes()
{
	if (USkeletalMesh* Chassis = ChassisMesh.LoadSynchronous())
	{
		GetMesh()->SetSkeletalMesh(Chassis);
	}
	USkeletalMesh* Damage = DamageBodyMesh.LoadSynchronous();
	BodySkin->SetSkeletalMesh(Damage);
	BodyStatic->SetStaticMesh(Damage ? nullptr : BodyMesh.LoadSynchronous());
	GlassStatic->SetStaticMesh(Damage ? nullptr : GlassMesh.LoadSynchronous());
	BodyComp = Damage ? static_cast<UMeshComponent*>(BodySkin) : (BodyStatic->GetStaticMesh() ? static_cast<UMeshComponent*>(BodyStatic) : nullptr);
	UStaticMesh* Wheel = WheelMesh.LoadSynchronous();
	for (UStaticMeshComponent* Visual : WheelVisuals)
	{
		Visual->SetStaticMesh(Wheel);
	}
}

void ADNVehicle::PreRegisterAllComponents()
{
	// as malhas entram antes do registro: o Chaos cria o veículo com o Physics Asset do chassi
	LoadCarMeshes();
	Super::PreRegisterAllComponents();

	// -DNCarRow=arcade-2 na linha de comando escolhe o preset sem abrir o editor
	FString IdFromCommandLine;
	if (FParse::Value(FCommandLine::Get(), TEXT("DNCarRow="), IdFromCommandLine))
	{
		CarId = FName(*IdFromCommandLine);
	}

	// Antes do registro: é o último momento em que dá para mexer no que o Chaos lê ao criar o veículo.
	bCarLoaded = LoadCar(CarId, Car);
	if (bCarLoaded)
	{
		WriteCreationParameters(Car);
	}
}

void ADNVehicle::BeginPlay()
{
	Super::BeginPlay();
	StartTransform = GetActorTransform();

	if (bCarLoaded)
	{
		ApplyRuntimeParameters(Car);
	}

	// câmera: os modos de Data/Cameras.csv; -DNCamera=<índice> começa num modo (ferramentas de teste)
	if (const UDNGameDataSubsystem* Data = DataSubsystemOf(this))
	{
		CameraModes = Data->GetCameraModes();
	}
	if (CameraModes.IsEmpty())
	{
		CameraModes.Add(FDNCameraMode{ TEXT("alta-longe"), FDNCameraConfig() });
	}
	int32 StartCamera = 0;
	FParse::Value(FCommandLine::Get(), TEXT("DNCamera="), StartCamera);
	bDriftAssist = !FParse::Param(FCommandLine::Get(), TEXT("DNNoAssist"));
	// capturas que vão para o Felipe: sem o texto de depuração (as de revisão continuam com ele)
	bHudHidden = FParse::Param(FCommandLine::Get(), TEXT("DNNoHud"));
	SetCameraMode(StartCamera);

	// lote 18: câmera de velocidade e de batida (Data/CameraFx.csv). Só o carro do jogador: os estacionados das ferramentas de medição
	// não mexem na escala de tempo nem pagam o desfoque
	if (const UDNGameDataSubsystem* Data = DataSubsystemOf(this))
	{
		CameraFx = Data->GetCameraFxSettings();
	}
	{
		const TCHAR* Cmd = FCommandLine::Get();
		bEdgeBlurOn = !FParse::Param(Cmd, TEXT("DNNoEdgeBlur"));
		ShakeGlobalScale = FParse::Param(Cmd, TEXT("DNNoShake")) ? 0.0f : 1.0f;
		bCrashCamOn = !FParse::Param(Cmd, TEXT("DNNoCrashCam"));
		FParse::Value(Cmd, TEXT("DNEdgeBlurForce="), EdgeBlurForce);
		if (UMaterialInterface* Blur = LoadObject<UMaterialInterface>(nullptr, EdgeBlurPath))
		{
			EdgeBlurMaterial = UMaterialInstanceDynamic::Create(Blur, this);
			EdgeBlurMaterial->SetScalarParameterValue(TEXT("Strength"), 0.0f);
			EdgeBlurMaterial->SetScalarParameterValue(TEXT("InnerRadius"), CameraFx.EdgeBlurInnerRadius);
			Camera->PostProcessSettings.AddBlendable(EdgeBlurMaterial, 0.0f); // peso 0: a engine nem roda o passe parado ou devagar
		}
		else
		{
			UE_LOG(LogDetonauto, Warning, TEXT("ADNVehicle: sem o material do desfoque nas bordas (%s). Rode Tools/Unreal/create_edge_blur_material.ps1"), EdgeBlurPath);
		}
		UE_LOG(LogDetonauto, Display, TEXT("ADNVehicle: câmera de velocidade e de batida: desfoque nas bordas %s, tremor %s, câmera de batida %s (dispara a %.1f m/s, escala %.2f)"),
			EdgeBlurMaterial && bEdgeBlurOn ? TEXT("ligado") : TEXT("desligado"), ShakeGlobalScale > 0.0f ? TEXT("ligado") : TEXT("desligado"),
			bCrashCamOn ? TEXT("ligada") : TEXT("desligada"), CameraFx.CrashCamMinDeltaVMs, CameraFx.CrashCamTimeScale);
	}

	// os ossos das rodas no pose de referência (sem Anim Blueprint, é o pose em que o esqueleto fica)
	WheelBoneRotations.Reset();
	if (const USkeletalMesh* Skeletal = GetMesh()->GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& RefSkeleton = Skeletal->GetRefSkeleton();
		for (const FName& Bone : WheelBones)
		{
			const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
			WheelBoneRotations.Add(BoneIndex != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex).GetRotation() : FQuat::Identity);
		}
	}

	// modo de teste: a barra começa cheia. O Core enche a barra no takedown; um quadro de takedown sem tempo é o jeito de
	// fazer isso sem mexer no estado do Core (o turbo só ganha por eventos).
	if (bTurboTestMode)
	{
		FDNTurboFrameInput Fill;
		Fill.bTakedown = true;
		Turbo->Step(Fill);
	}

	// contato do corpo físico sempre avisado: as peças de rua quebram mesmo com o dano desligado (lote 17c)
	GetMesh()->SetNotifyRigidBodyCollision(true);
	GetMesh()->OnComponentHit.AddUniqueDynamic(this, &ADNVehicle::OnCarHit);

	// lote 18b: as peças que soltam e os vidros. Sempre: a carroceria tem os vãos no lugar delas, e sem elas o carro sairia esburacado.
	// -DNNoParts só serve para comparar com a carroceria inteira de antes (capturas de antes e depois)
	if (!FParse::Param(FCommandLine::Get(), TEXT("DNNoParts")))
	{
		SpawnPartsAndGlass();
	}

	// Dano e amassado visual: ligado por padrão para o Felipe testar jogando; -DNNoDents desliga (medição de referência);
	// -DNDentMaterial liga só o material, sem amassados; -DNDents=N liga o material e aplica N amassados de demonstração.
	{
		const UDNGameDataSubsystem* Data = DataSubsystemOf(this);
		DamageConfig = Data ? Data->GetDamageConfig() : FDNDamageConfig();
		DamageConfig.MaxDents = FDNDentParams::EffectiveMaxDents(DamageConfig.MaxDents);

		const TCHAR* Cmd = FCommandLine::Get();
		int32 DemoDents = 0;
		FParse::Value(Cmd, TEXT("DNDents="), DemoDents);
		const bool bNoDents = FParse::Param(Cmd, TEXT("DNNoDents"));
		const bool bForceMaterial = FParse::Param(Cmd, TEXT("DNDentMaterial")) || DemoDents > 0;
		if (bForceMaterial || !bNoDents)
		{
			// Em medição (-DNDentMaterial ou -DNDents=N) a batida real fica desligada: o que se mede é só o custo do material.
			if (EnableDentMaterial(!bForceMaterial) && DemoDents > 0)
			{
				ApplyDemoDents(DemoDents);
			}
		}
	}
}

float ADNVehicle::SpeedKmh() const
{
	return GetVelocity().Size() * 0.036f; // cm/s -> km/h
}

int32 ADNVehicle::WheelCount() const
{
	const UChaosWheeledVehicleMovementComponent* Move = Chaos();
	return Move ? Move->GetNumWheels() : 0;
}

const FWheelStatus* ADNVehicle::WheelStatus(int32 Index) const
{
	const UChaosWheeledVehicleMovementComponent* Move = Chaos();
	return Move && Index >= 0 && Index < Move->GetNumWheels() ? &Move->GetWheelState(Index) : nullptr;
}

FVector ADNVehicle::EngineBayWorld() const
{
	FVector Center, Extent;
	GetBodyBox(Center, Extent);
	const FTransform& Frame = BodyComp ? BodyComp->GetComponentTransform() : GetActorTransform();
	return Frame.TransformPosition(Center + FVector(0.6f * Extent.X, 0.0f, 0.35f * Extent.Z));
}

FString ADNVehicle::TurboText() const
{
	if (Car.NitroAccelMps2 <= 0.0f)
	{
		return TEXT("Turbo: desligado neste preset");
	}
	const float Bar = Turbo->GetBar();
	const float MaxBar = Turbo->GetMaxBar();
	const int32 Filled = MaxBar > 0.0f ? FMath::Clamp(FMath::RoundToInt(Bar / MaxBar * 10.0f), 0, 10) : 0;
	const FString BarText = FString::ChrN(Filled, TEXT('#')) + FString::ChrN(10 - Filled, TEXT('-'));
	const TCHAR* State = Turbo->IsActive() ? TEXT("ATIVO") : (Bar <= 0.0f ? TEXT("vazio") : TEXT("pronto"));
	return FString::Printf(TEXT("Turbo (Shift): %s  [%s] %d"), State, *BarText, FMath::RoundToInt(Bar));
}

TArray<FString> ADNVehicle::HudLines() const
{
	TArray<FString> Lines = {
		FString::Printf(TEXT("Preset: %s     (1 = arcade-1   2 = arcade-2   3 = arcade-3   0 = Padrao)     Câmera: %s (C)"), *CarId.ToString(), *CameraName()),
		FString::Printf(TEXT("Velocidade: %.0f km/h"), SpeedKmh()),
		TurboText() };
	if (DentMaterial)
	{
		Lines.Add(FString::Printf(TEXT("Amassados: %d de %d     maior batida: %.1f m/s (%d batidas)"), DamageState.Dents.Num(), DamageConfig.MaxDents, MaxRealDeltaVMs, RealImpactCount));
		Lines.Add(TEXT("Batida simulada:  T Y U = frente (pequena, media, maxima)   G H J = lado esquerdo   V = limpar"));
	}
	return Lines;
}

void ADNVehicle::UpdateHud() const
{
	if (!GEngine || !IsLocallyControlled() || bHudHidden)
	{
		return;
	}
	// Conferido numa captura de tela: a mensagem adicionada por último fica em cima. Por isso a ordem de chamada é a
	// inversa da ordem na tela (turbo, velocidade, preset).
	const FVector2D Scale(1.6f, 1.6f);
	const TArray<FString> Lines = HudLines();
	// as duas linhas de amassado entram primeiro: ficam EMBAIXO (a última adicionada fica em cima)
	if (Lines.Num() >= 5)
	{
		GEngine->AddOnScreenDebugMessage(9105, 0.0f, FColor::Silver, Lines[4], false, Scale);
		GEngine->AddOnScreenDebugMessage(9104, 0.0f, FColor::Orange, Lines[3], false, Scale);
	}
	// câmera de velocidade e de batida (lote 18): abaixo do turbo; a escala de tempo fica escrita em cada captura da câmera lenta
	{
		const bool bCrash = CrashCam.Phase != EDNCrashCamPhase::Idle;
		const FString CameraLine = FString::Printf(TEXT("Câmera: FOV %.1f   desfoque %.2f   |   câmera de batida: %s   escala de tempo %.2f   (%.2f s reais)"),
			DisplayedFovDeg, AppliedEdgeBlur, CrashPhaseName(CrashCam.Phase), AppliedTimeScale, bCrash ? CrashCamClockS : 0.0f);
		GEngine->AddOnScreenDebugMessage(9106, 0.0f, bCrash ? FColor::Red : FColor::Silver, CameraLine, false, Scale);
	}
	GEngine->AddOnScreenDebugMessage(9101, 0.0f, Turbo->IsActive() ? FColor::Cyan : FColor::White, Lines[2], false, Scale);
	GEngine->AddOnScreenDebugMessage(9102, 0.0f, FColor::White, Lines[1], false, Scale);
	GEngine->AddOnScreenDebugMessage(9103, 0.0f, FColor::Yellow, Lines[0], false, Scale);
}

void ADNVehicle::UpdateWheelVisuals()
{
	UChaosWheeledVehicleMovementComponent* Move = Chaos();
	if (!Move->HasValidPhysicsState() || Move->Wheels.Num() != WheelVisuals.Num() || WheelBoneRotations.Num() != WheelVisuals.Num())
	{
		return;
	}
	for (int32 i = 0; i < WheelVisuals.Num(); ++i)
	{
		const UChaosVehicleWheel* Wheel = Move->Wheels[i];
		if (!Wheel || !WheelVisuals[i] || !Move->PhysicsVehicleOutput().IsValid())
		{
			continue;
		}
		// Os mesmos três números que o UVehicleAnimationInstance entrega ao Anim Blueprint do template, e a mesma conta do nó
		// FAnimNode_WheelController: giro (pitch) e esterçamento (yaw) valem no espaço do COMPONENTE (rotação nova = RotOffset x
		// rotação do osso, também no espaço do componente), e o curso da suspensão também. A roda visual está presa ao osso, então a
		// rotação relativa a ele é Osso^-1 x RotOffset x Osso. (A versão anterior aplicava RotOffset no espaço do osso: se o osso não
		// tem o eixo do eixo-de-roda em Y, a roda girava em torno do eixo errado e ficava "de lado".)
		const FVector LocOffsetComponent = -Wheel->GetSuspensionAxis() * Wheel->GetSuspensionOffset();
		WheelVisuals[i]->SetRelativeLocationAndRotation(FDNWheelVisual::RelativeLocation(WheelBoneRotations[i], LocOffsetComponent),
			FDNWheelVisual::RelativeRotation(WheelBoneRotations[i], Wheel->GetRotationAngle(), Wheel->GetSteerAngle(), WheelMeshBaseYaw[i]));
	}
}

void ADNVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Batida real: fecha a janela vencida e guarda a velocidade do corpo ANTES do passo de física deste quadro. O contato
	// chega depois do passo, com a velocidade já alterada; a diferença entre as duas é a variação de velocidade da batida.
	FlushImpacts(GetWorld()->GetTimeSeconds());
	VelocityBeforeStepCmS = GetMesh()->GetPhysicsLinearVelocity();
	AngularBeforeStepDegS = GetMesh()->GetPhysicsAngularVelocityInDegrees();
	PlowDebris();

	// dano visual, camada 1: a carroceria amassa por zona (o dano da batida que acabou de fechar já entra neste quadro)
	UpdateDamageMorphs(DeltaSeconds);

	// freio de mão: a perda de aderência da traseira sobe e desce com rampa
	HandbrakeRamp = FDNHandbrakeRamp::Step(HandbrakeRamp, bHandbrakeHeld, DeltaSeconds, Car.HandbrakeRampInSeconds, Car.HandbrakeRampOutSeconds);
	ApplyWheelGrip(false);

	// turbo: o Core decide barra e estado; só repassamos as entradas deste quadro. Sem eventos de ganho ainda: só consome.
	FDNTurboFrameInput Frame;
	Frame.DeltaTime = DeltaSeconds;
	Frame.bButtonHeld = bTurboHeld && Car.NitroAccelMps2 > 0.0f;
	Frame.SpeedKmh = SpeedKmh();
	Turbo->Step(Frame);
	if (Turbo->IsActive())
	{
		// aceleração extra para frente (força = massa x aceleração, em cm/s²)
		GetMesh()->AddForce(GetActorForwardVector() * (Car.NitroAccelMps2 * 100.0f * Chaos()->Mass));
	}

	// assistência de derrapagem (Core/DNDriftAssist): contra-esterço automático proporcional ao excesso de ângulo de deslize;
	// só no chão (A7), para não girar o carro no ar ou capotado depois de uma batida
	int32 WheelsInContact = 0;
	for (const UChaosVehicleWheel* Wheel : Chaos()->Wheels)
	{
		WheelsInContact += (Wheel && !Wheel->IsInAir()) ? 1 : 0;
	}
	if (bDriftAssist && Car.DriftAssistGain > 0.0f && FDNDriftAssist::IsGrounded(WheelsInContact))
	{
		const float Accel = FDNDriftAssist::YawAcceleration(Car.DriftAssist(), SlipAngleDegNow(), SpeedKmh(), GetMesh()->GetPhysicsAngularVelocityInDegrees().Z);
		if (Accel != 0.0f)
		{
			GetMesh()->AddTorqueInDegrees(FVector(0.0f, 0.0f, Accel), NAME_None, true); // aceleração angular [graus/s²]
		}
	}

	// modo de teste: barra vazia e desligado por um tempo, enche de novo
	if (bTurboTestMode && !Turbo->IsActive() && Turbo->GetBar() <= 0.0f)
	{
		TurboEmptySeconds += DeltaSeconds;
		if (TurboEmptySeconds >= TurboTestRefillSeconds)
		{
			TurboEmptySeconds = 0.0f;
			FDNTurboFrameInput Fill;
			Fill.bTakedown = true;
			Turbo->Step(Fill);
		}
	}
	else
	{
		TurboEmptySeconds = 0.0f;
	}

	// lote 18: câmera de velocidade e de batida em TEMPO REAL (o delta do quadro sem a escala de tempo): a câmera lenta muda a escala do
	// jogo, e se a câmera andasse no tempo do jogo a câmera lenta prolongaria a si mesma
	if (IsLocallyControlled())
	{
		const float RealDt = static_cast<float>(FApp::GetDeltaTime());
		UpdateCrashCam(RealDt);
		UpdateSpeedCamera(RealDt);
	}

	if (DentViewSecondsLeft > 0.0f)
	{
		DentViewSecondsLeft -= DeltaSeconds;
		if (DentViewSecondsLeft <= 0.0f)
		{
			RestoreChaseCamera();
		}
	}
	if (CameraSnapFrames > 0 && --CameraSnapFrames == 0 && !bShotView)
	{
		RestoreChaseCamera(); // depois do DNRestart: devolve o atraso de rotação do modo
	}

	UpdateWheelVisuals();
	UpdateHud();
}

void ADNVehicle::CreateInputAssets()
{
	ThrottleAction = NewObject<UInputAction>(this, TEXT("DN_ThrottleAction"));
	ThrottleAction->ValueType = EInputActionValueType::Axis1D;
	BrakeAction = NewObject<UInputAction>(this, TEXT("DN_BrakeAction"));
	BrakeAction->ValueType = EInputActionValueType::Axis1D;
	SteeringAction = NewObject<UInputAction>(this, TEXT("DN_SteeringAction"));
	SteeringAction->ValueType = EInputActionValueType::Axis1D;
	HandbrakeAction = NewObject<UInputAction>(this, TEXT("DN_HandbrakeAction"));
	HandbrakeAction->ValueType = EInputActionValueType::Boolean;
	TurboAction = NewObject<UInputAction>(this, TEXT("DN_TurboAction"));
	TurboAction->ValueType = EInputActionValueType::Boolean;

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("DN_CarMappingContext"));
	MappingContext->MapKey(ThrottleAction, EKeys::W);
	MappingContext->MapKey(BrakeAction, EKeys::S);
	MappingContext->MapKey(SteeringAction, EKeys::D);
	FEnhancedActionKeyMapping& Left = MappingContext->MapKey(SteeringAction, EKeys::A);
	Left.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
	MappingContext->MapKey(HandbrakeAction, EKeys::SpaceBar);
	MappingContext->MapKey(TurboAction, EKeys::LeftShift);
	CameraAction = NewObject<UInputAction>(this, TEXT("DN_CameraAction"));
	CameraAction->ValueType = EInputActionValueType::Boolean;
	MappingContext->MapKey(CameraAction, EKeys::C);

	// batidas simuladas de teste: T Y U = frente (pequena, média, máxima), G H J = lado esquerdo, V = limpar
	const FKey DentKeys[] = { EKeys::T, EKeys::Y, EKeys::U, EKeys::G, EKeys::H, EKeys::J, EKeys::V };
	DentActions.Reset();
	for (int32 i = 0; i < UE_ARRAY_COUNT(DentKeys); ++i)
	{
		UInputAction* Action = NewObject<UInputAction>(this, *FString::Printf(TEXT("DN_DentAction%d"), i));
		Action->ValueType = EInputActionValueType::Boolean;
		MappingContext->MapKey(Action, DentKeys[i]);
		DentActions.Add(Action);
	}

	// R: reinício completo na largada (DNRestart)
	RestartAction = NewObject<UInputAction>(this, TEXT("DN_RestartAction"));
	RestartAction->ValueType = EInputActionValueType::Boolean;
	MappingContext->MapKey(RestartAction, EKeys::R);

	// 1, 2, 3 e 0 trocam de preset (ver HotkeyRows)
	const FKey PresetKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Zero };
	PresetActions.Reset();
	for (int32 i = 0; i < UE_ARRAY_COUNT(PresetKeys); ++i)
	{
		UInputAction* Action = NewObject<UInputAction>(this, *FString::Printf(TEXT("DN_PresetAction%d"), i));
		Action->ValueType = EInputActionValueType::Boolean;
		MappingContext->MapKey(Action, PresetKeys[i]);
		PresetActions.Add(Action);
	}
}

void ADNVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	CreateInputAssets();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ADNVehicle::Throttle);
		Input->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ADNVehicle::Throttle);
		Input->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &ADNVehicle::Brake);
		Input->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ADNVehicle::Brake);
		Input->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &ADNVehicle::Steering);
		Input->BindAction(SteeringAction, ETriggerEvent::Completed, this, &ADNVehicle::Steering);
		Input->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ADNVehicle::HandbrakeStart);
		Input->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ADNVehicle::HandbrakeStop);
		Input->BindAction(TurboAction, ETriggerEvent::Started, this, &ADNVehicle::TurboStart);
		Input->BindAction(TurboAction, ETriggerEvent::Completed, this, &ADNVehicle::TurboStop);
		Input->BindAction(CameraAction, ETriggerEvent::Started, this, &ADNVehicle::CameraKey);
		Input->BindAction(RestartAction, ETriggerEvent::Started, this, &ADNVehicle::RestartKey);
		for (int32 i = 0; i < PresetActions.Num(); ++i)
		{
			Input->BindAction(PresetActions[i], ETriggerEvent::Started, this, &ADNVehicle::PresetKey, i);
		}
		for (int32 i = 0; i < DentActions.Num(); ++i)
		{
			Input->BindAction(DentActions[i], ETriggerEvent::Started, this, &ADNVehicle::DentKey, i);
		}
	}
	else
	{
		UE_LOG(LogDetonauto, Error, TEXT("'%s' precisa do Enhanced Input Component."), *GetNameSafe(this));
	}
}

void ADNVehicle::Throttle(const FInputActionValue& Value)
{
	const float Input = Value.Get<float>();
	// de onde vem o acelerador (17b: um -Play andou sozinho a 142 km/h): cada vez que ele liga pela entrada do jogador fica no log;
	// o roteiro -DNKeys registra as teclas que ele mesmo aperta, então tecla de fora aparece aqui sem linha do roteiro
	if (Input > 0.0f && !bThrottleHeld)
	{
		UE_LOG(LogDetonauto, Display, TEXT("ADNVehicle: acelerador ligado pela entrada do jogador (tecla ou controle) em t=%.2f s"), GetWorld()->GetTimeSeconds());
	}
	bThrottleHeld = Input > 0.0f;
	Chaos()->SetThrottleInput(Input);
}

void ADNVehicle::Brake(const FInputActionValue& Value)
{
	Chaos()->SetBrakeInput(Value.Get<float>());
}

void ADNVehicle::Steering(const FInputActionValue& Value)
{
	SteerInputRaw = FMath::Clamp(Value.Get<float>(), -1.0f, 1.0f); // também é o controle do destroço na câmera de batida
	Chaos()->SetSteeringInput(FMath::Clamp(Value.Get<float>() * AppliedSteeringGain, -1.0f, 1.0f));
}

void ADNVehicle::SetHandbrake(bool bHeld)
{
	bHandbrakeHeld = bHeld;
	Chaos()->SetHandbrakeInput(bHeld);
}

void ADNVehicle::SetTurboButton(bool bHeld)
{
	bTurboHeld = bHeld;
}

void ADNVehicle::HandbrakeStart(const FInputActionValue& Value)
{
	SetHandbrake(true);
}

void ADNVehicle::HandbrakeStop(const FInputActionValue& Value)
{
	SetHandbrake(false);
}

void ADNVehicle::TurboStart(const FInputActionValue& Value)
{
	SetTurboButton(true);
}

void ADNVehicle::TurboStop(const FInputActionValue& Value)
{
	SetTurboButton(false);
}

void ADNVehicle::CameraKey(const FInputActionValue& Value)
{
	CycleCamera();
}

void ADNVehicle::PresetKey(const FInputActionValue& Value, int32 HotkeyIndex)
{
	if (HotkeyRows().IsValidIndex(HotkeyIndex) && !SwitchPreset(HotkeyRows()[HotkeyIndex]))
	{
		UE_LOG(LogDetonauto, Error, TEXT("ADNVehicle: não consegui trocar para o preset '%s'"), *HotkeyRows()[HotkeyIndex].ToString());
	}
}

// =====================================================================================================================
// Amassado visual
// =====================================================================================================================

void ADNVehicle::GetBodyBox(FVector& OutCenter, FVector& OutExtent) const
{
	OutCenter = FVector::ZeroVector;
	OutExtent = FVector(230.0f, 100.0f, 70.0f); // só se a malha faltar
	FBoxSphereBounds Bounds;
	if (BodyAssetBounds(Bounds))
	{
		OutCenter = Bounds.Origin;
		OutExtent = Bounds.BoxExtent;
	}
}

bool ADNVehicle::BodyAssetBounds(FBoxSphereBounds& OutBounds) const
{
	if (const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(BodyComp); Static && Static->GetStaticMesh())
	{
		OutBounds = Static->GetStaticMesh()->GetBounds();
		return true;
	}
	if (const USkeletalMeshComponent* Skin = Cast<USkeletalMeshComponent>(BodyComp); Skin && Skin->GetSkeletalMeshAsset())
	{
		OutBounds = Skin->GetSkeletalMeshAsset()->GetImportedBounds(); // a malha intacta, como a do template
		return true;
	}
	return false;
}

bool ADNVehicle::EnableDentMaterial(bool bBindRealHits)
{
	if (DentMaterial)
	{
		return true;
	}
	FBoxSphereBounds BodyBounds;
	if (!BodyAssetBounds(BodyBounds))
	{
		UE_LOG(LogDetonauto, Error, TEXT("DNDent: o carro não tem a malha da carroceria; amassado desligado"));
		return false;
	}
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, DentParentPath);
	if (!Parent)
	{
		UE_LOG(LogDetonauto, Error, TEXT("DNDent: material-pai não existe (%s). Rode Tools/Unreal/create_dent_material.ps1 -Dest /Game/Detonauto/Vehicle"), DentParentPath);
		return false;
	}

	// Um material de amassado por slot que já usa uma instância do material de amassado (a pintura e o chassi feitos por
	// Tools/Unreal/create_dent_material.py, lote 16): o dinâmico nasce dela e o slot guarda o visual dele. Os outros slots (os
	// faróis da carroceria do lote 17) ficam com o material deles, sem as mossas locais, como o vidro separado de antes.
	OriginalBodyMaterials.Reset();
	DentMaterials.Reset();
	FString Slots;
	for (int32 i = 0; i < BodyComp->GetNumMaterials(); ++i)
	{
		UMaterialInterface* SlotMaterial = BodyComp->GetMaterial(i);
		OriginalBodyMaterials.Add(SlotMaterial);
		Slots += FString::Printf(TEXT(" %d=%s(base %s)"), i, SlotMaterial ? *SlotMaterial->GetName() : TEXT("nenhum"),
			SlotMaterial && SlotMaterial->GetBaseMaterial() ? *SlotMaterial->GetBaseMaterial()->GetName() : TEXT("nenhuma"));
		if (!SlotMaterial || SlotMaterial->GetBaseMaterial() != Parent->GetBaseMaterial())
		{
			continue;
		}
		UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(SlotMaterial, this);
		BodyComp->SetMaterial(i, Dynamic);
		DentMaterials.Add(Dynamic);
	}
	DentMaterial = DentMaterials.IsEmpty() ? nullptr : DentMaterials[0];
	if (!DentMaterial)
	{
		UE_LOG(LogDetonauto, Error, TEXT("DNDent: nenhum slot da carroceria usa o material de amassado (%s). Slots:%s. Rode Tools/Unreal/create_dent_material.ps1"),
			*Parent->GetName(), *Slots);
		return false;
	}
	RegisterPartMaterials(); // as peças presas e os vidros recebem os mesmos amassados e o mesmo desgaste
	PushDentsToMaterial(); // todos os slots desligados
	{
		FVector Center, Extent;
		GetBodyBox(Center, Extent);
		// desgaste da pintura (lote 17b): a caixa da carroceria e os valores de Damage.csv; os pesos das zonas vão em PushWearToMaterial
		for (UMaterialInstanceDynamic* Dynamic : DentMaterials)
		{
			Dynamic->SetVectorParameterValue(TEXT("BodyCenter"), FLinearColor(Center.X, Center.Y, Center.Z));
			Dynamic->SetVectorParameterValue(TEXT("BodyExtent"), FLinearColor(Extent.X, Extent.Y, Extent.Z));
			Dynamic->SetScalarParameterValue(TEXT("WearChipCoverage"), DamageConfig.WearChipCoverage);
			Dynamic->SetScalarParameterValue(TEXT("WearChipSizeCm"), DamageConfig.WearChipSizeCm);
			Dynamic->SetScalarParameterValue(TEXT("WearScratch"), DamageConfig.WearScratch);
			Dynamic->SetScalarParameterValue(TEXT("WearSoot"), DamageConfig.WearSoot);
		}
		PushWearToMaterial();
		UE_LOG(LogDetonauto, Display, TEXT("DNDent: carroceria '%s': %d de %d slot(s) de material com amassado, caixa local centro=%s extensao=%s"),
			*BodyComp->GetName(), DentMaterials.Num(), BodyComp->GetNumMaterials(), *Center.ToCompactString(), *Extent.ToCompactString());
	}

	// batida real: o corpo físico avisa cada contato (paredes, outros carros)
	// batida real (dano): o aviso de contato está sempre ligado (BeginPlay, para as peças de rua); aqui só liga o dano
	bRealHitDamage = bBindRealHits;
	return true;
}

void ADNVehicle::ApplyPropImpact(float DeltaVMs, const FVector& WorldPoint)
{
	// batida em peça de rua (lote 17c): o dano de sempre, com a variação de velocidade que a regra tirou do carro. Não entra na conta
	// das batidas reais nem abre a rodada de Crash nem a câmera de batida (FDNImpactPolicy)
	FVector Center, Extent;
	GetBodyBox(Center, Extent);
	const FVector Local = BodyComp ? BodyComp->GetComponentTransform().InverseTransformPosition(WorldPoint) : FVector::ZeroVector;
	FDNImpact Impact;
	Impact.DeltaVMs = DeltaVMs;
	Impact.LocalPositionCm = FDNDentParams::ProjectToBody(Local, Center, Extent, 20.0f);
	Impact.Zone = FDNDentParams::ZoneOfLocalPoint(Impact.LocalPositionCm, Center, Extent);
	const FDNDamageResult Result = FDNDamage::ApplyImpact(DamageConfig, DamageState, Impact);
	PushDentsToMaterial();
	EvaluatePartsAndGlass(&Impact);
	UE_LOG(LogDetonauto, Display, TEXT("DNDent: batida em peça de rua de %.1f m/s na zona %d: dano da zona +%.2f%s (rodada de Crash: %s, câmera de batida: %s)"),
		DeltaVMs, static_cast<int32>(Impact.Zone), Result.AddedZoneDamage, Result.bDentAdded ? TEXT(", amassado novo") : TEXT(""),
		FDNImpactPolicy::CountsForCrashRound(EDNImpactSource::Prop) ? TEXT("conta") : TEXT("não conta"),
		FDNImpactPolicy::TriggersCrashCam(EDNImpactSource::Prop) ? TEXT("dispara") : TEXT("não dispara"));
}

void ADNVehicle::PushDentsToMaterial()
{
	for (int32 i = 0; i < FDNDentParams::MaxSlots; ++i)
	{
		const FDNDentParams::FSlot Slot = FDNDentParams::Slot(DamageState.Dents, i);
		for (UMaterialInstanceDynamic* Dynamic : DentMaterials) // todos os slots deformam juntos (sem fresta entre pintura e janela)
		{
			Dynamic->SetVectorParameterValue(FDNDentParams::PositionName(i), Slot.Position);
			Dynamic->SetScalarParameterValue(FDNDentParams::RadiusName(i), Slot.Radius);
			Dynamic->SetScalarParameterValue(FDNDentParams::DepthName(i), Slot.Depth);
		}
	}
}

void ADNVehicle::SimulateImpact(float DeltaVMs, EDNDamageZone Zone, const FVector& LocalPositionCm)
{
	FDNImpact Impact;
	Impact.DeltaVMs = DeltaVMs;
	Impact.Zone = Zone;
	Impact.LocalPositionCm = LocalPositionCm;
	const FDNDamageResult Result = FDNDamage::ApplyImpact(DamageConfig, DamageState, Impact);
	PushDentsToMaterial();
	EvaluatePartsAndGlass(&Impact);
	UE_LOG(LogDetonauto, Display, TEXT("DNDent: batida de %.1f m/s na zona %d em %s: dano da zona +%.2f, amassado %s (profundidade %.1f cm, raio %.1f cm), %d amassado(s) guardado(s)%s"),
		DeltaVMs, static_cast<int32>(Zone), *LocalPositionCm.ToCompactString(), Result.AddedZoneDamage, Result.bDentAdded ? TEXT("novo") : TEXT("nenhum"),
		Result.Dent.DepthCm, Result.Dent.RadiusCm, DamageState.Dents.Num(), Result.bJustDestroyed ? TEXT(" | DESTRUIDO") : TEXT(""));
}

void ADNVehicle::SimulateImpactAtBoxFraction(float DeltaVMs, const FVector& Fraction)
{
	FVector Center, Extent;
	GetBodyBox(Center, Extent);
	const FVector Point = Center + Extent * Fraction;
	SimulateImpact(DeltaVMs, FDNDentParams::ZoneOfLocalPoint(Point, Center, Extent), Point);
}

void ADNVehicle::ApplyDemoDents(int32 Count)
{
	FVector Center, Extent;
	GetBodyBox(Center, Extent);
	// frente, traseira, lado esquerdo, lado direito, teto, quina da frente a esquerda, quina da frente a direita, quina de trás a esquerda
	const FVector Fractions[8] = {
		FVector(1.0f, 0.0f, 0.1f), FVector(-1.0f, 0.0f, 0.1f), FVector(0.0f, -1.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f),
		FVector(0.0f, 0.0f, 1.0f), FVector(0.8f, -0.8f, 0.0f), FVector(0.8f, 0.8f, 0.0f), FVector(-0.8f, -0.8f, 0.0f) };
	const int32 N = FMath::Clamp(Count, 0, 8);
	for (int32 i = 0; i < N; ++i)
	{
		const FVector Point = Center + Extent * Fractions[i];
		SimulateImpact(SimulatedDeltaV[2], FDNDentParams::ZoneOfLocalPoint(Point, Center, Extent), Point);
	}
}

void ADNVehicle::ClearDents()
{
	DamageState.Reset();
	MaxRealDeltaVMs = 0.0f;
	RealImpactCount = 0;
	PushDentsToMaterial();
	// carro novo: os alvos voltam a 0 (a regra nunca desce o peso; quem zera é quem troca o carro)
	FMemory::Memzero(MorphWeights);
	if (USkeletalMeshComponent* Skin = Cast<USkeletalMeshComponent>(BodyComp))
	{
		Skin->ClearMorphTargets();
	}
	ResetPartsAndGlass();
	PushWearToMaterial();
}

void ADNVehicle::DNRestart()
{
	// fora da câmera de batida e da câmera lenta
	CrashCam = FDNCrashCamState();
	bOrbitPending = false;
	if (AppliedTimeScale != 1.0f)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
		AppliedTimeScale = 1.0f;
	}
	// o carro na largada, parado e em ponto morto, como ao nascer; a janela de batida aberta não vira dano depois do salto
	SetActorTransform(StartTransform, false, nullptr, ETeleportType::ResetPhysics);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	GetMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	Chaos()->ResetVehicleState();
	ImpactGrouper = FDNImpactGrouper();
	GroupContacts = 0;
	// dano, alvos, desgaste, peças e vidros
	ClearDents();
	Turbo->ResetState();
	if (bTurboTestMode)
	{
		FDNTurboFrameInput Fill; // modo de teste: a barra volta cheia, como no BeginPlay
		Fill.bTakedown = true;
		Turbo->Step(Fill);
	}
	DentViewSecondsLeft = 0.0f;
	RestoreChaseCamera();
	// a câmera chega junto, sem girar devagar da direção da batida até a da largada: o braço guarda a última rotação, e um quadro sem
	// o atraso de rotação a acerta (o Tick devolve o atraso do modo)
	CameraBoom->bEnableCameraRotationLag = false;
	CameraSnapFrames = 2;
	// lote 19: fumaça, fogo e os disparos vivos (faíscas, cacos, pó) somem
	if (Effects)
	{
		Effects->ResetEffects();
	}
	if (UDNEffectsSubsystem* Fx = GetWorld()->GetSubsystem<UDNEffectsSubsystem>())
	{
		Fx->ResetAll();
	}
	LastImpactFxTime = -10.0;
	LastScrapeFxTime.Reset();
	// o mobiliário quebrado volta inteiro no lugar
	int32 Props = 0;
	for (TActorIterator<ADNBreakableProp> It(GetWorld()); It; ++It)
	{
		if (It->IsBroken())
		{
			It->ResetProp();
			++Props;
		}
	}
	int32 Attached = 0;
	for (const ADNCarPart* Part : Parts)
	{
		Attached += Part && Part->GetStage() == EDNPartStage::Attached ? 1 : 0;
	}
	UE_LOG(LogDetonauto, Display, TEXT("DNRestart: carro na largada %s a %.0f km/h, %d de %d peças presas, %d peça(s) de rua reposta(s); t do jogo %.2f s"),
		*GetActorLocation().ToCompactString(), SpeedKmh(), Attached, Parts.Num(), Props, GetWorld()->GetTimeSeconds());
}

void ADNVehicle::RestartKey(const FInputActionValue& Value)
{
	DNRestart();
}

void ADNVehicle::OnPartScraped(ADNCarPart* Part, const FVector& Point, float SpeedCmS)
{
	// lote 19: a peça avisa a cada contato com o chão; as faíscas saem em rajadas de ScrapeFxGapS com o tempo desde a anterior
	UDNEffectsSubsystem* Fx = GetWorld()->GetSubsystem<UDNEffectsSubsystem>();
	if (!Fx || !Part)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const TWeakObjectPtr<ADNCarPart> Key(Part);
	double* Last = LastScrapeFxTime.Find(Key);
	if (!Last)
	{
		Last = &LastScrapeFxTime.Add(Key, Now - FirstScrapeIntervalS);
	}
	if (Now - *Last < ScrapeFxGapS)
	{
		return;
	}
	Fx->OnScrape(Point, GetVelocity(), SpeedCmS / 100.0f, static_cast<float>(Now - *Last));
	*Last = Now;
}

void ADNVehicle::UpdateDamageMorphs(float DeltaSeconds)
{
	float Before[FDNDamageMorph::ZoneCount];
	FMemory::Memcpy(Before, MorphWeights, sizeof(Before));
	FDNDamageMorph::Weights(DamageConfig, DamageState, MorphWeights, DeltaSeconds);
	if (FMemory::Memcmp(Before, MorphWeights, sizeof(Before)) == 0)
	{
		return;
	}
	if (USkeletalMeshComponent* Skin = Cast<USkeletalMeshComponent>(BodyComp))
	{
		for (int32 Zone = 0; Zone < FDNDamageMorph::ZoneCount; ++Zone)
		{
			if (MorphWeights[Zone] != Before[Zone])
			{
				Skin->SetMorphTarget(FName(FDNDamageMorph::TargetName(Zone)), MorphWeights[Zone], false);
			}
		}
	}
	// lote 18b: as peças presas e penduradas e os vidros amassam junto (o mesmo campo, sem fresta); a solta ignora (voa já amassada)
	for (ADNCarPart* Part : Parts)
	{
		if (Part)
		{
			Part->SetMorphWeights(MorphWeights, FDNDamageMorph::ZoneCount);
		}
	}
	for (USkeletalMeshComponent* Glass : GlassComps)
	{
		for (int32 Zone = 0; Zone < FDNDamageMorph::ZoneCount && Glass; ++Zone)
		{
			Glass->SetMorphTarget(FName(FDNDamageMorph::TargetName(Zone)), MorphWeights[Zone], false);
		}
	}
	PushWearToMaterial(); // o desgaste da pintura segue o mesmo peso da deformação (também na carroceria estática)
}

void ADNVehicle::PushWearToMaterial()
{
	// os 5 pesos em dois vetores (o nó Custom só recebe RGB de um parâmetro vetorial): frente, traseira, esquerda | direita, teto
	const FLinearColor A(MorphWeights[0], MorphWeights[1], MorphWeights[2]);
	const FLinearColor B(MorphWeights[3], MorphWeights[4], 0.0f);
	for (UMaterialInstanceDynamic* Dynamic : DentMaterials)
	{
		Dynamic->SetVectorParameterValue(TEXT("ZoneWearA"), A);
		Dynamic->SetVectorParameterValue(TEXT("ZoneWearB"), B);
	}
}

void ADNVehicle::DNZone(int32 Zone, float Damage)
{
	if (Zone < 0 || Zone >= FDNDamageMorph::ZoneCount || !FMath::IsFinite(Damage))
	{
		UE_LOG(LogDetonauto, Warning, TEXT("DNZone: zona %d ou dano %f fora do esperado (zona 0 a 4, dano 0 a 1)"), Zone, Damage);
		return;
	}
	float& ZoneDamage = DamageState.ZoneDamage[Zone];
	ZoneDamage = FMath::Max(ZoneDamage, FMath::Clamp(Damage, 0.0f, 1.0f));
	UE_LOG(LogDetonauto, Display, TEXT("DNZone: zona %d (%s) com dano %.2f"), Zone, FDNDamageMorph::TargetName(Zone), ZoneDamage);
	// sem batida: as peças e os vidros seguem só o dano acumulado (a peça pendura e solta pela regra; o ponto da batida é o centro da zona)
	FVector Center, Extent;
	GetBodyBox(Center, Extent);
	const FVector ZoneDir[FDNDamageMorph::ZoneCount] = { FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, -1, 0), FVector(0, 1, 0), FVector(0, 0, 1) };
	LastImpactLocal = Center + Extent * ZoneDir[Zone];
	EvaluatePartsAndGlass(nullptr);
}

void ADNVehicle::DentKey(const FInputActionValue& Value, int32 DentIndex)
{
	if (!DentMaterial && !EnableDentMaterial())
	{
		return;
	}
	if (DentIndex == 6)
	{
		ClearDents();
		SetShotCamera(135.0f);
		DentViewSecondsLeft = 3.0f;
		return;
	}
	FVector Center, Extent;
	GetBodyBox(Center, Extent);
	const bool bFront = DentIndex < 3;
	// frente: a quina da frente à esquerda; lado esquerdo: a porta. O meio da frente tem a grade (aberta, sem vértices na caixa): um
	// amassado ali não aparece (playtest 1). Medido: 0,8 / -0,8 da extensão cai na superfície e deforma. A câmera gira para vê-los.
	const FVector Point = bFront ? Center + Extent * FVector(0.8f, -0.8f, 0.0f) : Center + Extent * FVector(0.0f, -1.0f, 0.0f);
	SimulateImpact(SimulatedDeltaV[DentIndex % 3], bFront ? EDNDamageZone::Front : EDNDamageZone::Left, Point);
	// A câmera de perseguição fica atrás do carro e não vê a frente nem o lado esquerdo (playtest 1: "nada acontece"): a câmera gira
	// para a frente-esquerda por 3 s para o amassado aparecer, e depois volta.
	SetShotCamera(135.0f);
	DentViewSecondsLeft = 3.0f;
}

void ADNVehicle::PlowDebris()
{
	if (VelocityBeforeStepCmS.SizeSquared2D() < FMath::Square(100.0))
	{
		return; // abaixo de 1 m/s não varre (B9)
	}
	// a caixa da carroceria, orientada, com 30 cm a mais na frente: o destroço sai antes de aparecer atravessando o capô
	FVector Center;
	FVector Extent;
	GetBodyBox(Center, Extent);
	const FTransform& Frame = BodyComp ? BodyComp->GetComponentTransform() : GetActorTransform();
	Center.X += 15.0f;
	Extent.X += 15.0f;
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Frame.TransformPosition(Center), Frame.GetRotation(), FCollisionObjectQueryParams(ECC_DNDebris),
		FCollisionShape::MakeBox(Extent * Frame.GetScale3D().GetAbs()));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ADNBreakableProp* Prop = Cast<ADNBreakableProp>(Overlap.GetActor());
		if (Prop && Overlap.GetComponent() == Prop->GetBodyComponent())
		{
			Prop->Plow(VelocityBeforeStepCmS); // a base e o toco também são destroço, mas não saem do lugar
		}
		// peça de carro solta na pista (a deste carro ou de outro): sai da frente como o mobiliário (a pendurada não muda)
		if (ADNCarPart* Part = Cast<ADNCarPart>(Overlap.GetActor()))
		{
			Part->Plow(VelocityBeforeStepCmS);
		}
	}
}

void ADNVehicle::OnCarHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// peça de rua destrutível (lote 17c): ela decide se quebra e já devolve o carro com a velocidade de antes menos a perda. Peça
	// já quebrada não conta de novo (vários contatos no mesmo passo). Abaixo do limiar ela é sólida e segue como batida comum.
	const ADNBreakableProp* SolidProp = nullptr;
	if (ADNBreakableProp* Prop = Cast<ADNBreakableProp>(OtherActor))
	{
		if (Prop->IsBroken() || Prop->HandleCarHit(*this, VelocityBeforeStepCmS, AngularBeforeStepDegS, Hit.ImpactPoint))
		{
			return;
		}
		SolidProp = Prop;
	}

	// Variação de velocidade medida no corpo: velocidade no início do quadro (antes do passo de física) menos a de agora.
	const float DeltaV = FDNDentParams::DeltaVMsBetween(VelocityBeforeStepCmS, GetMesh()->GetPhysicsLinearVelocity());
	// câmera de batida (lote 18): no contato, sem esperar a janela do dano fechar; vale também com o dano desligado. Peça de rua (mesmo
	// a que ficou sólida, abaixo do limiar) nunca dispara (FDNImpactPolicy)
	const EDNImpactSource Source = SolidProp ? EDNImpactSource::Prop : (Cast<ADNTrafficVehicle>(OtherActor) ? EDNImpactSource::Traffic : EDNImpactSource::World);
	TryCrashCam(DeltaV, Source, Hit.ImpactPoint);
	// lote 19: faísca e lascas no contato (superfície dura: muro, poste inteiro, carro), uma rajada por choque (ImpactFxGapS)
	if (UDNEffectsSubsystem* Fx = GetWorld()->GetSubsystem<UDNEffectsSubsystem>())
	{
		const double FxNow = GetWorld()->GetTimeSeconds();
		if (FxNow - LastImpactFxTime >= ImpactFxGapS && Fx->OnImpact(Hit.ImpactPoint, Hit.ImpactNormal, VelocityBeforeStepCmS, DeltaV) > 0)
		{
			LastImpactFxTime = FxNow;
		}
	}

	if (!BodyComp || !bRealHitDamage)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	FlushImpacts(Now); // uma janela vencida sai antes de este contato abrir outra
	if (DeltaV <= 0.0f)
	{
		return;
	}

	// Ponto do amassado: o contato sobe do chão (20 cm acima da base do corpo) e entra na caixa do corpo.
	FVector Center, Extent;
	GetBodyBox(Center, Extent);
	const FVector LocalContact = BodyComp->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
	const FVector LocalPoint = FDNDentParams::ProjectToBody(LocalContact, Center, Extent, 20.0f);

	const bool bWasOpen = ImpactGrouper.IsOpen();
	ImpactGrouper.Add(Now, DeltaV, LocalPoint);
	if (!bWasOpen && ImpactGrouper.IsOpen())
	{
		GroupVelocityBeforeCmS = VelocityBeforeStepCmS;
		GroupContacts = 0;
	}
	++GroupContacts;
	if (LoggedImpacts < 24 && DeltaV >= DamageConfig.DentMinDeltaVMs)
	{
		++LoggedImpacts;
		UE_LOG(LogDetonauto, Display, TEXT("DNDent: contato com '%s': velocidade antes %.1f km/h, depois %.1f km/h -> %.1f m/s; contato local %s, ponto do amassado %s"),
			*GetNameSafe(OtherActor), VelocityBeforeStepCmS.Size() * 0.036f, GetMesh()->GetPhysicsLinearVelocity().Size() * 0.036f, DeltaV,
			*LocalContact.ToCompactString(), *LocalPoint.ToCompactString());
	}
}

void ADNVehicle::FlushImpacts(double Now)
{
	FDNImpactGrouper::FGroup Group;
	if (!ImpactGrouper.TryFlush(Now, Group))
	{
		return;
	}
	++RealImpactCount;
	MaxRealDeltaVMs = FMath::Max(MaxRealDeltaVMs, Group.DeltaVMs);

	FVector Center, Extent;
	GetBodyBox(Center, Extent);
	const float WholeWindow = FDNDentParams::DeltaVMsBetween(GroupVelocityBeforeCmS, GetMesh()->GetPhysicsLinearVelocity());
	UE_LOG(LogDetonauto, Display,
		TEXT("DNDent: IMPACTO REAL (janela de %.2f s fechada, %d contato(s)): velocidade antes do 1o contato %.1f km/h, agora %.1f km/h | variação de velocidade: maior por quadro %.1f m/s (vale), janela inteira %.1f m/s (só informativo)"),
		FDNImpactGrouper::WindowSeconds, GroupContacts, GroupVelocityBeforeCmS.Size() * 0.036f, GetMesh()->GetPhysicsLinearVelocity().Size() * 0.036f,
		Group.DeltaVMs, WholeWindow);
	SimulateImpact(Group.DeltaVMs, FDNDentParams::ZoneOfLocalPoint(Group.LocalPoint, Center, Extent), Group.LocalPoint);
	GroupContacts = 0;
}

void ADNVehicle::RestoreChaseCamera()
{
	SetCameraMode(CameraIndex);
}

FString ADNVehicle::CameraName() const
{
	return CameraModes.IsValidIndex(CameraIndex) ? CameraModes[CameraIndex].Id : FString(TEXT("-"));
}

void ADNVehicle::CycleCamera()
{
	SetCameraMode(FDNCameraCycle::Next(CameraIndex, CameraModes.Num()));
	UE_LOG(LogDetonauto, Display, TEXT("ADNVehicle: câmera '%s'"), *CameraName());
}

void ADNVehicle::SetCameraMode(int32 Index)
{
	if (CameraModes.IsEmpty())
	{
		return;
	}
	CameraIndex = CameraModes.IsValidIndex(Index) ? Index : 0;
	bShotView = false;
	const FDNCameraConfig& C = CameraModes[CameraIndex].Config;
	CameraBoom->SetRelativeRotation(FRotator(C.PitchDeg, 0.0f, 0.0f));
	CameraBoom->bEnableCameraRotationLag = C.Smoothing > 0.0f;
	CameraBoom->CameraRotationLagSpeed = C.Smoothing > 0.0f ? C.Smoothing : 2.0f;
	if (C.Anchor == EDNCameraAnchor::Chase)
	{
		// atrás do carro, num braço: não balança nem inclina junto com a carroceria
		CameraBoom->SetRelativeLocation(FVector::ZeroVector);
		CameraBoom->TargetArmLength = C.DistanceCm;
		CameraBoom->SocketOffset = FVector(0.0f, 0.0f, C.HeightCm);
		CameraBoom->bInheritPitch = false;
		CameraBoom->bInheritRoll = false;
	}
	else
	{
		// presa ao carro (capô, para-choque, motorista): sem braço, na posição pedida, e acompanha a carroceria inteira
		CameraBoom->SetRelativeLocation(FVector(C.ForwardCm, C.SideCm, C.HeightCm));
		CameraBoom->TargetArmLength = 0.0f;
		CameraBoom->SocketOffset = FVector::ZeroVector;
		CameraBoom->bInheritPitch = true;
		CameraBoom->bInheritRoll = true;
	}
	Camera->SetFieldOfView(C.Fov);
	DisplayedFovDeg = C.Fov; // troca de modo é corte: a velocidade e o turbo voltam a somar a partir da base nova
}

float ADNVehicle::SlipAngleDegNow() const
{
	return FDNDriftAssist::SlipAngleDeg(GetActorTransform().InverseTransformVectorNoScale(GetVelocity()));
}

void ADNVehicle::SetShotCamera(float YawDegrees)
{
	CameraBoom->SetRelativeLocation(FVector::ZeroVector); // os modos presos ao carro deslocam o braço; a vista de foto orbita o carro
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->bInheritYaw = true;
	CameraBoom->SetRelativeRotation(FRotator(-12.0f, YawDegrees, 0.0f));
	CameraBoom->TargetArmLength = 560.0f;
	CameraBoom->SocketOffset.Z = 60.0f;
	bShotView = true;
}

// =====================================================================================================================
// Câmera de velocidade e de batida (lote 18, docs/regras-camera.md)
// =====================================================================================================================

void ADNVehicle::UpdateSpeedCamera(float RealDt)
{
	// o turbo abre o campo de visão (sensação de velocidade): o empurrão é grande (medido: +59 a +70% de velocidade em 3 s), mas só
	// aparece na tela se a câmera ajudar; no playtest 1 o Felipe disse que "quase não dá diferença"
	TurboFovBlend = FMath::FInterpTo(TurboFovBlend, Turbo->IsActive() ? 1.0f : 0.0f, RealDt, TurboFovBlendSpeed);
	float Blur = 0.0f;
	if (bShotView)
	{
		Camera->SetFieldOfView(ShotFovDegrees); // vista de foto: o FOV do modo de jogo (95) deixava o carro pequeno no quadro
		Camera->SetRelativeRotation(FRotator::ZeroRotator);
	}
	else if (CameraModes.IsValidIndex(CameraIndex))
	{
		const FDNCameraConfig& C = CameraModes[CameraIndex].Config;
		const float Speed = SpeedKmh();
		DisplayedFovDeg = FDNSpeedFov::Step(CameraFx, DisplayedFovDeg, FDNSpeedFov::Target(CameraFx, C.Fov, Speed, C.SpeedFovScale, TurboFovBlend), RealDt);
		Camera->SetFieldOfView(DisplayedFovDeg);
		// tremor: giro pequeno da lente sobre o braço (o braço continua mirando o carro)
		ShakeTimeS += RealDt;
		const FVector2D Shake = FDNSpeedShake::Offset(CameraFx, Speed, C.ShakeScale, ShakeGlobalScale, ShakeTimeS);
		Camera->SetRelativeRotation(FRotator(Shake.X, Shake.Y, 0.0f));
		Blur = bEdgeBlurOn ? FDNEdgeBlur::Strength(CameraFx, Speed) : 0.0f;
		if (EdgeBlurForce >= 0.0f)
		{
			Blur = EdgeBlurForce; // medição de custo: força fixa, mesmo parado
		}
	}
	if (EdgeBlurMaterial && Blur != AppliedEdgeBlur)
	{
		EdgeBlurMaterial->SetScalarParameterValue(TEXT("Strength"), Blur);
		Camera->PostProcessSettings.AddBlendable(EdgeBlurMaterial, Blur > 0.0f ? 1.0f : 0.0f); // peso 0: o passe nem roda
	}
	AppliedEdgeBlur = EdgeBlurMaterial ? Blur : 0.0f;
}

void ADNVehicle::TryCrashCam(float DeltaVMs, EDNImpactSource Source, const FVector& WorldPoint)
{
	if (!bCrashCamOn || bShotView || !IsLocallyControlled() || !FDNCrashCam::TryTrigger(CameraFx, CrashCam, DeltaVMs, Source))
	{
		return;
	}
	bOrbitPending = true;
	WreckSteerUsedMs = 0.0f;
	CrashCamClockS = 0.0f;
	CrashStartLocation = GetActorLocation();
	CrashImpactYawDeg = static_cast<float>((WorldPoint - CrashStartLocation).Rotation().Yaw);
	CrashScreenRight = FRotationMatrix(LastViewRotation).GetScaledAxis(EAxis::Y).GetSafeNormal2D(); // ainda a câmera do modo (peso 0)
	UE_LOG(LogDetonauto, Display, TEXT("DNCrashCam: dispara com %.1f m/s (%s) a %.0f km/h, batida a %.0f graus de guinada; t do jogo %.2f s"),
		DeltaVMs, Source == EDNImpactSource::Traffic ? TEXT("tráfego") : TEXT("parede"), VelocityBeforeStepCmS.Size() * 0.036f, CrashImpactYawDeg,
		GetWorld()->GetTimeSeconds());
}

void ADNVehicle::UpdateCrashCam(float RealDt)
{
	const EDNCrashCamPhase Before = CrashCam.Phase;
	if (Before == EDNCrashCamPhase::Idle && AppliedTimeScale == 1.0f)
	{
		return;
	}
	FDNCrashCam::Step(CameraFx, CrashCam, RealDt);
	CrashCamClockS += RealDt;
	if (!bOrbitPending && CrashCam.Phase != EDNCrashCamPhase::Idle && CrashCam.Phase != EDNCrashCamPhase::Cooldown)
	{
		OrbitYawDeg = FRotator::NormalizeAxis(OrbitYawDeg + OrbitSign * CameraFx.CrashCamOrbitDegPerS * RealDt);
	}

	// controle do destroço: a entrada esquerda e direita empurra o carro para o lado da tela, como aceleração (sem depender da massa)
	if (FDNCrashCam::SteerActive(CrashCam))
	{
		const FVector Delta = FDNWreckSteer::DeltaVelocity(CameraFx, true, SteerInputRaw, GetMesh()->GetPhysicsLinearVelocity(), CrashScreenRight, RealDt, WreckSteerUsedMs);
		if (!Delta.IsNearlyZero())
		{
			GetMesh()->SetPhysicsLinearVelocity(Delta, true);
		}
	}
	else if (FDNCrashCam::SteerActive(FDNCrashCamState{ Before, 0.0f }))
	{
		const float SideM = static_cast<float>(FVector::DotProduct(GetActorLocation() - CrashStartLocation, CrashScreenRight)) / 100.0f;
		UE_LOG(LogDetonauto, Display, TEXT("DNCrashCam: controle do destroço usou %.2f de %.2f m/s; o carro andou %.2f m para a direita da tela (a da câmera do jogador na batida) desde o disparo"),
			WreckSteerUsedMs, CameraFx.WreckSteerMaxDeltaVMs, SideM);
	}

	const float Scale = FDNCrashCam::TimeScale(CameraFx, CrashCam);
	if (Scale != AppliedTimeScale)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, Scale);
		AppliedTimeScale = Scale;
	}
	if (CrashCam.Phase != Before)
	{
		UE_LOG(LogDetonauto, Display, TEXT("DNCrashCam: %s -> %s em %.2f s reais (escala de tempo %.2f, %.0f km/h)"),
			CrashPhaseName(Before), CrashPhaseName(CrashCam.Phase), CrashCamClockS, AppliedTimeScale, SpeedKmh());
	}
}

void ADNVehicle::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult); // a câmera do modo (componente Camera, com o campo de visão e o tremor)
	const float Weight = FDNCrashCam::BlendWeight(CameraFx, CrashCam);
	if (Weight > 0.0f)
	{
		// órbita em volta do CARRO (ele sai girando), não do ponto da batida
		const FVector Aim = GetActorLocation() + FVector(0.0f, 0.0f, CrashCamAimUpCm);
		const FVector FromAim = OutResult.Location - Aim;
		const float ModeYaw = static_cast<float>(FromAim.Rotation().Yaw);
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(DNCrashCam), false, this);
		const FCollisionShape Probe = FCollisionShape::MakeSphere(CameraFx.CrashCamProbeCm);
		const float Radius = CameraFx.CrashCamOrbitRadiusM * 100.0f;
		const FVector OrbitUp(0.0f, 0.0f, CameraFx.CrashCamOrbitHeightM * 100.0f - CrashCamAimUpCm);
		if (bOrbitPending)
		{
			// a órbita começa CrashCamLeadDeg antes do lado da batida e gira para ele: mostra o amassado de três quartos. Dos dois lados,
			// o que tem mais espaço livre até a esfera bater (encostado num prédio, o outro); empate: o mais perto da câmera do modo.
			// Câmera já mais perto da batida que isso: começa onde ela está
			bOrbitPending = false;
			const float Lead = FMath::Clamp(CameraFx.CrashCamLeadDeg, 0.0f, 180.0f);
			const float ToImpact = FMath::FindDeltaAngleDegrees(ModeYaw, CrashImpactYawDeg);
			OrbitSign = ToImpact < 0.0f ? -1.0f : 1.0f;
			OrbitYawDeg = ModeYaw;
			if (FMath::Abs(ToImpact) > Lead)
			{
				auto Room = [&](float Sign)
				{
					const FVector End = Aim + FRotator(0.0f, CrashImpactYawDeg - Sign * Lead, 0.0f).Vector() * Radius + OrbitUp;
					FHitResult SideHit;
					return GetWorld()->SweepSingleByChannel(SideHit, Aim, End, FQuat::Identity, ECC_Camera, Probe, Params) && !SideHit.bStartPenetrating ? SideHit.Time : 1.0f;
				};
				const float Near = Room(OrbitSign);
				const float Far = Room(-OrbitSign);
				if (Far > Near + 0.05f)
				{
					OrbitSign = -OrbitSign;
				}
				OrbitYawDeg = FRotator::NormalizeAxis(CrashImpactYawDeg - OrbitSign * Lead);
				UE_LOG(LogDetonauto, Display, TEXT("DNCrashCam: órbita começa em %.0f graus (espaço livre: %.0f%% de um lado, %.0f%% do outro)"),
					OrbitYawDeg, Near * 100.0f, Far * 100.0f);
			}
		}
		// mistura em coordenadas polares em volta do carro (ângulo, distância e altura): sem corte e a câmera faz um arco, nunca atravessa
		// o carro como faria uma reta entre a câmera do modo e a órbita
		const float Yaw = ModeYaw + FMath::FindDeltaAngleDegrees(ModeYaw, OrbitYawDeg) * Weight;
		const float Distance = FMath::Lerp(static_cast<float>(FromAim.Size2D()), Radius, Weight);
		const float Up = FMath::Lerp(static_cast<float>(FromAim.Z), static_cast<float>(OrbitUp.Z), Weight);
		const FVector Wanted = Aim + FRotator(0.0f, Yaw, 0.0f).Vector() * Distance + FVector(0.0f, 0.0f, Up);
		// parede: esfera do carro até a posição; se bater, a câmera fica antes do contato (nunca dentro de prédio, poste ou destroço).
		// Entra na hora, mas volta devagar (CrashCamProbeRecoverPerS do caminho por segundo real): passar por trás de um poste não dá
		// tranco na saída
		float Fraction = 1.0f;
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit, Aim, Wanted, FQuat::Identity, ECC_Camera, Probe, Params) && !Hit.bStartPenetrating)
		{
			Fraction = Hit.Time;
			if (ProbeLogs < 6)
			{
				++ProbeLogs;
				UE_LOG(LogDetonauto, Display, TEXT("DNCrashCam: a esfera parou em '%s' (%s) a %.0f%% do caminho, %.2f s reais"),
					*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Time * 100.0f, CrashCamClockS);
			}
		}
		ProbeFraction = FMath::Min(Fraction, ProbeFraction + CrashCamProbeRecoverPerS * static_cast<float>(FApp::GetDeltaTime()));
		const FVector Location = FMath::Lerp(Aim, Wanted, ProbeFraction);
		OutResult.Rotation = FQuat::Slerp(OutResult.Rotation.Quaternion(), (Aim - Location).ToOrientationQuat(), Weight).Rotator();
		OutResult.Location = Location;
	}
	else
	{
		ProbeFraction = 1.0f;
		ProbeLogs = 0;
	}
	LastViewRotation = OutResult.Rotation;
}

void ADNVehicle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AppliedTimeScale != 1.0f)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f); // não deixa o mundo em câmera lenta sem o carro
		AppliedTimeScale = 1.0f;
	}
	// a peça solta fica na pista (até o fim da rodada); a presa e a pendurada saem com o carro
	for (ADNCarPart* Part : Parts)
	{
		if (Part && Part->GetStage() != EDNPartStage::Detached)
		{
			Part->Destroy();
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ADNVehicle::DNEdgeBlur(int32 bOn)
{
	bEdgeBlurOn = bOn != 0;
	UE_LOG(LogDetonauto, Display, TEXT("DNEdgeBlur: desfoque nas bordas %s"), bEdgeBlurOn ? TEXT("ligado") : TEXT("desligado"));
}

void ADNVehicle::DNShake(int32 bOn)
{
	ShakeGlobalScale = bOn != 0 ? 1.0f : 0.0f;
	UE_LOG(LogDetonauto, Display, TEXT("DNShake: tremor de velocidade %s"), bOn != 0 ? TEXT("ligado") : TEXT("desligado"));
}

// =====================================================================================================================
// Peças que soltam e vidros (lote 18b, docs/regras-pecas.md)
// =====================================================================================================================

void ADNVehicle::SpawnPartsAndGlass()
{
	const UDNGameDataSubsystem* Data = DataSubsystemOf(this);
	if (!Data || !GetMesh())
	{
		return;
	}
	UMaterialInterface* DentParent = LoadObject<UMaterialInterface>(nullptr, DentParentPath);
	auto MeshOf = [](const FString& Name) -> USkeletalMesh*
	{
		const FString Asset = FString::Printf(TEXT("SK_DN%s_%s"), CarModelId, *Name);
		return LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), PartsDir, *Asset, *Asset));
	};
	for (const FDNCarPartSpec& Spec : Data->GetCarParts(CarModelId))
	{
		USkeletalMesh* PartMesh = MeshOf(FDNCarParts::PartName(Spec.Part));
		if (!PartMesh)
		{
			UE_LOG(LogDetonauto, Error, TEXT("ADNVehicle: sem a malha da peça %s em %s (rode Tools/Unreal/damage-body.ps1 -Parts)"), FDNCarParts::PartName(Spec.Part), PartsDir);
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADNCarPart* Part = GetWorld()->SpawnActor<ADNCarPart>(GetActorLocation(), GetActorRotation(), Params);
		if (!Part)
		{
			continue;
		}
		const TArray<UMaterialInstanceDynamic*> Dynamics = Part->Init(*GetMesh(), *PartMesh, Spec, DentParent);
		Part->OnPartScrape.AddUObject(this, &ADNVehicle::OnPartScraped);
		Parts.Add(Part);
		PartSpecs.Add(Spec);
		PartStages.Add(EDNPartStage::Attached);
		TArray<TObjectPtr<UMaterialInstanceDynamic>> Owned;
		for (UMaterialInstanceDynamic* Dynamic : Dynamics)
		{
			Owned.Add(Dynamic);
		}
		PartMaterials.Add(Owned);
	}
	for (const FDNGlassSpec& Spec : Data->GetCarGlass(CarModelId))
	{
		USkeletalMesh* PartMesh = MeshOf(FString(TEXT("Glass_")) + FDNGlass::GlassName(Spec.Glass));
		if (!PartMesh)
		{
			UE_LOG(LogDetonauto, Error, TEXT("ADNVehicle: sem a malha do vidro %s em %s"), FDNGlass::GlassName(Spec.Glass), PartsDir);
			continue;
		}
		USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(this, *FString::Printf(TEXT("Glass_%s"), FDNGlass::GlassName(Spec.Glass)));
		Comp->SetupAttachment(GetMesh());
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetSkeletalMesh(PartMesh);
		Comp->RegisterComponent();
		TArray<TObjectPtr<UMaterialInstanceDynamic>> Dynamics;
		for (int32 i = 0; i < Comp->GetNumMaterials(); ++i)
		{
			if (UMaterialInterface* Material = Comp->GetMaterial(i))
			{
				UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, this);
				Comp->SetMaterial(i, Dynamic);
				Dynamics.Add(Dynamic);
			}
		}
		UE_LOG(LogDetonauto, Display, TEXT("ADNVehicle: vidro %s: limites %s, %d material(is) (%s), visível %d"), FDNGlass::GlassName(Spec.Glass),
			*Comp->Bounds.GetBox().ToString(), Comp->GetNumMaterials(), Comp->GetMaterial(0) ? *Comp->GetMaterial(0)->GetName() : TEXT("-"), Comp->IsVisible() ? 1 : 0);
		GlassComps.Add(Comp);
		GlassSpecs.Add(Spec);
		GlassBurst.Add(false);
		GlassMaterials.Add(Dynamics);
		GlassCrackCenter.Add(FVector::ZeroVector);
	}
	UE_LOG(LogDetonauto, Display, TEXT("ADNVehicle: %d peça(s) que soltam e %d vidro(s) do %s"), Parts.Num(), GlassComps.Num(), CarModelId);
}

void ADNVehicle::RegisterPartMaterials()
{
	if (!DentMaterial)
	{
		return;
	}
	const UMaterial* Base = DentMaterial->GetBaseMaterial();
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		for (UMaterialInstanceDynamic* Dynamic : PartMaterials[i])
		{
			if (Dynamic && PartStages[i] != EDNPartStage::Detached)
			{
				DentMaterials.AddUnique(Dynamic);
			}
		}
	}
	for (const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Dynamics : GlassMaterials)
	{
		for (UMaterialInstanceDynamic* Dynamic : Dynamics)
		{
			if (Dynamic && Dynamic->GetBaseMaterial() == Base)
			{
				DentMaterials.AddUnique(Dynamic); // a lente de farol (material do template) fica de fora, como na carroceria do lote 17
			}
		}
	}
}

EDNPartStage ADNVehicle::PartStage(EDNDamagePart Part) const
{
	const int32 Index = PartSpecs.IndexOfByPredicate([Part](const FDNCarPartSpec& S) { return S.Part == Part; });
	return PartStages.IsValidIndex(Index) ? PartStages[Index] : EDNPartStage::Attached;
}

float ADNVehicle::GlassCrack(EDNGlass Glass) const
{
	const int32 Index = GlassSpecs.IndexOfByPredicate([Glass](const FDNGlassSpec& S) { return S.Glass == Glass; });
	return GlassSpecs.IsValidIndex(Index) ? FDNGlass::Crack(GlassSpecs[Index], DamageState) : 0.0f;
}

bool ADNVehicle::IsGlassBurst(EDNGlass Glass) const
{
	const int32 Index = GlassSpecs.IndexOfByPredicate([Glass](const FDNGlassSpec& S) { return S.Glass == Glass; });
	return GlassBurst.IsValidIndex(Index) && GlassBurst[Index];
}

void ADNVehicle::EvaluatePartsAndGlass(const FDNImpact* Impact)
{
	if (Impact)
	{
		LastImpactLocal = Impact->LocalPositionCm;
	}
	FVector Center, Extent;
	GetBodyBox(Center, Extent);
	const FTransform CarFrame = BodyComp ? BodyComp->GetComponentTransform() : GetActorTransform();
	// para fora do carro, do centro da carroceria para o ponto da batida: o lado para onde a peça solta sai
	const FVector Outward = CarFrame.TransformVectorNoScale((LastImpactLocal - Center).GetSafeNormal());

	const TArray<FDNPartChange> Changes = FDNCarParts::Evaluate(PartSpecs, PartStages, DamageState, Impact);
	for (const FDNPartChange& Change : Changes)
	{
		ADNCarPart* Part = Parts.IsValidIndex(Change.Index) ? Parts[Change.Index].Get() : nullptr;
		if (!Part)
		{
			continue;
		}
		if (Change.To == EDNPartStage::Hanging)
		{
			Part->Hang(LastImpactLocal);
		}
		else if (Change.To == EDNPartStage::Detached)
		{
			Part->Detach(Outward);
			for (UMaterialInstanceDynamic* Dynamic : PartMaterials[Change.Index])
			{
				DentMaterials.Remove(Dynamic); // solta: voa com o amassado e o desgaste do momento
			}
		}
	}
	if (!Changes.IsEmpty())
	{
		ADNCarPart::EnforceBudget(*GetWorld(), DamageConfig.MaxActive);
	}

	// vidros: o centro da trinca é o ponto da batida mais perto do vidro quando a trinca começa, puxado para dentro da caixa do vidro
	// (GlassCrackInset): a borda do para-brisa fica embaixo do capô, e a trinca nascendo ali não aparece
	for (int32 i = 0; i < GlassSpecs.Num(); ++i)
	{
		const USkeletalMesh* WindowMesh = GlassComps[i] ? GlassComps[i]->GetSkeletalMeshAsset() : nullptr;
		if (GlassCrackCenter[i].IsZero() && WindowMesh && FDNGlass::Crack(GlassSpecs[i], DamageState) > 0.0f)
		{
			const FBoxSphereBounds B = WindowMesh->GetImportedBounds();
			const FVector Inner = B.BoxExtent * (1.0f - GlassCrackInset);
			GlassCrackCenter[i] = FBox(B.Origin - Inner, B.Origin + Inner).GetClosestPointTo(LastImpactLocal);
		}
	}
	const TArray<int32> Burst = FDNGlass::Evaluate(GlassSpecs, GlassBurst, DamageState, Impact);
	PushGlassCrack();
	for (const int32 i : Burst)
	{
		USkeletalMeshComponent* Glass = GlassComps.IsValidIndex(i) ? GlassComps[i].Get() : nullptr;
		if (!Glass)
		{
			continue;
		}
		Glass->SetVisibility(false);
		const FBoxSphereBounds B = Glass->Bounds;
		const FVector Velocity = GetMesh()->GetPhysicsLinearVelocityAtPoint(B.Origin) + Outward * 150.0;
		// lote 19: os cacos do Niagara pela área do vidro (as duas maiores medidas da caixa); sem o sistema, os cacos antigos
		TArray<double> Sides = { B.BoxExtent.X, B.BoxExtent.Y, B.BoxExtent.Z };
		Sides.Sort([](double A, double C) { return A > C; });
		const float AreaM2 = static_cast<float>((2.0 * Sides[0] / 100.0) * (2.0 * Sides[1] / 100.0));
		UDNEffectsSubsystem* Fx = GetWorld()->GetSubsystem<UDNEffectsSubsystem>();
		if (!Fx || !Fx->OnGlassBurst(B.Origin, B.BoxExtent * 0.8, Velocity, AreaM2))
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ADNGlassShards* Shards = GetWorld()->SpawnActor<ADNGlassShards>(FVector::ZeroVector, FRotator::ZeroRotator, Params))
			{
				UMaterialInterface* Material = GlassMaterials[i].IsEmpty() ? nullptr : GlassMaterials[i][0].Get();
				Shards->Burst(B.Origin, B.BoxExtent * 0.8, Velocity, ShardsPerGlass, Material);
			}
		}
		UE_LOG(LogDetonauto, Display, TEXT("DNGlass: %s estourou (trinca %.2f)"), FDNGlass::GlassName(GlassSpecs[i].Glass), FDNGlass::Crack(GlassSpecs[i], DamageState));
	}
}

void ADNVehicle::PushGlassCrack()
{
	for (int32 i = 0; i < GlassSpecs.Num(); ++i)
	{
		const float Crack = FDNGlass::Crack(GlassSpecs[i], DamageState);
		for (UMaterialInstanceDynamic* Dynamic : GlassMaterials[i])
		{
			if (!Dynamic)
			{
				continue;
			}
			// vidro de janela (material do amassado): a trinca do material; lente (material do template): fica fosca
			Dynamic->SetScalarParameterValue(TEXT("GlassCrack"), Crack);
			Dynamic->SetVectorParameterValue(TEXT("GlassCrackCenter"), FLinearColor(GlassCrackCenter[i].X, GlassCrackCenter[i].Y, GlassCrackCenter[i].Z));
			Dynamic->SetScalarParameterValue(TEXT("Opacity"), FMath::Lerp(LensOpacity, LensCrackedOpacity, Crack));
		}
	}
}

void ADNVehicle::ResetPartsAndGlass()
{
	const float Zero[FDNDamageMorph::ZoneCount] = {};
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		if (Parts[i])
		{
			Parts[i]->ResetPart();
			Parts[i]->SetMorphWeights(Zero, FDNDamageMorph::ZoneCount);
		}
		PartStages[i] = EDNPartStage::Attached;
	}
	for (int32 i = 0; i < GlassComps.Num(); ++i)
	{
		GlassBurst[i] = false;
		GlassCrackCenter[i] = FVector::ZeroVector;
		if (GlassComps[i])
		{
			GlassComps[i]->SetVisibility(true);
			GlassComps[i]->ClearMorphTargets();
		}
	}
	RegisterPartMaterials();
	PushDentsToMaterial();
	PushGlassCrack();
}

void ADNVehicle::DNShowGlass(int32 bOn)
{
	for (int32 i = 0; i < GlassComps.Num(); ++i)
	{
		if (GlassComps[i])
		{
			GlassComps[i]->SetVisibility(bOn != 0 && !GlassBurst[i]);
		}
	}
	UE_LOG(LogDetonauto, Display, TEXT("DNShowGlass: vidros %s"), bOn != 0 ? TEXT("à mostra") : TEXT("escondidos"));
}
