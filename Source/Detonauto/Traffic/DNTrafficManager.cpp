#include "Traffic/DNTrafficManager.h"
#include "Core/DNGameDataSubsystem.h"
#include "Detonauto.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

ADNTrafficManager::ADNTrafficManager()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADNTrafficManager::BeginPlay()
{
	Super::BeginPlay();
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (const UDNGameDataSubsystem* Data = GameInstance->GetSubsystem<UDNGameDataSubsystem>())
			{
				Settings = Data->GetTrafficSettings();
				DamageConfig = Data->GetDamageConfig();
				DamageConfig.MaxDents = FDNDentParams::EffectiveMaxDents(DamageConfig.MaxDents);
				Specs = Data->GetTrafficVehicles();
			}
		}
	}
	if (Specs.IsEmpty())
	{
		UE_LOG(LogDetonauto, Error, TEXT("ADNTrafficManager: sem carros de trafego (TrafficVehicles.csv invalido ou nao carregado); nenhum carro vai nascer"));
	}

	// "rolando": combina pelo mínimo com o piso, senão a média com o atrito 1,0 do piso daria ~0,5 e o carro frearia do mesmo jeito
	RollingMaterial = NewObject<UPhysicalMaterial>(this, TEXT("DN_TrafficRolling"));
	RollingMaterial->Friction = Settings.RollingFriction;
	RollingMaterial->FrictionCombineMode = EFrictionCombineMode::Min;
	RollingMaterial->bOverrideFrictionCombineMode = true;
	RollingMaterial->Restitution = 0.0f;
}

void ADNTrafficManager::SetPlayer(AActor* InPlayer)
{
	Player = InPlayer;
	if (Player)
	{
		const FBox Local = Player->CalculateComponentsBoundingBoxInLocalSpace(true);
		if (Local.IsValid)
		{
			PlayerHalfSizeCm = Local.GetExtent();
		}
	}
}

FDNTrafficBody ADNTrafficManager::PlayerBody() const
{
	FDNTrafficBody B;
	B.PositionCm = Player->GetActorLocation();
	B.Forward = Player->GetActorForwardVector();
	B.HalfLengthM = PlayerHalfSizeCm.X / 100.0f;
	B.HalfWidthM = PlayerHalfSizeCm.Y / 100.0f;
	B.VelocityCmS = Player->GetVelocity();
	return B;
}

void ADNTrafficManager::Configure(const FDNTrafficRoadConfig& InRoad, int32 InTargetCount, int32 InSeed, int32 InPoolMax, bool bInAllPhysics)
{
	Road = InRoad;
	PoolState = FDNTrafficPool(InPoolMax);
	bAllPhysics = bInAllPhysics;
	Slots = FDNTrafficRoad::PlanSlots(Road, InTargetCount, InSeed);
	NextSlot = 0;
	bConfigured = true;
	if (Slots.Num() < InTargetCount)
	{
		UE_LOG(LogDetonauto, Warning, TEXT("ADNTrafficManager: a estrada comporta %d carros, pediram %d"), Slots.Num(), InTargetCount);
	}
}

ADNTrafficVehicle* ADNTrafficManager::Acquire()
{
	// a política (reaproveitar antes de criar, tamanho máximo) é do Core: FDNTrafficPool
	const FDNTrafficPool::FAcquired Slot = PoolState.Acquire();
	if (Slot.Index == INDEX_NONE)
	{
		return nullptr; // pool cheio: não cria mais
	}
	if (Slot.bCreated)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADNTrafficVehicle* Created = GetWorld()->SpawnActor<ADNTrafficVehicle>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (Created)
		{
			Created->OnImpact.AddUObject(this, &ADNTrafficManager::HandleImpact);
			Created->OnDestroyed.AddUObject(this, &ADNTrafficManager::HandleDestroyed);
		}
		Pool.Add(Created);
		if (!Created)
		{
			PoolState.Release(Slot.Index); // não conseguiu criar: devolve o índice
		}
		return Created;
	}
	return Pool[Slot.Index];
}

