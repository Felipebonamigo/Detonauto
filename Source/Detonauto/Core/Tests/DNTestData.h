// Apoio dos testes de dados: linhas de configuração válidas, montadas a partir dos padrões das structs puras.

#pragma once

#include "CoreMinimal.h"
#include "Core/Data/DNDataRows.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"

namespace DNTest
{
	inline FDNTurboConfigRow TurboRow(const FDNTurboConfig& C = FDNTurboConfig())
	{
		FDNTurboConfigRow R;
		R.MaxBar = C.MaxBar;
		R.WrongWayMinSpeedKmh = C.WrongWayMinSpeedKmh;
		R.WrongWayGainPerSecond = C.WrongWayGainPerSecond;
		R.NearMissMaxLateralCm = C.NearMissMaxLateralCm;
		R.NearMissMinSpeedKmh = C.NearMissMinSpeedKmh;
		R.NearMissGain = C.NearMissGain;
		R.NearMissChainWindowSeconds = C.NearMissChainWindowSeconds;
		R.NearMissChainMultipliers = C.NearMissChainMultipliers;
		R.DriftMinAngleDeg = C.DriftMinAngleDeg;
		R.DriftMaxAngleDeg = C.DriftMaxAngleDeg;
		R.DriftMinSpeedKmh = C.DriftMinSpeedKmh;
		R.DriftGainPerSecond = C.DriftGainPerSecond;
		R.AirTimeThresholdSeconds = C.AirTimeThresholdSeconds;
		R.AirGainPerSecond = C.AirGainPerSecond;
		R.AirMaxGainPerJump = C.AirMaxGainPerJump;
		R.TakedownSetsBarTo = C.TakedownSetsBarTo;
		R.ConsumptionPerSecond = C.ConsumptionPerSecond;
		R.ExtendedBonus = C.ExtendedBonus;
		return R;
	}

	inline FDNCrashConfigRow CrashRow(const FDNCrashConfig& C = FDNCrashConfig())
	{
		FDNCrashConfigRow R;
		R.MaxMultiplier = C.MaxMultiplier;
		R.ChainWindowSeconds = C.ChainWindowSeconds;
		R.ChainBonusPercentPerLink = C.ChainBonusPercentPerLink;
		R.ChainBonusMaxPercent = C.ChainBonusMaxPercent;
		R.QuietSpeedMetersPerSecond = C.QuietSpeedMetersPerSecond;
		R.QuietSecondsToEnd = C.QuietSecondsToEnd;
		R.MaxSecondsAfterFirstImpact = C.MaxSecondsAfterFirstImpact;
		return R;
	}

	inline FDNTakedownConfigRow TakedownRow(const FDNTakedownConfig& C = FDNTakedownConfig())
	{
		FDNTakedownConfigRow R;
		R.MinRelativeSpeedKmh = C.MinRelativeSpeedKmh;
		R.RivalCrashWindowSeconds = C.RivalCrashWindowSeconds;
		R.VerticalMinHeightCm = C.VerticalMinHeightCm;
		R.BehindMaxAngleDegrees = C.BehindMaxAngleDegrees;
		R.TBoneMinAngleDegrees = C.TBoneMinAngleDegrees;
		R.TBoneMaxAngleDegrees = C.TBoneMaxAngleDegrees;
		R.SideMaxAngleDegrees = C.SideMaxAngleDegrees;
		R.ZoneHalfAngleDegrees = C.ZoneHalfAngleDegrees;
		return R;
	}

