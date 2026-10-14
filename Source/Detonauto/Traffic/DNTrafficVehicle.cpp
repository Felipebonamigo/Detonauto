#include "Traffic/DNTrafficVehicle.h"
#include "Vehicle/DNVehicleArena.h"
#include "Components/StaticMeshComponent.h"
#include "Detonauto.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Tamanho provisório da caixa por classe de TrafficVehicles.csv [cm] (comprimento x largura x altura) e a cor. */
	struct FBoxLook { FVector SizeCm; FLinearColor Color; };

	FBoxLook LookOf(const FString& Class)
	{
		if (Class == TEXT("compact")) { return { FVector(400.f, 175.f, 140.f), FLinearColor(0.85f, 0.75f, 0.10f) }; }
		if (Class == TEXT("sedan")) { return { FVector(460.f, 185.f, 145.f), FLinearColor(0.20f, 0.45f, 0.85f) }; }
		if (Class == TEXT("van")) { return { FVector(520.f, 200.f, 210.f), FLinearColor(0.20f, 0.70f, 0.35f) }; }
		if (Class == TEXT("bus")) { return { FVector(1100.f, 250.f, 320.f), FLinearColor(0.85f, 0.35f, 0.15f) }; }
		if (Class == TEXT("truck")) { return { FVector(850.f, 250.f, 320.f), FLinearColor(0.55f, 0.20f, 0.65f) }; }
		return { FVector(450.f, 185.f, 150.f), FLinearColor(0.6f, 0.6f, 0.6f) };
	}

	/** O cubo básico da engine tem 100 cm de lado, pivô no centro. */
	constexpr float CubeHalfCm = 50.0f;
}

ADNTrafficVehicle::ADNTrafficVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Flat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	if (Cube.Succeeded())
	{
		Body->SetStaticMesh(Cube.Object);
	}
	if (Flat.Succeeded())
	{
		Body->SetMaterial(0, Flat.Object);
	}
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetCollisionProfileName(FName("PhysicsActor"));
	Body->SetNotifyRigidBodyCollision(true);
	Body->SetGenerateOverlapEvents(false);
	Body->SetSimulatePhysics(false);
	Body->OnComponentHit.AddDynamic(this, &ADNTrafficVehicle::OnBodyHit);

	// começa desligado: quem o pega do pool liga
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ADNTrafficVehicle::Activate(const FDNTrafficVehicleEntry& Spec, const FVector& StartCm, const FVector& Direction, float SpeedKmh, float InRailLengthCm, const FDNDamageConfig& InDamageConfig)
{
	// zera o que sobrou de uma vida anterior
	Body->SetSimulatePhysics(false);
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Body->SetPhysMaterialOverride(nullptr);
	DamageState.Reset();
	Ledger = FDNCrashVehicleLedger();
	ImpactGrouper = FDNImpactGrouper();
	PendingImpact = 0.0f;
	RailHitsBeforeWake = 0;
	bDriving = true;

	const FBoxLook Look = LookOf(Spec.Row.Class);
	HalfSizeCm = Look.SizeCm * 0.5f;
	Body->SetWorldScale3D(Look.SizeCm / (2.0f * CubeHalfCm));
	if (UMaterialInstanceDynamic* Dynamic = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		Dynamic->SetVectorParameterValue(TEXT("Color"), Look.Color);
	}

	DamageConfig = InDamageConfig;
	MassKg = Spec.Row.MassKg;
	ValueCash = Spec.Row.ValueCash;
	RailStart = StartCm + FVector(0.0f, 0.0f, Look.SizeCm.Z * 0.5f);
	RailDirection = Direction.GetSafeNormal2D();
	RailDistanceCm = 0.0f;
	RailLengthCm = InRailLengthCm;
	DesiredSpeedCmS = SpeedKmh / 0.036f;
	RailSpeedCmS = DesiredSpeedCmS;

	// o carro olha para o sentido da trilha
	SetActorLocationAndRotation(RailStart, RailDirection.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);

	Level = EDNTrafficLevel::Rail;
	bActive = true;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(false);
}

