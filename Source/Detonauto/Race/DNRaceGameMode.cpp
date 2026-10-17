#include "Race/DNRaceGameMode.h"
#include "Race/DNCourseActor.h"
#include "Vehicle/DNVehicle.h"
#include "Vehicle/DNVehicleArena.h"
#include "Core/DNCourse.h"
#include "Core/DNRivalDriver.h"
#include "Core/DNTakedown.h"
#include "Core/DNDamage.h"
#include "Vehicle/DNTurboComponent.h"
#include "Core/DNGameDataSubsystem.h"
#include "Detonauto.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Engine/Engine.h"
#include "Core/DNRaceFormat.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
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
	HUDClass = ADNRaceHUD::StaticClass();
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
	FParse::Value(Cmd, TEXT("DNRivals="), RivalLimit);
	FParse::Value(Cmd, TEXT("DNRaceCountdownS="), CountdownS);
	bAuto = FParse::Param(Cmd, TEXT("DNRaceAuto"));
	bHunt = FParse::Param(Cmd, TEXT("DNRaceHunt"));
	float AutoKmh = 0.0f;
	if (FParse::Value(Cmd, TEXT("DNRaceAutoKmh="), AutoKmh) && AutoKmh > AutoDriver.CornerKmh)
	{
		AutoDriver.StraightKmh = AutoKmh; // velocidade do piloto automático na reta (capturas a uma velocidade escolhida)
	}
	if (bHunt)
	{
		AutoDriver.StraightKmh = 200.0f;
		AutoDriver.CornerKmh = 80.0f;
	}

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
		// o jogador sai da última vaga da grade, atrás dos rivais
		const UDNGameDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDNGameDataSubsystem>() : nullptr;
		const int32 RivalCount = Data ? FMath::Min(Data->GetRivals().Num(), FMath::Max(RivalLimit, 0)) : 0;
		const FTransform T = Course->GridTransform(RivalCount, 120.0f);
		Start = GetWorld()->SpawnActor<APlayerStart>(T.GetLocation(), T.Rotator(), Params);
	}
	return Start;
}

void ADNRaceGameMode::StartPlay()
{
	Super::StartPlay();
	RaceTime = 0.0f;
	SinceStartS = 0.0f;
	if (!Course)
	{
		return;
	}
	const UDNGameDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDNGameDataSubsystem>() : nullptr;
	if (!Data)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const TArray<TPair<FString, FDNRivalConfig>> Defs = Data->GetRivals();
	for (int32 i = 0; i < Defs.Num() && i < RivalLimit; ++i)
	{
		const FTransform T = Course->GridTransform(i, 120.0f);
		FRival R;
		R.Car = GetWorld()->SpawnActor<ADNVehicle>(ADNVehicle::StaticClass(), T.GetLocation(), T.Rotator(), Params);
		R.Name = Defs[i].Key;
		R.Config = Defs[i].Value;
		if (R.Car.IsValid())
		{
			// sem controlador o Chaos ignora as entradas por padrão; quem dirige o rival é este GameMode
			if (UChaosVehicleMovementComponent* Move = R.Car->GetVehicleMovementComponent())
			{
				Move->SetRequiresControllerForInputs(false);
			}
			Rivals.Add(R);
		}
	}
	UE_LOG(LogDetonauto, Display, TEXT("DNRace: %d rival(is) na grade; largada em %.0f s."), Rivals.Num(), CountdownS);
}

void ADNRaceGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ADNVehicle* Vehicle = FindVehicle();
	if (!Course || !Vehicle || Course->GetPoints().Num() < 2)
	{
		return;
	}

	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->WasInputKeyJustPressed(EKeys::R))
		{
			ResetRace();
		}
	}
	SinceStartS += DeltaSeconds;
	if (SinceStartS < CountdownS)
	{
		if (bAuto)
		{
			if (UChaosWheeledVehicleMovementComponent* Hold = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovement()))
			{
				Hold->SetBrakeInput(1.0f);
			}
		}
		return;
	}
	RaceTime += DeltaSeconds;
	if (!bPlayerHitBound)
	{
		bPlayerHitBound = true;
		Vehicle->GetMesh()->OnComponentHit.AddUniqueDynamic(this, &ADNRaceGameMode::OnPlayerHit);
		if (const UDNGameDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDNGameDataSubsystem>() : nullptr)
		{
			TakedownConfig = Data->GetTakedownConfig();
		}
	}
	const FVector Pos = Vehicle->GetActorLocation();
	const float Progress = FDNCourse::Progress(Course->GetPoints(), Pos, Player.ProgressCm, WindowBackCm, WindowAheadCm);
	const bool bJustFinished = FDNRace::Step(Player, Progress, RaceTime, Course->GetLengthCm());
	if (Player.ProgressCm >= LastAdvanceCm + 100.0f)
	{
		LastAdvanceCm = Player.ProgressCm;
		LastAdvanceAtS = RaceTime;
	}

	// jogador destruído: naufrágio por PlayerWreckSeconds e renasce na linha, inteiro, com o turbo zerado; a corrida não acaba por isso
	{
		const bool bPlayerDestroyed = Vehicle->GetDamage().bDestroyed;
		const double NowS = GetWorld()->GetTimeSeconds();
		if (bPlayerDestroyed && PlayerDestroyedAtS < 0.0)
		{
			PlayerDestroyedAtS = NowS;
		}
		if (FDNRaceTakedown::Step(PlayerWreck, DeltaSeconds, bPlayerDestroyed, PlayerWreckSeconds))
		{
			const TArray<FVector>& Pts = Course->GetPoints();
			const FVector Back = FDNCourse::LocationAt(Pts, Player.ProgressCm);
			const FVector Dir = (FDNCourse::LocationAt(Pts, Player.ProgressCm + 100.0f) - Back).GetSafeNormal2D();
			Vehicle->RestartAt(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.0f), FVector(Back.X, Back.Y, 120.0f)));
			if (UDNTurboComponent* Turbo = Vehicle->GetTurbo())
			{
				Turbo->ResetState();
			}
			PlayerDestroyedAtS = -1.0;
			LastAdvanceAtS = RaceTime;
			UE_LOG(LogDetonauto, Display, TEXT("DNRace: o jogador renasce na linha aos %.1f s no ponto %.0f m, inteiro e com o turbo zerado."), RaceTime, Player.ProgressCm / 100.0f);
		}
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
			if (!PlayerWreck.bWrecked)
			{
				DriveAuto(*Vehicle);
			}
		}
		else if (!bFinishedLogged)
		{
			UE_LOG(LogDetonauto, Display, TEXT("DNRace: %s aos %.0f s no progresso %.0f de %.0f m; máxima %.0f km/h; maior distância da linha %.1f m."), RaceTime <= MaxSeconds ? TEXT("PRESO") : TEXT("TEMPO ESGOTADO"), RaceTime, Player.ProgressCm / 100.0f, Course->GetLengthCm() / 100.0f, MaxSpeedKmh, MaxOffsetCm / 100.0f);
			LogStandings(TEXT("corrida interrompida"));
			FPlatformMisc::RequestExit(false);
		}
		bool bAllFinished = bFinishedLogged;
		for (const FRival& R : Rivals)
		{
			bAllFinished &= R.State.bFinished;
		}
		if (bFinishedLogged && RaceTime - FinishedAt > 4.0f && (bAllFinished || RaceTime - FinishedAt > 12.0f))
		{
			LogStandings(bAllFinished ? TEXT("todos chegaram") : TEXT("12 s depois do jogador"));
			FPlatformMisc::RequestExit(false);
		}
	}
	TickRivals(DeltaSeconds);
	TickTakedowns(*Vehicle);
}

