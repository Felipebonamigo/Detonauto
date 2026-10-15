#include "Crash/DNBreakableProp.h"
#include "Crash/DNBreakableSubsystem.h"
#include "Vehicle/DNVehicle.h"
#include "Core/DNGameDataSubsystem.h"
#include "Detonauto.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
	/** Abaixo desta velocidade [m/s] a peça solta desliga o CCD (só vale enquanto ela anda rápido). */
	constexpr float CcdSpeedMs = 5.0f;
}

ADNBreakableProp::ADNBreakableProp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // inteira não custa nada: só a peça solta ticka

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	// a base (ou o toco) fica e não segura o carro: ele passa por cima do flange e do toco, como num arcade. É destroço desde o início,
	// senão as rodas subiam no toco de 40 cm da árvore
	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(Root);
	Base->SetMobility(EComponentMobility::Movable);
	Base->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MakeDebris(*Base);

	// a parte que quebra: sólida até a batida passar do limiar
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionObjectType(ECC_WorldDynamic);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->SetSimulatePhysics(false);
}

void ADNBreakableProp::ApplyMeshes()
{
	Base->SetStaticMesh(BaseMesh);
	Body->SetStaticMesh(BreakMesh);
}

void ADNBreakableProp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMeshes();
}

void ADNBreakableProp::BeginPlay()
{
	Super::BeginPlay();
	ApplyMeshes();
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UDNGameDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UDNGameDataSubsystem>() : nullptr;
	Settings = Data ? Data->GetBreakableSettings() : FDNBreakableSettings();
	bKindKnown = Data && Data->TryGetBreakableKind(Kind.ToString(), KindConfig);
	if (!bKindKnown)
	{
		UE_LOG(LogDetonauto, Warning, TEXT("DNProp: '%s' com tipo '%s' sem linha válida em Breakables.csv: a peça fica sólida"), *GetName(), *Kind.ToString());
	}
}

bool ADNBreakableProp::IsSimulating() const
{
	return Body && Body->IsSimulatingPhysics();
}

void ADNBreakableProp::MakeDebris(UPrimitiveComponent& Comp)
{
	Comp.SetCollisionObjectType(ECC_DNDebris);
	Comp.SetCollisionResponseToAllChannels(ECR_Block);
	Comp.SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	Comp.SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void ADNBreakableProp::SetBodyBlocksCar(bool bBlocks)
{
	Body->SetCollisionResponseToChannel(ECC_Vehicle, bBlocks ? ECR_Block : ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Pawn, bBlocks ? ECR_Block : ECR_Ignore);
}

bool ADNBreakableProp::HandleCarHit(ADNVehicle& Car, const FVector& CarVelocityBeforeCmS, const FVector& CarAngularBeforeDegS, const FVector& ImpactPoint)
{
	if (bBroken || !bKindKnown || !Body->GetStaticMesh())
	{
		return false;
	}
	FDNBreakInput Input;
	Input.CarVelocityCmS = CarVelocityBeforeCmS;
	Input.CarMassKg = Car.GetCarConfig().MassKg;
	Input.CarToProp = Body->GetComponentLocation() - Car.GetActorLocation();
	// o pé da parte que quebra fica no corte (o pivô das malhas cortadas é o chão); o tombo gira em volta dele (B3)
	const FVector Foot = Body->GetComponentLocation() + FVector::UpVector * (KindConfig.BreakHeightCm * Body->GetComponentScale().Z);
	const FVector Com = Body->GetCenterOfMass();
	Input.PivotToComCm = (Com - Body->GetComponentLocation()).IsNearlyZero() ? Body->Bounds.Origin - Foot : Com - Foot;
	const FDNBreakResult R = FDNBreakable::Hit(KindConfig, Settings, Input);
	if (!R.bBreaks)
	{
		return false; // sólida: o carro bate como num obstáculo
	}
	bBroken = true;
	Last = R;

	// o carro: velocidade de antes do contato menos a perda (só o módulo), e o giro de antes (sem guinada)
	USkeletalMeshComponent* CarBody = Car.GetMesh();
	CarBody->SetPhysicsLinearVelocity(R.CarVelocityCmS);
	CarBody->SetPhysicsAngularVelocityInDegrees(CarAngularBeforeDegS);

	// a peça vira destroço e sai lançada (e tombando, se for do tipo que tomba). Destroço nunca mais segura carro, nem o chassi nem as
	// rodas: ignorar só 0,3 s (o IgnoreCarS do contrato) não bastava, a lixeira lançada a 35 m/s caía à frente, o carro a alcançava e
	// ela ficava presa no para-choque (17c, a 100 km/h: o carro parou nela). "Bater e seguir" pede o carro livre do que derrubou
	MakeDebris(*Body);
	Body->SetSimulatePhysics(true);
	Body->SetMassOverrideInKg(NAME_None, KindConfig.MassKg, true);
	Body->SetUseCCD(true);
	Body->SetPhysicsLinearVelocity(R.PropVelocityCmS);
	Body->SetPhysicsAngularVelocityInRadians(R.PropAngularRadS);
	StillSeconds = 0.0f;
	SetActorTickEnabled(true);
	if (UDNBreakableSubsystem* Props = GetWorld()->GetSubsystem<UDNBreakableSubsystem>())
	{
		Props->OnPropBroken(*this);
	}

	Car.ApplyPropImpact(R.CarDeltaVMs, ImpactPoint);
	UE_LOG(LogDetonauto, Display, TEXT("DNProp: %s (%s) quebrou: carro %.1f -> %.1f km/h (variação %.2f m/s), peça lançada a %.1f m/s, giro %.2f rad/s, braço %.2f m, %d ponto(s)"),
		*Kind.ToString(), *GetName(), CarVelocityBeforeCmS.Size() * 0.036, R.CarVelocityCmS.Size() * 0.036, R.CarDeltaVMs,
		R.PropVelocityCmS.Size() / 100.0, R.PropAngularRadS.Size(), Input.PivotToComCm.Size() / 100.0, R.Points);
	return true;
}

void ADNBreakableProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsSimulating())
	{
		SetActorTickEnabled(false);
		return;
	}
	const float SpeedMs = Body->GetPhysicsLinearVelocity().Size() / 100.0f;
	if (SpeedMs < CcdSpeedMs)
	{
		Body->SetUseCCD(false); // CCD só enquanto anda rápido
	}
	if (FDNBreakable::StepSleep(Settings, SpeedMs, DeltaSeconds, StillSeconds))
	{
		Freeze();
	}
}

