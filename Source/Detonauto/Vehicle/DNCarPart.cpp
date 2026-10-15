#include "Vehicle/DNCarPart.h"
#include "Crash/DNBreakableProp.h"
#include "Core/DNBreakable.h"
#include "Core/DNDamageMorph.h"
#include "Core/DNGameDataSubsystem.h"
#include "Detonauto.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "EngineUtils.h"

namespace
{
	/** O sentido do deslocamento do limite da dobradiça: conferido no motor (o capô abre para cima, a porta para fora, o para-choque cai). */
	constexpr float HingeOffsetSign = 1.0f;
	/** Empurrão inicial para a peça pendurada abrir [rad/s]: sem ele o capô ficava deitado até o próximo solavanco. */
	constexpr float HangKickRadS = 3.0f;
	/** Peça solta abaixo disto por SleepAfterS dorme (o mesmo do mobiliário) [cm/s]. */
	constexpr float SleepSpeedCmS = 20.0f;
	constexpr float SleepAfterS = 1.0f;
	/** Uma linha de log de raspar por segundo, no máximo. */
	constexpr double ScrapeLogEveryS = 1.0;
	/** Quando a peça solta registra onde está [s de jogo depois de soltar; 1 s real na câmera lenta da batida]. */
	constexpr double FlightLogAfterS = 0.25;
	/** A caixa física é menor que a malha: começando encostada no chão (o para-choque) ou na vizinha, a física empurrava o carro parado
	 *  a 6 km/h (primeiro teste do lote 18b). */
	constexpr float BoxShrink = 0.85f;
	/** Teto da velocidade com que a física desfaz uma interpenetração da caixa [cm/s]. */
	constexpr float MaxDepenetrationCmS = 100.0f;
	/** Mola do capô e da tampa pendurados (segura no meio da faixa): força e amortecimento do drive da restrição. */
	constexpr float HoodSpring = 4000.0f;
	constexpr float HoodSpringDamping = 150.0f;
	/** Peça solta: quanto do empurrão é para cima (1 = 45 graus) e o espalhamento de lado [graus]. */
	constexpr float LaunchUpShare = 1.0f;
	constexpr float LaunchSpreadDeg = 25.0f;
}

ADNCarPart::ADNCarPart()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetGenerateOverlapEvents(false);
	Box->SetCanEverAffectNavigation(false);
	RootComponent = Box;

	Visual = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Box);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);
}

TArray<UMaterialInstanceDynamic*> ADNCarPart::Init(USkeletalMeshComponent& InChassis, USkeletalMesh& Mesh, const FDNCarPartSpec& InSpec, UMaterialInterface* DentParent)
{
	Chassis = &InChassis;
	Spec = InSpec;
	Visual->SetSkeletalMesh(&Mesh);
	const FBoxSphereBounds Bounds = Mesh.GetImportedBounds();
	CenterLocal = Bounds.Origin;
	ExtentLocal = Bounds.BoxExtent.ComponentMax(FVector(2.0));
	Box->SetBoxExtent(ExtentLocal * BoxShrink);
	Visual->SetRelativeLocation(-CenterLocal); // a malha está no espaço do carro: a caixa no centro dela, a malha para trás
	AttachToComponent(&InChassis, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Box->SetRelativeLocationAndRotation(CenterLocal, FRotator::ZeroRotator);

	TArray<UMaterialInstanceDynamic*> Dynamics;
	for (int32 i = 0; i < Visual->GetNumMaterials(); ++i)
	{
		UMaterialInterface* Material = Visual->GetMaterial(i);
		if (Material && DentParent && Material->GetBaseMaterial() == DentParent->GetBaseMaterial())
		{
			UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, this);
			Visual->SetMaterial(i, Dynamic);
			Dynamics.Add(Dynamic);
		}
	}
	return Dynamics;
}

