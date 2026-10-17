#include "Race/DNCourseActor.h"
#include "Core/DNCourse.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"
#include "Math/RandomStream.h"

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

	// acabamento do City Sample (docs/percurso-1.md, passo a2); se os assets não estão no projeto, fica a pista cinza
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SidewalkMesh(TEXT("/Game/Road/Kit_Sidewalk_A/Mesh/SM_Sidewalk_1-5_3_A.SM_Sidewalk_1-5_3_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CurbMesh(TEXT("/Game/Road/Kit_Small_Curb_A/Mesh/SM_Small_Curb_A_Straight_01.SM_Small_Curb_A_Straight_01"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Asphalt(TEXT("/Game/Road/Material/MI/M_Asphalt_Master_Inst.M_Asphalt_Master_Inst"));
	for (const TCHAR* Name : { TEXT("01"), TEXT("02"), TEXT("03"), TEXT("04"), TEXT("05"), TEXT("07"), TEXT("08") })
	{
		const FString Path = FString::Printf(TEXT("/Game/Building/CH/C/Kit_Bldg_CHC_L10_A/Mesh/SM_BLDG_CHC_L10_A_Wall_%s_N1.SM_BLDG_CHC_L10_A_Wall_%s_N1"), Name, Name);
		ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Found(*Path);
		if (Found.Succeeded())
		{
			UInstancedStaticMeshComponent* Ism = MakeInstances(*FString::Printf(TEXT("Facade%s"), Name));
			Ism->SetStaticMesh(Found.Get());
			Ism->EmptyOverrideMaterials();
			Facades.Add(Ism);
		}
	}
	BuildingMass = MakeInstances(TEXT("BuildingMass"));
	Sidewalk = MakeInstances(TEXT("Sidewalk"));
	Curb = MakeInstances(TEXT("Curb"));
	Sidewalk->SetStaticMesh(SidewalkMesh.Succeeded() ? SidewalkMesh.Object : nullptr);
	Curb->SetStaticMesh(CurbMesh.Succeeded() ? CurbMesh.Object : nullptr);
	AsphaltMaterial = Asphalt.Succeeded() ? Asphalt.Object : nullptr;

	// Luz: o mapa vazio não traz nenhuma (as mesmas da arena de teste).
	UDirectionalLightComponent* Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-50.0f, -35.0f, 0.0f));
	Sun->SetIntensity(6.0f);
	Sun->ForwardShadingPriority = 1;
	Sun->SetAtmosphereSunLight(true);

	UDirectionalLightComponent* Fill = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Fill"));
	Fill->SetupAttachment(Root);
	Fill->SetMobility(EComponentMobility::Movable);
	Fill->SetRelativeRotation(FRotator(-35.0f, 145.0f, 0.0f));
	Fill->SetIntensity(3.0f);
	Fill->SetCastShadows(false);

	// céu e névoa: o mapa vazio é um fundo preto sem eles
	USkyAtmosphereComponent* Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);
	UExponentialHeightFogComponent* Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(0.004f);

	USkyLightComponent* Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(Root);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->bLowerHemisphereIsBlack = false;
	Sky->bRealTimeCapture = true;
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
	// trecho antes da largada para a grade (40 m, na direÃ§Ã£o do primeiro trecho, ao contrÃ¡rio)
	constexpr float RunUpCm = 4000.0f;
	GeoPoints = Points;
	GeoPoints.Insert(Points[0] - (Points[1] - Points[0]).GetSafeNormal2D() * RunUpCm, 0);
	GeoLengthCm = FDNCourse::Length(GeoPoints);

	// chão: cobre a caixa da linha com uma margem, com o atrito do piso da arena de teste (os presets do carro foram medidos nele)
	FVector Min = GeoPoints[0], Max = GeoPoints[0];
	for (const FVector& P : GeoPoints)
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
	for (int32 i = 1; i < GeoPoints.Num(); ++i)
	{
		const FVector A = GeoPoints[i - 1];
		const FVector B = GeoPoints[i];
		const FVector Mid = (A + B) * 0.5f;
		const float Len = static_cast<float>(FVector::Dist2D(A, B));
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
		Road->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(Mid.X, Mid.Y, -RoadThicknessCm * 0.5f),
			FVector((Len + SegmentOverlapCm) / CubeCm, WidthCm / CubeCm, RoadThicknessCm / CubeCm)), true);
	}
	if (AsphaltMaterial)
	{
		Road->SetMaterial(0, AsphaltMaterial);
	}
	else if (UMaterialInstanceDynamic* Color = Road->CreateAndSetMaterialInstanceDynamic(0))
	{
		Color->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.02f, 0.025f));
	}
	AddSidewalks();
	AddBuildings();

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

