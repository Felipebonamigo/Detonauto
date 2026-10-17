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
class UMaterialInterface;

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

	/** A vaga Slot da grade de largada: atrás da linha de largada, uma fileira a cada 6 m, em duas colunas (±3 m), vaga 0 na frente. */
	FTransform GridTransform(int32 Slot, float HeightCm) const;

private:
	/** Calçada e meio-fio do City Sample dos dois lados da pista, em peças retas ao longo da linha (passo a2). Sem os assets: nada. */
	void AddSidewalks();

	/** Faixas no asfalto: linha central tracejada e bordas contínuas, em caixas finas ao longo da linha. */
	void AddRoadMarkings();

	/** Fachadas de prédio do City Sample (kit CH/C L10_A) dos dois lados, atrás da calçada, com a ordem das peças sorteada por uma semente
	 *  fixa (o percurso é sempre igual); passo a3. Sem os assets: nada. */
	void AddBuildings();

	/** Um pórtico (dois postes e uma viga) atravessando a pista no progresso dado. */
	void AddGantry(float ProgressCm, float HeightCm, float BeamHeightCm);

	TArray<FVector> Points;
	float WidthCm = 0.0f;
	float LengthCm = 0.0f;
	/** A linha da geometria: a do jogo com um trecho de 40 m antes da largada, onde fica a grade. O progresso e as marcas usam Points. */
	TArray<FVector> GeoPoints;
	float GeoLengthCm = 0.0f;
	TArray<float> Marks;

	UPROPERTY(VisibleAnywhere, Category="Percurso")
	TObjectPtr<UStaticMeshComponent> Ground;

	UPROPERTY(VisibleAnywhere, Category="Percurso")
	TObjectPtr<UInstancedStaticMeshComponent> Road;

	UPROPERTY(VisibleAnywhere, Category="Percurso")
	TObjectPtr<UInstancedStaticMeshComponent> Posts;

	UPROPERTY(VisibleAnywhere, Category="Percurso")
	TObjectPtr<UInstancedStaticMeshComponent> Markings;

	UPROPERTY(VisibleAnywhere, Category="Percurso")
	TObjectPtr<UInstancedStaticMeshComponent> Sidewalk;

	UPROPERTY(VisibleAnywhere, Category="Percurso")
	TObjectPtr<UInstancedStaticMeshComponent> Curb;

	/** O miolo de cada prédio, atrás da fachada: sem ele as janelas mostram o chão do outro lado (a peça é só a casca). */
	UPROPERTY(VisibleAnywhere, Category="Percurso")
	TObjectPtr<UInstancedStaticMeshComponent> BuildingMass;

	/** Uma instância por variante da fachada (cada peça tem a própria malha e o próprio comprimento). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Facades;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AsphaltMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> GroundMaterial;
};
