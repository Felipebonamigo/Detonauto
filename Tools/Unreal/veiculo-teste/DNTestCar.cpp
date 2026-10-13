#include "DNTestCar.h"
#include "DNTestCarWheels.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "HAL/PlatformMisc.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "PhysicsEngine/BodyInstance.h"
#include "VeiculoTeste.h"

namespace
{
	// Assets reaproveitados do carro esportivo do template (copiados para Content/Vehicles/SportsCar).
	const TCHAR* SkeletalMeshPath = TEXT("/Game/Vehicles/SportsCar/SKM_SportsCar");
	const TCHAR* AnimBlueprintPath = TEXT("/Game/Vehicles/SportsCar/ABP_SportsCar");
	const TCHAR* BodyMeshPath = TEXT("/Game/Vehicles/SportsCar/SM_SportsCar");
	const TCHAR* GlassMeshPath = TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Glass");
	const TCHAR* WheelMeshPath = TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Wheel");

	const FName WheelBones[4] = { FName("Phys_Wheel_FL"), FName("Phys_Wheel_FR"), FName("Phys_Wheel_BL"), FName("Phys_Wheel_BR") };

	constexpr float ProbeSeconds = 10.0f;
}

ADNTestCar::ADNTestCar()
{
	PrimaryActorTick.bCanEverTick = true;

	// --- malha, Physics Asset (vem junto com a malha) e Anim Blueprint ---
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> SkelMesh(SkeletalMeshPath);
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimClass(AnimBlueprintPath);
	if (SkelMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(SkelMesh.Object);
	}
	if (AnimClass.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(AnimClass.Class);
	}
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(FName("Vehicle"));

	// --- partes visuais (estáticas), presas ao esqueleto ---
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(BodyMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GlassMesh(GlassMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelMesh(WheelMeshPath);

	auto MakeVisual = [this](const TCHAR* Name, UStaticMesh* Asset, FName Bone) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(GetMesh(), Bone);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetStaticMesh(Asset);
		return Comp;
	};
	if (BodyMesh.Succeeded())
	{
		MakeVisual(TEXT("Body"), BodyMesh.Object, NAME_None);
	}
	if (GlassMesh.Succeeded())
	{
		MakeVisual(TEXT("Glass"), GlassMesh.Object, NAME_None);
	}
	if (WheelMesh.Succeeded())
	{
		MakeVisual(TEXT("WheelFL"), WheelMesh.Object, WheelBones[0]);
		MakeVisual(TEXT("WheelFR"), WheelMesh.Object, WheelBones[1]);
		MakeVisual(TEXT("WheelBL"), WheelMesh.Object, WheelBones[2]);
		MakeVisual(TEXT("WheelBR"), WheelMesh.Object, WheelBones[3]);
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

	// --- Chaos: chassi, rodas, motor, transmissão e direção, tudo em C++ ---
	UChaosWheeledVehicleMovementComponent* Move = Chaos();

	Move->ChassisHeight = 144.0f;
	Move->DragCoefficient = 0.31f;
	Move->bLegacyWheelFrictionPosition = true;

	Move->WheelSetups.SetNum(4);
	Move->WheelSetups[0].WheelClass = UDNTestWheelFront::StaticClass();
	Move->WheelSetups[1].WheelClass = UDNTestWheelFront::StaticClass();
	Move->WheelSetups[2].WheelClass = UDNTestWheelRear::StaticClass();
	Move->WheelSetups[3].WheelClass = UDNTestWheelRear::StaticClass();
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
	Torque->AddKey(0.0f, 0.6f);
	Torque->AddKey(2000.0f, 0.9f);
	Torque->AddKey(4500.0f, 1.0f);
	Torque->AddKey(7000.0f, 0.7f);

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

UChaosWheeledVehicleMovementComponent* ADNTestCar::Chaos() const
{
	return CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
}

bool ADNTestCar::LoadTuning(FDNCarTuning& OutTuning) const
{
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), FDNCarTuning::CsvRelativePath);
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *Path))
	{
		UE_LOG(LogVeiculoTeste, Error, TEXT("DNTestCar: não consegui ler %s"), *Path);
		return false;
	}

	UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
	Table->RowStruct = FDNTestCarRow::StaticStruct();
	for (const FString& Problem : Table->CreateTableFromCSVString(Csv))
	{
		UE_LOG(LogVeiculoTeste, Error, TEXT("DNTestCar: CSV: %s"), *Problem);
		return false;
	}

	const FDNTestCarRow* Row = Table->FindRow<FDNTestCarRow>(TuningRow, TEXT("DNTestCar"), false);
	if (!Row)
	{
		UE_LOG(LogVeiculoTeste, Error, TEXT("DNTestCar: linha '%s' não existe em %s"), *TuningRow.ToString(), *Path);
		return false;
	}

	FString Error;
	if (!FDNCarTuning::ValidateRow(*Row, Error))
	{
		UE_LOG(LogVeiculoTeste, Error, TEXT("DNTestCar: linha '%s' inválida: %s"), *TuningRow.ToString(), *Error);
		return false;
	}

	OutTuning = FDNCarTuning::FromRow(*Row);
	return true;
}

