#include "Race/DNCourseActor.h"
#include "Core/DNCourse.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// O cubo básico da engine mede 100 cm de lado, com o pivô no centro.
	constexpr float CubeCm = 100.0f;
	constexpr float RoadThicknessCm = 2.0f;
	constexpr float GroundThicknessCm = 100.0f;
	constexpr float GroundMarginCm = 30000.0f;
	// emendas entre trechos da linha: cada caixa avança um pouco, para a curva poligonal não abrir fresta no lado de fora
	constexpr float SegmentOverlapCm = 120.0f;
}

ADNCourseActor::ADNCourseActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Flat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Ground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ground"));
	Ground->SetupAttachment(Root);
	Ground->SetMobility(EComponentMobility::Movable);
	Ground->SetCollisionProfileName(FName("BlockAll"));
	if (Cube.Succeeded())
	{
		Ground->SetStaticMesh(Cube.Object);
	}
	if (Flat.Succeeded())
	{
		Ground->SetMaterial(0, Flat.Object);
	}

	auto MakeInstances = [&](const TCHAR* Name) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* Ism = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Ism->SetupAttachment(Root);
		Ism->SetMobility(EComponentMobility::Movable);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision); // só visual; o chão é quem colide
		if (Cube.Succeeded())
		{
			Ism->SetStaticMesh(Cube.Object);
		}
		if (Flat.Succeeded())
		{
			Ism->SetMaterial(0, Flat.Object);
		}
		return Ism;
	};
	Road = MakeInstances(TEXT("Road"));
	Posts = MakeInstances(TEXT("Posts"));

	// Luz: o mapa vazio não traz nenhuma (as mesmas da arena de teste).
	UDirectionalLightComponent* Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-50.0f, -35.0f, 0.0f));
	Sun->SetIntensity(6.0f);
	Sun->ForwardShadingPriority = 1;

	UDirectionalLightComponent* Fill = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Fill"));
	Fill->SetupAttachment(Root);
	Fill->SetMobility(EComponentMobility::Movable);
	Fill->SetRelativeRotation(FRotator(-35.0f, 145.0f, 0.0f));
	Fill->SetIntensity(3.0f);
	Fill->SetCastShadows(false);

	USkyLightComponent* Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(Root);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->bLowerHemisphereIsBlack = false;
	Sky->SetIntensity(1.0f);
}

void ADNCourseActor::Build(const TArray<FVector>& InPoints, float InWidthCm, float SpacingCm)
{
	Points = InPoints;
	WidthCm = InWidthCm;
	LengthCm = FDNCourse::Length(Points);
	Marks = FDNCourse::Marks(LengthCm, SpacingCm);
	if (Points.Num() < 2 || LengthCm <= 0.0f)
	{
		return;
	}

	// chão: cobre a caixa da linha com uma margem, com o atrito do piso da arena de teste (os presets do carro foram medidos nele)
	FVector Min = Points[0], Max = Points[0];
	for (const FVector& P : Points)
	{
		Min = Min.ComponentMin(P);
		Max = Max.ComponentMax(P);
	}
	const FVector Center = (Min + Max) * 0.5f;
	const FVector Size = (Max - Min) + FVector(2.0f * GroundMarginCm, 2.0f * GroundMarginCm, 0.0f);
	Ground->SetWorldLocation(FVector(Center.X, Center.Y, -GroundThicknessCm * 0.5f - 5.0f)); // topo 5 cm abaixo do asfalto
	Ground->SetWorldScale3D(FVector(Size.X, Size.Y, GroundThicknessCm) / CubeCm);
	GroundMaterial = NewObject<UPhysicalMaterial>(this, TEXT("DN_GroundMaterial"));
	GroundMaterial->Friction = 1.0f;
	GroundMaterial->Restitution = 0.0f;
	Ground->SetPhysMaterialOverride(GroundMaterial);
	if (UMaterialInstanceDynamic* Color = Ground->CreateAndSetMaterialInstanceDynamic(0))
	{
		Color->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.09f, 0.03f));
	}

	// asfalto: uma caixa por trecho da linha
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		const FVector A = Points[i - 1];
		const FVector B = Points[i];
		const FVector Mid = (A + B) * 0.5f;
		const float Len = static_cast<float>(FVector::Dist2D(A, B));
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
		Road->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(Mid.X, Mid.Y, -RoadThicknessCm * 0.5f),
			FVector((Len + SegmentOverlapCm) / CubeCm, WidthCm / CubeCm, RoadThicknessCm / CubeCm)), true);
	}
	if (UMaterialInstanceDynamic* Color = Road->CreateAndSetMaterialInstanceDynamic(0))
	{
		Color->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.02f, 0.025f));
	}

	// largada (progresso 0), pontos de verificação e chegada (a última marca)
	AddGantry(0.0f, 450.0f, 60.0f);
	for (int32 i = 0; i < Marks.Num(); ++i)
	{
		const bool bFinish = i == Marks.Num() - 1;
		AddGantry(Marks[i], bFinish ? 450.0f : 300.0f, bFinish ? 60.0f : 25.0f);
	}
	if (UMaterialInstanceDynamic* Color = Posts->CreateAndSetMaterialInstanceDynamic(0))
	{
		Color->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.55f, 0.05f));
	}
}

void ADNCourseActor::AddGantry(float ProgressCm, float HeightCm, float BeamHeightCm)
{
	const FVector P = FDNCourse::LocationAt(Points, ProgressCm);
	const FVector Ahead = FDNCourse::LocationAt(Points, ProgressCm + 100.0f);
	const FVector Behind = FDNCourse::LocationAt(Points, FMath::Max(ProgressCm - 100.0f, 0.0f));
	const FVector Dir = (Ahead - Behind).GetSafeNormal2D();
	const FVector Side(-Dir.Y, Dir.X, 0.0f);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	const float Half = WidthCm * 0.5f + 60.0f;
	const float PostCm = 40.0f;
	for (const float Sign : { -1.0f, 1.0f })
	{
		Posts->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), P + Side * (Sign * Half) + FVector(0.0f, 0.0f, HeightCm * 0.5f),
			FVector(PostCm, PostCm, HeightCm) / CubeCm), true);
	}
	Posts->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), P + FVector(0.0f, 0.0f, HeightCm - BeamHeightCm * 0.5f),
		FVector(PostCm, 2.0f * Half, BeamHeightCm) / CubeCm), true);
}

FTransform ADNCourseActor::StartTransform(float HeightCm) const
{
	if (Points.Num() < 2)
	{
		return FTransform(FVector(0.0f, 0.0f, HeightCm));
	}
	const FVector Dir = (Points[1] - Points[0]).GetSafeNormal2D();
	return FTransform(FRotator(0.0f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.0f), FVector(Points[0].X, Points[0].Y, HeightCm));
}
