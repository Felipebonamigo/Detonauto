#include "DNMeasureGameMode.h"
#include "DNTestCar.h"
#include "WheeledVehiclePawn.h"
#include "ChaosVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "VeiculoTeste.h"

namespace
{
	const TCHAR* TemplateCarPath = TEXT("/Game/VehicleTemplate/Blueprints/SportsCar/BP_SportsCar_Pawn.BP_SportsCar_Pawn_C");
}

ADNMeasureGameMode::ADNMeasureGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
}

UClass* ADNMeasureGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	return CarClass ? CarClass.Get() : Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ADNMeasureGameMode::BeginPlay()
{
	Super::BeginPlay();

	const TCHAR* Cmd = FCommandLine::Get();
	FString CarKind = TEXT("Template");
	FParse::Value(Cmd, TEXT("DNCarClass="), CarKind);
	FParse::Value(Cmd, TEXT("DNExtra="), ExtraRequested);
	FParse::Value(Cmd, TEXT("DNWarmup="), WarmupSeconds);
	FParse::Value(Cmd, TEXT("DNMeasure="), MeasureSeconds);
	FParse::Value(Cmd, TEXT("DNSeed="), Seed);
	bExtraMove = FParse::Param(Cmd, TEXT("DNExtraMove"));

	if (CarKind.Equals(TEXT("DN"), ESearchCase::IgnoreCase))
	{
		CarClass = ADNTestCar::StaticClass();
	}
	else
	{
		CarClass = LoadClass<APawn>(nullptr, TemplateCarPath);
		if (!CarClass)
		{
			UE_LOG(LogVeiculoTeste, Error, TEXT("DNMeasure: não achei a classe do carro do template: %s"), TemplateCarPath);
			FPlatformMisc::RequestExit(true);
			return;
		}
	}

	// O pawn do jogador já foi criado antes do BeginPlay com a classe padrão; troca pelo carro escolhido.
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* Old = PC->GetPawn(); Old && !Old->IsA(CarClass))
		{
			UE_LOG(LogVeiculoTeste, Display, TEXT("DNMeasure: pawn inicial %s será trocado por %s"), *Old->GetName(), *CarClass->GetName());
			PC->UnPossess();
			Old->Destroy();
			RestartPlayer(PC);
		}
	}

	SpawnExtras();

	UE_LOG(LogVeiculoTeste, Display, TEXT("DNMeasure: carro=%s extras=%d (movendo=%d) aquecimento=%.0f s medição=%.0f s semente=%d"),
		*CarClass->GetName(), Extras.Num(), bExtraMove ? 1 : 0, WarmupSeconds, MeasureSeconds, Seed);
}

void ADNMeasureGameMode::SpawnExtras()
{
	if (ExtraRequested <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* Start = UGameplayStatics::GetActorOfClass(World, APlayerStart::StaticClass());
	const FVector Origin = Start ? Start->GetActorLocation() : FVector::ZeroVector;
	const FVector Fwd = Start ? Start->GetActorForwardVector() : FVector::ForwardVector;
	const FVector Right = Start ? Start->GetActorRightVector() : FVector::RightVector;

	FRandomStream Random(Seed);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DNMeasureGround), false);

	int32 Attempt = 0;
	while (Extras.Num() < ExtraRequested && Attempt < ExtraRequested * 6)
	{
		const int32 Slot = Attempt++;
		const int32 Row = Slot / 8;
		const int32 Col = Slot % 8;
		const FVector Flat = Origin + Fwd * (1500.0f + Row * 650.0f) + Right * ((Col - 3.5f) * 500.0f);

		FHitResult Hit;
		const bool bGround = World->LineTraceSingleByChannel(Hit, Flat + FVector(0, 0, 1500.0f), Flat - FVector(0, 0, 3000.0f), ECC_WorldStatic, Query);
		if (!bGround)
		{
			continue;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FRotator Yaw(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f);
		AWheeledVehiclePawn* Car = World->SpawnActor<AWheeledVehiclePawn>(CarClass, Hit.ImpactPoint + FVector(0, 0, 150.0f), Yaw, Params);
		if (Car)
		{
			Car->GetVehicleMovementComponent()->SetRequiresControllerForInputs(false);
			Extras.Add(Car);
			ExtraPhase.Add(Random.FRandRange(0.0f, 6.28f));
		}
	}
	UE_LOG(LogVeiculoTeste, Display, TEXT("DNMeasure: %d de %d carros extras colocados (%d tentativas)"), Extras.Num(), ExtraRequested, Attempt);
}

