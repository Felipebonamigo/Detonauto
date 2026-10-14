#include "Vehicle/DNVehicleTestGameMode.h"
#include "Vehicle/DNVehicle.h"
#include "Vehicle/DNVehicleArena.h"
#include "Vehicle/DNTurboComponent.h"
#include "Detonauto.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformMisc.h"
#include "UnrealClient.h"

ADNVehicleTestGameMode::ADNVehicleTestGameMode()
{
	DefaultPawnClass = ADNVehicle::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void ADNVehicleTestGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// piso, parede e luz: o mapa é vazio
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Arena = GetWorld()->SpawnActor<ADNTestArena>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
}

AActor* ADNVehicleTestGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!Start)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Start = GetWorld()->SpawnActor<APlayerStart>(FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator, Params);
	}
	return Start;
}

void ADNVehicleTestGameMode::StartPlay()
{
	Super::StartPlay();

	const TCHAR* Cmd = FCommandLine::Get();
	bShotDent = FParse::Param(Cmd, TEXT("DNShotDent"));
	bShot = bShotDent || FParse::Param(Cmd, TEXT("DNCarShot"));
	bProbe = FParse::Param(Cmd, TEXT("DNCarProbe"));
	if (bProbe)
	{
		Scenario = TEXT("Sin");
		FParse::Value(Cmd, TEXT("DNCarScenario="), Scenario);
		FParse::Value(Cmd, TEXT("DNWallKmh="), WallTargetKmh);
		ScenarioSeconds = (Scenario == TEXT("Handbrake") || Scenario == TEXT("Turn") || Scenario == TEXT("Switch")) ? 9.0f
			: (Scenario == TEXT("Turbo") || Scenario == TEXT("Accel")) ? 3.6f : (Scenario == TEXT("Wall") ? 7.0f : 10.0f);
		UE_LOG(LogDetonauto, Display, TEXT("DNCarProbe: cenário '%s' de %.0f s iniciado"), *Scenario, ScenarioSeconds);
	}
}

ADNVehicle* ADNVehicleTestGameMode::FindVehicle() const
{
	return Cast<ADNVehicle>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void ADNVehicleTestGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bProbe && !bShot)
	{
		return;
	}
	ADNVehicle* Vehicle = FindVehicle();
	if (!Vehicle)
	{
		return;
	}
	if (!bPrepared)
	{
		bPrepared = true;
		if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovement()))
		{
			Move->SetRequiresControllerForInputs(false);
		}
	}
	if (bShot)
	{
		ShotTick(*Vehicle, DeltaSeconds);
	}
	else
	{
		ScenarioTick(*Vehicle, DeltaSeconds);
	}
}

void ADNVehicleTestGameMode::ShotTick(ADNVehicle& Vehicle, float DeltaSeconds)
{
	const float Before = ShotTime;
	ShotTime += DeltaSeconds;
	auto Crossed = [&](float T) { return Before < T && ShotTime >= T; };
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle.GetVehicleMovement());

	if (bShotDent)
	{
		// Carro parado. Vista A (frente e lado esquerdo): antes, depois dos 8 amassados. Vista B (traseira e lado direito):
		// depois, e depois de limpar (antes). As capturas são só da cena (sem HUD).
		Move->SetThrottleInput(0.0f);
		Vehicle.SetHandbrake(true);
		auto Shot = [](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(Name, false, false); };
		if (Crossed(0.3f)) { Vehicle.SetShotCamera(135.0f); }
		if (Crossed(1.6f)) { Shot(TEXT("dn-dent-A-antes")); }
		if (Crossed(2.0f)) { Vehicle.ApplyDemoDents(Vehicle.MaxDentCount()); }
		if (Crossed(2.8f)) { Shot(TEXT("dn-dent-A-depois")); }
		if (Crossed(3.2f)) { Vehicle.SetShotCamera(-45.0f); }
		if (Crossed(3.9f)) { Shot(TEXT("dn-dent-B-depois")); }
		if (Crossed(4.3f)) { Vehicle.ClearDents(); }
		if (Crossed(5.0f)) { Shot(TEXT("dn-dent-B-antes")); }
		if (Crossed(5.8f)) { FPlatformMisc::RequestExit(false); }
		return;
	}

	// 0-3,6 s: acelera reto; turbo de 2,0 s em diante; em 3,4 s pede a captura de tela (com o HUD); fecha em 5 s.
	Move->SetThrottleInput(ShotTime < 3.6f ? 1.0f : 0.0f);
	if (Crossed(2.0f)) { Vehicle.SetTurboButton(true); }
	if (Crossed(3.4f))
	{
		for (const FString& Line : Vehicle.HudLines()) { UE_LOG(LogDetonauto, Display, TEXT("DNCarShot HUD: %s"), *Line); }
		FScreenshotRequest::RequestScreenshot(TEXT("dn-hud"), true, false);
	}
	if (Crossed(5.0f))
	{
		FPlatformMisc::RequestExit(false);
	}
}

