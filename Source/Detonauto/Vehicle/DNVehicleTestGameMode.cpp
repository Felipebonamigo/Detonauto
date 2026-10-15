#include "Vehicle/DNVehicleTestGameMode.h"
#include "Vehicle/DNVehicle.h"
#include "Vehicle/DNVehicleArena.h"
#include "Crash/DNBreakableProp.h"
#include "Crash/DNBreakableSubsystem.h"
#include "Vehicle/DNTurboComponent.h"
#include "Detonauto.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformMisc.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

ADNVehicleTestGameMode::ADNVehicleTestGameMode()
{
	DefaultPawnClass = ADNVehicle::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void ADNVehicleTestGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// mapa de verdade (cena de aparência): o PlayerStart dele tem a tag DNMapStart. O /Engine/Maps/Entry também tem um PlayerStart
	// (sem a tag), por isso a tag e não só "existe PlayerStart"
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(MapStartTag))
		{
			MapStart = *It;
			break;
		}
	}
	if (MapStart)
	{
		UE_LOG(LogDetonauto, Display, TEXT("ADNVehicleTestGameMode: o mapa '%s' tem PlayerStart com a tag %s; sem arena de teste"), *MapName, *MapStartTag.ToString());
		return;
	}

	// piso, parede e luz: o mapa é vazio
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Arena = GetWorld()->SpawnActor<ADNTestArena>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
}

AActor* ADNVehicleTestGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (MapStart)
	{
		return MapStart;
	}
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
	// roteiro de teclas: -DNKeys=W@0.5-9;T@3;SHOT@4;CAM90@5;QUIT@8   (tecla@início[-fim]; separado por ponto e vírgula)
	FString KeyScript;
	if (FParse::Value(Cmd, TEXT("DNKeys="), KeyScript))
	{
		TArray<FString> Items;
		KeyScript.ParseIntoArray(Items, TEXT(";"));
		for (const FString& Item : Items)
		{
			FString What, Times;
			if (!Item.Split(TEXT("@"), &What, &Times))
			{
				continue;
			}
			FScriptEvent Event;
			Event.What = What;
			FString StartText, EndText;
			if (Times.Split(TEXT("-"), &StartText, &EndText))
			{
				Event.Start = FCString::Atof(*StartText);
				Event.End = FCString::Atof(*EndText);
			}
			else
			{
				Event.Start = FCString::Atof(*Times);
			}
			ScriptEvents.Add(Event);
		}
	}
	int32 ParkedCars = 0;
	if (FParse::Value(Cmd, TEXT("DNParkedCars="), ParkedCars) && ParkedCars > 0)
	{
		SpawnParkedCars(FMath::Min(ParkedCars, 12));
	}
	float WallAheadM = 0.0f;
	if (FParse::Value(Cmd, TEXT("DNWallAheadM="), WallAheadM) && WallAheadM > 0.0f)
	{
		SpawnWallAhead(WallAheadM);
	}
	// lote 17c: peças de rua na faixa do carro, piloto automático de velocidade e capturas de cada quebra
	FString PropLine;
	if (FParse::Value(Cmd, TEXT("DNPropLine="), PropLine, false) && !PropLine.IsEmpty())
	{
		float StartM = 60.0f;
		float SpacingM = 45.0f;
		FParse::Value(Cmd, TEXT("DNPropStartM="), StartM);
		FParse::Value(Cmd, TEXT("DNPropSpacingM="), SpacingM);
		SpawnPropLine(PropLine, StartM, SpacingM);
	}
	FString PropCamSpec;
	if (FParse::Value(Cmd, TEXT("DNPropCam="), PropCamSpec, false) && !PropCamSpec.IsEmpty())
	{
		SpawnPropCam(PropCamSpec);
	}
	FString Delays;
	if (FParse::Value(Cmd, TEXT("DNPropShotDelays="), Delays, false))
	{
		TArray<FString> Items;
		Delays.ParseIntoArray(Items, TEXT(","));
		for (const FString& Item : Items)
		{
			const float Seconds = FCString::Atof(*Item);
			if (Seconds > 0.0f)
			{
				PropShotDelays.Add(Seconds);
			}
		}
	}
	if (FParse::Value(Cmd, TEXT("DNCruiseKmh="), CruiseKmh) && CruiseKmh > 0.0f)
	{
		UE_LOG(LogDetonauto, Display, TEXT("DNCruise: piloto automático segurando %.0f km/h (o acelerador vem dele, não de tecla)"), CruiseKmh);
	}
	bPropShots = FParse::Param(Cmd, TEXT("DNPropShots"));
	if (bPropShots)
	{
		if (UDNBreakableSubsystem* Props = GetWorld()->GetSubsystem<UDNBreakableSubsystem>())
		{
			Props->OnBroken.AddUObject(this, &ADNVehicleTestGameMode::HandlePropBroken);
		}
	}
	bProbe = FParse::Param(Cmd, TEXT("DNCarProbe"));
	if (bProbe)
	{
		Scenario = TEXT("Sin");
		FParse::Value(Cmd, TEXT("DNCarScenario="), Scenario);
		FParse::Value(Cmd, TEXT("DNWallKmh="), WallTargetKmh);
		FParse::Value(Cmd, TEXT("DNSteer="), SteerInput);
		bBoostTurbo = FParse::Param(Cmd, TEXT("DNTurboOn"));
		HandbrakeSteer = FParse::Value(Cmd, TEXT("DNSteer="), HandbrakeSteer) ? HandbrakeSteer : 0.5f;
		FParse::Value(Cmd, TEXT("DNHbKmh="), HandbrakeEntryKmh);
		FParse::Value(Cmd, TEXT("DNThrottle="), HandbrakeThrottle);
		ScenarioSeconds = (Scenario == TEXT("Handbrake") || Scenario == TEXT("Turn") || Scenario == TEXT("Switch")) ? 9.0f
			: (Scenario == TEXT("Turbo") || Scenario == TEXT("Accel")) ? 3.6f : (Scenario == TEXT("Wall") ? 7.0f : (Scenario == TEXT("Steer") ? 16.0f : (Scenario == TEXT("Boost") ? 16.0f : 10.0f)));
		UE_LOG(LogDetonauto, Display, TEXT("DNCarProbe: cenário '%s' de %.0f s iniciado"), *Scenario, ScenarioSeconds);
	}
}