void ADNMeasureGameMode::DriveCar(AWheeledVehiclePawn* Car, float Time, float PhaseOffset) const
{
	UChaosVehicleMovementComponent* Move = Car->GetVehicleMovementComponent();
	Move->SetThrottleInput(0.7f);
	Move->SetSteeringInput(FMath::Sin(Time * 0.8f + PhaseOffset));
}

void ADNMeasureGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;

	// jogador: dirige sozinho em vai-e-vem
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AWheeledVehiclePawn* Player = Cast<AWheeledVehiclePawn>(PC->GetPawn()))
		{
			DriveCar(Player, Elapsed, 0.0f);
		}
	}

	// extras: em movimento simples ou parados (freio de mão, mantidos acordados para a física contar)
	WakeTimer += DeltaSeconds;
	for (int32 i = 0; i < Extras.Num(); ++i)
	{
		AWheeledVehiclePawn* Car = Extras[i];
		if (!IsValid(Car)) { continue; }
		if (bExtraMove)
		{
			DriveCar(Car, Elapsed, ExtraPhase[i]);
		}
		else
		{
			Car->GetVehicleMovementComponent()->SetHandbrakeInput(true);
			if (WakeTimer >= 0.5f)
			{
				Car->GetMesh()->WakeAllRigidBodies();
			}
		}
	}
	if (WakeTimer >= 0.5f) { WakeTimer = 0.0f; }

	switch (Phase)
	{
	case EPhase::Warmup:
		if (Elapsed >= WarmupSeconds)
		{
			Phase = EPhase::Measuring;
			GEngine->Exec(GetWorld(), TEXT("CsvProfile Start"));
			{
				// condições reais, lidas agora que o viewport existe (r.ScreenPercentage 0 = padrão do jogo, 100%)
				FVector2D Size = FVector2D::ZeroVector;
				if (GEngine->GameViewport) { GEngine->GameViewport->GetViewportSize(Size); }
				const IConsoleVariable* Sp = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
				const IConsoleVariable* MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
				UE_LOG(LogVeiculoTeste, Display, TEXT("DNMeasure: medição iniciada (t=%.1f s) | viewport=%dx%d r.ScreenPercentage=%.1f t.MaxFPS=%.0f"),
					Elapsed, FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y), Sp ? Sp->GetFloat() : -1.0f, MaxFps ? MaxFps->GetFloat() : -1.0f);
			}
		}
		break;

	case EPhase::Measuring:
		FrameMs.Add(DeltaSeconds * 1000.0f);
		if (Elapsed >= WarmupSeconds + MeasureSeconds)
		{
			GEngine->Exec(GetWorld(), TEXT("CsvProfile Stop"));
			Phase = EPhase::Finishing;
			LogSummary();
		}
		break;

	case EPhase::Finishing:
		// dá tempo ao CSV Profiler de gravar o arquivo antes de sair
		FinishTimer += DeltaSeconds;
		if (FinishTimer >= 5.0f)
		{
			FPlatformMisc::RequestExit(false);
		}
		break;
	}
}

void ADNMeasureGameMode::LogSummary() const
{
	TArray<float> V = FrameMs;
	if (V.Num() == 0) { return; }
	V.Sort();
	double Sum = 0.0;
	for (float X : V) { Sum += X; }
	int32 Alive = 0;
	float MinZ = TNumericLimits<float>::Max();
	for (const AWheeledVehiclePawn* Car : Extras)
	{
		if (IsValid(Car)) { ++Alive; MinZ = FMath::Min(MinZ, Car->GetActorLocation().Z); }
	}
	UE_LOG(LogVeiculoTeste, Display,
		TEXT("DNMeasure: FIM. %d quadros | intervalo entre quadros (ms): média=%.2f p95=%.2f máx=%.2f | extras vivos=%d de %d"),
		V.Num(), Sum / V.Num(), V[FMath::Min(V.Num() - 1, FMath::FloorToInt(V.Num() * 0.95f))], V.Last(), Alive, Extras.Num());
}