void ADNVehicleTestGameMode::ScenarioTick(ADNVehicle& Vehicle, float DeltaSeconds)
{
	const float Before = ScenarioTime;
	ScenarioTime += DeltaSeconds;
	auto Crossed = [&](float T) { return Before < T && ScenarioTime >= T; };
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle.GetVehicleMovement());
	const float SpeedKmh = Vehicle.SpeedKmh();

	if (Scenario == TEXT("Handbrake") || Scenario == TEXT("Turn"))
	{
		// Acelera reto até 55 km/h (ou 3,5 s). Aí começa a manobra (T0): volante a 0,5 e acelerador a 0,3 por 1 s, com o
		// freio de mão apertado de T0+0,1 s a T0+0,9 s. Depois solta tudo. Mede o deslizamento e a guinada em T0..T0+3 s.
		// A parede fica a 60 m da partida; por isso a manobra começa cedo e a janela é curta.
		if (ManeuverT0 < 0.0f && (SpeedKmh >= 55.0f || ScenarioTime >= 3.5f))
		{
			ManeuverT0 = ScenarioTime;
			SpeedAtManeuver = SpeedKmh;
		}
		const float Since = ManeuverT0 < 0.0f ? -1.0f : ScenarioTime - ManeuverT0;
		if (Since < 0.0f)
		{
			Move->SetThrottleInput(1.0f);
			Move->SetSteeringInput(0.0f);
		}
		else if (Since < 1.0f)
		{
			Move->SetThrottleInput(0.3f);
			Move->SetSteeringInput(FMath::Clamp(0.5f * Vehicle.AppliedSteeringGain, -1.0f, 1.0f));
		}
		else
		{
			Move->SetThrottleInput(0.0f);
			Move->SetSteeringInput(0.0f);
		}
		const bool bUseHandbrake = Scenario == TEXT("Handbrake"); // o cenário Turn faz a mesma curva SEM freio de mão
		if (bUseHandbrake && Since >= 0.1f && Since < 0.9f) { Vehicle.SetHandbrake(true); }
		if (Since >= 0.9f) { Vehicle.SetHandbrake(false); }

		if (Since >= 0.0f && Since < 3.0f)
		{
			const FVector Local = Vehicle.GetActorTransform().InverseTransformVectorNoScale(Vehicle.GetVelocity());
			if (Local.Size() > 300.0f) // acima de ~11 km/h o ângulo faz sentido
			{
				MaxSlipDeg = FMath::Max(MaxSlipDeg, FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(Local.Y, FMath::Abs(Local.X)))));
			}
			MaxYawRateDegS = FMath::Max(MaxYawRateDegS, FMath::Abs(Vehicle.GetMesh()->GetPhysicsAngularVelocityInDegrees().Z));
			MinSpeedKmh = FMath::Min(MinSpeedKmh, SpeedKmh);
		}
	}
	else if (Scenario == TEXT("Wall"))
	{
		// Reto contra a parede (~3,3 s a fundo); com -DNWallKmh=N segura a velocidade em N km/h (controle proporcional:
		// acelerador cheio abaixo de ~8 km/h do alvo e 0,4 no alvo, porque o freio-motor do carro é forte).
		// Depois a câmera vai para a frente-esquerda e tira a foto do estrago.
		const float Throttle = WallTargetKmh > 0.0f ? FMath::Clamp(0.4f + (WallTargetKmh - SpeedKmh) * 0.08f, 0.0f, 1.0f) : 1.0f;
		Move->SetThrottleInput(ScenarioTime < 4.5f ? Throttle : 0.0f);
		Move->SetSteeringInput(0.0f);
		if (Crossed(5.0f)) { Vehicle.SetShotCamera(135.0f); }
		if (Crossed(6.0f)) { FScreenshotRequest::RequestScreenshot(TEXT("dn-dent-parede"), false, false); }
	}
	else if (Scenario == TEXT("Turbo") || Scenario == TEXT("Accel"))
	{
		// reto, acelerador a fundo; no cenário Turbo, Shift apertado de 1,2 s a 2,4 s
		Move->SetThrottleInput(1.0f);
		Move->SetSteeringInput(0.0f);
		if (Scenario == TEXT("Turbo") && Crossed(1.2f)) { Vehicle.SetTurboButton(true); }
		if (Scenario == TEXT("Turbo") && Crossed(2.4f)) { Vehicle.SetTurboButton(false); }
	}
	else if (Scenario == TEXT("Switch"))
	{
		Move->SetThrottleInput(1.0f);
		Move->SetSteeringInput(0.0f);
		if (Crossed(1.4f)) { UE_LOG(LogDetonauto, Display, TEXT("DNCarProbe: trocando para arcade-2 a %.1f km/h -> %s"), SpeedKmh, Vehicle.SwitchPreset(FName("arcade-2")) ? TEXT("ok") : TEXT("FALHOU")); }
		if (Crossed(2.2f)) { UE_LOG(LogDetonauto, Display, TEXT("DNCarProbe: trocando para Padrao a %.1f km/h -> %s"), SpeedKmh, Vehicle.SwitchPreset(FName("Padrao")) ? TEXT("ok") : TEXT("FALHOU")); }
	}
	else
	{
		// anda de verdade: acelera e esterça em vai-e-vem
		Move->SetThrottleInput(1.0f);
		Move->SetSteeringInput(FMath::Sin(ScenarioTime * 1.5f) * Vehicle.AppliedSteeringGain);
	}

	// diagnóstico a cada ~2 s (a cada 0,2 s nos cenários curtos)
	const float Period = (Scenario == TEXT("Switch") || Scenario == TEXT("Turbo") || Scenario == TEXT("Accel")) ? 0.2f : 2.0f;
	if (FMath::FloorToInt(ScenarioTime / Period) != FMath::FloorToInt(Before / Period))
	{
		int32 InAir = 0;
		for (const UChaosVehicleWheel* W : Move->Wheels) { InAir += W && W->IsInAir() ? 1 : 0; }
		UE_LOG(LogDetonauto, Display, TEXT("DNCarProbe t=%.1f: preset=%s velocidade=%.1f km/h marcha=%d rpm=%.0f rodas=%d no ar=%d turbo=%s pos=%s"),
			ScenarioTime, *Vehicle.CarId.ToString(), SpeedKmh, Move->GetCurrentGear(), Move->GetEngineRotationSpeed(),
			Move->Wheels.Num(), InAir, *Vehicle.HudLines()[2], *Vehicle.GetActorLocation().ToCompactString());
	}
	if (ScenarioTime >= ScenarioSeconds)
	{
		ProbeFinish(Vehicle);
	}
}