	inline FDNDamageConfigRow DamageRow(const FDNDamageConfig& C = FDNDamageConfig())
	{
		FDNDamageConfigRow R;
		R.DentMinDeltaVMs = C.DentMinDeltaVMs;
		R.ZoneDestroyDeltaVMs = C.ZoneDestroyDeltaVMs;
		R.DentMaxDepthCm = C.DentMaxDepthCm;
		R.DentDepthPerMs = C.DentDepthPerMs;
		R.DentBaseRadiusCm = C.DentBaseRadiusCm;
		R.MaxDents = C.MaxDents;
		R.PartDetachDeltaVMs = C.PartDetachDeltaVMs;
		R.PartDetachZoneDamage = C.PartDetachZoneDamage;
		R.DestroyedTotalDamage = C.DestroyedTotalDamage;
		R.MaxActive = C.MaxActive;
		R.MorphCurveExponent = C.MorphCurveExponent;
		R.MorphRisePerSecond = C.MorphRisePerSecond;
		R.WearChipCoverage = C.WearChipCoverage;
		R.WearChipSizeCm = C.WearChipSizeCm;
		R.WearScratch = C.WearScratch;
		R.WearSoot = C.WearSoot;
		return R;
	}

	inline FDNBreakableRow BreakableRow(const FDNBreakableKind& K = FDNBreakableKind())
	{
		FDNBreakableRow R;
		R.MassKg = K.MassKg;
		R.MinBreakSpeedKmh = K.MinBreakSpeedKmh;
		R.LaunchFactor = K.LaunchFactor;
		R.LaunchUpMs = K.LaunchUpMs;
		R.TopplesAtBase = K.bTopplesAtBase;
		R.BreakHeightCm = K.BreakHeightCm;
		R.Points = K.Points;
		return R;
	}

	inline FDNBreakableSettingsRow BreakableSettingsRow(const FDNBreakableSettings& S = FDNBreakableSettings())
	{
		FDNBreakableSettingsRow R;
		R.ArcadeMassFactor = S.ArcadeMassFactor;
		R.MaxSpeedLossPct = S.MaxSpeedLossPct;
		R.MaxLaunchMs = S.MaxLaunchMs;
		R.MaxActiveProps = S.MaxActiveProps;
		R.SleepSpeedMs = S.SleepSpeedMs;
		R.SleepAfterS = S.SleepAfterS;
		R.PlowFactor = S.PlowFactor;
		R.PlowUpMs = S.PlowUpMs;
		return R;
	}

	inline FDNTrafficSettingsRow SettingsRow(const FDNTrafficSettings& C = FDNTrafficSettings())
	{
		FDNTrafficSettingsRow R;
		R.WakeDistanceM = C.WakeDistanceM;
		R.WakeAheadM = C.WakeAheadM;
		R.WakeAheadMinClosingKmh = C.WakeAheadMinClosingKmh;
		R.DespawnDistanceM = C.DespawnDistanceM;
		R.StillSpeedMs = C.StillSpeedMs;
		R.MaxSpawnPerFrame = C.MaxSpawnPerFrame;
		R.FollowMinGapM = C.FollowMinGapM;
		R.FollowCatchUpGapM = C.FollowCatchUpGapM;
		R.RollingFriction = C.RollingFriction;
		return R;
	}

	inline FDNCarConfigRow CarRow(const FDNCarConfig& C = FDNCarConfig())
	{
		FDNCarConfigRow R;
		R.MassKg = C.MassKg;
		R.MaxTorque = C.MaxTorque;
		R.LateralGrip = C.LateralGrip;
		R.SuspensionStiffness = C.SuspensionStiffness;
		R.SteeringGain = C.SteeringGain;
		R.CenterOfMassDropCm = C.CenterOfMassDropCm;
		R.SuspensionDampingRatio = C.SuspensionDampingRatio;
		R.SuspensionTravelCm = C.SuspensionTravelCm;
		R.RollbarScaling = C.RollbarScaling;
		R.RearGripFactor = C.RearGripFactor;
		R.HandbrakeRearGrip = C.HandbrakeRearGrip;
		R.HandbrakeRampInSeconds = C.HandbrakeRampInSeconds;
		R.HandbrakeRampOutSeconds = C.HandbrakeRampOutSeconds;
		R.HandbrakeTorque = C.HandbrakeTorque;
		R.NitroAccelMps2 = C.NitroAccelMps2;
		R.DriftAssistStartDeg = C.DriftAssistStartDeg;
		R.DriftAssistGain = C.DriftAssistGain;
		R.DriftAssistMax = C.DriftAssistMax;
		return R;
	}