const TArray<FName>& ADNTestCar::HotkeyRows()
{
	static const TArray<FName> Rows = { FName(TEXT("arcade-1")), FName(TEXT("arcade-2")), FName(TEXT("arcade-3")), FName(TEXT("Padrao")) };
	return Rows;
}

void ADNTestCar::WriteCreationParameters(const FDNCarTuning& T)
{
	UChaosWheeledVehicleMovementComponent* Move = Chaos();

	// Massa e torque entram na criação do veículo (UpdateMassProperties usa Mass; SetMassOverrideInKg não vale aqui).
	Move->Mass = T.MassKg;
	Move->EngineSetup.MaxTorque = T.MaxTorque;

	// O Chaos lê as classes de roda (CDO) ao criar o veículo; a barra estabilizadora (RollbarScaling) só existe aí:
	// não há função para mudá-la depois. Por isso gravamos nas classes e, ao trocar de preset, recriamos o estado físico.
	UDNTestWheelFront* Front = GetMutableDefault<UDNTestWheelFront>();
	UDNTestWheelRear* Rear = GetMutableDefault<UDNTestWheelRear>();
	for (UChaosVehicleWheel* Wheel : { static_cast<UChaosVehicleWheel*>(Front), static_cast<UChaosVehicleWheel*>(Rear) })
	{
		Wheel->SpringRate = T.SpringRate;
		Wheel->SuspensionDampingRatio = T.SuspensionDampingRatio;
		Wheel->SuspensionMaxRaise = T.SuspensionTravelCm;
		Wheel->SuspensionMaxDrop = T.SuspensionTravelCm;
		Wheel->RollbarScaling = T.RollbarScaling;
	}
	Rear->FrictionForceMultiplier = UDNTestWheelRear::TemplateFriction * T.RearGripFactor;
	Rear->MaxHandBrakeTorque = T.HandbrakeTorque;

	AppliedSteeringGain = T.SteeringGain;
}