void ADNCarPart::SetMorphWeights(const float* Weights, int32 Count)
{
	if (Stage == EDNPartStage::Detached || !Weights)
	{
		return; // solta: voa já amassada
	}
	for (int32 Zone = 0; Zone < FMath::Min(Count, FDNDamageMorph::ZoneCount); ++Zone)
	{
		Visual->SetMorphTarget(FName(FDNDamageMorph::TargetName(Zone)), Weights[Zone], false);
	}
}

void ADNCarPart::MakePartDebris(UPrimitiveComponent& Comp)
{
	ADNBreakableProp::MakeDebris(Comp);
	// a esfera da câmera sai do meio do carro: a peça pendurada ou voando logo ao lado a pararia a 10% do caminho, com a câmera dentro
	// do carro. A peça é pequena e passa rápido; a câmera a atravessa
	Comp.SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void ADNCarPart::StartSimulating()
{
	const FVector Point = Box->GetComponentLocation();
	const FVector Velocity = Chassis ? Chassis->GetPhysicsLinearVelocityAtPoint(Point) : FVector::ZeroVector;
	const FVector Spin = Chassis ? Chassis->GetPhysicsAngularVelocityInRadians() : FVector::ZeroVector;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	MakePartDebris(*Box); // nunca segura o carro (nem o chassi nem as rodas)
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (FBodyInstance* Body = Box->GetBodyInstance())
	{
		Body->SetMaxDepenetrationVelocity(MaxDepenetrationCmS);
	}
	Box->SetNotifyRigidBodyCollision(true);
	Box->OnComponentHit.AddUniqueDynamic(this, &ADNCarPart::OnBoxHit);
	Box->SetSimulatePhysics(true);
	Box->SetMassOverrideInKg(NAME_None, Spec.MassKg, true);
	Box->SetAngularDamping(Spec.HingeDamping);
	Box->SetLinearDamping(0.05f);
	Box->SetPhysicsLinearVelocity(Velocity);
	Box->SetPhysicsAngularVelocityInRadians(Spin);
	StillSeconds = 0.0f;
	SetActorTickEnabled(true);
}

void ADNCarPart::Hang(const FVector& ImpactLocal)
{
	if (Stage != EDNPartStage::Attached || !Spec.HasHinge() || !Chassis)
	{
		return;
	}
	const FVector& C = CenterLocal;
	const FVector& E = ExtentLocal;
	FVector Axis = FVector::YAxisVector;
	FVector Opening = FVector::UpVector; // para onde a ponta livre anda ao abrir
	switch (Spec.HingeEdge)
	{
	case EDNHingeEdge::TopRear: // capô: a borda do para-brisa; a frente sobe
		PivotLocal = FVector(C.X - E.X, C.Y, C.Z + E.Z);
		FreeEndLocal = FVector(C.X + E.X, C.Y, C.Z + E.Z);
		break;
	case EDNHingeEdge::TopFront: // tampa traseira: a borda da frente; a traseira sobe
		PivotLocal = FVector(C.X + E.X, C.Y, C.Z + E.Z);
		FreeEndLocal = FVector(C.X - E.X, C.Y, C.Z + E.Z);
		break;
	case EDNHingeEdge::SideFront: // porta: a borda da frente, eixo vertical; a traseira abre para fora
	{
		const float Side = C.Y >= 0.0f ? 1.0f : -1.0f;
		PivotLocal = FVector(C.X + E.X, C.Y + Side * E.Y, C.Z);
		FreeEndLocal = FVector(C.X - E.X, C.Y + Side * E.Y, C.Z);
		Axis = FVector::UpVector;
		Opening = FVector(0.0f, Side, 0.0f);
		break;
	}
	case EDNHingeEdge::Lateral: // para-choque: a ponta longe da batida fica presa, a outra cai até o chão
	{
		const float Keep = ImpactLocal.Y >= C.Y ? -1.0f : 1.0f;
		PivotLocal = FVector(C.X, C.Y + Keep * E.Y, C.Z + E.Z);
		FreeEndLocal = FVector(C.X, C.Y - Keep * E.Y, C.Z + E.Z);
		Axis = FVector::ForwardVector;
		Opening = FVector::DownVector;
		break;
	}
	default:
		return;
	}
	// girar +ε em volta do eixo leva a ponta livre na direção Eixo x (ponta - pivô): o eixo aponta para o lado que abre
	if (FVector::DotProduct(FVector::CrossProduct(Axis, FreeEndLocal - PivotLocal), Opening) < 0.0)
	{
		Axis = -Axis;
	}
	const FTransform Car = Chassis->GetComponentTransform();
	const FVector AxisWorld = Car.TransformVectorNoScale(Axis).GetSafeNormal();
	StartSimulating();
	// pendurada não bate nas outras peças (as caixas das vizinhas se tocam: o capô "raspava" na do para-choque e as duas empurravam
	// o carro); solta, volta a bater (Detach)
	Box->SetCollisionResponseToChannel(ECC_DNDebris, ECR_Ignore);

	Hinge = NewObject<UPhysicsConstraintComponent>(this, TEXT("Hinge"));
	Hinge->RegisterComponent();
	Hinge->SetWorldLocationAndRotation(Car.TransformPosition(PivotLocal), FRotationMatrix::MakeFromX(AxisWorld).Rotator());
	Hinge->SetDisableCollision(true);
	Hinge->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
	Hinge->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
	Hinge->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
	Hinge->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
	Hinge->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
	// o giro (twist, em volta do X da restrição = a dobradiça) vai de 0 a HingeMaxDeg para um lado só: o limite é simétrico, então o zero
	// dele é deslocado de meio ângulo e a peça começa numa ponta da faixa
	const float Half = 0.5f * Spec.HingeMaxDeg;
	Hinge->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, Half);
	Hinge->ConstraintInstance.AngularRotationOffset = FRotator(0.0f, 0.0f, HingeOffsetSign * Half);
	if (Spec.HingeEdge == EDNHingeEdge::TopRear || Spec.HingeEdge == EDNHingeEdge::TopFront)
	{
		// capô e tampa: sem nada, a gravidade fecha a peça e ninguém vê que pendurou. Uma mola fraca a segura no meio da faixa (o zero do
		// giro, deslocado de meio ângulo acima) e ela balança com o carro, como a tampa amassada que não fecha mais
		Hinge->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
		Hinge->SetAngularOrientationDrive(false, true);
		Hinge->SetAngularOrientationTarget(FRotator::ZeroRotator);
		Hinge->SetAngularDriveParams(HoodSpring, HoodSpringDamping, 0.0f);
	}
	Hinge->SetConstrainedComponents(Chassis, NAME_None, Box, NAME_None);
	Box->SetPhysicsAngularVelocityInRadians(Box->GetPhysicsAngularVelocityInRadians() + AxisWorld * HangKickRadS);
	Stage = EDNPartStage::Hanging;
	HangSeconds = 0.0f;
	UE_LOG(LogDetonauto, Display, TEXT("DNCarPart: %s pendurada (borda %s, até %.0f graus)"), FDNCarParts::PartName(Spec.Part),
		FDNCarParts::HingeEdgeName(Spec.HingeEdge), Spec.HingeMaxDeg);
}

