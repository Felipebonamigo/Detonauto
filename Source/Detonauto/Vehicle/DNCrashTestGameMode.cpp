#include "Vehicle/DNCrashTestGameMode.h"
#include "Vehicle/DNVehicle.h"
#include "Vehicle/DNVehicleArena.h"
#include "Crash/DNCrashRoundComponent.h"
#include "Traffic/DNTrafficManager.h"
#include "Traffic/DNTrafficVehicle.h"
#include "Core/DNGameDataSubsystem.h"
#include "Core/DNRecordsSubsystem.h"
#include "Vehicle/DNTurboComponent.h"
#include "Detonauto.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace
{
	/** Quantos segundos o cenário espera depois do fim da rodada (ou do fim da medição) antes de fechar. */
	constexpr float ExitDelaySeconds = 1.5f;
	/** O cenário Crash desiste se não terminar neste tempo [s]. */
	constexpr float CrashScenarioTimeoutSeconds = 70.0f;
	/** Segundos até o jogador ganhar a velocidade do cenário (espera as rodas tocarem o chão). */
	constexpr float LaunchSeconds = 0.8f;
}

ADNCrashTestGameMode::ADNCrashTestGameMode()
{
	CrashRound = CreateDefaultSubobject<UDNCrashRoundComponent>(TEXT("CrashRound"));
}

void ADNCrashTestGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// a estrada de 600 m não tem parede a 60 m
	if (Arena)
	{
		Arena->SetWallEnabled(false);
	}
}

void ADNCrashTestGameMode::StartPlay()
{
	Super::StartPlay();

	const TCHAR* Cmd = FCommandLine::Get();
	FString ScenarioName;
	FParse::Value(Cmd, TEXT("DNCrashScenario="), ScenarioName);
	Scenario = ScenarioName == TEXT("Crash") ? EScenario::Crash : (ScenarioName == TEXT("Traffic") ? EScenario::Traffic : EScenario::None);
	FParse::Value(Cmd, TEXT("DNTraffic="), TrafficCount);
	FParse::Value(Cmd, TEXT("DNSeed="), Seed);
	FParse::Value(Cmd, TEXT("DNCrashKmh="), TargetKmh);
	FParse::Value(Cmd, TEXT("DNWarmup="), Warmup);
	FParse::Value(Cmd, TEXT("DNMeasure="), MeasureSeconds);
	FString Mode;
	FParse::Value(Cmd, TEXT("DNTrafficMode="), Mode);
	bAllPhysics = Mode == TEXT("Physics");

	// tráfego
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Traffic = GetWorld()->SpawnActor<ADNTrafficManager>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	Traffic->Configure(FDNTrafficRoadConfig(), TrafficCount, Seed, 64, bAllPhysics);
	Traffic->OnVehicleImpact.AddUObject(this, &ADNCrashTestGameMode::HandleImpact);
	Traffic->OnVehicleDestroyed.AddUObject(this, &ADNCrashTestGameMode::HandleDestroyed);
	Traffic->SetCrashRoundActive(true);

	// regras do Core
	CrashRound->OnRoundEnded.AddUObject(this, &ADNCrashTestGameMode::HandleRoundEnded);
	UGameInstance* GameInstance = GetGameInstance();
	if (const UDNGameDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UDNGameDataSubsystem>() : nullptr)
	{
		bHasThresholds = Data->TryGetMedalThresholds(CrossingId, Thresholds);
	}
	if (!bHasThresholds)
	{
		UE_LOG(LogDetonauto, Error, TEXT("DNCrash: o cruzamento '%s' nao existe em Crossings.csv ou tem limites invalidos; sem medalha"), *CrossingId);
	}

	// registro de recordes: nunca o save real (a menos que -DNRealSave)
	if (UDNRecordsSubsystem* Records = GameInstance ? GameInstance->GetSubsystem<UDNRecordsSubsystem>() : nullptr)
	{
		if (!FParse::Param(Cmd, TEXT("DNRealSave")))
		{
			SavePathUsed = Scenario == EScenario::None
				? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("DetonautoCrashTest.json"))
				: FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CrashProbe"), FGuid::NewGuid().ToString(EGuidFormats::Digits), TEXT("Detonauto.json"));
			Records->SetSavePath(SavePathUsed);
			Records->LoadFromFile(SavePathUsed); // arquivo de teste: vazio se não existe
		}
		else
		{
			SavePathUsed = Records->GetSavePath();
		}
	}
	UE_LOG(LogDetonauto, Display, TEXT("DNCrash: rodada pronta: %d carros de trafego (%s), semente %d, cruzamento %s, registro em %s"),
		TrafficCount, bAllPhysics ? TEXT("todos Physics") : TEXT("Rail"), Seed, *CrossingId, *SavePathUsed);
}

