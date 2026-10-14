#include "Core/DNTraffic.h"

namespace
{
	bool IsFiniteAll(float A, float B, float C, float D)
	{
		return FMath::IsFinite(A) && FMath::IsFinite(B) && FMath::IsFinite(C) && FMath::IsFinite(D);
	}
}

EDNTrafficLevel FDNTraffic::NextLevel(const FDNTrafficSettings& Settings, const FDNTrafficVehicleInput& Vehicle)
{
	if (Vehicle.Level == EDNTrafficLevel::Despawn)
	{
		return EDNTrafficLevel::Despawn;
	}

	EDNTrafficLevel Level = Vehicle.Level;

	// 1. Acordar: só o Rail muda; comparações com NaN dão falso e não acordam.
	if (Level == EDNTrafficLevel::Rail)
	{
		const bool bHit = Vehicle.ImpactForce > 0.0f;
		const bool bPhysicsNear = Vehicle.NearestPhysicsDistanceM <= Settings.WakeDistanceM;
		const bool bPlayerAhead = Vehicle.PlayerDistanceM < Settings.WakeAheadM
			&& Vehicle.PlayerProjectionOnVehicleForwardM > 0.0f
			&& Vehicle.ClosingSpeedKmh >= Settings.WakeAheadMinClosingKmh;
		if (bHit || bPhysicsNear || bPlayerAhead)
		{
			Level = EDNTrafficLevel::Physics;
		}
	}

	// 2. Despawn, sobre o nível resultante.
	const bool bFarBehind = Vehicle.PlayerDistanceM > Settings.DespawnDistanceM && Vehicle.VehicleProjectionOnPlayerForwardM < 0.0f;
	if (bFarBehind)
	{
		// Velocidade NaN conta como em movimento: na dúvida, o veículo não some durante a rodada.
		const bool bMoving = !FMath::IsFinite(Vehicle.SpeedMs) || FMath::Abs(Vehicle.SpeedMs) > Settings.StillSpeedMs;
		const bool bProtected = Level == EDNTrafficLevel::Physics && Vehicle.bCrashRoundActive && bMoving;
		if (!bProtected)
		{
			return EDNTrafficLevel::Despawn;
		}
	}
	return Level;
}

FDNRailAdvance FDNTraffic::AdvanceOnRail(float Distance, float Speed, float DeltaTime, float TrackLength, bool bLoop)
{
	FDNRailAdvance Result;
	Result.Distance = Distance;

	if (!IsFiniteAll(Distance, Speed, DeltaTime, TrackLength) || TrackLength <= 0.0f)
	{
		Result.bError = true;
		return Result;
	}
	if (!bLoop && (Distance < 0.0f || Distance > TrackLength))
	{
		Result.bError = true;
		return Result;
	}

	const bool bMoves = DeltaTime > 0.0f && Speed > 0.0f;
	const double Length = TrackLength;
	const double Target = static_cast<double>(Distance) + (bMoves ? static_cast<double>(Speed) * static_cast<double>(DeltaTime) : 0.0);

	if (!bLoop)
	{
		// Depois do arredondamento para float o valor pode igualar o comprimento: nesse caso também chegou ao fim.
		const float AsFloat = static_cast<float>(Target);
		if (bMoves && (Target >= Length || AsFloat >= TrackLength))
		{
			Result.Distance = TrackLength;
			Result.bReachedEnd = true;
		}
		else
		{
			Result.Distance = bMoves ? AsFloat : Distance;
		}
		return Result;
	}

	// Com loop: módulo em precisão dupla; o resultado fica em [0, TrackLength) mesmo depois do arredondamento para float.
	double Wrapped = FMath::Fmod(Target, Length);
	if (Wrapped < 0.0)
	{
		Wrapped += Length;
	}
	float AsFloat = static_cast<float>(Wrapped);
	if (!(AsFloat >= 0.0f && AsFloat < TrackLength))
	{
		AsFloat = 0.0f;
	}
	Result.Distance = AsFloat;
	return Result;
}

float FDNTraffic::FollowSpeed(const FDNTrafficSettings& Settings, float DesiredMs, float GapM, float LeaderSpeedMs)
{
	if (!FMath::IsFinite(DesiredMs) || DesiredMs <= 0.0f)
	{
		return 0.0f;
	}
	if (!FMath::IsFinite(GapM) || GapM >= Settings.FollowCatchUpGapM)
	{
		return DesiredMs; // sem líder, ou longe dele
	}
	if (GapM <= 0.0f)
	{
		return 0.0f;
	}
	const float Leader = FMath::Min(FMath::IsFinite(LeaderSpeedMs) ? FMath::Max(LeaderSpeedMs, 0.0f) : 0.0f, DesiredMs);
	const float MinGap = Settings.FollowMinGapM;
	if (GapM <= MinGap)
	{
		return Leader * GapM / MinGap;
	}
	const float Blend = (GapM - MinGap) / (Settings.FollowCatchUpGapM - MinGap);
	return Leader + (DesiredMs - Leader) * Blend;
}

int32 FDNTraffic::PlanSpawns(int32 Target, int32 Active, int32 MaxPerFrame)
{
	const int64 Missing = static_cast<int64>(FMath::Max(Target, 0)) - static_cast<int64>(FMath::Max(Active, 0));
	const int64 Limit = FMath::Max(MaxPerFrame, 0);
	return static_cast<int32>(FMath::Clamp<int64>(Missing, 0, Limit));
}