void ADNVehicleTestGameMode::SpawnParkedCars(int32 Count)
{
	const AActor* Spot = MapStart ? static_cast<AActor*>(MapStart) : static_cast<AActor*>(Start);
	const FTransform Base = Spot ? Spot->GetActorTransform() : FTransform(FVector(0.0f, 0.0f, 120.0f));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	// -DNParkedDamage=0 estaciona intactos: a diferença para os amassados é o custo dos alvos de deformação
	float Damage = 1.0f;
	FParse::Value(FCommandLine::Get(), TEXT("DNParkedDamage="), Damage);
	int32 Spawned = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		// 9 m entre eles, à frente e 4 m à direita (a faixa ao lado na cena de aparência): a câmera de jogo os vê, e o carro do
		// jogador passa por eles no cenário "andando" sem bater
		const FVector Location = Base.TransformPosition(FVector(900.0f * (i + 1), 400.0f, 0.0f));
		if (ADNVehicle* Car = GetWorld()->SpawnActor<ADNVehicle>(ADNVehicle::StaticClass(), Location, Base.Rotator(), Params))
		{
			for (int32 Zone = 0; Zone < FDNDamageMorph::ZoneCount && Damage > 0.0f; ++Zone)
			{
				Car->DNZone(Zone, Damage);
			}
			++Spawned;
		}
	}
	UE_LOG(LogDetonauto, Display, TEXT("ADNVehicleTestGameMode: %d de %d carro(s) estacionado(s) com dano %.2f nas 5 zonas"), Spawned, Count, Damage);
}

