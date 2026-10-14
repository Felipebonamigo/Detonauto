#include "Vehicle/DNVehicleArena.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

void ADNTestArena::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	FloorMaterial = NewObject<UPhysicalMaterial>(this, TEXT("DN_FloorMaterial"));
	FloorMaterial->Friction = FloorFriction;
	FloorMaterial->Restitution = 0.0f;
	Floor->SetPhysMaterialOverride(FloorMaterial);

	if (UMaterialInstanceDynamic* RoadColor = Road->CreateAndSetMaterialInstanceDynamic(0))
	{
		RoadColor->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.02f, 0.025f));
	}
}

void ADNTestArena::SetWallEnabled(bool bEnabled)
{
	Wall->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Wall->SetVisibility(bEnabled);
}

ADNTestArena::ADNTestArena()
{
	PrimaryActorTick.bCanEverTick = false;

	// A raiz fica na origem, sem escala: as escalas do piso e da parede não passam para os filhos.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);

	// O cubo básico da engine mede 100 cm de lado, com o pivô no centro.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Flat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Grid(TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));

	auto MakeBlock = [this, Root](const TCHAR* Name, const FVector& Center, const FVector& SizeCm) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Block = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Block->SetupAttachment(Root);
		if (Cube.Succeeded())
		{
			Block->SetStaticMesh(Cube.Object);
		}
		if (Grid.Succeeded())
		{
			Block->SetMaterial(0, Grid.Object);
		}
		Block->SetMobility(EComponentMobility::Static);
		Block->SetCollisionProfileName(FName("BlockAll"));
		Block->SetRelativeLocation(Center);
		Block->SetRelativeScale3D(SizeCm / 100.0f);
		return Block;
	};

	// piso: de X = -200 m a X = +800 m, 400 m de largura (cobre a estrada de 600 m e a parede a 60 m)
	Floor = MakeBlock(TEXT("Floor"), FVector(30000.0f, 0.0f, -50.0f), FVector(100000.0f, 40000.0f, 100.0f));
	Wall = MakeBlock(TEXT("Wall"), FVector(WallX + 50.0f, 0.0f, 200.0f), FVector(100.0f, 2000.0f, 400.0f));

	// estrada: faixa escura sobre o piso (só visual; o piso é quem colide), de X = 0 a RoadLengthCm, com a largura das faixas
	Road = MakeBlock(TEXT("Road"), FVector(RoadLengthCm * 0.5f, 0.0f, 0.5f), FVector(RoadLengthCm, RoadWidthCm, 1.0f));
	Road->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Road->SetMaterial(0, Flat.Succeeded() ? Flat.Object : nullptr);

	// Luz: o mapa vazio não traz nenhuma.
	UDirectionalLightComponent* Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-50.0f, -35.0f, 0.0f));
	Sun->SetIntensity(6.0f);
	Sun->ForwardShadingPriority = 1; // com duas luzes direcionais, o motor avisa na tela qual vale no forward shading: o sol

	// luz de preenchimento do lado oposto: sem ela o lado do carro que fica na sombra do sol aparece preto nas câmeras laterais
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
