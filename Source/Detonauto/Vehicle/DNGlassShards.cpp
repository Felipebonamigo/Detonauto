#include "Vehicle/DNGlassShards.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
	constexpr float GravityCmS2 = 980.0f;
	/** Espalhamento da velocidade de cada caco [cm/s] e a fração que sobra num quique. */
	constexpr float SpreadCmS = 350.0f;
	constexpr float Bounce = 0.25f;
}

ADNGlassShards::ADNGlassShards()
{
	PrimaryActorTick.bCanEverTick = true;
	Shards = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Shards"));
	Shards->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shards->SetCastShadow(false); // cacos de 1 a 2 cm: sombra não aparece e custa
	Shards->SetCanEverAffectNavigation(false);
	RootComponent = Shards;
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		Shards->SetStaticMesh(Cube);
	}
}

void ADNGlassShards::Burst(const FVector& Center, const FVector& Extent, const FVector& Velocity, int32 Count, UMaterialInterface* Material)
{
	SetActorLocation(FVector::ZeroVector); // as instâncias vão em coordenadas do mundo
	if (Material)
	{
		Shards->SetMaterial(0, Material);
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DNGlassShardsGround), false);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Center, Center - FVector(0.0, 0.0, 500.0), ECC_Visibility, Params))
	{
		GroundZ = Hit.ImpactPoint.Z;
	}
	Items.Reset(Count);
	TArray<FTransform> Transforms;
	for (int32 i = 0; i < Count; ++i)
	{
		FShard S;
		S.Position = Center + FVector(FMath::FRandRange(-1.0f, 1.0f) * Extent.X, FMath::FRandRange(-1.0f, 1.0f) * Extent.Y, FMath::FRandRange(-1.0f, 1.0f) * Extent.Z);
		S.Velocity = Velocity + FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(0.0f, 1.0f)) * SpreadCmS;
		S.Rotation = FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f));
		S.Spin = FRotator(FMath::FRandRange(-720.0f, 720.0f), FMath::FRandRange(-720.0f, 720.0f), FMath::FRandRange(-720.0f, 720.0f));
		S.Scale = FMath::FRandRange(0.008f, 0.02f); // o cubo da engine tem 100 cm: cacos de 0,8 a 2 cm
		Items.Add(S);
		Transforms.Add(FTransform(S.Rotation, S.Position, FVector(S.Scale)));
	}
	Shards->AddInstances(Transforms, false, true);
	Age = 0.0f;
}

void ADNGlassShards::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age >= LifeSeconds || Items.IsEmpty())
	{
		Destroy();
		return;
	}
	// no último terço os cacos encolhem até sumir
	const float Fade = FMath::Clamp((LifeSeconds - Age) / (LifeSeconds / 3.0f), 0.0f, 1.0f);
	TArray<FTransform> Transforms;
	Transforms.Reserve(Items.Num());
	for (FShard& S : Items)
	{
		S.Velocity.Z -= GravityCmS2 * DeltaSeconds;
		S.Position += S.Velocity * DeltaSeconds;
		if (S.Position.Z < GroundZ + 1.0)
		{
			S.Position.Z = GroundZ + 1.0;
			S.Velocity = FVector(S.Velocity.X * 0.6, S.Velocity.Y * 0.6, -S.Velocity.Z * Bounce);
			S.Spin *= 0.5f;
		}
		S.Rotation += S.Spin * DeltaSeconds;
		Transforms.Add(FTransform(S.Rotation, S.Position, FVector(S.Scale * Fade)));
	}
	Shards->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
}