void ADNRaceGameMode::TickRivals(float DeltaSeconds)
{
	const TArray<FVector>& Points = Course->GetPoints();
	for (FRival& R : Rivals)
	{
		ADNVehicle* Car = R.Car.Get();
		if (!Car)
		{
			continue;
		}
		// destroçado (dano passou do limite, D12): fica parado por WreckSeconds e volta à pista, inteiro, no ponto da linha onde estava
		if (FDNRaceTakedown::Step(R.Wreck, DeltaSeconds, Car->GetDamage().bDestroyed || R.bWreckNow, WreckSeconds))
		{
			const FVector Back = FDNCourse::LocationAt(Points, R.State.ProgressCm);
			const FVector Dir = (FDNCourse::LocationAt(Points, R.State.ProgressCm + 100.0f) - Back).GetSafeNormal2D();
			Car->RestartAt(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.0f), FVector(Back.X, Back.Y, 120.0f)));
			UE_LOG(LogDetonauto, Display, TEXT("DNRace: rival %s volta à pista aos %.1f s no ponto %.0f m"), *R.Name, RaceTime, R.State.ProgressCm / 100.0f);
		}
		R.bWreckNow = false;
		if (R.Wreck.bWrecked)
		{
			if (UChaosWheeledVehicleMovementComponent* Hold = Cast<UChaosWheeledVehicleMovementComponent>(Car->GetVehicleMovement()))
			{
				Hold->SetThrottleInput(0.0f);
				Hold->SetBrakeInput(0.3f);
				Hold->SetSteeringInput(0.0f);
			}
			continue;
		}
		const FVector Pos = Car->GetActorLocation();
		FDNRace::Step(R.State, FDNCourse::Progress(Points, Pos, R.State.ProgressCm, WindowBackCm, WindowAheadCm), RaceTime, Course->GetLengthCm());
		UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Car->GetVehicleMovement());
		if (!Move)
		{
			continue;
		}
		if (R.State.bFinished)
		{
			Move->SetThrottleInput(0.0f);
			Move->SetBrakeInput(0.5f);
			continue;
		}
		const float SpeedKmh = Car->SpeedKmh();
		const float LookAheadCm = FDNRivalDriver::LookAheadCm(SpeedKmh, R.Config);
		const FVector Forward = Car->GetActorForwardVector();
		float Near = FDNCourse::HeadingErrorDeg(Pos, Forward, FDNCourse::LocationAt(Points, R.State.ProgressCm + LookAheadCm));
		float Far = FDNCourse::HeadingErrorDeg(Pos, Forward, FDNCourse::LocationAt(Points, R.State.ProgressCm + 2.0f * LookAheadCm));
		// W6: tocado, o rival revida por AngerSeconds mirando no jogador; se o jogador está atrás dele (mais de 100 graus), segue a linha
		R.AngerLeftS = FDNRaceTakedown::AngerStep(R.AngerLeftS, DeltaSeconds, R.bTouched, AngerSeconds);
		R.bTouched = false;
		if (R.AngerLeftS > 0.0f)
		{
			if (const ADNVehicle* Target = FindVehicle())
			{
				const float ToPlayer = FDNCourse::HeadingErrorDeg(Pos, Forward, Target->GetActorLocation());
				if (FMath::Abs(ToPlayer) <= 100.0f)
				{
					Near = Far = ToPlayer;
				}
			}
		}
		const FDNDriveCommand Cmd = FDNRivalDriver::Command(Near, Far, SpeedKmh, R.Config);
		Move->SetSteeringInput(FMath::Clamp(Cmd.Steer * Car->AppliedSteeringGain, -1.0f, 1.0f));
		Move->SetThrottleInput(Cmd.Throttle);
		Move->SetBrakeInput(Cmd.Brake);
		if (bAuto && FMath::FloorToInt(RaceTime / 5.0f) != FMath::FloorToInt((RaceTime - DeltaSeconds) / 5.0f))
		{
			UE_LOG(LogDetonauto, Display, TEXT("DNRace: rival %s t=%.0f s progresso %.0f m, %.0f km/h, acel %.2f freio %.2f esterco %.2f, pos (%.0f, %.0f, %.0f)"), *R.Name, RaceTime, R.State.ProgressCm / 100.0f, SpeedKmh, Cmd.Throttle, Cmd.Brake, Cmd.Steer, Pos.X, Pos.Y, Pos.Z);
		}
	}
}