void ADNVehicleTestGameMode::SpawnPropLine(const FString& Kinds, float StartM, float SpacingM)
{
	// cada peça é cópia (malhas e escala) de uma peça daquele tipo que já está no mapa: a cena de aparência tem todos os tipos
	const AActor* Spot = MapStart ? static_cast<AActor*>(MapStart) : static_cast<AActor*>(Start);
	const FTransform Base = Spot ? Spot->GetActorTransform() : FTransform(FVector(0.0f, 0.0f, 120.0f));
	TArray<FString> List;
	Kinds.ParseIntoArray(List, TEXT(","));
	int32 Spawned = 0;
	for (int32 i = 0; i < List.Num(); ++i)
	{
		const FName Kind(*List[i].TrimStartAndEnd());
		const ADNBreakableProp* Model = nullptr;
		for (TActorIterator<ADNBreakableProp> It(GetWorld()); It; ++It)
		{
			if (It->Kind == Kind && It->BreakMesh)
			{
				Model = *It;
				break;
			}
		}
		if (!Model)
		{
			UE_LOG(LogDetonauto, Warning, TEXT("DNPropLine: nenhuma peça '%s' no mapa para copiar"), *Kind.ToString());
			continue;
		}
		FVector Location = Base.TransformPosition(FVector((StartM + SpacingM * i) * 100.0f, 0.0f, 0.0f));
		Location.Z = 0.0f; // no asfalto (a cena tem o chão da rua em z = 0)
		const FTransform At(Base.Rotator() + FRotator(0.0f, 90.0f, 0.0f), Location, Model->GetActorScale3D());
		ADNBreakableProp* Prop = GetWorld()->SpawnActorDeferred<ADNBreakableProp>(ADNBreakableProp::StaticClass(), At);
		if (!Prop)
		{
			continue;
		}
		Prop->Kind = Kind;
		Prop->BaseMesh = Model->BaseMesh;
		Prop->BreakMesh = Model->BreakMesh;
		Prop->FinishSpawning(At);
		LineProps.Add(Prop);
		++Spawned;
	}
	UE_LOG(LogDetonauto, Display, TEXT("DNPropLine: %d peça(s) na faixa do carro, a partir de %.0f m, a cada %.0f m"), Spawned, StartM, SpacingM);
}

void ADNVehicleTestGameMode::LogPropLineSummary() const
{
	if (LineProps.IsEmpty() || CruiseKmh <= 0.0f)
	{
		return;
	}
	int32 Broken = 0;
	for (const TWeakObjectPtr<ADNBreakableProp>& Prop : LineProps)
	{
		Broken += Prop.IsValid() && Prop->IsBroken() ? 1 : 0;
	}
	const float MinKmh = Broken > 0 ? LineMinKmh : 0.0f;
	const bool bPass = Broken == LineProps.Num() && MinKmh >= 0.6f * CruiseKmh && LineMaxOffsetM <= 1.5f;
	UE_LOG(LogDetonauto, Display, TEXT("DNPropLine: resumo %d de %d quebrada(s) | menor velocidade depois da 1a quebra %.0f km/h (mínimo %.0f) | maior desvio %.2f m (máximo 1,5) | %s"),
		Broken, LineProps.Num(), MinKmh, 0.6f * CruiseKmh, LineMaxOffsetM, bPass ? TEXT("APROVADO") : TEXT("REPROVADO"));
}