void ADNTestCar::ApplyRuntimeParameters(const FDNCarTuning& T)
{
	// centro de massa: deslocamento em relação ao calculado pela malha (COMNudge do corpo)
	GetMesh()->SetCenterOfMass(FVector(0.0f, 0.0f, -T.CenterOfMassDropCm), NAME_None);

	// câmera
	CameraBoom->TargetArmLength = T.CameraDistanceCm;
	CameraBoom->SocketOffset.Z = T.CameraHeightCm;
	Camera->SetFieldOfView(T.CameraFov);

	// aderência: a lateral (curva de deslizamento) vale nas 4 rodas; a traseira também leva o fator do eixo e o freio de mão
	HandbrakeRamp = 0.0f;
	AppliedRearMultiplier = -1.0f;
	ApplyWheelGrip(true);

	if (const FBodyInstance* Body = GetMesh()->GetBodyInstance())
	{
		UE_LOG(LogVeiculoTeste, Display,
			TEXT("DNTestCar: preset '%s' aplicado: massa pedida=%.0f kg (no corpo=%.1f kg), COM local=%s (queda pedida=%.0f cm), torque=%.0f, "
				"aderência lateral=%.2f, traseira x%.2f, freio de mão traseira x%.2f (rampa %.2f/%.2f s, torque %.0f), mola=%.0f, amortecimento=%.2f, "
				"curso=%.0f cm, barra=%.2f, ganho do volante=%.2f, câmera=%.0f/%.0f cm FOV %.0f, nitro=%.0f m/s²"),
			*TuningRow.ToString(), T.MassKg, Body->GetBodyMass(), *Body->GetMassSpaceLocal().GetTranslation().ToCompactString(), T.CenterOfMassDropCm,
			T.MaxTorque, T.LateralGrip, T.RearGripFactor, T.HandbrakeRearGrip, T.HandbrakeRampInSeconds, T.HandbrakeRampOutSeconds, T.HandbrakeTorque,
			T.SpringRate, T.SuspensionDampingRatio, T.SuspensionTravelCm, T.RollbarScaling, T.SteeringGain,
			T.CameraDistanceCm, T.CameraHeightCm, T.CameraFov, T.NitroAccelMps2);
	}
}

void ADNTestCar::ApplyWheelGrip(bool bForce)
{
	const float HandbrakeMultiplier = FDNHandbrakeRamp::RearGripMultiplier(Tuning.HandbrakeRearGrip, HandbrakeRamp);
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
		const float AxleFactor = bRear ? Tuning.RearGripFactor * HandbrakeMultiplier : 1.0f;
		Move->SetWheelSlipGraphMultiplier(i, Tuning.LateralGrip * AxleFactor);
		if (bRear)
		{
			Move->SetWheelFrictionMultiplier(i, UDNTestWheelRear::TemplateFriction * AxleFactor);
		}
	}
}

bool ADNTestCar::SwitchPreset(FName Row)
{
	const FName Previous = TuningRow;
	TuningRow = Row;
	FDNCarTuning New;
	if (!LoadTuning(New))
	{
		TuningRow = Previous;
		return false;
	}
	Tuning = New;
	bTuningLoaded = true;

	WriteCreationParameters(Tuning);
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
	ApplyRuntimeParameters(Tuning);
	return true;
}

void ADNTestCar::PreRegisterAllComponents()
{
	Super::PreRegisterAllComponents();

	// -DNCarRow=arcade-2 na linha de comando escolhe a linha do CSV sem abrir o editor
	FString RowFromCommandLine;
	if (FParse::Value(FCommandLine::Get(), TEXT("DNCarRow="), RowFromCommandLine))
	{
		TuningRow = FName(*RowFromCommandLine);
	}

	// Antes do registro: é o último momento em que dá para mexer no que o Chaos lê ao criar o veículo.
	bTuningLoaded = LoadTuning(Tuning);
	if (bTuningLoaded)
	{
		WriteCreationParameters(Tuning);
	}
}

void ADNTestCar::BeginPlay()
{
	Super::BeginPlay();

	if (bTuningLoaded)
	{
		ApplyRuntimeParameters(Tuning);
	}

	bShot = FParse::Param(FCommandLine::Get(), TEXT("DNCarShot"));
	bProbe = FParse::Param(FCommandLine::Get(), TEXT("DNCarProbe"));
	if (bProbe)
	{
		ProbeScenario = TEXT("Sin");
		FParse::Value(FCommandLine::Get(), TEXT("DNCarScenario="), ProbeScenario);
		ProbeSeconds = (ProbeScenario == TEXT("Handbrake") || ProbeScenario == TEXT("Turn") || ProbeScenario == TEXT("Switch")) ? 9.0f : (ProbeScenario == TEXT("Nitro") || ProbeScenario == TEXT("Accel")) ? 3.6f : 10.0f;
		Chaos()->SetRequiresControllerForInputs(false);
		UE_LOG(LogVeiculoTeste, Display, TEXT("DNTestCar: medição '%s' de %.0f s iniciada"), *ProbeScenario, ProbeSeconds);
	}
}