	inline FDNCameraConfigRow CameraRow(const FDNCameraConfig& C = FDNCameraConfig())
	{
		FDNCameraConfigRow R;
		R.Anchor = C.Anchor == EDNCameraAnchor::Car ? TEXT("Car") : TEXT("Chase");
		R.DistanceCm = C.DistanceCm;
		R.HeightCm = C.HeightCm;
		R.ForwardCm = C.ForwardCm;
		R.SideCm = C.SideCm;
		R.PitchDeg = C.PitchDeg;
		R.Fov = C.Fov;
		R.Smoothing = C.Smoothing;
		R.SpeedFovScale = C.SpeedFovScale;
		R.ShakeScale = C.ShakeScale;
		return R;
	}

	inline FDNCameraFxRow CameraFxRow(const FDNCameraFxSettings& S = FDNCameraFxSettings())
	{
		FDNCameraFxRow R;
		R.SpeedFovStartKmh = S.SpeedFovStartKmh;
		R.SpeedFovFullKmh = S.SpeedFovFullKmh;
		R.SpeedFovMaxAddDeg = S.SpeedFovMaxAddDeg;
		R.TurboFovAddDeg = S.TurboFovAddDeg;
		R.MaxFovDeg = S.MaxFovDeg;
		R.FovBlendDegPerS = S.FovBlendDegPerS;
		R.ShakeStartKmh = S.ShakeStartKmh;
		R.ShakeFullKmh = S.ShakeFullKmh;
		R.ShakeMaxDeg = S.ShakeMaxDeg;
		R.ShakeHz = S.ShakeHz;
		R.EdgeBlurStartKmh = S.EdgeBlurStartKmh;
		R.EdgeBlurFullKmh = S.EdgeBlurFullKmh;
		R.EdgeBlurMax = S.EdgeBlurMax;
		R.EdgeBlurInnerRadius = S.EdgeBlurInnerRadius;
		R.CrashCamMinDeltaVMs = S.CrashCamMinDeltaVMs;
		R.CrashCamTimeScale = S.CrashCamTimeScale;
		R.CrashCamEnterS = S.CrashCamEnterS;
		R.CrashCamHoldS = S.CrashCamHoldS;
		R.CrashCamExitS = S.CrashCamExitS;
		R.CrashCamCooldownS = S.CrashCamCooldownS;
		R.CrashCamOrbitRadiusM = S.CrashCamOrbitRadiusM;
		R.CrashCamOrbitHeightM = S.CrashCamOrbitHeightM;
		R.CrashCamOrbitDegPerS = S.CrashCamOrbitDegPerS;
		R.CrashCamProbeCm = S.CrashCamProbeCm;
		R.CrashCamLeadDeg = S.CrashCamLeadDeg;
		R.WreckSteerAccelMs2 = S.WreckSteerAccelMs2;
		R.WreckSteerMaxDeltaVMs = S.WreckSteerMaxDeltaVMs;
		return R;
	}

	/** Uma linha válida de CarParts.csv (lote 18b): por padrão, o para-choque dianteiro do contrato. */
	inline FDNCarPartRow CarPartRow(const TCHAR* Part = TEXT("BumperFront"), const TCHAR* Zone = TEXT("Front"), const TCHAR* Secondary = TEXT(""),
		const TCHAR* Edge = TEXT("Lateral"), const TCHAR* CarId = TEXT("SportsCar"))
	{
		FDNCarPartRow R;
		R.CarId = CarId;
		R.Part = Part;
		R.Zone = Zone;
		R.SecondaryZone = Secondary;
		R.MassKg = 8.0f;
		R.HangDamage = 0.30f;
		R.HangDeltaVMs = 8.0f;
		R.DetachDamage = 0.60f;
		R.DetachDeltaVMs = 12.0f;
		R.HingeEdge = Edge;
		R.HingeMaxDeg = 60.0f;
		R.HingeDamping = 2.0f;
		R.LaunchMs = 4.0f;
		return R;
	}