void ADNTrafficManager::Release(ADNTrafficVehicle* Vehicle)
{
	Vehicle->Deactivate();
	PoolState.Release(Pool.IndexOfByKey(Vehicle));
	++Stats.Despawned;
}

void ADNTrafficManager::SpawnFromPlan()
{
	if (Specs.IsEmpty())
	{
		return;
	}
	// quantos criar neste quadro: o que falta até o planejado, no máximo MaxSpawnPerFrame (Core)
	const int32 Active = Stats.Spawned - Stats.Despawned;
	const int32 Remaining = Slots.Num() - NextSlot;
	const int32 Count = FMath::Min(FDNTraffic::PlanSpawns(Slots.Num() - Stats.Despawned, Active, Settings.MaxSpawnPerFrame), Remaining);
	for (int32 i = 0; i < Count; ++i)
	{
		ADNTrafficVehicle* Vehicle = Acquire();
		if (!Vehicle)
		{
			return;
		}
		const FDNTrafficSlot& Slot = Slots[NextSlot];
		const FDNTrafficVehicleEntry& Spec = Specs[NextSlot % Specs.Num()];
		++NextSlot;

		const FVector Direction = Slot.bOncoming ? -FVector::ForwardVector : FVector::ForwardVector;
		const FVector Start = RoadOriginCm + FVector(Slot.PositionM * 100.0f, FDNTrafficRoad::LaneOffsetM(Road, Slot.Lane) * 100.0f, 0.0f);
		// a trilha vai até o fim da estrada (a favor) ou até 150 m depois do início dela (contrária: o piso de teste continua ali), para
		// o carro não parar no meio de uma medição
		const float RailLengthCm = (Slot.bOncoming ? Slot.PositionM + OncomingOvershootM : Road.LengthM - Slot.PositionM) * 100.0f;
		Vehicle->SetRollingMaterial(RollingMaterial);
		Vehicle->Activate(Spec, Start, Direction, Slot.SpeedKmh, RailLengthCm, DamageConfig);
		++Stats.Spawned;
		if (bAllPhysics)
		{
			Vehicle->SetMaintainSpeed(true);
			Vehicle->WakeUp(); // medição: tudo Physics desde o nascimento (não conta como "acordou")
		}
	}
}

FDNTrafficGeometryInput ADNTrafficManager::MakeGeometry(const ADNTrafficVehicle* Vehicle) const
{
	FDNTrafficGeometryInput G;
	G.Level = Vehicle->GetLevel();
	G.bCrashRoundActive = bCrashRoundActive;
	G.VehiclePositionCm = Vehicle->GetPositionCm();
	G.VehicleForward = Vehicle->GetForward();
	G.VehicleVelocityCmS = Vehicle->GetVelocityCmS();
	G.bHasPlayer = Player != nullptr; // sem jogador: nada acorda nem some por causa dele (o tick do gerente pode vir antes do SetPlayer)
	if (Player)
	{
		G.PlayerPositionCm = Player->GetActorLocation();
		G.PlayerForward = Player->GetActorForwardVector();
		G.PlayerVelocityCmS = Player->GetVelocity();
	}
	return G;
}

void ADNTrafficManager::UpdateRailVehicle(ADNTrafficVehicle* Vehicle, float DeltaTime, const TArray<FDNTrafficBody>& AllBodies, const TArray<FDNTrafficBody>& PhysicsBodies)
{
	// segue o carro da frente na mesma faixa (todos os outros veículos, inclusive o jogador, contam como possíveis líderes)
	const FDNTrafficLeader Leader = FDNTrafficGeometry::LeaderAhead(Vehicle->AsBody(), AllBodies, Road.LaneWidthM * 0.5f);
	Stats.MinRailGapM = FMath::Min(Stats.MinRailGapM, Leader.GapM);
	Vehicle->FollowLeader(Settings, Leader);
	Vehicle->AdvanceRail(DeltaTime);

	FDNTrafficGeometryInput G = MakeGeometry(Vehicle);
	G.ImpactForce = Vehicle->TakePendingImpact();
	G.NearestPhysicsDistanceM = FDNTrafficGeometry::NearestGapM(Vehicle->AsBody(), PhysicsBodies);
	switch (FDNTraffic::NextLevel(Settings, FDNTrafficGeometry::BuildInput(G)))
	{
	case EDNTrafficLevel::Physics:
		Vehicle->WakeUp();
		++Stats.Woken;
		break;
	case EDNTrafficLevel::Despawn:
		Release(Vehicle);
		break;
	default:
		break;
	}
}

