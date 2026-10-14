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
//   TrafficSettings.csv Id, WakeDistanceM, WakeAheadM, WakeAheadMinClosingKmh, DespawnDistanceM, StillSpeedMs, MaxSpawnPerFrame,
//                       FollowMinGapM, FollowCatchUpGapM, RollingFriction (uma linha)
//   Turbo.csv           Id + os campos de FDNTurboConfig, na mesma ordem (uma linha, id "padrao")
//   Crash.csv           Id + os campos de FDNCrashConfig (uma linha)
//   Takedown.csv        Id + os campos de FDNTakedownConfig (uma linha)
//   Damage.csv          Id + os campos de FDNDamageConfig (uma linha)
//   Cars.csv            Id + os campos de FDNCarConfig (uma linha por preset; "Padrao" obrigatória)
//   Cameras.csv         Id + Anchor + os campos de FDNCameraConfig (um modo por linha, na ordem da tecla C; "alta-longe" obrigatória)
//   Breakables.csv      Id, MassKg, MinBreakSpeedKmh, LaunchFactor, LaunchUpMs, TopplesAtBase, BreakHeightCm, Points (um tipo de peça
//                       de rua por linha)
//   BreakableSettings.csv Id + os campos de FDNBreakableSettings (uma linha, id "padrao")
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
#include "Core/DNDamage.h"
#include "Core/DNCar.h"
#include "Core/DNCamera.h"
#include "Core/DNBreakable.h"
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

	/** Folga mínima atrás do carro da frente na mesma faixa [m]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FollowMinGapM = 8.0f;

	/** Folga a partir da qual o carro em trilho volta à própria velocidade [m]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FollowCatchUpGapM = 20.0f;

	/** Atrito do carro acordado que ainda "dirige" (rolando), até a primeira batida com dano. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RollingFriction = 0.02f;

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
		S.FollowMinGapM = FollowMinGapM;
		S.FollowCatchUpGapM = FollowCatchUpGapM;
		S.RollingFriction = RollingFriction;
		return S;
	}
};
// Linhas de ajuste de pontuação. Os campos têm os MESMOS nomes dos campos das structs puras (FDNTurboConfig,
// FDNCrashConfig, FDNTakedownConfig) e começam zerados: os números de verdade moram em Data/*.csv, e o teste de
// deriva (DNDataConfigs.spec.cpp) garante que o CSV e o padrão da struct pura continuam iguais.
// AO ACRESCENTAR UM CAMPO a FDNTurboConfig / FDNCrashConfig / FDNTakedownConfig: acrescente a coluna no CSV, o campo
// na struct de linha abaixo, a linha em ToConfig(), a regra em DNDataValidation e a comparação no teste de deriva.

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

/** Data/Damage.csv (uma linha). Significado de cada campo: FDNDamageConfig em Core/DNDamage.h. */
USTRUCT(BlueprintType)
struct FDNDamageConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DentMinDeltaVMs = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ZoneDestroyDeltaVMs = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DentMaxDepthCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DentDepthPerMs = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DentBaseRadiusCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxDents = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PartDetachDeltaVMs = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PartDetachZoneDamage = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DestroyedTotalDamage = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxActive = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MorphCurveExponent = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MorphRisePerSecond = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WearChipCoverage = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WearChipSizeCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WearScratch = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WearSoot = 0.0f;

	FDNDamageConfig ToConfig() const
	{
		FDNDamageConfig C;
		C.DentMinDeltaVMs = DentMinDeltaVMs;
		C.ZoneDestroyDeltaVMs = ZoneDestroyDeltaVMs;
		C.DentMaxDepthCm = DentMaxDepthCm;
		C.DentDepthPerMs = DentDepthPerMs;
		C.DentBaseRadiusCm = DentBaseRadiusCm;
		C.MaxDents = MaxDents;
		C.PartDetachDeltaVMs = PartDetachDeltaVMs;
		C.PartDetachZoneDamage = PartDetachZoneDamage;
		C.DestroyedTotalDamage = DestroyedTotalDamage;
		C.MaxActive = MaxActive;
		C.MorphCurveExponent = MorphCurveExponent;
		C.MorphRisePerSecond = MorphRisePerSecond;
		C.WearChipCoverage = WearChipCoverage;
		C.WearChipSizeCm = WearChipSizeCm;
		C.WearScratch = WearScratch;
		C.WearSoot = WearSoot;
		return C;
	}
};