	/** Uma linha válida de CarGlass.csv (lote 18b): por padrão, o vidro lateral esquerdo do contrato. */
	inline FDNCarGlassRow CarGlassRow(const TCHAR* Glass = TEXT("SideLeft"), const TCHAR* Zone = TEXT("Left"), const TCHAR* Secondary = TEXT(""),
		const TCHAR* CarId = TEXT("SportsCar"))
	{
		FDNCarGlassRow R;
		R.CarId = CarId;
		R.Glass = Glass;
		R.Zone = Zone;
		R.SecondaryZone = Secondary;
		R.CrackFrom = 0.30f;
		R.BurstDamage = 0.60f;
		R.BurstDeltaVMs = 12.0f;
		return R;
	}

	template <typename EntryType, typename RowType>
	EntryType Entry(const RowType& Row, const TCHAR* Id = TEXT("padrao"))
	{
		EntryType E;
		E.Id = Id;
		E.Row = Row;
		return E;
	}

	/** Uma linha de Effects.csv (lote 19). */
	inline FDNEffectRow EffectRow(const TCHAR* Effect, float TriggerMin, float TriggerFull, float MinSpeedKmh, int32 CountMin, int32 CountMax,
		float PerUnit, float Extra, float LifeMin, float LifeMax, float SpeedMin, float SpeedMax, int32 Pool, const TCHAR* Props = TEXT(""))
	{
		FDNEffectRow R;
		R.Effect = Effect;
		R.TriggerMin = TriggerMin;
		R.TriggerFull = TriggerFull;
		R.MinSpeedKmh = MinSpeedKmh;
		R.CountMin = CountMin;
		R.CountMax = CountMax;
		R.CountPerUnit = PerUnit;
		R.Extra = Extra;
		R.LifeMinS = LifeMin;
		R.LifeMaxS = LifeMax;
		R.SpeedMinMs = SpeedMin;
		R.SpeedMaxMs = SpeedMax;
		R.PoolMax = Pool;
		R.Props = Props;
		return R;
	}

	/** A tabela do contrato de Effects.csv (docs/regras-efeitos.md e as decisões E2 a E7), uma linha por efeito, na ordem do enum. */
	inline TArray<FDNEffectEntry> EffectEntries()
	{
		return {
			Entry<FDNEffectEntry>(EffectRow(TEXT("SparkImpact"), 4.f, 0.f, 0.f, 10, 120, 6.f, 0.f, 0.3f, 0.8f, 4.f, 12.f, 8), TEXT("faisca-impacto")),
			Entry<FDNEffectEntry>(EffectRow(TEXT("SparkScrape"), 3.f, 0.f, 0.f, 0, 300, 15.f, 0.f, 0.2f, 0.5f, 2.f, 8.f, 8), TEXT("faisca-raspao")),
			Entry<FDNEffectEntry>(EffectRow(TEXT("GlassShards"), 0.f, 0.f, 0.f, 60, 150, 250.f, 20.f, 1.5f, 1.5f, 1.f, 4.f, 4), TEXT("cacos-vidro")),
			Entry<FDNEffectEntry>(EffectRow(TEXT("PlasticChips"), 12.f, 30.f, 0.f, 10, 30, 0.f, 0.f, 1.f, 2.f, 3.f, 9.f, 4), TEXT("lascas-plastico")),
			Entry<FDNEffectEntry>(EffectRow(TEXT("TireSmoke"), 8.f, 25.f, 20.f, 0, 30, 0.f, 0.6f, 2.f, 4.f, 0.5f, 1.5f, 4), TEXT("fumaca-pneu")),
			Entry<FDNEffectEntry>(EffectRow(TEXT("EngineSmoke"), 0.6f, 0.8f, 0.f, 0, 20, 0.f, 0.f, 3.f, 5.f, 0.5f, 1.5f, 6), TEXT("fumaca-motor")),
			Entry<FDNEffectEntry>(EffectRow(TEXT("EngineFire"), 0.9f, 0.f, 0.f, 0, 30, 0.f, 0.7f, 0.4f, 0.8f, 1.f, 3.f, 6), TEXT("fogo")),
			Entry<FDNEffectEntry>(EffectRow(TEXT("PropDust"), 0.f, 0.f, 0.f, 20, 40, 0.f, 0.f, 1.f, 2.f, 0.5f, 2.f, 4, TEXT("poste;arvore")), TEXT("po-mobiliario")),
			Entry<FDNEffectEntry>(EffectRow(TEXT("PropLeaves"), 0.f, 0.f, 0.f, 30, 40, 0.f, 0.f, 3.f, 5.f, 0.5f, 2.f, 4, TEXT("arvore")), TEXT("folhas")) };
	}