void ADNVehicleTestGameMode::ProbeFinish(ADNVehicle& Vehicle)
{
	bProbe = false;
	SetActorTickEnabled(false);
	UE_LOG(LogDetonauto, Display, TEXT("DNCarProbe: fim de %.1f s | velocidade final=%.0f km/h | turbo: barra %.0f"), ScenarioTime, Vehicle.SpeedKmh(), Vehicle.GetTurbo()->GetBar());
	if (Vehicle.IsDentMaterialOn())
	{
		UE_LOG(LogDetonauto, Display, TEXT("DNDent: ao fim: %d amassado(s) de %d, %d batida(s) reais, maior variação de velocidade %.1f m/s, destruído=%d"),
			Vehicle.DentCount(), Vehicle.MaxDentCount(), Vehicle.RealImpactTotal(), Vehicle.MaxRealImpactMs(), Vehicle.IsDestroyed() ? 1 : 0);
	}
	if (Scenario == TEXT("Handbrake") || Scenario == TEXT("Turn"))
	{
		UE_LOG(LogDetonauto, Display,
			TEXT("DNCarProbe: MANOBRA preset=%s | velocidade ao entrar=%.0f km/h | ângulo de deslizamento máximo=%.1f graus | guinada máxima=%.0f graus/s | velocidade mínima=%.0f km/h"),
			*Vehicle.CarId.ToString(), SpeedAtManeuver, MaxSlipDeg, MaxYawRateDegS, MinSpeedKmh);
	}
	FPlatformMisc::RequestExit(false);
}
