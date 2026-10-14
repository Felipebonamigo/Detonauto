#include "Vehicle/DNVehicle.h"
#include "Vehicle/DNVehicleWheels.h"
#include "Vehicle/DNTurboComponent.h"
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

	// --- Chaos: chassi, rodas, motor, transmissão e direção, tudo em C++ ---
	UChaosWheeledVehicleMovementComponent* Move = Chaos();

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
	SetCameraMode(StartCamera);

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
	if (!GEngine || !IsLocallyControlled())
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

	// o turbo abre o campo de visão (sensação de velocidade): o empurrão é grande (medido: +59 a +70% de velocidade em 3 s), mas só
	// aparece na tela se a câmera ajudar; no playtest 1 o Felipe disse que "quase não dá diferença"
	{
		const float KickTarget = Turbo->IsActive() ? 1.0f : 0.0f;
		TurboFovBlend = FMath::FInterpTo(TurboFovBlend, KickTarget, DeltaSeconds, TurboFovBlendSpeed);
		if (CameraModes.IsValidIndex(CameraIndex))
		{
			Camera->SetFieldOfView(CameraModes[CameraIndex].Config.Fov + TurboFovKickDegrees * TurboFovBlend);
		}
	}

	if (DentViewSecondsLeft > 0.0f)
	{
		DentViewSecondsLeft -= DeltaSeconds;
		if (DentViewSecondsLeft <= 0.0f)
		{
			RestoreChaseCamera();
		}
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
	Chaos()->SetThrottleInput(Value.Get<float>());
}

void ADNVehicle::Brake(const FInputActionValue& Value)
{
	Chaos()->SetBrakeInput(Value.Get<float>());
}

void ADNVehicle::Steering(const FInputActionValue& Value)
{
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
	if (bBindRealHits)
	{
		GetMesh()->SetNotifyRigidBodyCollision(true);
		GetMesh()->OnComponentHit.AddUniqueDynamic(this, &ADNVehicle::OnCarHit);
	}
	return true;
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
	PushWearToMaterial();
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

void ADNVehicle::OnCarHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!BodyComp)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	FlushImpacts(Now); // uma janela vencida sai antes de este contato abrir outra

	// Variação de velocidade medida no corpo: velocidade no início do quadro (antes do passo de física) menos a de agora.
	const float DeltaV = FDNDentParams::DeltaVMsBetween(VelocityBeforeStepCmS, GetMesh()->GetPhysicsLinearVelocity());
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
}
