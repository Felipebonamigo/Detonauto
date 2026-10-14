// Arena de teste do carro (lote 12): um piso grande e uma parede, criados em runtime por ADNVehicleTestGameMode. Existe para o
// carro poder rodar em um mapa vazio sem editar nenhum .umap à mão (o que nenhuma sessão consegue ler ou revisar). Só malhas
// básicas da engine (/Engine/BasicShapes) e a luz criada aqui; nenhum asset do projeto.
//
// Medidas (cm): piso de 400 m x 400 m com o topo em Z = 0; parede de 20 m x 4 m x 1 m, com a face voltada para o carro em
// X = WallX, centrada em Y = 0. O carro nasce em X = 0 olhando para +X.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DNVehicleArena.generated.h"

class UStaticMeshComponent;
class UPhysicalMaterial;

UCLASS()
class DETONAUTO_API ADNTestArena : public AActor
{
	GENERATED_BODY()

public:
	ADNTestArena();

	/** Distância da origem até a face da parede [cm]. */
	static constexpr float WallX = 6000.0f;

	/**
	 *  Atrito do piso. O Chaos multiplica o atrito das rodas pelo do material físico da superfície, e os presets de Data/Cars.csv foram
	 *  medidos na pista do template Vehicle, que usa o material NonSlippery (atrito 1,0, restituição 0); o material padrão da engine
	 *  tem 0,7 e deixaria o freio de mão rodar o carro (docs/presets-carro.md). A parede fica com o material padrão da engine, como
	 *  na calibração das batidas.
	 */
	static constexpr float FloorFriction = 1.0f;

	/** Estrada de teste (lote 13a): começa em X = 0, corre em +X; 3 faixas de 3,5 m (FDNTrafficRoadConfig). Só visual. */
	static constexpr float RoadLengthCm = 60000.0f;
	static constexpr float RoadWidthCm = 1050.0f;

	/** Liga ou desliga a parede (colisão e visual): a rodada de Crash corre pela estrada sem parede a 60 m. */
	void SetWallEnabled(bool bEnabled);

protected:
	virtual void PostInitializeComponents() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> FloorMaterial;

	UPROPERTY(VisibleAnywhere, Category="Arena")
	TObjectPtr<UStaticMeshComponent> Floor;

	UPROPERTY(VisibleAnywhere, Category="Arena")
	TObjectPtr<UStaticMeshComponent> Wall;

	UPROPERTY(VisibleAnywhere, Category="Arena")
	TObjectPtr<UStaticMeshComponent> Road;
};