void ADNCarPart::DestroyHinge()
{
	if (Hinge)
	{
		Hinge->BreakConstraint();
		Hinge->DestroyComponent();
		Hinge = nullptr;
	}
}

void ADNCarPart::Detach(const FVector& ImpactDirWorld)
{
	if (Stage == EDNPartStage::Detached || !Chassis)
	{
		return;
	}
	if (Stage == EDNPartStage::Attached)
	{
		StartSimulating();
	}
	DestroyHinge();
	Box->SetCollisionResponseToChannel(ECC_DNDebris, ECR_Block); // solta: bate nas outras peças e no mobiliário caído
	// o empurrão sai para fora do carro pelo lado da peça, sem entrar no obstáculo (P5, FDNCarParts::LaunchHeading), e sobe. Um pouco de
	// giro de lado espalha as peças
	const FVector Out = Box->GetComponentLocation() - Chassis->GetComponentLocation();
	const FVector2D Heading = FDNCarParts::LaunchHeading(FVector2D(Out.X, Out.Y), FVector2D(ImpactDirWorld.X, ImpactDirWorld.Y), FMath::RandBool() ? 1.0f : -1.0f);
	const FVector Dir = (FVector(Heading.X, Heading.Y, 0.0) + FVector::UpVector * LaunchUpShare).GetSafeNormal().RotateAngleAxis(FMath::FRandRange(-LaunchSpreadDeg, LaunchSpreadDeg), FVector::UpVector);
	const FVector Push = Dir * (Spec.LaunchMs * 100.0f);
	Box->SetUseCCD(true); // sai rápido e é fina: sem CCD atravessava o chão
	Box->SetPhysicsLinearVelocity(Box->GetPhysicsLinearVelocity() + Push);
	Box->SetPhysicsAngularVelocityInRadians(Box->GetPhysicsAngularVelocityInRadians() + FVector(FMath::FRandRange(-2.0f, 2.0f), FMath::FRandRange(-2.0f, 2.0f), FMath::FRandRange(-1.0f, 1.0f)));
	Stage = EDNPartStage::Detached;
	StillSeconds = 0.0f;
	bFlightLogged = false;
	DetachTime = GetWorld()->GetTimeSeconds();
	SetActorTickEnabled(true);
	UE_LOG(LogDetonauto, Display, TEXT("DNCarPart: %s solta a %.1f m/s"), FDNCarParts::PartName(Spec.Part), Box->GetPhysicsLinearVelocity().Size() / 100.0f);
}

