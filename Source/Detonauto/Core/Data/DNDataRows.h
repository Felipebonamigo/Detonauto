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
//   TrafficSettings.csv Id, WakeDistanceM, WakeAheadM, WakeAheadMinClosingKmh, DespawnDistanceM, StillSpeedMs, MaxSpawnPerFrame (uma linha)
//   Turbo.csv           Id + os campos de FDNTurboConfig, na mesma ordem (uma linha, id "padrao")
//   Crash.csv           Id + os campos de FDNCrashConfig (uma linha)
//   Takedown.csv        Id + os campos de FDNTakedownConfig (uma linha)
//
// Ids: só letras sem acento, números, hífen e sublinhado; únicos sem diferenciar maiúsculas de minúsculas (como
// o FName). As referências (RegionId, CityId) também ignoram a caixa. Validação em DNDataValidation.h.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/DNTraffic.h"
#include "Core/DNTurbo.h"
#include "Core/DNCrash.h"
#include "Core/DNTakedown.h"
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

/** Parâmetros do tráfego simplificado (docs/regras-trafego.md). O arquivo tem UMA linha. */
USTRUCT(BlueprintType)
struct FDNTrafficSettingsRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Veículo Physics a esta distância (inclusive) acorda o Rail [m]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WakeDistanceM = 6.0f;

	/** Jogador a menos desta distância (estrito), na frente, acorda o Rail [m]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WakeAheadM = 25.0f;

	/** Velocidade de aproximação mínima (inclusiva) para acordar pela regra da frente [km/h]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WakeAheadMinClosingKmh = 20.0f;

	/** Mais longe que isto (estrito) e atrás do jogador, o veículo some [m]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DespawnDistanceM = 200.0f;

	/** Physics acima desta velocidade (estrito) não some durante a rodada de Crash [m/s]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StillSpeedMs = 2.0f;

	/** Máximo de veículos criados por quadro. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxSpawnPerFrame = 2;

	/** Os mesmos números no tipo puro usado pelas regras de Core/. */
	FDNTrafficSettings ToSettings() const
	{
		FDNTrafficSettings S;
		S.WakeDistanceM = WakeDistanceM;
		S.WakeAheadM = WakeAheadM;
		S.WakeAheadMinClosingKmh = WakeAheadMinClosingKmh;
		S.DespawnDistanceM = DespawnDistanceM;
		S.StillSpeedMs = StillSpeedMs;
		S.MaxSpawnPerFrame = MaxSpawnPerFrame;
		return S;
	}
};
// Linhas de ajuste de pontuação. Os campos têm os MESMOS nomes dos campos das structs puras (FDNTurboConfig,
// FDNCrashConfig, FDNTakedownConfig) e começam zerados: os números de verdade moram em Data/*.csv, e o teste de
// deriva (DNDataConfigs.spec.cpp) garante que o CSV e o padrão da struct pura continuam iguais.