void ADNVehicleTestGameMode::SpawnPropCam(const FString& Spec)
{
	TArray<FString> Parts;
	Spec.ParseIntoArray(Parts, TEXT(","));
	const int32 Index = Parts.Num() > 0 ? FCString::Atoi(*Parts[0]) : 0;
	const float AheadM = Parts.Num() > 1 ? FCString::Atof(*Parts[1]) : 10.0f;
	const float SideM = Parts.Num() > 2 ? FCString::Atof(*Parts[2]) : 5.0f;
	const float HeightM = Parts.Num() > 3 ? FCString::Atof(*Parts[3]) : 1.5f;
	const ADNBreakableProp* Prop = LineProps.IsValidIndex(Index) ? LineProps[Index].Get() : nullptr;
	const AActor* Spot = MapStart ? static_cast<AActor*>(MapStart) : static_cast<AActor*>(Start);
	if (!Prop || !Spot)
	{
		UE_LOG(LogDetonauto, Warning, TEXT("DNPropCam: sem a peça %d da fileira (-DNPropLine) ou sem largada; câmera não criada"), Index);
		return;
	}
	const FVector Target = Prop->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
	const FVector Location = Prop->GetActorLocation() + Spot->GetActorForwardVector() * (AheadM * 100.0f)
		+ Spot->GetActorRightVector() * (SideM * 100.0f) + FVector(0.0f, 0.0f, HeightM * 100.0f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PropCam = GetWorld()->SpawnActor<ACameraActor>(Location, (Target - Location).Rotation(), Params);
	if (PropCam)
	{
		PropCam->GetCameraComponent()->SetFieldOfView(60.0f);
		PropCam->GetCameraComponent()->SetConstraintAspectRatio(false);
		UE_LOG(LogDetonauto, Display, TEXT("DNPropCam: câmera parada na peça %d (%s): %.1f m à frente, %.1f m ao lado, %.1f m de altura"),
			Index, *Prop->Kind.ToString(), AheadM, SideM, HeightM);
	}
}

void ADNVehicleTestGameMode::HandlePropBroken(ADNBreakableProp& Prop)
{
	// captura no contato e depois (a de antes sai em PropShotTick, a 12 m da peça)
	++PropShots;
	FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("dn-prop-%d-%s-contato"), PropShots, *Prop.Kind.ToString()), false, false);
	if (PropShotDelays.IsEmpty())
	{
		PendingShots.Add({ 0.8f, FString::Printf(TEXT("dn-prop-%d-%s-depois"), PropShots, *Prop.Kind.ToString()) });
	}
	for (const float Delay : PropShotDelays)
	{
		PendingShots.Add({ Delay, FString::Printf(TEXT("dn-prop-%d-%s-t%03dms"), PropShots, *Prop.Kind.ToString(), FMath::RoundToInt(Delay * 1000.0f)) });
	}
}

void ADNVehicleTestGameMode::PropShotTick(ADNVehicle& Vehicle, float DeltaSeconds)
{
	if (PropCam && !bPropCamInView)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->SetViewTarget(PropCam);
			bPropCamInView = true;
		}
	}
	// uma captura por quadro (o pedido seguinte no mesmo quadro trocaria o nome da anterior); a que venceu junto sai no próximo
	bool bShotThisFrame = false;
	for (int32 i = 0; i < PendingShots.Num(); ++i)
	{
		PendingShots[i].Left -= DeltaSeconds;
		if (PendingShots[i].Left <= 0.0f && !bShotThisFrame)
		{
			FScreenshotRequest::RequestScreenshot(PendingShots[i].Name, false, false);
			PendingShots.RemoveAt(i--);
			bShotThisFrame = true;
		}
	}
	const FVector Car = Vehicle.GetActorLocation();
	const FVector Forward = Vehicle.GetActorForwardVector();
	for (TActorIterator<ADNBreakableProp> It(GetWorld()); It; ++It)
	{
		const FVector To = It->GetActorLocation() - Car;
		const double Ahead = FVector::DotProduct(To, Forward);
		const double Side = (To - Forward * Ahead).Size2D();
		if (!bShotThisFrame && !It->IsBroken() && Ahead > 0.0 && Ahead < 1200.0 && Side < 300.0 && !PropsShotBefore.Contains(*It))
		{
			bShotThisFrame = true; // a próxima espera um quadro, senão trocaria o nome desta
			PropsShotBefore.Add(*It);
			FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("dn-prop-%d-%s-antes"), PropShots + 1, *It->Kind.ToString()), false, false);
			UE_LOG(LogDetonauto, Display, TEXT("DNPropShots: %s a %.1f m, carro a %.1f km/h"), *It->Kind.ToString(), Ahead / 100.0, Vehicle.SpeedKmh());
		}
	}
}