void ADNTrafficManager::CheckPhysicsVehicle(ADNTrafficVehicle* Vehicle)
{
	FDNTrafficGeometryInput G = MakeGeometry(Vehicle);
	if (FDNTraffic::NextLevel(Settings, FDNTrafficGeometry::BuildInput(G)) == EDNTrafficLevel::Despawn)
	{
		Release(Vehicle);
	}
}

void ADNTrafficManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bConfigured)
	{
		return;
	}
	SpawnFromPlan();

	// caixas de todos os veículos (para seguir o da frente) e só dos Physics: o jogador e os carros já acordados (para acordar)
	TArray<FDNTrafficBody> AllBodies;
	TArray<FDNTrafficBody> PhysicsBodies;
	if (Player)
	{
		AllBodies.Add(PlayerBody());
		PhysicsBodies.Add(AllBodies.Last());
	}
	for (const ADNTrafficVehicle* Vehicle : Pool)
	{
		if (Vehicle && Vehicle->IsActive())
		{
			AllBodies.Add(Vehicle->AsBody());
			if (Vehicle->GetLevel() == EDNTrafficLevel::Physics)
			{
				PhysicsBodies.Add(AllBodies.Last());
			}
		}
	}

	PhysicsCheckTimer += DeltaSeconds;
	const bool bCheckPhysics = PhysicsCheckTimer >= PhysicsCheckSeconds;
	if (bCheckPhysics)
	{
		PhysicsCheckTimer = 0.0f;
	}

	Stats.ActiveRail = 0;
	Stats.ActivePhysics = 0;
	Stats.RailHitsBeforeWake = 0;
	Stats.DrivingPhysics = 0;
	double DrivingSpeedSum = 0.0;
	for (ADNTrafficVehicle* Vehicle : Pool)
	{
		if (!Vehicle || !Vehicle->IsActive())
		{
			continue;
		}
		if (Vehicle->GetLevel() == EDNTrafficLevel::Rail)
		{
			UpdateRailVehicle(Vehicle, DeltaSeconds, AllBodies, PhysicsBodies);
		}
		else if (bCheckPhysics)
		{
			CheckPhysicsVehicle(Vehicle);
		}
		if (Vehicle->IsActive())
		{
			(Vehicle->GetLevel() == EDNTrafficLevel::Rail ? Stats.ActiveRail : Stats.ActivePhysics) += 1;
			if (Vehicle->GetLevel() == EDNTrafficLevel::Physics && Vehicle->IsDriving())
			{
				++Stats.DrivingPhysics;
				DrivingSpeedSum += Vehicle->GetSpeedMs();
			}
		}
		Stats.RailHitsBeforeWake += Vehicle->GetRailHitsBeforeWake();
	}
	Stats.DrivingPhysicsMeanSpeedMs = Stats.DrivingPhysics > 0 ? static_cast<float>(DrivingSpeedSum / Stats.DrivingPhysics) : 0.0f;
}

float ADNTrafficManager::MaxWreckSpeedMs() const
{
	float Max = 0.0f;
	for (const ADNTrafficVehicle* Vehicle : Pool)
	{
		if (Vehicle && Vehicle->IsActive() && Vehicle->GetLevel() == EDNTrafficLevel::Physics && !Vehicle->IsDriving())
		{
			Max = FMath::Max(Max, Vehicle->GetSpeedMs());
		}
	}
	return Max;
}

void ADNTrafficManager::HandleImpact(ADNTrafficVehicle* Vehicle, AActor* Other)
{
	OnVehicleImpact.Broadcast(Vehicle, Other);
}

void ADNTrafficManager::HandleDestroyed(ADNTrafficVehicle* Vehicle, int32 ValueCash)
{
	++Stats.Destroyed;
	OnVehicleDestroyed.Broadcast(Vehicle, ValueCash);
}