float ADNTestCar::SpeedKmh() const
{
	return GetVelocity().Size() * 0.036f; // cm/s -> km/h
}

FString ADNTestCar::NitroText() const
{
	if (Tuning.NitroAccelMps2 <= 0.0f)
	{
		return TEXT("Nitro: desligado neste preset");
	}
	const int32 Filled = FMath::Clamp(FMath::RoundToInt(Nitro.Bar / FDNTestNitro::MaxBar * 10.0f), 0, 10);
	const FString BarText = FString::ChrN(Filled, TEXT('#')) + FString::ChrN(10 - Filled, TEXT('-'));
	const TCHAR* State = Nitro.bActive ? TEXT("ATIVO") : (Nitro.Bar <= 0.0f ? TEXT("vazio") : (Nitro.Bar < FDNTestNitro::MaxBar ? TEXT("recarregando") : TEXT("pronto")));
	return FString::Printf(TEXT("Nitro (Shift): %s  [%s] %d"), State, *BarText, FMath::RoundToInt(Nitro.Bar));
}

TArray<FString> ADNTestCar::HudLines() const
{
	return {
		FString::Printf(TEXT("Preset: %s     (1 = arcade-1   2 = arcade-2   3 = arcade-3   0 = Padrao)"), *TuningRow.ToString()),
		FString::Printf(TEXT("Velocidade: %.0f km/h"), SpeedKmh()),
		NitroText() };
}

void ADNTestCar::UpdateHud() const
{
	if (!GEngine || !IsLocallyControlled())
	{
		return;
	}
	// Conferido numa captura de tela: a mensagem adicionada por último fica em cima. Por isso a ordem de chamada é a
	// inversa da ordem na tela (nitro, velocidade, preset).
	const FVector2D Scale(1.6f, 1.6f);
	const TArray<FString> Lines = HudLines();
	GEngine->AddOnScreenDebugMessage(9101, 0.0f, Nitro.bActive ? FColor::Cyan : FColor::White, Lines[2], false, Scale);
	GEngine->AddOnScreenDebugMessage(9102, 0.0f, FColor::White, Lines[1], false, Scale);
	GEngine->AddOnScreenDebugMessage(9103, 0.0f, FColor::Yellow, Lines[0], false, Scale);
}

void ADNTestCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// freio de mão: a perda de aderência da traseira sobe e desce com rampa
	HandbrakeRamp = FDNHandbrakeRamp::Step(HandbrakeRamp, bHandbrakeHeld, DeltaSeconds, Tuning.HandbrakeRampInSeconds, Tuning.HandbrakeRampOutSeconds);
	ApplyWheelGrip(false);

	// nitro de teste: aceleração extra para frente enquanto segura (força = massa x aceleração, em cm/s²)
	const bool bNitroAllowed = Tuning.NitroAccelMps2 > 0.0f;
	Nitro.Step(bNitroHeld && bNitroAllowed, DeltaSeconds, Tuning.NitroDrainPerSecond, Tuning.NitroRegenPerSecond);
	if (Nitro.bActive)
	{
		GetMesh()->AddForce(GetActorForwardVector() * (Tuning.NitroAccelMps2 * 100.0f * Chaos()->Mass));
	}

	UpdateHud();
	if (bProbe)
	{
		ProbeTick(DeltaSeconds);
	}
	if (bShot)
	{
		ShotTick(DeltaSeconds);
	}
}

void ADNTestCar::ShotTick(float DeltaSeconds)
{
	// 0-3,4 s: acelera reto; nitro de 2,0 s em diante; em 3,4 s pede a captura de tela; fecha em 5 s.
	const float Before = ShotTime;
	ShotTime += DeltaSeconds;
	auto Crossed = [&](float T) { return Before < T && ShotTime >= T; };
	Chaos()->SetThrottleInput(ShotTime < 3.6f ? 1.0f : 0.0f);
	if (Crossed(2.0f)) { NitroStart(FInputActionValue()); }
	if (Crossed(3.4f))
	{
		for (const FString& Line : HudLines()) { UE_LOG(LogVeiculoTeste, Display, TEXT("DNCarShot HUD: %s"), *Line); }
		FScreenshotRequest::RequestScreenshot(TEXT("dn-hud"), true, false);
	}
	if (Crossed(5.0f))
	{
		FPlatformMisc::RequestExit(false);
	}
}