void ADNCrashTestGameMode::OnMultiplierPickup()
{
	CrashRound->CollectMultiplier();
}

void ADNCrashTestGameMode::HandleImpact(ADNTrafficVehicle* Vehicle, AActor* Other)
{
	// o contato de um carro de tráfego (com o jogador ou com outro carro) liga os relógios do fim da rodada
	++ImpactEvents;
	if (!CrashRound->GetState().bFirstImpactHappened)
	{
		FirstImpactAt = Elapsed;
		UE_LOG(LogDetonauto, Display, TEXT("DNCrash: primeiro impacto em t=%.2f s (com '%s')"), Elapsed, *GetNameSafe(Other));
	}
	CrashRound->RegisterImpact();
}

void ADNCrashTestGameMode::HandleDestroyed(ADNTrafficVehicle* Vehicle, int32 ValueCash)
{
	const int32 Points = CrashRound->RegisterDestruction(ValueCash);
	UE_LOG(LogDetonauto, Display, TEXT("DNCrash: carro destruido (valor %d): +%d pontos, total %d, destruidos %d"),
		ValueCash, Points, CrashRound->GetScore(), CrashRound->GetState().DestroyedCount);
}

void ADNCrashTestGameMode::HandleRoundEnded(int32 FinalScore)
{
	if (bEnded)
	{
		return;
	}
	bEnded = true;
	EndedAt = Elapsed;
	RoundDuration = CrashRound->GetState().SecondsSinceFirstImpact;
	Traffic->SetCrashRoundActive(false);
	Medal = bHasThresholds ? FDNMedals::Evaluate(Thresholds, FinalScore) : EDNMedal::None;

	// o recorde é gravado UMA vez por rodada (OnRoundEnded dispara uma vez; a guarda cobre qualquer repetição)
	if (!bRecordRegistered)
	{
		bRecordRegistered = true;
		++RecordRegistrations;
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UDNRecordsSubsystem* Records = GameInstance->GetSubsystem<UDNRecordsSubsystem>())
			{
				bNewRecord = Records->RegisterResult(CrossingId, FinalScore);
			}
		}
	}
	LogResult();
}

void ADNCrashTestGameMode::LogResult()
{
	const FDNCrashState& S = CrashRound->GetState();
	const FDNTrafficStats& T = Traffic->GetStats();
	UE_LOG(LogDetonauto, Display,
		TEXT("DNCrash: RESULTADO pontos=%d destruidos=%d multiplicador=%d medalha=%s novo_recorde=%d duracao_da_rodada=%.2f s primeiro_impacto_em=%.2f s fim_em=%.2f s | trafego: nasceram=%d acordaram=%d despachados=%d (rail=%d physics=%d) batidas_em_rail_antes_de_acordar=%d eventos_de_impacto=%d registros_gravados=%d"),
		S.Score.Total, S.DestroyedCount, S.Multiplier, FDNMedals::ToString(Medal), bNewRecord ? 1 : 0, RoundDuration, FirstImpactAt, EndedAt,
		T.Spawned, T.Woken, T.Despawned, T.ActiveRail, T.ActivePhysics, T.RailHitsBeforeWake, ImpactEvents, RecordRegistrations);
}