	/** Os tipos de peça de rua citados por Effects.csv (poste e arvore), para a validação das referências. */
	inline TArray<FDNBreakableEntry> EffectProps()
	{
		return { Entry<FDNBreakableEntry>(BreakableRow(), TEXT("poste")), Entry<FDNBreakableEntry>(BreakableRow(), TEXT("arvore")) };
	}

	inline FDNEffectsSettingsRow EffectsSettingsRow(const FDNEffectsSettings& S = FDNEffectsSettings())
	{
		FDNEffectsSettingsRow R;
		R.MaxGpuParticles = S.MaxGpuParticles;
		R.MaxSmokeSprites = S.MaxSmokeSprites;
		R.SmokeFadeNearCm = S.SmokeFadeNearCm;
		R.SmokeFadeFarCm = S.SmokeFadeFarCm;
		return R;
	}

	/** Um ponto de CoursePoints.csv. */
	inline FDNCoursePointEntry CoursePoint(const TCHAR* Course, int32 Order, float X, float Y, float Width = 1400.f)
	{
		FDNCoursePointRow R;
		R.Course = Course;
		R.Order = Order;
		R.XCm = X;
		R.YCm = Y;
		R.WidthCm = Width;
		return Entry<FDNCoursePointEntry>(R, *FString::Printf(TEXT("%s-p%d"), Course, Order));
	}

	/** Um percurso válido em L (1000 cm para +X, 500 cm para +Y), fora de ordem de propósito no arquivo. */
	inline TArray<FDNCoursePointEntry> CourseEntries()
	{
		return { CoursePoint(TEXT("rua"), 2, 1000.f, 500.f), CoursePoint(TEXT("rua"), 0, 0.f, 0.f), CoursePoint(TEXT("rua"), 1, 1000.f, 0.f) };
	}

	inline FDNTurboConfigEntry TurboEntry(const TCHAR* Id = TEXT("padrao")) { return Entry<FDNTurboConfigEntry>(TurboRow(), Id); }
	inline FDNCrashConfigEntry CrashEntry(const TCHAR* Id = TEXT("padrao")) { return Entry<FDNCrashConfigEntry>(CrashRow(), Id); }
	inline FDNTakedownConfigEntry TakedownEntry(const TCHAR* Id = TEXT("padrao")) { return Entry<FDNTakedownConfigEntry>(TakedownRow(), Id); }
	inline FDNDamageConfigEntry DamageEntry(const TCHAR* Id = TEXT("padrao")) { return Entry<FDNDamageConfigEntry>(DamageRow(), Id); }
	inline FDNCameraConfigEntry CameraEntry(const TCHAR* Id = TEXT("alta-longe")) { return Entry<FDNCameraConfigEntry>(CameraRow(), Id); }
	inline FDNCarConfigEntry CarEntry(const TCHAR* Id = TEXT("Padrao")) { return Entry<FDNCarConfigEntry>(CarRow(), Id); }

	/**
	 *  Subsistema de GameInstance sem mundo: a classe exige um UGameInstance como dono (ClassWithin), então cada um ganha
	 *  o seu. Os donos ficam segurados até o fim da sessão de testes para o coletor de lixo não levar o objeto no meio.
	 */
	template <typename SubsystemType>
	SubsystemType* NewSubsystem()
	{
		static TArray<TStrongObjectPtr<UGameInstance>> Owners;
		UGameInstance* Owner = NewObject<UGameInstance>(GetTransientPackage());
		Owners.Add(TStrongObjectPtr<UGameInstance>(Owner));
		return NewObject<SubsystemType>(Owner);
	}
}