void ADNCourseActor::AddSidewalks()
{
	// peças retas de calçada e de meio-fio, lado a lado ao longo da linha, nos dois lados; o eixo mais comprido de cada peça segue a linha
	auto Place = [this](UInstancedStaticMeshComponent* Ism, float InnerOffsetCm, bool bFlipOutside)
	{
		const UStaticMesh* Mesh = Ism->GetStaticMesh();
		if (!Mesh)
		{
			return;
		}
		const FBoxSphereBounds B = Mesh->GetBounds();
		const bool bAlongX = B.BoxExtent.X >= B.BoxExtent.Y;
		const float PieceLen = 2.0f * (bAlongX ? B.BoxExtent.X : B.BoxExtent.Y);
		const float PieceWid = 2.0f * (bAlongX ? B.BoxExtent.Y : B.BoxExtent.X);
		UE_LOG(LogTemp, Display, TEXT("DNCourse: peça '%s' mede %.0f x %.0f x %.0f cm (comprimento ao longo de %s), centro local (%.0f, %.0f, %.0f)"), *Mesh->GetName(), 2.0f * B.BoxExtent.X, 2.0f * B.BoxExtent.Y, 2.0f * B.BoxExtent.Z, bAlongX ? TEXT("X") : TEXT("Y"), B.Origin.X, B.Origin.Y, B.Origin.Z);
		if (PieceLen < 20.0f)
		{
			return;
		}
		const float LocalYawFix = bAlongX ? 0.0f : -90.0f;
		for (const float Sign : { -1.0f, 1.0f })
		{
			for (float S = 0.0f; S < GeoLengthCm; S += PieceLen)
			{
				const float Mid = FMath::Min(S + PieceLen * 0.5f, GeoLengthCm);
				const FVector P = FDNCourse::LocationAt(GeoPoints, Mid);
				const FVector Dir = (FDNCourse::LocationAt(GeoPoints, Mid + 50.0f) - FDNCourse::LocationAt(GeoPoints, FMath::Max(Mid - 50.0f, 0.0f))).GetSafeNormal2D();
				const FVector Side(-Dir.Y, Dir.X, 0.0f);
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)) + LocalYawFix + ((bFlipOutside && Sign > 0.0f) || (!bFlipOutside && Sign < 0.0f) ? 180.0f : 0.0f);
				const FRotator Rot(0.0f, Yaw, 0.0f);
				const FVector Target = P + Side * (Sign * (WidthCm * 0.5f + InnerOffsetCm + PieceWid * 0.5f));
				const FVector CenterLocal(B.Origin.X, B.Origin.Y, 0.0f);
				Ism->AddInstance(FTransform(Rot, Target - Rot.RotateVector(CenterLocal)), true);
			}
		}
	};
	Place(Curb, 0.0f, true);
	const UStaticMesh* CurbMesh = Curb->GetStaticMesh();
	const float CurbWidth = CurbMesh ? 2.0f * FMath::Min(CurbMesh->GetBounds().BoxExtent.X, CurbMesh->GetBounds().BoxExtent.Y) : 0.0f;
	Place(Sidewalk, CurbWidth, true);
}

