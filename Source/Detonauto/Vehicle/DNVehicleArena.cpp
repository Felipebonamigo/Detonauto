#include "Vehicle/DNVehicleArena.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

void ADNTestArena::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	FloorMaterial = NewObject<UPhysicalMaterial>(this, TEXT("DN_FloorMaterial"));
	FloorMaterial->Friction = FloorFriction;
	FloorMaterial->Restitution = 0.0f;
	Floor->SetPhysMaterialOverride(FloorMaterial);
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

	Floor = MakeBlock(TEXT("Floor"), FVector(0.0f, 0.0f, -50.0f), FVector(40000.0f, 40000.0f, 100.0f));
	Wall = MakeBlock(TEXT("Wall"), FVector(WallX + 50.0f, 0.0f, 200.0f), FVector(100.0f, 2000.0f, 400.0f));

	// Luz: o mapa vazio não traz nenhuma.
	UDirectionalLightComponent* Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-50.0f, -35.0f, 0.0f));
	Sun->SetIntensity(6.0f);

	USkyLightComponent* Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(Root);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->bLowerHemisphereIsBlack = false;
	Sky->SetIntensity(1.0f);
}