void ADNCarPart::ResetPart()
{
	DestroyHinge();
	Box->SetSimulatePhysics(false);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Chassis)
	{
		AttachToComponent(Chassis, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Box->SetRelativeLocationAndRotation(CenterLocal, FRotator::ZeroRotator);
	}
	Stage = EDNPartStage::Attached;
	Scrapes = 0;
	SetActorTickEnabled(false);
}

FVector ADNCarPart::FreeEndWorld() const
{
	return Box->GetComponentTransform().TransformPosition(FreeEndLocal - CenterLocal);
}

void ADNCarPart::Plow(const FVector& CarVelocityCmS)
{
	if (Stage != EDNPartStage::Detached)
	{
		return;
	}
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UDNGameDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UDNGameDataSubsystem>() : nullptr;
	const FDNBreakableSettings Settings = Data ? Data->GetBreakableSettings() : FDNBreakableSettings();
	const bool bWasSimulating = Box->IsSimulatingPhysics();
	const FVector Before = bWasSimulating ? Box->GetPhysicsLinearVelocity() : FVector::ZeroVector;
	const FVector After = FDNBreakable::Plow(Settings, CarVelocityCmS, Before);
	if (After.Equals(Before, 1.0))
	{
		return;
	}
	if (!bWasSimulating)
	{
		Box->SetSimulatePhysics(true);
	}
	Box->WakeRigidBody();
	Box->SetPhysicsLinearVelocity(After);
	StillSeconds = 0.0f;
	SetActorTickEnabled(true);
}

