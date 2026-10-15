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
		R.WreckSteerAccelMs2 = S.WreckSteerAccelMs2;
		R.WreckSteerMaxDeltaVMs = S.WreckSteerMaxDeltaVMs;
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
