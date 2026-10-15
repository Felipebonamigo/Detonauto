// Peça de rua destrutível (lote 17c, docs/regras-mobiliario.md): poste, árvore, banco, lixeira, hidrante, placa, paraciclo.
//
// Inteira, é sólida e não custa nada (sem tick, sem simulação). Quando o carro bate acima do limiar do tipo (FDNBreakable), a parte
// que quebra (Body) solta com física, é lançada e tomba; a base ou o toco (Base) fica. O carro sai com a velocidade de antes do
// contato menos a perda da regra, e o dano dele passa pelo FDNDamage de sempre como batida em peça (que não conta para a rodada de
// Crash nem para a câmera de batida). A peça solta vira destroço (ECC_DNDebris): nunca mais segura o carro nem as rodas dele, mas
// bate no chão, nos prédios, nas outras peças e no tráfego. Dorme parada onde caiu e respeita o orçamento de peças simulando
// (UDNBreakableSubsystem). Pode ser reiniciada (ResetProp): serve de pool quando a cidade trocar instâncias por atores.
//
// O tipo (Kind) é o id de Data/Breakables.csv. Tipo sem linha no CSV: a peça fica sólida e o log avisa.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DNBreakable.h"
#include "DNBreakableProp.generated.h"

class ADNVehicle;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class DETONAUTO_API ADNBreakableProp : public AActor
{
	GENERATED_BODY()

public:
	ADNBreakableProp();

	/** Tipo da peça: id de Data/Breakables.csv (poste, arvore, banco, lixeira, hidrante, placa, paraciclo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DN Mobiliario")
	FName Kind;

	/** O que fica no lugar (base do poste, toco da árvore). Vazio: a peça sai inteira. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DN Mobiliario")
	TObjectPtr<UStaticMesh> BaseMesh;

	/** O que quebra e voa. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DN Mobiliario")
	TObjectPtr<UStaticMesh> BreakMesh;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 *  O carro encostou (chamado pelo ADNVehicle no contato). Velocidade e giro do carro ANTES do contato. Verdadeiro se a peça
	 *  quebrou: o carro já saiu com a velocidade nova e o dano. Falso: a peça é sólida (ou já quebrou) e o carro segue o caminho de
	 *  sempre de uma batida.
	 */
	bool HandleCarHit(ADNVehicle& Car, const FVector& CarVelocityBeforeCmS, const FVector& CarAngularBeforeDegS, const FVector& ImpactPoint);

	/** B9: o carro alcançou este destroço (velocidade do carro em cm/s): ele sai da frente; o que já dormiu volta a simular. */
	void Plow(const FVector& CarVelocityCmS);

	/** Para de simular onde está (sono ou orçamento estourado). */
	void Freeze();

	/** Volta inteira ao lugar (reinício da rodada, pool). */
	void ResetProp();

	bool IsBroken() const { return bBroken; }
	bool IsSimulating() const;
	const FDNBreakResult& LastBreak() const { return Last; }
	const UStaticMeshComponent* GetBaseComponent() const { return Base; }
	const UStaticMeshComponent* GetBodyComponent() const { return Body; }

	/** Colisão de destroço (ECC_DNDebris): bloqueia chão, prédios, peças e outro destroço; não segura o carro nem as rodas dele. */
	static void MakeDebris(UPrimitiveComponent& Comp);

private:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Body;

	void ApplyMeshes();
	void SetBodyBlocksCar(bool bBlocks);

	FDNBreakableKind KindConfig;
	bool bKindKnown = false;
	FDNBreakableSettings Settings;
	FDNBreakResult Last;
	bool bBroken = false;
	float StillSeconds = 0.0f;
};
