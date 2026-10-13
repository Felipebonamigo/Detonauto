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