void ADNBreakableProp::Plow(const FVector& CarVelocityCmS)
{
	if (!bBroken)
	{
		return;
	}
	const bool bWasSimulating = IsSimulating();
	const FVector Before = bWasSimulating ? Body->GetPhysicsLinearVelocity() : FVector::ZeroVector;
	const FVector SpinBefore = bWasSimulating ? Body->GetPhysicsAngularVelocityInRadians() : FVector::ZeroVector;
	const FVector After = FDNBreakable::Plow(Settings, CarVelocityCmS, Before);
	// o giro leva o topo para a frente: sem ele, o poste que caía por cima do carro atravessava o capô
	const FVector SpinAfter = FDNBreakable::PlowSpin(Settings, CarVelocityCmS, SpinBefore);
	if (After.Equals(Before, 1.0) && SpinAfter.Equals(SpinBefore, 0.01))
	{
		return;
	}
	if (!bWasSimulating)
	{
		// destroço que já dormiu acorda e volta ao orçamento de peças simulando
		Body->SetSimulatePhysics(true);
		StillSeconds = 0.0f;
		SetActorTickEnabled(true);
		if (UDNBreakableSubsystem* Props = GetWorld()->GetSubsystem<UDNBreakableSubsystem>())
		{
			Props->OnPropWoke(*this);
		}
	}
	Body->SetUseCCD(After.Size() / 100.0 >= CcdSpeedMs);
	Body->SetPhysicsLinearVelocity(After);
	Body->SetPhysicsAngularVelocityInRadians(SpinAfter);
}

FDNFreezeCandidate ADNBreakableProp::FreezeCandidate() const
{
	FDNFreezeCandidate C;
	C.SpeedMs = IsSimulating() ? static_cast<float>(Body->GetPhysicsLinearVelocity().Size()) / 100.0f : 0.0f;
	const FBoxSphereBounds& B = Body->Bounds;
	const FVector Bottom = B.Origin - FVector(0.0, 0.0, B.BoxExtent.Z + 20.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DNPropGrounded), false, this);
	FHitResult Hit;
	C.bGrounded = GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, B.Origin, Bottom, ECC_Visibility, Params);
	return C;
}

void ADNBreakableProp::Freeze()
{
	if (IsSimulating())
	{
		Body->SetSimulatePhysics(false); // fica onde está (solta da raiz desde a quebra), à vista; os veículos passam por ela
		Body->SetUseCCD(false);
		if (UDNBreakableSubsystem* Props = GetWorld() ? GetWorld()->GetSubsystem<UDNBreakableSubsystem>() : nullptr)
		{
			Props->OnPropFrozen(*this);
		}
	}
	SetActorTickEnabled(false);
}

void ADNBreakableProp::ResetProp()
{
	Freeze();
	Body->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Body->SetRelativeTransform(FTransform::Identity);
	Body->SetCollisionObjectType(ECC_WorldDynamic); // inteira de novo: segura o carro e as rodas veem
	SetBodyBlocksCar(true);
	StillSeconds = 0.0f;
	bBroken = false;
	Last = FDNBreakResult();
	SetActorTickEnabled(false);
}