void ADNCarPart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Stage == EDNPartStage::Hanging && Chassis)
	{
		// 1 s depois de pendurar: o quanto abriu e para que lado (confere o sentido da dobradiça no motor)
		const float Before = HangSeconds;
		HangSeconds += DeltaSeconds;
		if (Before < 1.0f && HangSeconds >= 1.0f)
		{
			const FVector Now = Chassis->GetComponentTransform().InverseTransformPosition(FreeEndWorld());
			const FVector A = (FreeEndLocal - PivotLocal).GetSafeNormal();
			const FVector B = (Now - PivotLocal).GetSafeNormal();
			UE_LOG(LogDetonauto, Display, TEXT("DNCarPart: %s pendurada há 1 s: aberta %.0f graus, a ponta livre andou %s (no carro)"),
				FDNCarParts::PartName(Spec.Part), FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.0, 1.0))),
				*(Now - FreeEndLocal).ToCompactString());
		}
		return;
	}
	if (Stage != EDNPartStage::Detached || !Box->IsSimulatingPhysics())
	{
		return;
	}
	// onde a peça solta está (confere que ela voa à vista da câmera de batida): FlightLogAfterS de jogo depois de soltar e ao dormir
	auto LogWhere = [this](const TCHAR* When)
	{
		const FVector Local = Chassis ? Chassis->GetComponentTransform().InverseTransformPosition(Box->GetComponentLocation()) - CenterLocal : FVector::ZeroVector;
		UE_LOG(LogDetonauto, Display, TEXT("DNCarPart: %s solta, %s: andou %s cm no espaço do carro, a %.1f m/s"),
			FDNCarParts::PartName(Spec.Part), When, *Local.ToCompactString(), Box->GetPhysicsLinearVelocity().Size() / 100.0f);
	};
	if (!bFlightLogged && GetWorld()->GetTimeSeconds() - DetachTime >= FlightLogAfterS)
	{
		bFlightLogged = true;
		LogWhere(TEXT("0,25 s depois"));
	}
	// solta e parada: dorme (acorda quando o carro a varre)
	StillSeconds = Box->GetPhysicsLinearVelocity().Size() < SleepSpeedCmS ? StillSeconds + DeltaSeconds : 0.0f;
	if (StillSeconds >= SleepAfterS)
	{
		LogWhere(TEXT("dormiu"));
		Box->PutRigidBodyToSleep();
		SetActorTickEnabled(false);
	}
}

void ADNCarPart::OnBoxHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// pendurada e encostando no chão (estático, voltado para cima): raspa
	if (Stage != EDNPartStage::Hanging || Hit.ImpactNormal.Z < 0.6 || !OtherComp || OtherComp->GetCollisionObjectType() != ECC_WorldStatic)
	{
		return;
	}
	++Scrapes;
	const float Speed = Chassis ? static_cast<float>(Chassis->GetPhysicsLinearVelocity().Size()) : 0.0f;
	OnPartScrape.Broadcast(this, Hit.ImpactPoint, Speed);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastScrapeLogTime >= ScrapeLogEveryS)
	{
		LastScrapeLogTime = Now;
		UE_LOG(LogDetonauto, Display, TEXT("DNCarPart: OnPartScrape %s raspando no chão em %s, carro a %.0f km/h (%d contatos)"), FDNCarParts::PartName(Spec.Part),
			*Hit.ImpactPoint.ToCompactString(), Speed * 0.036f, Scrapes);
	}
}

void ADNCarPart::EnforceBudget(UWorld& World, int32 MaxActive)
{
	TArray<ADNCarPart*> Simulating;
	TArray<ADNCarPart*> Loose;
	for (TActorIterator<ADNCarPart> It(&World); It; ++It)
	{
		if (It->Box->IsSimulatingPhysics())
		{
			Simulating.Add(*It);
			if (It->Stage == EDNPartStage::Detached)
			{
				Loose.Add(*It);
			}
		}
	}
	const FDNPartPoolPlan Plan = FDNPartPool::Plan(MaxActive, Simulating.Num(), 0);
	if (Plan.Recycle <= 0)
	{
		return;
	}
	Loose.Sort([](const ADNCarPart& A, const ADNCarPart& B) { return A.DetachTime < B.DetachTime; });
	for (int32 i = 0; i < FMath::Min(Plan.Recycle, Loose.Num()); ++i)
	{
		Loose[i]->Box->SetSimulatePhysics(false); // para onde está e continua na pista (colisão de destroço, sem segurar o carro)
		Loose[i]->SetActorTickEnabled(false);
		UE_LOG(LogDetonauto, Display, TEXT("DNCarPart: orçamento de %d corpos: %s para onde está"), MaxActive, FDNCarParts::PartName(Loose[i]->Spec.Part));
	}
}