void ADNTestCar::ProbeTick(float DeltaSeconds)
{
	const double Now = FPlatformTime::Seconds();
	if (LastTickSeconds > 0.0)
	{
		ProbeFrameMs.Add(static_cast<float>((Now - LastTickSeconds) * 1000.0));
	}
	LastTickSeconds = Now;

	const float Before = ProbeTime;
	ProbeTime += DeltaSeconds;
	auto Crossed = [&](float T) { return Before < T && ProbeTime >= T; };
	UChaosWheeledVehicleMovementComponent* Move = Chaos();

	if (ProbeScenario == TEXT("Handbrake") || ProbeScenario == TEXT("Turn"))
	{
		// Acelera reto até 55 km/h (ou 3,5 s). Aí começa a manobra (T0): volante a 0,5 e acelerador a 0,3 por 1 s, com o
		// freio de mão apertado de T0+0,1 s a T0+0,9 s. Depois solta tudo. Mede o deslizamento e a guinada em T0..T0+3 s.
		// O mapa tem parede a ~50 m do ponto de partida; por isso a manobra começa cedo e a janela é curta.
		const FInputActionValue None;
		if (ProbeT0 < 0.0f && (SpeedKmh() >= 55.0f || ProbeTime >= 3.5f))
		{
			ProbeT0 = ProbeTime;
			ProbeSpeedAtManeuver = SpeedKmh();
		}
		const float Since = ProbeT0 < 0.0f ? -1.0f : ProbeTime - ProbeT0;
		if (Since < 0.0f)
		{
			Move->SetThrottleInput(1.0f);
			Move->SetSteeringInput(0.0f);
		}
		else if (Since < 1.0f)
		{
			Move->SetThrottleInput(0.3f);
			Move->SetSteeringInput(FMath::Clamp(0.5f * AppliedSteeringGain, -1.0f, 1.0f));
		}
		else
		{
			Move->SetThrottleInput(0.0f);
			Move->SetSteeringInput(0.0f);
		}
		const bool bUseHandbrake = ProbeScenario == TEXT("Handbrake"); // o cenário Turn faz a mesma curva SEM freio de mão
		if (bUseHandbrake && Since >= 0.1f && Since < 0.9f && !bHandbrakeHeld) { HandbrakeStart(None); }
		if (Since >= 0.9f && bHandbrakeHeld) { HandbrakeStop(None); }

		if (Since >= 0.0f && Since < 3.0f)
		{
			const FVector Local = GetActorTransform().InverseTransformVectorNoScale(GetVelocity());
			if (Local.Size() > 300.0f) // acima de ~11 km/h o ângulo faz sentido
			{
				ProbeMaxSlipDeg = FMath::Max(ProbeMaxSlipDeg, FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(Local.Y, FMath::Abs(Local.X)))));
			}
			ProbeMaxYawRateDegS = FMath::Max(ProbeMaxYawRateDegS, FMath::Abs(GetMesh()->GetPhysicsAngularVelocityInDegrees().Z));
			ProbeMinSpeedKmh = FMath::Min(ProbeMinSpeedKmh, SpeedKmh());
		}
	}
	else if (ProbeScenario == TEXT("Nitro") || ProbeScenario == TEXT("Accel"))
	{
		// reto, acelerador a fundo; no cenário Nitro, Shift apertado de 1,2 s a 2,4 s
		Move->SetThrottleInput(1.0f);
		Move->SetSteeringInput(0.0f);
		if (ProbeScenario == TEXT("Nitro") && Crossed(1.2f)) { NitroStart(FInputActionValue()); }
		if (ProbeScenario == TEXT("Nitro") && Crossed(2.4f)) { NitroStop(FInputActionValue()); }
	}
	else if (ProbeScenario == TEXT("Switch"))
	{
		Move->SetThrottleInput(1.0f);
		Move->SetSteeringInput(0.0f);
		if (Crossed(1.4f)) { UE_LOG(LogVeiculoTeste, Display, TEXT("DNCarProbe: trocando para arcade-2 a %.1f km/h -> %s"), SpeedKmh(), SwitchPreset(FName("arcade-2")) ? TEXT("ok") : TEXT("FALHOU")); }
		if (Crossed(2.2f)) { UE_LOG(LogVeiculoTeste, Display, TEXT("DNCarProbe: trocando para Padrao a %.1f km/h -> %s"), SpeedKmh(), SwitchPreset(FName("Padrao")) ? TEXT("ok") : TEXT("FALHOU")); }
	}
	else
	{
		// anda de verdade: acelera e esterça em vai-e-vem
		Move->SetThrottleInput(1.0f);
		Move->SetSteeringInput(FMath::Sin(ProbeTime * 1.5f) * AppliedSteeringGain);
	}

	// diagnóstico a cada ~2 s (a cada 0,2 s no cenário Switch)
	const float Period = (ProbeScenario == TEXT("Switch") || ProbeScenario == TEXT("Nitro") || ProbeScenario == TEXT("Accel")) ? 0.2f : 2.0f;
	if (FMath::FloorToInt(ProbeTime / Period) != FMath::FloorToInt(Before / Period))
	{
		int32 InAir = 0;
		for (const UChaosVehicleWheel* W : Move->Wheels) { InAir += W && W->IsInAir() ? 1 : 0; }
		UE_LOG(LogVeiculoTeste, Display, TEXT("DNCarProbe t=%.1f: preset=%s velocidade=%.1f km/h marcha=%d rpm=%.0f rodas=%d no ar=%d nitro=%s pos=%s"),
			ProbeTime, *TuningRow.ToString(), SpeedKmh(), Move->GetCurrentGear(), Move->GetEngineRotationSpeed(),
			Move->Wheels.Num(), InAir, *NitroText(), *GetActorLocation().ToCompactString());
	}
	if (ProbeTime >= ProbeSeconds)
	{
		ProbeFinish();
	}
}

