#include "Core/DNTrafficWorld.h"
#include <limits>
#include "Math/RandomStream.h"

namespace
{
	int32 EffectiveOncoming(const FDNTrafficRoadConfig& Config)
	{
		return FMath::Clamp(Config.OncomingLanes, 0, FMath::Max(Config.LaneCount - 1, 0));
	}

	bool IsValid(const FDNTrafficRoadConfig& C)
	{
		return FMath::IsFinite(C.LengthM) && C.LengthM > 0.0f
			&& C.LaneCount >= 1
			&& FMath::IsFinite(C.LaneWidthM) && C.LaneWidthM >= 0.0f
			&& FMath::IsFinite(C.MinGapM) && C.MinGapM > 0.0f
			&& FMath::IsFinite(C.EdgeMarginM) && C.EdgeMarginM >= 0.0f
			&& C.LengthM - 2.0f * C.EdgeMarginM >= 0.0f
			&& FMath::IsFinite(C.MinSpeedKmh) && C.MinSpeedKmh >= 0.0f
			&& FMath::IsFinite(C.MaxSpeedKmh) && C.MaxSpeedKmh >= C.MinSpeedKmh;
	}

	/** Projeção de V no eixo F, em metros (V em cm). F de tamanho zero ou NaN: 0. */
	float ProjectM(const FVector& V, const FVector& Forward)
	{
		const double L2 = Forward.SizeSquared();
		if (!(L2 > 1.0e-12) || !FMath::IsFinite(L2))
		{
			return 0.0f;
		}
		return static_cast<float>(FVector::DotProduct(V, Forward) / FMath::Sqrt(L2) / 100.0);
	}
}

float FDNTrafficRoad::LaneOffsetM(const FDNTrafficRoadConfig& Config, int32 Lane)
{
	return (static_cast<float>(Lane) - static_cast<float>(Config.LaneCount - 1) * 0.5f) * Config.LaneWidthM;
}

bool FDNTrafficRoad::IsOncoming(const FDNTrafficRoadConfig& Config, int32 Lane)
{
	return Lane >= 0 && Lane < Config.LaneCount && Lane >= Config.LaneCount - EffectiveOncoming(Config);
}

TArray<FDNTrafficSlot> FDNTrafficRoad::PlanSlots(const FDNTrafficRoadConfig& Config, int32 Count, int32 Seed)
{
	TArray<FDNTrafficSlot> Slots;
	if (Count <= 0 || !IsValid(Config))
	{
		return Slots;
	}

	const float Usable = Config.LengthM - 2.0f * Config.EdgeMarginM;
	// posições em 0, gap, 2 gap... dentro do comprimento útil: n carros cabem se (n-1) x gap <= útil
	const int32 Capacity = FMath::FloorToInt(Usable / Config.MinGapM) + 1;

	FRandomStream Random(Seed);
	for (int32 Lane = 0; Lane < Config.LaneCount; ++Lane)
	{
		const int32 Share = Count / Config.LaneCount + (Lane < Count % Config.LaneCount ? 1 : 0);
		const int32 N = FMath::Min(Share, Capacity);
		if (N <= 0)
		{
			continue;
		}
		// A folga que sobra depois do espaço mínimo é repartida ao acaso: n sorteios ordenados em [0, folga], somados ao degrau
		// de MinGapM de cada carro. Entre dois carros a distância é MinGapM + (diferença dos sorteios) >= MinGapM.
		const float Slack = Usable - static_cast<float>(N - 1) * Config.MinGapM;
		TArray<float> Draws;
		for (int32 i = 0; i < N; ++i)
		{
			Draws.Add(Random.FRandRange(0.0f, Slack));
		}
		Draws.Sort();
		for (int32 i = 0; i < N; ++i)
		{
			FDNTrafficSlot& Slot = Slots.AddDefaulted_GetRef();
			Slot.Lane = Lane;
			Slot.PositionM = Config.EdgeMarginM + static_cast<float>(i) * Config.MinGapM + Draws[i];
			Slot.SpeedKmh = Random.FRandRange(Config.MinSpeedKmh, Config.MaxSpeedKmh);
			Slot.bOncoming = IsOncoming(Config, Lane);
		}
	}
	Slots.Sort([](const FDNTrafficSlot& A, const FDNTrafficSlot& B) { return A.PositionM < B.PositionM || (A.PositionM == B.PositionM && A.Lane < B.Lane); });
	return Slots;
}

FDNTrafficPool::FAcquired FDNTrafficPool::Acquire()
{
	FAcquired Result;
	for (int32 i = 0; i < InUse.Num(); ++i)
	{
		if (!InUse[i])
		{
			InUse[i] = true;
			Result.Index = i;
			return Result;
		}
	}
	if (InUse.Num() < MaxSize)
	{
		Result.Index = InUse.Add(true);
		Result.bCreated = true;
	}
	return Result;
}

bool FDNTrafficPool::Release(int32 Index)
{
	if (!InUse.IsValidIndex(Index) || !InUse[Index])
	{
		return false;
	}
	InUse[Index] = false;
	return true;
}

int32 FDNTrafficPool::Active() const
{
	int32 Count = 0;
	for (const bool bUsed : InUse) { Count += bUsed ? 1 : 0; }
	return Count;
}