void ADNCourseActor::AddBuildings()
{
	// atalho: só paredes retas (sem quinas nem colunas): a rua do percurso 1 não tem cruzamento; quina entra com o primeiro cruzamento
	TArray<UInstancedStaticMeshComponent*> Variants;
	for (UInstancedStaticMeshComponent* Ism : Facades)
	{
		if (Ism && Ism->GetStaticMesh())
		{
			Variants.Add(Ism);
		}
	}
	if (Variants.IsEmpty())
	{
		return;
	}
	const float SidewalkDepthCm = 150.0f;
	const float CurbCm = 15.0f;
	constexpr float FloorCm = 325.0f; // a altura de um andar da peça
	FRandomStream Seed(7);
	for (const float Sign : { -1.0f, 1.0f })
	{
		float S = 0.0f;
		float BuildingLeftCm = 0.0f; // largura que falta para o prédio atual acabar; o número de andares vale para o prédio todo
		int32 Floors = 5;
		while (S < GeoLengthCm)
		{
			if (BuildingLeftCm <= 0.0f)
			{
				BuildingLeftCm = Seed.FRandRange(2500.0f, 4500.0f);
				Floors = Seed.RandRange(4, 10);
			}
			UInstancedStaticMeshComponent* Ism = Variants[Seed.RandRange(0, Variants.Num() - 1)];
			const FBoxSphereBounds B = Ism->GetStaticMesh()->GetBounds();
			const bool bAlongX = B.BoxExtent.X >= B.BoxExtent.Y;
			const float Len = 2.0f * (bAlongX ? B.BoxExtent.X : B.BoxExtent.Y);
			const float Depth = 2.0f * (bAlongX ? B.BoxExtent.Y : B.BoxExtent.X);
			if (Len < 100.0f)
			{
				break;
			}
			const float Mid = FMath::Min(S + Len * 0.5f, GeoLengthCm);
			const FVector P = FDNCourse::LocationAt(GeoPoints, Mid);
			const FVector Dir = (FDNCourse::LocationAt(GeoPoints, Mid + 50.0f) - FDNCourse::LocationAt(GeoPoints, FMath::Max(Mid - 50.0f, 0.0f))).GetSafeNormal2D();
			const FVector Side(-Dir.Y, Dir.X, 0.0f);
			const float AlignYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)) + (bAlongX ? 0.0f : -90.0f);
			const float Yaw = AlignYaw + (Sign > 0.0f ? 180.0f : 0.0f); // a frente da peça (palpite: -Y local) olha para a pista
			const FRotator Rot(0.0f, Yaw, 0.0f);
			const FVector Target = P + Side * (Sign * (WidthCm * 0.5f + CurbCm + SidewalkDepthCm + Depth * 0.5f));
			const FVector CenterLocal(B.Origin.X, B.Origin.Y, 0.0f);
			for (int32 Floor = 0; Floor < Floors; ++Floor)
			{
				Ism->AddInstance(FTransform(Rot, Target - Rot.RotateVector(CenterLocal) + FVector(0.0f, 0.0f, Floor * FloorCm)), true);
			}
			// miolo atrás da peça: caixa de 30 m de fundo, da altura do prédio, alinhada com a linha
			constexpr float MassDepthCm = 3000.0f;
			const FVector MassCenter = P + Side * (Sign * (WidthCm * 0.5f + CurbCm + SidewalkDepthCm + Depth + MassDepthCm * 0.5f)) + FVector(0.0f, 0.0f, Floors * FloorCm * 0.5f);
			BuildingMass->AddInstance(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.0f), MassCenter, FVector(Len + 40.0f, MassDepthCm, Floors * FloorCm) / 100.0f), true);
			S += Len;
			BuildingLeftCm -= Len;
		}
	}
	if (UMaterialInstanceDynamic* Color = BuildingMass->CreateAndSetMaterialInstanceDynamic(0))
	{
		Color->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.30f, 0.28f, 0.26f));
	}
	for (UInstancedStaticMeshComponent* Ism : Variants)
	{
		const FBoxSphereBounds B = Ism->GetStaticMesh()->GetBounds();
		UE_LOG(LogTemp, Display, TEXT("DNCourse: fachada '%s' mede %.0f x %.0f x %.0f cm, centro local (%.0f, %.0f, %.0f), %d instâncias"), *Ism->GetStaticMesh()->GetName(), 2.0f * B.BoxExtent.X, 2.0f * B.BoxExtent.Y, 2.0f * B.BoxExtent.Z, B.Origin.X, B.Origin.Y, B.Origin.Z, Ism->GetNumInstances());
	}
}

FTransform ADNCourseActor::GridTransform(int32 Slot, float HeightCm) const
{
	const FTransform Start = StartTransform(HeightCm);
	const FVector Forward = Start.GetRotation().GetForwardVector();
	const FVector Right = Start.GetRotation().GetRightVector();
	const float BackCm = 600.0f * (Slot / 2 + 1);
	const float SideCm = (Slot % 2 == 0 ? -300.0f : 300.0f);
	return FTransform(Start.GetRotation(), Start.GetLocation() - Forward * BackCm + Right * SideCm);
}