float ADNCrashTestGameMode::PlayerSpeedMs(const ADNVehicle& Vehicle) const
{
	return static_cast<float>(Vehicle.GetVelocity().Size() / 100.0);
}

void ADNCrashTestGameMode::UpdateHud(const ADNVehicle& Vehicle) const
{
	if (!GEngine)
	{
		return;
	}
	const FDNCrashState& S = CrashRound->GetState();
	const FVector2D Scale(1.6f, 1.6f);
	const EDNMedal Current = bHasThresholds ? FDNMedals::Evaluate(Thresholds, S.Score.Total) : EDNMedal::None;
	// a última mensagem adicionada fica em cima: ordem inversa da tela (o turbo já está nas linhas do próprio carro)
	GEngine->AddOnScreenDebugMessage(9205, 0.0f, FColor::White, FString::Printf(TEXT("Tempo: %.1f s     Rodada: %s"), Elapsed, bEnded ? TEXT("ENCERRADA") : (S.bFirstImpactHappened ? TEXT("em andamento") : TEXT("esperando a primeira batida"))), false, Scale);
	GEngine->AddOnScreenDebugMessage(9204, 0.0f, FColor::Silver, FString::Printf(TEXT("Medalha: %s     (cruzamento %s)"), FDNMedals::ToString(Current), *CrossingId), false, Scale);
	GEngine->AddOnScreenDebugMessage(9203, 0.0f, FColor::Orange, FString::Printf(TEXT("Destruidos: %d     Multiplicador: x%d"), S.DestroyedCount, S.Multiplier), false, Scale);
	GEngine->AddOnScreenDebugMessage(9202, 0.0f, FColor::Yellow, FString::Printf(TEXT("PONTOS: %d"), S.Score.Total), false, Scale);
}

void ADNCrashTestGameMode::DriveCrashScenario(ADNVehicle& Vehicle, float DeltaSeconds)
{
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle.GetVehicleMovement());
	const float SpeedKmh = Vehicle.SpeedKmh();
	if (!bLaunched && Elapsed >= LaunchSeconds)
	{
		// o jogador entra na estrada já na velocidade do cenário (o que se mede é a batida, não a arrancada)
		bLaunched = true;
		Vehicle.GetMesh()->SetPhysicsLinearVelocity(Vehicle.GetActorForwardVector() * (TargetKmh / 0.036f));
	}
	// segura a velocidade (controle proporcional, como o cenário Wall do carro) e zigue-zague leve para cruzar as faixas
	// depois do primeiro impacto o jogador larga o acelerador (senão a rodada nunca silenciaria: só o limite de 30 s a encerraria)
	const bool bCrashed = FirstImpactAt >= 0.0f;
	const float Throttle = (bLaunched && !bCrashed) ? FMath::Clamp(0.4f + (TargetKmh - SpeedKmh) * 0.08f, 0.0f, 1.0f) : 0.0f;
	const float Weave = (bLaunched && !bCrashed) ? 0.18f * FMath::Sin((Elapsed - LaunchSeconds) * 1.1f) : 0.0f;
	if (!bEnded)
	{
		Move->SetThrottleInput(Throttle);
		Move->SetSteeringInput(Weave);
	}
	else
	{
		Move->SetThrottleInput(0.0f);
		Move->SetSteeringInput(0.0f);
	}

	const bool bTimedOut = Elapsed >= CrashScenarioTimeoutSeconds;
	if ((bEnded && Elapsed >= EndedAt + ExitDelaySeconds) || bTimedOut)
	{
		if (!bEnded)
		{
			UE_LOG(LogDetonauto, Display, TEXT("DNCrash: a rodada NAO terminou em %.0f s (primeiro impacto: %s)"), CrashScenarioTimeoutSeconds, FirstImpactAt >= 0.0f ? TEXT("sim") : TEXT("nao houve"));
			LogResult();
		}
		FPlatformMisc::RequestExit(false);
	}
}

