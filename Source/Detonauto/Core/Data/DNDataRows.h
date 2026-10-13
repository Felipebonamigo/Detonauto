// Linhas dos CSVs de Data/ (schemas). Cada struct herda de FTableRowBase para a importação como DataTable.
//
// A PRIMEIRA COLUNA de cada CSV é a chave da linha no DataTable (o id) e fica fora da struct. Os nomes das
// demais colunas são os nomes dos campos abaixo. Por isso as linhas lidas viajam em TDNTableEntry (id + linha).
//
// Schemas (primeira linha de cada arquivo):
//   Regions.csv         Id, Name, Order, Unlocked
//   Cities.csv          Id, Name, RegionId, Latitude, Longitude, District, Order, Unlocked, Playable
//   Landmarks.csv       Id, CityId, Name, Kind, ShortText, SourceNote, NeedsReview
//   Crossings.csv       Id, CityId, Order, Bronze, Silver, Gold
//   TrafficVehicles.csv Id, Name, ValueCash, MassKg, Class
//
// Ids: só letras sem acento, números, hífen e sublinhado; únicos sem diferenciar maiúsculas de minúsculas (como
// o FName). As referências (RegionId, CityId) também ignoram a caixa. Validação em DNDataValidation.h.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DNDataRows.generated.h"

USTRUCT(BlueprintType)
struct FDNRegionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	/** Posição na lista de regiões. Maior que zero e sem repetição. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Unlocked = false;
};

USTRUCT(BlueprintType)
struct FDNCityRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	/** Id da região em Regions.csv. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RegionId;

	/** Graus, em [-90, 90]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Latitude = 0.0;

	/** Graus, em [-180, 180]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Longitude = 0.0;

	/** Distrito do jogo desta cidade. Obrigatório quando Playable = true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString District;

	/** Posição dentro da região. Maior que zero e sem repetição na região. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Unlocked = false;

	/** Falso: a cidade aparece no mapa como "em breve" e não entra no total jogável. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Playable = false;
};

USTRUCT(BlueprintType)
struct FDNLandmarkRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Id da cidade em Cities.csv. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString CityId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Kind;

	/** Texto curto. Só entra depois de conferido em fonte confiável: com NeedsReview = true fica vazio. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ShortText;

	/** De onde veio o texto. Obrigatória quando ShortText não está vazio. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SourceNote;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool NeedsReview = true;
};

USTRUCT(BlueprintType)
struct FDNCrossingRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Id da cidade em Cities.csv. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString CityId;

	/** Posição na cidade. Maior que zero e sem repetição na cidade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Order = 0;

	/** Limites das medalhas: maiores que zero e estritamente crescentes (FDNMedals::Validate). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Bronze = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Silver = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Gold = 0;
};

USTRUCT(BlueprintType)
struct FDNTrafficVehicleRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Nome FICTÍCIO: nenhuma marca real. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	/** Valor em dinheiro do jogo, inteiro e maior que zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ValueCash = 0;

	/** Massa em kg, finita e maior que zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MassKg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Class;
};

/** Uma linha lida: o id (primeira coluna) e os campos. O id fica como texto exato do arquivo. */
template <typename RowType>
struct TDNTableEntry
{
	FString Id;
	RowType Row;
};

using FDNRegionEntry = TDNTableEntry<FDNRegionRow>;
using FDNCityEntry = TDNTableEntry<FDNCityRow>;
using FDNLandmarkEntry = TDNTableEntry<FDNLandmarkRow>;
using FDNCrossingEntry = TDNTableEntry<FDNCrossingRow>;
using FDNTrafficVehicleEntry = TDNTableEntry<FDNTrafficVehicleRow>;

/** Todas as tabelas lidas. */
struct FDNData
{
	TArray<FDNRegionEntry> Regions;
	TArray<FDNCityEntry> Cities;
	TArray<FDNLandmarkEntry> Landmarks;
	TArray<FDNCrossingEntry> Crossings;
	TArray<FDNTrafficVehicleEntry> Vehicles;
};
