// Cacos de um vidro que estourou (lote 18b, docs/regras-pecas.md): cubinhos de vidro temperado que saem do vidro com a velocidade do
// carro, caem e somem em ~0,9 s. Sem física: instâncias de um cubo (uma chamada de desenho) movidas a cada quadro, quicando num chão
// achado por um traço no começo. O contrato pede Niagara; o Niagara entra no lote 19 (faíscas, fumaça, fogo) e este efeito vira um
// sistema dele. Até lá, isto não depende de asset feito à mão.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DNGlassShards.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;

UCLASS()
class DETONAUTO_API ADNGlassShards : public AActor
{
	GENERATED_BODY()

public:
	ADNGlassShards();

	/** Solta Count cacos espalhados na caixa (Center, Extent, no mundo), com a velocidade Velocity [cm/s] mais um espalhamento. */
	void Burst(const FVector& Center, const FVector& Extent, const FVector& Velocity, int32 Count, UMaterialInterface* Material);

	static constexpr float LifeSeconds = 0.9f;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Shards;

	struct FShard
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FRotator Spin = FRotator::ZeroRotator;
		float Scale = 0.015f;
	};
	TArray<FShard> Items;
	float Age = 0.0f;
	/** Altura do chão embaixo do vidro (os cacos quicam nela) [cm]. */
	double GroundZ = -1.0e9;
};