/** Data/Turbo.csv (uma linha). Significado de cada campo: comentários de FDNTurboConfig em Core/DNTurbo.h. */
USTRUCT(BlueprintType)
struct FDNTurboConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxBar = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WrongWayMinSpeedKmh = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WrongWayGainPerSecond = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float NearMissMaxLateralCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float NearMissMinSpeedKmh = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float NearMissGain = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float NearMissChainWindowSeconds = 0.0f;
	/** No CSV: texto de tupla da Unreal entre aspas, por exemplo "(1.0,1.25,1.5)". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<float> NearMissChainMultipliers;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DriftMinAngleDeg = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DriftMaxAngleDeg = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DriftMinSpeedKmh = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DriftGainPerSecond = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AirTimeThresholdSeconds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AirGainPerSecond = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AirMaxGainPerJump = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TakedownSetsBarTo = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ConsumptionPerSecond = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ExtendedBonus = 0.0f;

	FDNTurboConfig ToConfig() const
	{
		FDNTurboConfig C;
		C.MaxBar = MaxBar;
		C.WrongWayMinSpeedKmh = WrongWayMinSpeedKmh;
		C.WrongWayGainPerSecond = WrongWayGainPerSecond;
		C.NearMissMaxLateralCm = NearMissMaxLateralCm;
		C.NearMissMinSpeedKmh = NearMissMinSpeedKmh;
		C.NearMissGain = NearMissGain;
		C.NearMissChainWindowSeconds = NearMissChainWindowSeconds;
		C.NearMissChainMultipliers = NearMissChainMultipliers;
		C.DriftMinAngleDeg = DriftMinAngleDeg;
		C.DriftMaxAngleDeg = DriftMaxAngleDeg;
		C.DriftMinSpeedKmh = DriftMinSpeedKmh;
		C.DriftGainPerSecond = DriftGainPerSecond;
		C.AirTimeThresholdSeconds = AirTimeThresholdSeconds;
		C.AirGainPerSecond = AirGainPerSecond;
		C.AirMaxGainPerJump = AirMaxGainPerJump;
		C.TakedownSetsBarTo = TakedownSetsBarTo;
		C.ConsumptionPerSecond = ConsumptionPerSecond;
		C.ExtendedBonus = ExtendedBonus;
		return C;
	}
};

/** Data/Crash.csv (uma linha). Significado de cada campo: FDNCrashConfig em Core/DNCrash.h. */
USTRUCT(BlueprintType)
struct FDNCrashConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxMultiplier = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChainWindowSeconds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ChainBonusPercentPerLink = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ChainBonusMaxPercent = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float QuietSpeedMetersPerSecond = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float QuietSecondsToEnd = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxSecondsAfterFirstImpact = 0.0f;

	FDNCrashConfig ToConfig() const
	{
		FDNCrashConfig C;
		C.MaxMultiplier = MaxMultiplier;
		C.ChainWindowSeconds = ChainWindowSeconds;
		C.ChainBonusPercentPerLink = ChainBonusPercentPerLink;
		C.ChainBonusMaxPercent = ChainBonusMaxPercent;
		C.QuietSpeedMetersPerSecond = QuietSpeedMetersPerSecond;
		C.QuietSecondsToEnd = QuietSecondsToEnd;
		C.MaxSecondsAfterFirstImpact = MaxSecondsAfterFirstImpact;
		return C;
	}
};

/** Data/Takedown.csv (uma linha). Significado de cada campo: FDNTakedownConfig em Core/DNTakedown.h. */
USTRUCT(BlueprintType)
struct FDNTakedownConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinRelativeSpeedKmh = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RivalCrashWindowSeconds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float VerticalMinHeightCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BehindMaxAngleDegrees = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TBoneMinAngleDegrees = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TBoneMaxAngleDegrees = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SideMaxAngleDegrees = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ZoneHalfAngleDegrees = 0.0f;

	FDNTakedownConfig ToConfig() const
	{
		FDNTakedownConfig C;
		C.MinRelativeSpeedKmh = MinRelativeSpeedKmh;
		C.RivalCrashWindowSeconds = RivalCrashWindowSeconds;
		C.VerticalMinHeightCm = VerticalMinHeightCm;
		C.BehindMaxAngleDegrees = BehindMaxAngleDegrees;
		C.TBoneMinAngleDegrees = TBoneMinAngleDegrees;
		C.TBoneMaxAngleDegrees = TBoneMaxAngleDegrees;
		C.SideMaxAngleDegrees = SideMaxAngleDegrees;
		C.ZoneHalfAngleDegrees = ZoneHalfAngleDegrees;
		return C;
	}
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
using FDNTrafficSettingsEntry = TDNTableEntry<FDNTrafficSettingsRow>;
using FDNTurboConfigEntry = TDNTableEntry<FDNTurboConfigRow>;
using FDNCrashConfigEntry = TDNTableEntry<FDNCrashConfigRow>;
using FDNTakedownConfigEntry = TDNTableEntry<FDNTakedownConfigRow>;

/** Todas as tabelas lidas. */
struct FDNData
{
	TArray<FDNRegionEntry> Regions;
	TArray<FDNCityEntry> Cities;
	TArray<FDNLandmarkEntry> Landmarks;
	TArray<FDNCrossingEntry> Crossings;
	TArray<FDNTrafficVehicleEntry> Vehicles;
	TArray<FDNTrafficSettingsEntry> TrafficSettings;
	TArray<FDNTurboConfigEntry> TurboConfig;
	TArray<FDNCrashConfigEntry> CrashConfig;
	TArray<FDNTakedownConfigEntry> TakedownConfig;
};