void ADNRaceGameMode::LogStandings(const TCHAR* Why) const
{
	TArray<FDNRacerState> All;
	TArray<FString> Names;
	All.Add(Player);
	Names.Add(TEXT("Jogador"));
	for (const FRival& R : Rivals)
	{
		All.Add(R.State);
		Names.Add(R.Name);
	}
	const TArray<int32> Order = FDNRace::Order(All);
	FString Line;
	for (int32 Place = 0; Place < Order.Num(); ++Place)
	{
		const FDNRacerState& S = All[Order[Place]];
		Line += FString::Printf(TEXT(" %d) %s %s;"), Place + 1, *Names[Order[Place]], S.bFinished ? *FString::Printf(TEXT("%.1f s"), S.FinishTimeS) : *FString::Printf(TEXT("%.0f m"), S.ProgressCm / 100.0f));
	}
	UE_LOG(LogDetonauto, Display, TEXT("DNRace: CLASSIFICACAO (%s):%s"), Why, *Line);
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
	FVector NearTarget = FDNCourse::LocationAt(Points, Player.ProgressCm + LookAheadCm);
	FVector FarTarget = FDNCourse::LocationAt(Points, Player.ProgressCm + 2.0f * LookAheadCm);
	if (bHunt)
	{
		// só para provar o takedown: mira no rival mais perto, à frente e a menos de 80 m, em vez de seguir a linha
		double Best = 8000.0;
		for (const FRival& R : Rivals)
		{
			const ADNVehicle* Car = R.Car.Get();
			if (!Car || R.Wreck.bWrecked || R.State.ProgressCm <= Player.ProgressCm)
			{
				continue;
			}
			const double D = FVector::Dist2D(Car->GetActorLocation(), Pos);
			if (D < Best)
			{
				Best = D;
				NearTarget = FarTarget = Car->GetActorLocation();
			}
		}
	}
	const float Near = FDNCourse::HeadingErrorDeg(Pos, Forward, NearTarget);
	const float Far = FDNCourse::HeadingErrorDeg(Pos, Forward, FarTarget);
	const FDNDriveCommand Cmd = FDNRivalDriver::Command(Near, Far, SpeedKmh, AutoDriver);
	Move->SetSteeringInput(FMath::Clamp(Cmd.Steer * Vehicle.AppliedSteeringGain, -1.0f, 1.0f));
	Move->SetThrottleInput(Cmd.Throttle);
	Move->SetBrakeInput(Cmd.Brake);
}

void ADNRaceGameMode::OnPlayerHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const ADNVehicle* PlayerCar = FindVehicle();
	if (!PlayerCar || !OtherActor)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < Rivals.Num(); ++i)
	{
		if (Rivals[i].Car.Get() != OtherActor || Rivals[i].Wreck.bWrecked)
		{
			continue;
		}
		Rivals[i].bTouched = true; // W6: o rival tocado revida
		// um toque só abre uma janela; os contatos seguintes da mesma batida (quadros seguidos) não abrem outra
		for (const FPendingContact& P : Pending)
		{
			if (P.RivalIndex == i && Now - P.Time < TakedownConfig.RivalCrashWindowSeconds)
			{
				return;
			}
		}
		const FVector RivalVelocity = PrevRivalVelocity.IsValidIndex(i) ? PrevRivalVelocity[i] : OtherActor->GetVelocity();
		FPendingContact C;
		C.RivalIndex = i;
		C.Time = Now;
		C.RelativeSpeedKmh = static_cast<float>((PrevPlayerVelocity - RivalVelocity).Size()) * 0.036f;
		C.RivalDamageBefore = FDNDamage::TotalDamage(Rivals[i].Car->GetDamage());
		C.Classify.PlayerForward = PlayerCar->GetActorForwardVector();
		C.Classify.RivalForward = OtherActor->GetActorForwardVector();
		C.Classify.ContactNormal = (OtherActor->GetActorLocation() - PlayerCar->GetActorLocation()).GetSafeNormal2D();
		C.Classify.PlayerHeightAboveRivalCm = static_cast<float>(PlayerCar->GetActorLocation().Z - OtherActor->GetActorLocation().Z);
		C.Classify.bPlayerFalling = PlayerCar->GetVelocity().Z < -50.0f;
		Pending.Add(C);
		UE_LOG(LogDetonauto, Display, TEXT("DNRace: toque no rival %s a %.0f km/h de velocidade relativa"), *Rivals[i].Name, C.RelativeSpeedKmh);
		return;
	}
}