/** Data/Cars.csv (uma linha por preset; a linha "Padrao" é obrigatória). Significado de cada campo: FDNCarConfig em Core/DNCar.h. */
USTRUCT(BlueprintType)
struct FDNCarConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MassKg = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxTorque = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LateralGrip = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SuspensionStiffness = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SteeringGain = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CenterOfMassDropCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SuspensionDampingRatio = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SuspensionTravelCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RollbarScaling = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RearGripFactor = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HandbrakeRearGrip = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HandbrakeRampInSeconds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HandbrakeRampOutSeconds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HandbrakeTorque = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float NitroAccelMps2 = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DriftAssistStartDeg = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DriftAssistGain = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DriftAssistMax = 0.0f;

	FDNCarConfig ToConfig() const
	{
		FDNCarConfig C;
		C.MassKg = MassKg;
		C.MaxTorque = MaxTorque;
		C.LateralGrip = LateralGrip;
		C.SuspensionStiffness = SuspensionStiffness;
		C.SteeringGain = SteeringGain;
		C.CenterOfMassDropCm = CenterOfMassDropCm;
		C.SuspensionDampingRatio = SuspensionDampingRatio;
		C.SuspensionTravelCm = SuspensionTravelCm;
		C.RollbarScaling = RollbarScaling;
		C.RearGripFactor = RearGripFactor;
		C.HandbrakeRearGrip = HandbrakeRearGrip;
		C.HandbrakeRampInSeconds = HandbrakeRampInSeconds;
		C.HandbrakeRampOutSeconds = HandbrakeRampOutSeconds;
		C.HandbrakeTorque = HandbrakeTorque;
		C.NitroAccelMps2 = NitroAccelMps2;
		C.DriftAssistStartDeg = DriftAssistStartDeg;
		C.DriftAssistGain = DriftAssistGain;
		C.DriftAssistMax = DriftAssistMax;
		return C;
	}
};

/** Data/Cameras.csv (um modo de câmera por linha; "alta-longe" é obrigatória). Significado de cada campo: FDNCameraConfig em Core/DNCamera.h. */
USTRUCT(BlueprintType)
struct FDNCameraConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	/** "Chase" (câmera de perseguição) ou "Car" (presa ao carro). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Anchor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DistanceCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HeightCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ForwardCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SideCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PitchDeg = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Fov = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Smoothing = 0.0f;

	/** Âncora desconhecida cai em Chase (a validação já recusou o arquivo antes). */
	FDNCameraConfig ToConfig() const
	{
		FDNCameraConfig C;
		FDNCameraCycle::ParseAnchor(Anchor, C.Anchor);
		C.DistanceCm = DistanceCm;
		C.HeightCm = HeightCm;
		C.ForwardCm = ForwardCm;
		C.SideCm = SideCm;
		C.PitchDeg = PitchDeg;
		C.Fov = Fov;
		C.Smoothing = Smoothing;
		return C;
	}
};

/** Data/Breakables.csv: uma linha por tipo de peça de rua (poste, arvore, banco...). Significado de cada campo:
 *  FDNBreakableKind em Core/DNBreakable.h; contrato em docs/regras-mobiliario.md. */
USTRUCT(BlueprintType)
struct FDNBreakableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MassKg = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinBreakSpeedKmh = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LaunchFactor = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LaunchUpMs = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool TopplesAtBase = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BreakHeightCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Points = 0;

	FDNBreakableKind ToKind() const
	{
		FDNBreakableKind K;
		K.MassKg = MassKg;
		K.MinBreakSpeedKmh = MinBreakSpeedKmh;
		K.LaunchFactor = LaunchFactor;
		K.LaunchUpMs = LaunchUpMs;
		K.bTopplesAtBase = TopplesAtBase;
		K.BreakHeightCm = BreakHeightCm;
		K.Points = Points;
		return K;
	}
};

/** Data/BreakableSettings.csv: os valores globais do mobiliário destrutível (uma linha, id "padrao"). Campos: FDNBreakableSettings. */
USTRUCT(BlueprintType)
struct FDNBreakableSettingsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ArcadeMassFactor = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxSpeedLossPct = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxLaunchMs = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxActiveProps = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SleepSpeedMs = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SleepAfterS = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float IgnoreCarS = 0.0f;

	FDNBreakableSettings ToSettings() const
	{
		FDNBreakableSettings S;
		S.ArcadeMassFactor = ArcadeMassFactor;
		S.MaxSpeedLossPct = MaxSpeedLossPct;
		S.MaxLaunchMs = MaxLaunchMs;
		S.MaxActiveProps = MaxActiveProps;
		S.SleepSpeedMs = SleepSpeedMs;
		S.SleepAfterS = SleepAfterS;
		S.IgnoreCarS = IgnoreCarS;
		return S;
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
using FDNDamageConfigEntry = TDNTableEntry<FDNDamageConfigRow>;
using FDNCarConfigEntry = TDNTableEntry<FDNCarConfigRow>;
using FDNCameraConfigEntry = TDNTableEntry<FDNCameraConfigRow>;
using FDNBreakableEntry = TDNTableEntry<FDNBreakableRow>;
using FDNBreakableSettingsEntry = TDNTableEntry<FDNBreakableSettingsRow>;

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
	TArray<FDNDamageConfigEntry> DamageConfig;
	TArray<FDNCarConfigEntry> CarConfigs;
	TArray<FDNCameraConfigEntry> CameraModes;
	TArray<FDNBreakableEntry> Breakables;
	TArray<FDNBreakableSettingsEntry> BreakableSettings;
};
