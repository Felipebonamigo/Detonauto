// Uma peça do carro que solta (lote 18b, docs/regras-pecas.md): para-choque, capô, tampa, porta, para-lama, aerofólio. Só cola: QUANDO
// ela pendura e solta é regra pura (FDNCarParts, Core/DNCarParts.h); o carro (ADNVehicle) avalia e chama Hang e Detach.
//
//  - Presa: a malha (com os mesmos 5 alvos de deformação da carroceria, copiados pelo carro a cada mudança) presa ao chassi, sem colisão;
//  - Pendurada: corpo físico (uma caixa do tamanho da malha) ligado ao chassi por uma restrição de física (UPhysicsConstraintComponent)
//    que só deixa girar em volta da dobradiça, até HingeMaxDeg, para um lado só. Colisão de destroço (DNDebris): nunca segura o carro.
//    Encostando no chão, avisa OnPartScrape (faísca no lote 19, som no lote 21);
//  - Solta: a restrição quebra; a peça sai com a velocidade do ponto do carro onde estava mais LaunchMs na direção da batida, já amassada
//    (os alvos congelam), e fica na pista: dorme quando para, acorda quando o carro a varre.
//
// A malha da peça está no espaço do carro (a origem é a do carro, como a carroceria): a caixa fica no centro dela e a malha, deslocada
// para trás. Assim o material do amassado e do desgaste lê a mesma posição local que a carroceria.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DNCarParts.h"
#include "DNCarPart.generated.h"

class UBoxComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UPhysicsConstraintComponent;
class UMaterialInstanceDynamic;
class ADNCarPart;

/** A peça pendurada raspou no chão: o ponto do contato e a velocidade do carro [cm/s]. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FDNOnPartScrape, ADNCarPart* /*Part*/, const FVector& /*Point*/, float /*SpeedCmS*/);

UCLASS()
class DETONAUTO_API ADNCarPart : public AActor
{
	GENERATED_BODY()

public:
	ADNCarPart();

	/** Monta a peça presa ao Chassis (o componente físico do carro): a malha, a caixa e os materiais. Devolve os materiais dinâmicos
	 *  dos slots que usam o material do amassado (o carro manda neles os amassados e o desgaste, como na carroceria). */
	TArray<UMaterialInstanceDynamic*> Init(USkeletalMeshComponent& Chassis, USkeletalMesh& Mesh, const FDNCarPartSpec& InSpec, UMaterialInterface* DentParent);

	/** Pendura (só peça com dobradiça e presa). ImpactLocal: o ponto da batida no espaço do carro (o para-choque prende a ponta longe dele). */
	void Hang(const FVector& ImpactLocal);

	/** Solta (presa ou pendurada): sai com a velocidade do ponto do carro mais LaunchMs na direção ImpactDirWorld (para fora do carro). */
	void Detach(const FVector& ImpactDirWorld);

	/** Volta a peça presa, intacta de física (os alvos o carro zera). */
	void ResetPart();

	/** Os 5 pesos dos alvos de deformação (ordem de EDNDamageZone, sem a Under). Ignorado com a peça solta: ela voa já amassada. */
	void SetMorphWeights(const float* Weights, int32 Count);

	/** B9 do mobiliário: o carro varre o destroço que alcança (só a peça solta). */
	void Plow(const FVector& CarVelocityCmS);

	/** Orçamento de corpos físicos (FDNPartPool, MaxActive de Data/Damage.csv) somando as peças penduradas e soltas de todos os carros do
	 *  mundo: passando dele, as soltas mais antigas param onde estão (continuam na pista, sem simular). */
	static void EnforceBudget(UWorld& World, int32 MaxActive);

	/** Colisão da peça pendurada ou solta: destroço (ADNBreakableProp::MakeDebris) que a câmera atravessa. */
	static void MakePartDebris(UPrimitiveComponent& Comp);

	EDNPartStage GetStage() const { return Stage; }
	const FDNCarPartSpec& GetSpec() const { return Spec; }
	UBoxComponent* GetBox() const { return Box; }
	/** Ponta livre da peça pendurada no mundo (para conferir que a dobradiça abre para o lado certo). */
	FVector FreeEndWorld() const;
	/** Quantas vezes a peça pendurada raspou no chão (log e testes no motor). */
	int32 ScrapeCount() const { return Scrapes; }

	FDNOnPartScrape OnPartScrape;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION()
	void OnBoxHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Liga a física da caixa (destroço, massa, amortecimento) com a velocidade do ponto do carro onde a peça está. */
	void StartSimulating();
	void DestroyHinge();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> Visual;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsConstraintComponent> Hinge;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Chassis;

	FDNCarPartSpec Spec;
	EDNPartStage Stage = EDNPartStage::Attached;
	/** Centro e meia caixa da malha no espaço do carro [cm]. */
	FVector CenterLocal = FVector::ZeroVector;
	FVector ExtentLocal = FVector(10.0);
	/** Pivô e ponta livre da dobradiça no espaço do carro, escolhidos no Hang. */
	FVector PivotLocal = FVector::ZeroVector;
	FVector FreeEndLocal = FVector::ZeroVector;
	float StillSeconds = 0.0f;
	float HangSeconds = 0.0f;
	/** Quando soltou (tempo do jogo): a mais antiga para primeiro no orçamento. */
	double DetachTime = 0.0;
	int32 Scrapes = 0;
	double LastScrapeLogTime = -10.0;
	bool bFlightLogged = false;
};