void ADNRaceGameMode::TickTakedowns(ADNVehicle& Vehicle)
{
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 k = Pending.Num() - 1; k >= 0; --k)
	{
		const FPendingContact& C = Pending[k];
		const ADNVehicle* Rival = Rivals.IsValidIndex(C.RivalIndex) ? Rivals[C.RivalIndex].Car.Get() : nullptr;
		const float Since = static_cast<float>(Now - C.Time);
		if (Rival && FDNRaceTakedown::RivalCrashed(C.RivalDamageBefore, FDNDamage::TotalDamage(Rival->GetDamage()), Rival->GetDamage().bDestroyed, MinCrashDamageGain))
		{
			FDNTakedownContact Contact;
			Contact.RelativeSpeedKmh = C.RelativeSpeedKmh;
			Contact.bRivalCrashed = true;
			Contact.RivalCrashSecondsAfterContact = Since;
			// W5: o jogador só desqualifica se se destruiu antes de o rival ser batido; empate vale o detona
			const float PlayerDestroyedSince = PlayerDestroyedAtS >= 0.0 ? static_cast<float>(PlayerDestroyedAtS - C.Time) : -1.0f;
			Contact.bPlayerDestroyed = FDNRaceTakedown::PlayerDestroyedFirst(PlayerDestroyedSince, Since);
			Contact.PlayerDestroyedSecondsAfterContact = PlayerDestroyedSince;
			if (FDNTakedown::Qualifies(TakedownConfig, Contact))
			{
				const EDNTakedownType Type = FDNTakedown::Classify(TakedownConfig, C.Classify);
				++Takedowns;
				LastTakedownAt = Now;
				Rivals[C.RivalIndex].bWreckNow = true;
				LastTakedownText = FString::Printf(TEXT("DETONOU! %s (%s)"), *Rivals[C.RivalIndex].Name, Type == EDNTakedownType::FromBehind ? TEXT("por trás") : FDNTakedown::ToString(Type));
				if (UDNTurboComponent* Turbo = Vehicle.GetTurbo())
				{
					FDNTurboFrameInput Fill;
					Fill.bTakedown = true;
					Turbo->Step(Fill);
				}
				UE_LOG(LogDetonauto, Display, TEXT("DNRace: TAKEDOWN %d: %s, tipo %s, %.0f km/h relativos, %.2f s depois do toque; turbo %.0f"), Takedowns, *Rivals[C.RivalIndex].Name, FDNTakedown::ToString(Type), C.RelativeSpeedKmh, Since, Vehicle.GetTurbo() ? Vehicle.GetTurbo()->GetBar() : 0.0f);
				Pending.RemoveAt(k);
				continue;
			}
			UE_LOG(LogDetonauto, Display, TEXT("DNRace: rival %s bateu mas o takedown não vale (relativa %.0f km/h, mínimo %.0f; jogador destruído: %d)"), *Rivals[C.RivalIndex].Name, C.RelativeSpeedKmh, TakedownConfig.MinRelativeSpeedKmh, Vehicle.GetDamage().bDestroyed ? 1 : 0);
			Pending.RemoveAt(k);
			continue;
		}
		if (Since > TakedownConfig.RivalCrashWindowSeconds)
		{
			Pending.RemoveAt(k);
		}
	}
	PrevPlayerVelocity = Vehicle.GetVelocity();
	PrevRivalVelocity.SetNum(Rivals.Num());
	for (int32 i = 0; i < Rivals.Num(); ++i)
	{
		PrevRivalVelocity[i] = Rivals[i].Car.IsValid() ? Rivals[i].Car->GetVelocity() : FVector::ZeroVector;
	}
}