void ADNTrafficVehicle::Deactivate()
{
	Body->SetSimulatePhysics(false);
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	bActive = false;
	Level = EDNTrafficLevel::Despawn;
}

void ADNTrafficVehicle::AdvanceRail(float DeltaTime)
{
	if (!bActive || Level != EDNTrafficLevel::Rail || RailSpeedCmS <= 0.0f)
	{
		return;
	}
	const FDNRailAdvance Step = FDNTraffic::AdvanceOnRail(RailDistanceCm, RailSpeedCmS, DeltaTime, RailLengthCm, false);
	if (Step.bError)
	{
		RailSpeedCmS = 0.0f;
		DesiredSpeedCmS = 0.0f;
		return;
	}
	RailDistanceCm = Step.Distance;
	if (Step.bReachedEnd)
	{
		RailSpeedCmS = 0.0f; // chegou ao fim da trilha: para (os de trás param atrás dele, com folga)
		DesiredSpeedCmS = 0.0f;
	}
	// sem varredura: o corpo cinemático é arrastado pela física (empurra o jogador), e a batida chega por OnBodyHit
	SetActorLocation(RailStart + RailDirection * RailDistanceCm, false, nullptr, ETeleportType::None);
}

void ADNTrafficVehicle::WakeUp()
{
	if (!bActive || Level != EDNTrafficLevel::Rail)
	{
		return;
	}
	const FVector Velocity = GetVelocityCmS();
	Level = EDNTrafficLevel::Physics;
	Body->SetSimulatePhysics(true);
	Body->SetMassOverrideInKg(NAME_None, MassKg, true);
	if (bDriving && RollingMaterial)
	{
		Body->SetPhysMaterialOverride(RollingMaterial); // rola até a primeira batida com dano, em vez de frear como uma caixa
	}
	Body->SetPhysicsLinearVelocity(Velocity);
	VelocityBeforeStepCmS = Velocity;
	SetActorTickEnabled(true);
}

void ADNTrafficVehicle::FollowLeader(const FDNTrafficSettings& Settings, const FDNTrafficLeader& Leader)
{
	if (Level == EDNTrafficLevel::Rail)
	{
		RailSpeedCmS = FDNTraffic::FollowSpeed(Settings, DesiredSpeedCmS / 100.0f, Leader.GapM, Leader.SpeedMs) * 100.0f;
	}
}

FDNTrafficBody ADNTrafficVehicle::AsBody() const
{
	FDNTrafficBody B;
	B.PositionCm = GetPositionCm();
	B.Forward = GetActorForwardVector(); // o real: um carro acordado pode ter girado
	B.HalfLengthM = HalfSizeCm.X / 100.0f;
	B.HalfWidthM = HalfSizeCm.Y / 100.0f;
	B.VelocityCmS = GetVelocityCmS();
	return B;
}

FVector ADNTrafficVehicle::GetPositionCm() const
{
	return GetActorLocation();
}

FVector ADNTrafficVehicle::GetVelocityCmS() const
{
	if (Level == EDNTrafficLevel::Physics)
	{
		return Body->GetPhysicsLinearVelocity();
	}
	return Level == EDNTrafficLevel::Rail ? RailDirection * RailSpeedCmS : FVector::ZeroVector;
}

float ADNTrafficVehicle::TakePendingImpact()
{
	const float Impact = PendingImpact;
	PendingImpact = 0.0f;
	return Impact;
}

void ADNTrafficVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive || Level != EDNTrafficLevel::Physics)
	{
		return;
	}
	// fecha a janela de contatos vencida e guarda a velocidade ANTES do passo de física deste quadro (como no carro do jogador)
	FlushImpacts(GetWorld()->GetTimeSeconds());
	if (bMaintainSpeed && bDriving && DesiredSpeedCmS > 0.0f)
	{
		const FVector Current = Body->GetPhysicsLinearVelocity();
		Body->SetPhysicsLinearVelocity(FVector(RailDirection.X * DesiredSpeedCmS, RailDirection.Y * DesiredSpeedCmS, Current.Z));
	}
	VelocityBeforeStepCmS = Body->GetPhysicsLinearVelocity();
}