void ADNVehicleTestGameMode::SpawnWallAhead(float DistanceM)
{
	const AActor* Spot = MapStart ? static_cast<AActor*>(MapStart) : static_cast<AActor*>(Start);
	const FTransform Base = Spot ? Spot->GetActorTransform() : FTransform(FVector(0.0f, 0.0f, 120.0f));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube)
	{
		UE_LOG(LogDetonauto, Error, TEXT("ADNVehicleTestGameMode: sem o cubo da engine; barreira não criada"));
		return;
	}
	// barreira de concreto atravessando a rua inteira (30 m), 1 m de espessura e 3 m de altura, com o pé no chão (z = 0 na cena)
	FVector Location = Base.TransformPosition(FVector(DistanceM * 100.0f, 0.0f, 0.0f));
	Location.Z = 150.0f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* WallActor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, Base.Rotator(), Params);
	if (!WallActor)
	{
		return;
	}
	UStaticMeshComponent* Comp = WallActor->GetStaticMeshComponent();
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(Cube);
	Comp->SetWorldScale3D(FVector(1.0f, 30.0f, 3.0f));
	Comp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// o concreto do meio-fio da cena de aparência, se existir (no mapa de teste, o material do cubo)
	if (UMaterialInterface* Concrete = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Detonauto/Lookdev/Materials/MI_MeioFio.MI_MeioFio")))
	{
		Comp->SetMaterial(0, Concrete);
	}
	UE_LOG(LogDetonauto, Display, TEXT("ADNVehicleTestGameMode: barreira a %.0f m à frente da largada"), DistanceM);
}

ADNVehicle* ADNVehicleTestGameMode::FindVehicle() const
{
	return Cast<ADNVehicle>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void ADNVehicleTestGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bProbe && !bShot && ScriptEvents.IsEmpty() && CruiseKmh <= 0.0f && !bPropShots)
	{
		return;
	}
	ADNVehicle* Vehicle = FindVehicle();
	if (!Vehicle)
	{
		return;
	}
	if (!ScriptEvents.IsEmpty())
	{
		KeyScriptTick(*Vehicle, DeltaSeconds);
	}
	if (CruiseKmh > 0.0f)
	{
		// piloto automático do teste (-DNCruiseKmh): mesmo controle do cenário Wall; o log de início diz que o acelerador vem dele
		if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovement()))
		{
			Move->SetThrottleInput(FMath::Clamp(0.4f + (CruiseKmh - Vehicle->SpeedKmh()) * 0.08f, 0.0f, 1.0f));
			Move->SetBrakeInput(0.0f);
			// mantém a faixa da largada: depois de uma batida o carro sai um pouco torto, e sem volante ia parar na calçada
			const AActor* Spot = MapStart ? static_cast<AActor*>(MapStart) : static_cast<AActor*>(Start);
			if (Spot)
			{
				const FVector Right = Spot->GetActorRightVector();
				const float OffsetM = static_cast<float>(FVector::DotProduct(Vehicle->GetActorLocation() - Spot->GetActorLocation(), Right)) / 100.0f;
				const float HeadingRad = FMath::Asin(FMath::Clamp(static_cast<float>(FVector::DotProduct(Vehicle->GetActorForwardVector(), Right)), -1.0f, 1.0f));
				const float YawRateRadS = static_cast<float>(Vehicle->GetMesh()->GetPhysicsAngularVelocityInRadians().Z);
				// ganho menor em alta (a 100 km/h o volante cheio fazia o carro oscilar e sair da faixa) e amortecido pelo giro
				const float Gain = FMath::Clamp(15.0f / FMath::Max(Vehicle->SpeedKmh() / 3.6f, 1.0f), 0.2f, 1.0f);
				const float Steer = FMath::Clamp(Gain * (-0.25f * OffsetM - 1.2f * HeadingRad) - 0.3f * YawRateRadS, -1.0f, 1.0f);
				Move->SetSteeringInput(Steer);
				if (LineProps.ContainsByPredicate([](const TWeakObjectPtr<ADNBreakableProp>& P) { return P.IsValid() && P->IsBroken(); }))
				{
					LineMinKmh = FMath::Min(LineMinKmh, Vehicle->SpeedKmh());
					LineMaxOffsetM = FMath::Max(LineMaxOffsetM, FMath::Abs(OffsetM));
				}
				CruiseLogLeft -= DeltaSeconds;
				if (CruiseLogLeft <= 0.0f)
				{
					CruiseLogLeft = 0.5f;
					UE_LOG(LogDetonauto, Display, TEXT("DNCruise: %.0f km/h | desvio %.2f m | rumo %.1f graus | giro %.2f rad/s | volante %.2f"),
						Vehicle->SpeedKmh(), OffsetM, FMath::RadiansToDegrees(HeadingRad), YawRateRadS, Steer);
				}
			}
		}
	}
	if (bPropShots)
	{
		PropShotTick(*Vehicle, DeltaSeconds);
	}
	if (!bProbe && !bShot)
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