void ADNRaceGameMode::ResetRace()
{
	Player = FDNRacerState();
	RaceTime = 0.0f;
	SinceStartS = 0.0f;
	NextMark = 0;
	LastCheckpointAtS = 0.0f;
	LastAdvanceAtS = 0.0f;
	LastAdvanceCm = 0.0f;
	bFinishedLogged = false;
	FinishedAt = -1.0f;
	Takedowns = 0;
	LastTakedownAt = -100.0;
	PlayerWreck = FDNWreckState();
	PlayerDestroyedAtS = -1.0;
	Pending.Reset();
	for (int32 i = 0; i < Rivals.Num(); ++i)
	{
		FRival& R = Rivals[i];
		R.State = FDNRacerState();
		R.Wreck = FDNWreckState();
		R.bWreckNow = false;
		R.bTouched = false;
		R.AngerLeftS = 0.0f;
		if (ADNVehicle* Car = R.Car.Get())
		{
			Car->RestartAt(Course->GridTransform(i, 120.0f));
		}
	}
	UE_LOG(LogDetonauto, Display, TEXT("DNRace: corrida reiniciada (tecla R)."));
}

FDNRaceHudData ADNRaceGameMode::BuildHudData() const
{
	FDNRaceHudData D;
	const ADNVehicle* Vehicle = FindVehicle();
	if (!Course || !Vehicle)
	{
		return D;
	}
	const float Length = FMath::Max(Course->GetLengthCm(), 1.0f);
	TArray<FDNRacerState> All;
	TArray<FString> Names;
	All.Add(Player);
	Names.Add(TEXT("Você"));
	for (const FRival& R : Rivals)
	{
		All.Add(R.State);
		Names.Add(R.Name);
		D.RivalProgress.Add(R.State.ProgressCm / Length);
	}
	D.Position = FDNRace::Position(All, 0);
	D.Total = All.Num();
	D.TimeS = bFinishedLogged ? FinishedAt : RaceTime;
	D.SpeedKmh = Vehicle->SpeedKmh();
	if (const UDNTurboComponent* Turbo = Vehicle->GetTurbo())
	{
		D.TurboBar = Turbo->GetBar();
		D.TurboMax = Turbo->GetMaxBar();
	}
	D.Takedowns = Takedowns;
	D.TakedownAgeS = static_cast<float>(GetWorld()->GetTimeSeconds() - LastTakedownAt);
	D.TakedownText = LastTakedownText;
	D.PlayerProgress = Player.ProgressCm / Length;
	D.CountdownLeftS = FMath::Max(CountdownS - SinceStartS, 0.0f);
	D.bFinished = bFinishedLogged;
	D.bPlayerWrecked = PlayerWreck.bWrecked && !bFinishedLogged;
	D.PlayerWreckLeftS = PlayerWreck.LeftS;
	if (D.bFinished)
	{
		const TArray<int32> Order = FDNRace::Order(All);
		for (int32 Place = 0; Place < Order.Num(); ++Place)
		{
			const FDNRacerState& S = All[Order[Place]];
			FDNRaceHudRow Row;
			Row.Place = FDNRaceFormat::Place(Place + 1);
			Row.Name = Names[Order[Place]];
			Row.Value = S.bFinished ? FDNRaceFormat::Time(S.FinishTimeS) : FString::Printf(TEXT("%.0f m"), S.ProgressCm / 100.0f);
			Row.bPlayer = Order[Place] == 0;
			D.Rows.Add(Row);
		}
	}
	return D;
}
