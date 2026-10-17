// A rua do percurso (docs/percurso-1.md, passo a1): monta, a partir da linha central de Data/CoursePoints.csv, o chão, o asfalto, as
// marcas de largada, de pontos de verificação e de chegada, e a luz da cena. Só malhas básicas da engine (/Engine/BasicShapes); o
// acabamento do City Sample entra nos passos a2 e a3. Nenhuma regra aqui: a linha e as marcas vêm de Core/DNCourse.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DNCourseActor.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UPhysicalMaterial;

UCLASS()
class DETONAUTO_API ADNCourseActor : public AActor
{
	GENERATED_BODY()

public:
	ADNCourseActor();

	/** Monta a rua sobre a linha central (cm, no plano Z = 0). SpacingCm é a distância entre os pontos de verificação. */
	void Build(const TArray<FVector>& InPoints, float InWidthCm, float SpacingCm);

	const TArray<FVector>& GetPoints() const { return Points; }
	float GetWidthCm() const { return WidthCm; }
	float GetLengthCm() const { return LengthCm; }
	/** Os progressos das marcas [cm]; a última é a chegada. */
	const TArray<float>& GetMarks() const { return Marks; }

	/** Onde e virado para onde o carro nasce: no início da linha, de frente para o primeiro trecho. */
	FTransform StartTransform(float HeightCm) const;

private:
	/** Um pórtico (dois postes e uma viga) atravessando a pista no progresso dado. */
	void AddGantry(float ProgressCm, float HeightCm, float BeamHeightCm);

	TArray<FVector> Points;
	float WidthCm = 0.0f;
	float LengthCm = 0.0f;
	TArray<float> Marks;

	UPROPERTY(VisibleAnywhere, Category="Percurso")
	TObjectPtr<UStaticMeshComponent> Ground;

	UPROPERTY(VisibleAnywhere, Category="Percurso")
	TObjectPtr<UInstancedStaticMeshComponent> Road;

	UPROPERTY(VisibleAnywhere, Category="Percurso")
	TObjectPtr<UInstancedStaticMeshComponent> Posts;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> GroundMaterial;
};