void ADNTrafficVehicle::OnBodyHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bActive || Cast<ADNTestArena>(OtherActor))
	{
		return; // piso da arena: apoio, não é batida
	}
	PendingImpact = FMath::Max(PendingImpact, FMath::Max(static_cast<float>(NormalImpulse.Size()), 1.0f));
	OnImpact.Broadcast(this, OtherActor);

	if (Level != EDNTrafficLevel::Physics)
	{
		++RailHitsBeforeWake; // sem velocidade medida: o gerente acorda o carro no quadro seguinte
		return;
	}
	// medição "todos em Physics": a velocidade imposta a cada quadro vence o contato, e o carro empurrava o destroço da frente como um
	// trator (a rodada nunca silenciava). No primeiro contato que não é o piso, para de impor a velocidade (lote 13b)
	bMaintainSpeed = false;

	const double Now = GetWorld()->GetTimeSeconds();
	FlushImpacts(Now);
	const float DeltaV = FDNDentParams::DeltaVMsBetween(VelocityBeforeStepCmS, Body->GetPhysicsLinearVelocity());
	if (DeltaV <= 0.0f)
	{
		return;
	}
	// ponto da batida no espaço do corpo, cortado na caixa do cubo (±50)
	const FVector Extent(CubeHalfCm, CubeHalfCm, CubeHalfCm);
	const FVector LocalContact = Body->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
	ImpactGrouper.Add(Now, DeltaV, FDNDentParams::ProjectToBody(LocalContact, FVector::ZeroVector, Extent, 0.0f));
}

void ADNTrafficVehicle::FlushImpacts(double Now)
{
	FDNImpactGrouper::FGroup Group;
	if (!ImpactGrouper.TryFlush(Now, Group))
	{
		return;
	}
	FDNImpact Impact;
	Impact.DeltaVMs = Group.DeltaVMs;
	Impact.Zone = FDNDentParams::ZoneOfLocalPoint(Group.LocalPoint, FVector::ZeroVector, FVector(CubeHalfCm, CubeHalfCm, CubeHalfCm));
	Impact.LocalPositionCm = Group.LocalPoint;
	const float TotalBefore = FDNDamage::TotalDamage(DamageState);
	const FDNDamageResult Result = FDNDamage::ApplyImpact(DamageConfig, DamageState, Impact);
	const float AddedTotal = FDNDamage::TotalDamage(DamageState) - TotalBefore;
	if (AddedTotal > 0.0f || Result.bJustDestroyed)
	{
		OnDamaged.Broadcast(this, ValueCash, AddedTotal, Result.bJustDestroyed); // pontos por dano (lote 13c)
	}
	if (bDriving && (Result.AddedZoneDamage > 0.0f || Result.bJustDestroyed))
	{
		// batida acima do limiar de dano: o motorista parou de dirigir; volta o atrito normal e acaba o "manter velocidade"
		bDriving = false;
		Body->SetPhysMaterialOverride(nullptr);
	}
	static int32 Logged = 0;
	if (Logged < 40)
	{
		++Logged;
		UE_LOG(LogDetonauto, Display, TEXT("DNTraffic: batida em '%s' (valor %d): variacao de velocidade %.1f m/s, zona %d, dano da zona +%.2f, total %.2f%s"),
			*GetName(), ValueCash, Group.DeltaVMs, static_cast<int32>(Impact.Zone), Result.AddedZoneDamage, FDNDamage::TotalDamage(DamageState), Result.bJustDestroyed ? TEXT(" | DESTRUIDO") : TEXT(""));
	}
	if (Result.bJustDestroyed)
	{
		OnDestroyed.Broadcast(this, ValueCash);
	}
}
