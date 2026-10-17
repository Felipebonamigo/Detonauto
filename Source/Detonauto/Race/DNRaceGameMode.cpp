#include "Race/DNRaceGameMode.h"
#include "Race/DNCourseActor.h"
#include "Vehicle/DNVehicle.h"
#include "Vehicle/DNVehicleArena.h"
#include "Core/DNCourse.h"
#include "Core/DNRivalDriver.h"
#include "Core/DNGameDataSubsystem.h"
#include "Detonauto.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformMisc.h"

namespace
{
	constexpr float WindowBackCm = 2000.0f;
	constexpr float WindowAheadCm = 6000.0f;
}

ADNRaceGameMode::ADNRaceGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADNRaceGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// o piso de 400 m da arena de teste não cobre o percurso: ele traz o próprio chão e a própria luz
	if (Arena)
	{
		Arena->Destroy();
		Arena = nullptr;
	}

	const TCHAR* Cmd = FCommandLine::Get();
	FParse::Value(Cmd, TEXT("DNCourse="), CourseId);
	FParse::Value(Cmd, TEXT("DNCheckpointM="), CheckpointM);
	FParse::Value(Cmd, TEXT("DNRaceMaxS="), MaxSeconds);
	bAuto = FParse::Param(Cmd, TEXT("DNRaceAuto"));

	UGameInstance* GameInstance = GetGameInstance();
	const UDNGameDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UDNGameDataSubsystem>() : nullptr;
	TArray<FVector> Points;
	float WidthCm = 0.0f;
	if (!Data || !Data->GetCourse(CourseId, Points, WidthCm))
	{
		UE_LOG(LogDetonauto, Error, TEXT("DNRace: o percurso '%s' não existe ou Data/CoursePoints.csv é inválido; sem pista."), *CourseId);
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Course = GetWorld()->SpawnActor<ADNCourseActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	Course->Build(Points, WidthCm, CheckpointM * 100.0f);
	UE_LOG(LogDetonauto, Display, TEXT("DNRace: percurso '%s' com %d pontos, %.0f m, %.0f m de largura, %d marcas."), *CourseId, Points.Num(), Course->GetLengthCm() / 100.0f, WidthCm / 100.0f, Course->GetMarks().Num());
}

AActor* ADNRaceGameMode::ChoosePlayerStart_Implementation(AController* PlayerController)
{
	if (!Course)
	{
		return Super::ChoosePlayerStart_Implementation(PlayerController);
	}
	if (!Start)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FTransform T = Course->StartTransform(120.0f);
		Start = GetWorld()->SpawnActor<APlayerStart>(T.GetLocation(), T.Rotator(), Params);
	}
	return Start;
}

void ADNRaceGameMode::StartPlay()
{
	Super::StartPlay();
	RaceTime = 0.0f;
}

void ADNRaceGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ADNVehicle* Vehicle = FindVehicle();
	if (!Course || !Vehicle || Course->GetPoints().Num() < 2)
	{
		return;
	}

	RaceTime += DeltaSeconds;
	const FVector Pos = Vehicle->GetActorLocation();
	const float Progress = FDNCourse::Progress(Course->GetPoints(), Pos, Player.ProgressCm, WindowBackCm, WindowAheadCm);
	const bool bJustFinished = FDNRace::Step(Player, Progress, RaceTime, Course->GetLengthCm());
	if (Player.ProgressCm >= LastAdvanceCm + 100.0f)
	{
		LastAdvanceCm = Player.ProgressCm;
		LastAdvanceAtS = RaceTime;
	}

	// quanto o carro está fora da linha central
	MaxOffsetCm = FMath::Max(MaxOffsetCm, static_cast<float>(FVector::Dist2D(Pos, FDNCourse::LocationAt(Course->GetPoints(), Player.ProgressCm))));
	MaxSpeedKmh = FMath::Max(MaxSpeedKmh, Vehicle->SpeedKmh());

	const TArray<float>& Marks = Course->GetMarks();
	while (Marks.IsValidIndex(NextMark) && Player.ProgressCm >= Marks[NextMark] && NextMark < Marks.Num() - 1)
	{
		UE_LOG(LogDetonauto, Display, TEXT("DNRace: ponto de verificação %d de %d aos %.1f s (parcial %.1f s), %.0f km/h"), NextMark + 1, Marks.Num() - 1, RaceTime, RaceTime - LastCheckpointAtS, Vehicle->SpeedKmh());
		LastCheckpointAtS = RaceTime;
		++NextMark;
	}
	if (bJustFinished && !bFinishedLogged)
	{
		bFinishedLogged = true;
		FinishedAt = RaceTime;
		UE_LOG(LogDetonauto, Display, TEXT("DNRace: CHEGOU em %.1f s; máxima %.0f km/h; maior distância da linha central %.1f m."), RaceTime, MaxSpeedKmh, MaxOffsetCm / 100.0f);
	}

	if (bAuto && FMath::FloorToInt(RaceTime / 5.0f) != FMath::FloorToInt((RaceTime - DeltaSeconds) / 5.0f))
	{
		UE_LOG(LogDetonauto, Display, TEXT("DNRace: t=%.0f s progresso %.0f m, %.0f km/h, a %.1f m da linha, pos (%.0f, %.0f, %.0f)"), RaceTime, Player.ProgressCm / 100.0f, Vehicle->SpeedKmh(), FVector::Dist2D(Pos, FDNCourse::LocationAt(Course->GetPoints(), Player.ProgressCm)) / 100.0f, Pos.X, Pos.Y, Pos.Z);
	}
	if (bAuto)
	{
		if (!bFinishedLogged && RaceTime <= MaxSeconds && RaceTime - LastAdvanceAtS <= 20.0f)
		{
			DriveAuto(*Vehicle);
		}
		else if (!bFinishedLogged)
		{
			UE_LOG(LogDetonauto, Display, TEXT("DNRace: %s aos %.0f s no progresso %.0f de %.0f m; máxima %.0f km/h; maior distância da linha %.1f m."), RaceTime <= MaxSeconds ? TEXT("PRESO") : TEXT("TEMPO ESGOTADO"), RaceTime, Player.ProgressCm / 100.0f, Course->GetLengthCm() / 100.0f, MaxSpeedKmh, MaxOffsetCm / 100.0f);
			FPlatformMisc::RequestExit(false);
		}
		if (bFinishedLogged && RaceTime - FinishedAt > 3.0f)
		{
			FPlatformMisc::RequestExit(false);
		}
	}
	UpdateHud(*Vehicle);
}

void ADNRaceGameMode::DriveAuto(ADNVehicle& Vehicle) const
{
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle.GetVehicleMovement());
	if (!Move)
	{
		return;
	}
	const TArray<FVector>& Points = Course->GetPoints();
	const float SpeedKmh = Vehicle.SpeedKmh();
	const float LookAheadCm = FDNRivalDriver::LookAheadCm(SpeedKmh, AutoDriver);
	const FVector Pos = Vehicle.GetActorLocation();
	const FVector Forward = Vehicle.GetActorForwardVector();
	const float Near = FDNCourse::HeadingErrorDeg(Pos, Forward, FDNCourse::LocationAt(Points, Player.ProgressCm + LookAheadCm));
	const float Far = FDNCourse::HeadingErrorDeg(Pos, Forward, FDNCourse::LocationAt(Points, Player.ProgressCm + 2.0f * LookAheadCm));
	const FDNDriveCommand Cmd = FDNRivalDriver::Command(Near, Far, SpeedKmh, AutoDriver);
	Move->SetSteeringInput(FMath::Clamp(Cmd.Steer * Vehicle.AppliedSteeringGain, -1.0f, 1.0f));
	Move->SetThrottleInput(Cmd.Throttle);
	Move->SetBrakeInput(Cmd.Brake);
}

void ADNRaceGameMode::UpdateHud(const ADNVehicle& Vehicle) const
{
	if (!GEngine)
	{
		return;
	}
	const float Scale = 1.6f;
	const float Length = Course->GetLengthCm();
	GEngine->AddOnScreenDebugMessage(9301, 0.0f, bFinishedLogged ? FColor::Green : FColor::White,
		FString::Printf(TEXT("%s     Tempo: %.1f s"), bFinishedLogged ? TEXT("CHEGADA") : TEXT("CORRIDA"), bFinishedLogged ? FinishedAt : RaceTime), false, FVector2D(Scale));
	GEngine->AddOnScreenDebugMessage(9302, 0.0f, FColor::Yellow,
		FString::Printf(TEXT("Percurso: %.0f de %.0f m (%.0f%%)     Ponto %d/%d"), Player.ProgressCm / 100.0f, Length / 100.0f, Length > 0.0f ? 100.0f * Player.ProgressCm / Length : 0.0f, NextMark, Course->GetMarks().Num() - 1), false, FVector2D(Scale));
}