void ADNTestCar::ProbeFinish()
{
	SetActorTickEnabled(false);
	bProbe = false;

	auto Stats = [](TArray<float> V, float& Avg, float& P95, float& Max)
	{
		Avg = P95 = Max = 0.0f;
		if (V.Num() == 0) { return; }
		V.Sort();
		for (float X : V) { Avg += X; }
		Avg /= V.Num();
		P95 = V[FMath::Min(V.Num() - 1, FMath::FloorToInt(V.Num() * 0.95f))];
		Max = V.Last();
	};

	float FAvg, FP95, FMax;
	Stats(ProbeFrameMs, FAvg, FP95, FMax);

	UE_LOG(LogVeiculoTeste, Display,
		TEXT("DNCarProbe: %d quadros em %.1f s | intervalo entre quadros no Game thread (ms): média=%.2f p95=%.2f máx=%.2f | velocidade final=%.0f km/h"),
		ProbeFrameMs.Num(), ProbeTime, FAvg, FP95, FMax, SpeedKmh());
	if (ProbeScenario == TEXT("Handbrake") || ProbeScenario == TEXT("Turn"))
	{
		UE_LOG(LogVeiculoTeste, Display,
			TEXT("DNCarProbe: MANOBRA preset=%s | velocidade ao entrar=%.0f km/h | ângulo de deslizamento máximo=%.1f graus | guinada máxima=%.0f graus/s | velocidade mínima=%.0f km/h"),
			*TuningRow.ToString(), ProbeSpeedAtManeuver, ProbeMaxSlipDeg, ProbeMaxYawRateDegS, ProbeMinSpeedKmh);
	}

	FPlatformMisc::RequestExit(false);
}