void ADNCrashTestGameMode::DriveTrafficScenario(ADNVehicle& Vehicle, float DeltaSeconds)
{
	// o jogador fica parado ao lado da estrada (30 m do centro, fora do alcance de acordar): só o custo do tráfego conta
	if (!bLaunched)
	{
		bLaunched = true;
		Vehicle.SetActorLocation(FVector(3000.0f, -3000.0f, 120.0f), false, nullptr, ETeleportType::ResetPhysics);
	}
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle.GetVehicleMovement());
	Move->SetThrottleInput(0.0f);
	Move->SetHandbrakeInput(true);

	if (MeasureSeconds > 0.0f)
	{
		if (!bCsvStarted && Elapsed >= Warmup)
		{
			bCsvStarted = true;
			GEngine->Exec(GetWorld(), TEXT("CsvProfile Start"));
			const FDNTrafficStats& T = Traffic->GetStats();
			UE_LOG(LogDetonauto, Display, TEXT("DNCrash: medicao do trafego iniciada: ativos rail=%d physics=%d"), T.ActiveRail, T.ActivePhysics);
		}
		if (bCsvStarted && !bCsvStopped && Elapsed >= Warmup + MeasureSeconds)
		{
			bCsvStopped = true;
			GEngine->Exec(GetWorld(), TEXT("CsvProfile Stop"));
			const FDNTrafficStats& T = Traffic->GetStats();
			UE_LOG(LogDetonauto, Display, TEXT("DNCrash: TRAFEGO fim da medicao: nasceram=%d acordaram=%d despachados=%d ativos rail=%d physics=%d"), T.Spawned, T.Woken, T.Despawned, T.ActiveRail, T.ActivePhysics);
			EndedAt = Elapsed;
		}
		if (bCsvStopped && Elapsed >= EndedAt + 4.0f) // dá tempo ao CSV Profiler de gravar o arquivo
		{
			FPlatformMisc::RequestExit(false);
		}
	}
	else if (Elapsed >= Warmup)
	{
		const FDNTrafficStats& T = Traffic->GetStats();
		UE_LOG(LogDetonauto, Display, TEXT("DNCrash: TRAFEGO t=%.1f s: nasceram=%d acordaram=%d despachados=%d ativos rail=%d physics=%d"), Elapsed, T.Spawned, T.Woken, T.Despawned, T.ActiveRail, T.ActivePhysics);
		FPlatformMisc::RequestExit(false);
	}
}

void ADNCrashTestGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ADNVehicle* Vehicle = FindVehicle();
	if (!Vehicle || !Traffic)
	{
		return;
	}
	Elapsed += DeltaSeconds;
	if (!bPlayerLinked)
	{
		bPlayerLinked = true;
		Traffic->SetPlayer(Vehicle);
	}

	if (Scenario == EScenario::Crash)
	{
		if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovement()))
		{
			Move->SetRequiresControllerForInputs(false);
		}
		DriveCrashScenario(*Vehicle, DeltaSeconds);
	}
	else if (Scenario == EScenario::Traffic)
	{
		if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovement()))
		{
			Move->SetRequiresControllerForInputs(false);
		}
		DriveTrafficScenario(*Vehicle, DeltaSeconds);
	}

	// fim da rodada pelas regras do Core: a velocidade que vale é a dos veículos que estão em jogo (Physics): o jogador e o tráfego
	// que já acordou. Carros em trilha andam sempre e nunca deixariam a rodada "silenciar".
	if (!bEnded && Scenario != EScenario::Traffic)
	{
		CrashRound->Step(DeltaSeconds, FMath::Max(PlayerSpeedMs(*Vehicle), Traffic->MaxPhysicsSpeedMs()));
	}
	UpdateHud(*Vehicle);
}