void ADNVehicleTestGameMode::KeyScriptTick(ADNVehicle& Vehicle, float DeltaSeconds)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}
	auto Send = [PC](const FKey& Key, EInputEvent Event) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, Event, Event == IE_Released ? 0.0f : 1.0f, 1)); };
	// soltar as teclas de direção no começo e no fim do roteiro: cobre tecla presa de uma execução interrompida (17b)
	auto ReleaseDriveKeys = [&Send]()
	{
		for (const FKey& Key : { EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right, EKeys::LeftShift, EKeys::SpaceBar })
		{
			Send(Key, IE_Released);
		}
	};
	if (ScriptTime == 0.0f)
	{
		ReleaseDriveKeys();
		UE_LOG(LogDetonauto, Display, TEXT("DNKeys: teclas de direção soltas no começo do roteiro"));
	}
	ScriptTime += DeltaSeconds;
	for (FScriptEvent& E : ScriptEvents)
	{
		if (E.bDone || ScriptTime < E.Start)
		{
			continue;
		}
		if (E.What == TEXT("SHOT"))
		{
			E.bDone = true;
			FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("dn-play-%d"), ++ScriptShots), E.End > 0.0f, false); // fim > 0: com o HUD
		}
		else if (E.What == TEXT("QUIT"))
		{
			E.bDone = true;
			ReleaseDriveKeys();
			LogPropLineSummary();
			FPlatformMisc::RequestExit(false);
		}
		else if (E.What.StartsWith(TEXT("DENT")))
		{
			// DENT<fx>:<fy>:<fz>: uma batida máxima na caixa da carroceria nesta fração (-1..1) da extensão; só para achar a superfície
			E.bDone = true;
			TArray<FString> F;
			E.What.Mid(4).ParseIntoArray(F, TEXT(":"));
			if (F.Num() == 3)
			{
				Vehicle.SimulateImpactAtBoxFraction(20.0f, FVector(FCString::Atof(*F[0]), FCString::Atof(*F[1]), FCString::Atof(*F[2])));
			}
		}
		else if (E.What.StartsWith(TEXT("DEMO")))
		{
			E.bDone = true;
			Vehicle.ApplyDemoDents(FCString::Atoi(*E.What.Mid(4)));
		}
		else if (E.What.StartsWith(TEXT("CAM")))
		{
			E.bDone = true;
			Vehicle.SetShotCamera(FCString::Atof(*E.What.Mid(3)));
		}
		else if (E.What.StartsWith(TEXT("EXEC:")))
		{
			// EXEC:<comando de console>, ex. EXEC:csvprofile start (o comando não pode ter ';' nem '@', os separadores do roteiro)
			E.bDone = true;
			PC->ConsoleCommand(E.What.Mid(5));
			UE_LOG(LogDetonauto, Display, TEXT("DNKeys: t=%.2f s comando '%s'"), ScriptTime, *E.What.Mid(5));
		}
		else
		{
			const FKey Key(*E.What);
			const float End = E.End >= 0.0f ? E.End : E.Start + 0.15f;
			if (!E.bPressed)
			{
				E.bPressed = true;
				Send(Key, IE_Pressed);
				UE_LOG(LogDetonauto, Display, TEXT("DNKeys: t=%.2f s o roteiro apertou %s"), ScriptTime, *E.What);
			}
			if (ScriptTime >= End)
			{
				E.bDone = true;
				Send(Key, IE_Released);
			}
		}
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
		if (ManeuverT0 < 0.0f && (SpeedKmh >= HandbrakeEntryKmh || ScenarioTime >= 8.0f))
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
			Move->SetThrottleInput(HandbrakeThrottle);
			Move->SetSteeringInput(FMath::Clamp(HandbrakeSteer * Vehicle.AppliedSteeringGain, -1.0f, 1.0f));
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
			// recuperou = depois de soltar o freio de mão (0,9 s), deslize abaixo de 10 graus e guinada abaixo de 40 graus/s
			if (RecoverySeconds < 0.0f && Since >= 0.9f && FMath::Abs(Vehicle.SlipAngleDegNow()) < 10.0f
				&& FMath::Abs(Vehicle.GetMesh()->GetPhysicsAngularVelocityInDegrees().Z) < 40.0f && SpeedKmh > 15.0f)
			{
				RecoverySeconds = Since - 0.9f;
			}
			MaxSlipFullDeg = FMath::Max(MaxSlipFullDeg, FMath::Abs(Vehicle.SlipAngleDegNow()));
		}
	}
	else if (Scenario == TEXT("Boost"))
	{
		// Turbo (J3): segura 60 km/h (controle proporcional), espera 1 s estável e acelera a fundo por 3 s, com o turbo (-DNTurboOn) ou sem.
		// Mede a velocidade final aos 3 s.
		if (BoostT0 < 0.0f)
		{
			Move->SetThrottleInput(FMath::Clamp(0.4f + (60.0f - SpeedKmh) * 0.08f, 0.0f, 1.0f));
			Move->SetSteeringInput(0.0f);
			BoostAtSpeedSince = FMath::Abs(SpeedKmh - 60.0f) < 3.0f ? (BoostAtSpeedSince < 0.0f ? ScenarioTime : BoostAtSpeedSince) : -1.0f;
			if (BoostAtSpeedSince >= 0.0f && ScenarioTime - BoostAtSpeedSince >= 1.0f)
			{
				BoostT0 = ScenarioTime;
				SpeedAtManeuver = SpeedKmh;
				Vehicle.SetTurboButton(bBoostTurbo);
			}
		}
		else
		{
			Move->SetThrottleInput(1.0f);
			Move->SetSteeringInput(0.0f);
			if (ScenarioTime - BoostT0 >= 3.0f)
			{
				UE_LOG(LogDetonauto, Display, TEXT("DNCarProbe: TURBO preset=%s turbo=%s | entrou a %.1f km/h | velocidade aos 3 s: %.1f km/h | barra do turbo: %.0f"),
					*Vehicle.CarId.ToString(), bBoostTurbo ? TEXT("ligado") : TEXT("desligado"), SpeedAtManeuver, SpeedKmh, Vehicle.GetTurbo()->GetBar());
				ScenarioSeconds = ScenarioTime;
			}
		}
	}
	else if (Scenario == TEXT("Steer"))
	{
		// Curva em regime (playtest 1: "curva dura"): segura a velocidade -DNWallKmh (controle proporcional), espera 1 s dentro de
		// 3 km/h do alvo e entra com o esterço -DNSteer mantido por 6 s. Mede, nos 3 s finais (regime), a guinada [graus/s], o raio
		// (velocidade / guinada) e o ângulo real das rodas da frente.
		const float Target = WallTargetKmh > 0.0f ? WallTargetKmh : 60.0f;
		Move->SetThrottleInput(FMath::Clamp(0.4f + (Target - SpeedKmh) * 0.08f, 0.0f, 1.0f));
		if (SteerT0 < 0.0f)
		{
			Move->SetSteeringInput(0.0f);
			SteerAtSpeedSince = FMath::Abs(SpeedKmh - Target) < 3.0f ? (SteerAtSpeedSince < 0.0f ? ScenarioTime : SteerAtSpeedSince) : -1.0f;
			if (SteerAtSpeedSince >= 0.0f && ScenarioTime - SteerAtSpeedSince >= 1.0f)
			{
				SteerT0 = ScenarioTime;
				SpeedAtManeuver = SpeedKmh;
			}
		}
		else
		{
			Move->SetSteeringInput(FMath::Clamp(SteerInput * Vehicle.AppliedSteeringGain, -1.0f, 1.0f));
			const float Since = ScenarioTime - SteerT0;
			const double YawDegS = Vehicle.GetMesh()->GetPhysicsAngularVelocityInDegrees().Z;
			if (Since >= 3.0f && Since < 6.0f && FMath::Abs(YawDegS) > 1.0)
			{
				const double V = Vehicle.GetVelocity().Size2D() / 100.0;
				SteerRadiusSum += V / FMath::DegreesToRadians(FMath::Abs(YawDegS));
				SteerYawSum += FMath::Abs(YawDegS);
				SteerFrontAngleSum += FMath::Abs(Move->Wheels[0]->GetSteerAngle());
				++SteerSamples;
			}
			if (Since >= 6.0f)
			{
				ScenarioSeconds = ScenarioTime; // fecha agora
			}
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
	if (Scenario == TEXT("Steer"))
	{
		const double N = FMath::Max(SteerSamples, 1);
		UE_LOG(LogDetonauto, Display, TEXT("DNCarProbe: CURVA preset=%s | alvo=%.0f km/h (entrou a %.0f) | esterço=%.2f | regime (3 s): guinada=%.1f graus/s, raio=%.1f m, rodas da frente=%.1f graus, amostras=%d, aceleração lateral=%.2f g"),
			*Vehicle.CarId.ToString(), WallTargetKmh > 0.0f ? WallTargetKmh : 60.0f, SpeedAtManeuver, SteerInput, SteerYawSum / N, SteerRadiusSum / N, SteerFrontAngleSum / N, SteerSamples,
			(SteerSamples > 0 ? (Vehicle.SpeedKmh() / 3.6) * FMath::DegreesToRadians(SteerYawSum / N) / 9.81 : 0.0));
	}
	if (Scenario == TEXT("Handbrake") || Scenario == TEXT("Turn"))
	{
		UE_LOG(LogDetonauto, Display,
			TEXT("DNCarProbe: MANOBRA preset=%s | velocidade ao entrar=%.0f km/h | ângulo de deslizamento máximo=%.1f graus | guinada máxima=%.0f graus/s | velocidade mínima=%.0f km/h"),
			*Vehicle.CarId.ToString(), SpeedAtManeuver, MaxSlipDeg, MaxYawRateDegS, MinSpeedKmh);
		UE_LOG(LogDetonauto, Display, TEXT("DNCarProbe: RECUPERACAO preset=%s assistencia=%s | deslize maximo (-180..180)=%.1f graus | recuperou em %.2f s depois de soltar o freio de mao (-1 = nao recuperou)"),
			*Vehicle.CarId.ToString(), Vehicle.bDriftAssist ? TEXT("ligada") : TEXT("desligada"), MaxSlipFullDeg, RecoverySeconds);
	}
	FPlatformMisc::RequestExit(false);
}