void ADNTestCar::CreateInputAssets()
{
	ThrottleAction = NewObject<UInputAction>(this, TEXT("DN_ThrottleAction"));
	ThrottleAction->ValueType = EInputActionValueType::Axis1D;
	BrakeAction = NewObject<UInputAction>(this, TEXT("DN_BrakeAction"));
	BrakeAction->ValueType = EInputActionValueType::Axis1D;
	SteeringAction = NewObject<UInputAction>(this, TEXT("DN_SteeringAction"));
	SteeringAction->ValueType = EInputActionValueType::Axis1D;
	HandbrakeAction = NewObject<UInputAction>(this, TEXT("DN_HandbrakeAction"));
	HandbrakeAction->ValueType = EInputActionValueType::Boolean;
	NitroAction = NewObject<UInputAction>(this, TEXT("DN_NitroAction"));
	NitroAction->ValueType = EInputActionValueType::Boolean;

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("DN_CarMappingContext"));
	MappingContext->MapKey(ThrottleAction, EKeys::W);
	MappingContext->MapKey(BrakeAction, EKeys::S);
	MappingContext->MapKey(SteeringAction, EKeys::D);
	FEnhancedActionKeyMapping& Left = MappingContext->MapKey(SteeringAction, EKeys::A);
	Left.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
	MappingContext->MapKey(HandbrakeAction, EKeys::SpaceBar);
	MappingContext->MapKey(NitroAction, EKeys::LeftShift);

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

void ADNTestCar::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
		Input->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ADNTestCar::Throttle);
		Input->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ADNTestCar::Throttle);
		Input->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &ADNTestCar::Brake);
		Input->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ADNTestCar::Brake);
		Input->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &ADNTestCar::Steering);
		Input->BindAction(SteeringAction, ETriggerEvent::Completed, this, &ADNTestCar::Steering);
		Input->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ADNTestCar::HandbrakeStart);
		Input->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ADNTestCar::HandbrakeStop);
		Input->BindAction(NitroAction, ETriggerEvent::Started, this, &ADNTestCar::NitroStart);
		Input->BindAction(NitroAction, ETriggerEvent::Completed, this, &ADNTestCar::NitroStop);
		for (int32 i = 0; i < PresetActions.Num(); ++i)
		{
			Input->BindAction(PresetActions[i], ETriggerEvent::Started, this, &ADNTestCar::PresetKey, i);
		}
	}
	else
	{
		UE_LOG(LogVeiculoTeste, Error, TEXT("'%s' precisa do Enhanced Input Component."), *GetNameSafe(this));
	}
}

void ADNTestCar::Throttle(const FInputActionValue& Value)
{
	Chaos()->SetThrottleInput(Value.Get<float>());
}

void ADNTestCar::Brake(const FInputActionValue& Value)
{
	Chaos()->SetBrakeInput(Value.Get<float>());
}

void ADNTestCar::Steering(const FInputActionValue& Value)
{
	Chaos()->SetSteeringInput(FMath::Clamp(Value.Get<float>() * AppliedSteeringGain, -1.0f, 1.0f));
}

void ADNTestCar::HandbrakeStart(const FInputActionValue& Value)
{
	bHandbrakeHeld = true;
	Chaos()->SetHandbrakeInput(true);
}

void ADNTestCar::HandbrakeStop(const FInputActionValue& Value)
{
	bHandbrakeHeld = false;
	Chaos()->SetHandbrakeInput(false);
}

void ADNTestCar::NitroStart(const FInputActionValue& Value)
{
	bNitroHeld = true;
}

void ADNTestCar::NitroStop(const FInputActionValue& Value)
{
	bNitroHeld = false;
}

void ADNTestCar::PresetKey(const FInputActionValue& Value, int32 HotkeyIndex)
{
	if (HotkeyRows().IsValidIndex(HotkeyIndex) && !SwitchPreset(HotkeyRows()[HotkeyIndex]))
	{
		UE_LOG(LogVeiculoTeste, Error, TEXT("DNTestCar: não consegui trocar para o preset '%s'"), *HotkeyRows()[HotkeyIndex].ToString());
	}
}