FDNTrafficVehicleInput FDNTrafficGeometry::BuildInput(const FDNTrafficGeometryInput& In)
{
	FDNTrafficVehicleInput Out;
	Out.Level = In.Level;
	Out.ImpactForce = In.ImpactForce;
	Out.NearestPhysicsDistanceM = In.NearestPhysicsDistanceM;
	Out.bCrashRoundActive = In.bCrashRoundActive;
	Out.SpeedMs = static_cast<float>(In.VehicleVelocityCmS.Size() / 100.0);

	if (!In.bHasPlayer)
	{
		// NextLevel trata NaN como "não": nem a regra da frente acorda nem o despawn por distância age
		Out.PlayerDistanceM = std::numeric_limits<float>::quiet_NaN();
		return Out;
	}

	const FVector ToPlayer = In.PlayerPositionCm - In.VehiclePositionCm;
	Out.PlayerDistanceM = static_cast<float>(ToPlayer.Size() / 100.0);
	Out.PlayerProjectionOnVehicleForwardM = ProjectM(ToPlayer, In.VehicleForward);
	Out.VehicleProjectionOnPlayerForwardM = ProjectM(-ToPlayer, In.PlayerForward);

	// aproximação = quanto a distância entre os centros diminui por segundo: componente da velocidade relativa do veículo na
	// direção do jogador (cm/s -> km/h = x 0,036)
	const double Dist = ToPlayer.Size();
	Out.ClosingSpeedKmh = (Dist > 1.0e-6 && FMath::IsFinite(Dist))
		? static_cast<float>(FVector::DotProduct(In.VehicleVelocityCmS - In.PlayerVelocityCmS, ToPlayer / Dist) * 0.036)
		: 0.0f;
	return Out;
}

float FDNTrafficGeometry::NearestDistanceM(const FVector& PositionCm, const TArray<FVector>& OthersCm)
{
	double Best = TNumericLimits<double>::Max();
	for (const FVector& Other : OthersCm)
	{
		const double D = (Other - PositionCm).Size();
		if (FMath::IsFinite(D) && D < Best)
		{
			Best = D;
		}
	}
	return Best == TNumericLimits<double>::Max() ? TNumericLimits<float>::Max() : static_cast<float>(Best / 100.0);
}

namespace
{
	/** Eixo de avanço no plano, unitário (avanço inválido: +X). */
	FVector2D Axis2D(const FVector& Forward)
	{
		const FVector2D F(Forward.X, Forward.Y);
		const double L = F.Size();
		return (L > 1.0e-9 && FMath::IsFinite(L)) ? F / L : FVector2D(1.0, 0.0);
	}

	/** Quanto a caixa avança na direção Dir (unitária) [m]: meio comprimento x |cos| + meia largura x |sen|. */
	double SupportM(const FDNTrafficBody& B, const FVector2D& Dir)
	{
		const FVector2D F = Axis2D(B.Forward);
		const FVector2D R(-F.Y, F.X);
		return FMath::Abs(FVector2D::DotProduct(Dir, F)) * B.HalfLengthM + FMath::Abs(FVector2D::DotProduct(Dir, R)) * B.HalfWidthM;
	}
}

float FDNTrafficGeometry::BoxGapM(const FDNTrafficBody& A, const FDNTrafficBody& B)
{
	const FVector2D D(B.PositionCm.X - A.PositionCm.X, B.PositionCm.Y - A.PositionCm.Y);
	const double Dist = D.Size();
	if (!FMath::IsFinite(Dist))
	{
		return std::numeric_limits<float>::quiet_NaN();
	}
	const FVector2D Dir = Dist > 1.0e-6 ? D / Dist : Axis2D(A.Forward); // mesmo ponto: mede no eixo de A (sempre sobreposto)
	return static_cast<float>(Dist / 100.0 - SupportM(A, Dir) - SupportM(B, Dir));
}

float FDNTrafficGeometry::NearestGapM(const FDNTrafficBody& Self, const TArray<FDNTrafficBody>& Others)
{
	float Best = TNumericLimits<float>::Max();
	for (const FDNTrafficBody& Other : Others)
	{
		const float Gap = BoxGapM(Self, Other);
		if (FMath::IsFinite(Gap) && Gap < Best)
		{
			Best = Gap;
		}
	}
	return Best;
}

FDNTrafficLeader FDNTrafficGeometry::LeaderAhead(const FDNTrafficBody& Self, const TArray<FDNTrafficBody>& Others, float LaneHalfWidthM)
{
	FDNTrafficLeader Leader;
	const FVector2D F = Axis2D(Self.Forward);
	const FVector2D R(-F.Y, F.X);
	for (const FDNTrafficBody& Other : Others)
	{
		const FVector2D D((Other.PositionCm.X - Self.PositionCm.X) / 100.0, (Other.PositionCm.Y - Self.PositionCm.Y) / 100.0);
		const double Along = FVector2D::DotProduct(D, F);
		const double Side = FMath::Abs(FVector2D::DotProduct(D, R));
		if (!(Along > 0.0) || !(Side < LaneHalfWidthM)) // NaN cai fora
		{
			continue;
		}
		const float Gap = static_cast<float>(Along - Self.HalfLengthM - SupportM(Other, F));
		if (Gap < Leader.GapM)
		{
			Leader.GapM = Gap;
			Leader.SpeedMs = static_cast<float>(FVector2D::DotProduct(FVector2D(Other.VelocityCmS.X, Other.VelocityCmS.Y), F) / 100.0);
		}
	}
	return Leader;
}
