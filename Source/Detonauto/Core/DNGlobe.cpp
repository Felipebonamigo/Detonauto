#include "Core/DNGlobe.h"

namespace
{
	constexpr double RadToDeg = 180.0 / UE_DOUBLE_PI;
	constexpr double DegToRad = UE_DOUBLE_PI / 180.0;

	/** Seno e cosseno em graus, exatos nos múltiplos de 90 (0, 1, 0, -1...), para os pontos cardeais saírem limpos. */
	void SinCosDegrees(double Degrees, double& OutSin, double& OutCos)
	{
		const double Quarter = Degrees / 90.0;
		if (Quarter == FMath::FloorToDouble(Quarter))
		{
			const int64 Index = ((static_cast<int64>(Quarter) % 4) + 4) % 4;
			switch (Index)
			{
			case 0: OutSin = 0.0;  OutCos = 1.0;  return;
			case 1: OutSin = 1.0;  OutCos = 0.0;  return;
			case 2: OutSin = 0.0;  OutCos = -1.0; return;
			default: OutSin = -1.0; OutCos = 0.0; return;
			}
		}
		const double Rad = Degrees * DegToRad;
		OutSin = FMath::Sin(Rad);
		OutCos = FMath::Cos(Rad);
	}

	bool IsUsable(const FVector& V)
	{
		const double LengthSquared = V.SizeSquared();
		return FMath::IsFinite(LengthSquared) && LengthSquared > 1.0e-24;
	}

	/** Ângulo em graus no intervalo (-180, 180]. */
	double WrapYaw(double Degrees)
	{
		if (Degrees <= -180.0)
		{
			Degrees += 360.0;
		}
		return Degrees;
	}
}

bool FDNGlobe::IsValidLatLon(double LatitudeDegrees, double LongitudeDegrees)
{
	return FMath::IsFinite(LatitudeDegrees) && FMath::IsFinite(LongitudeDegrees)
		&& LatitudeDegrees >= -90.0 && LatitudeDegrees <= 90.0
		&& LongitudeDegrees >= -180.0 && LongitudeDegrees <= 180.0;
}

FVector FDNGlobe::LatLonToDirection(double LatitudeDegrees, double LongitudeDegrees)
{
	if (!IsValidLatLon(LatitudeDegrees, LongitudeDegrees))
	{
		return FVector::ZeroVector;
	}

	double SinLat, CosLat, SinLon, CosLon;
	SinCosDegrees(LatitudeDegrees, SinLat, CosLat);
	SinCosDegrees(LongitudeDegrees, SinLon, CosLon);
	return FVector(CosLat * CosLon, CosLat * SinLon, SinLat);
}

bool FDNGlobe::DirectionToYawPitch(const FVector& Direction, double& OutYawDegrees, double& OutPitchDegrees)
{
	if (!IsUsable(Direction))
	{
		return false;
	}

	const FVector N = Direction / Direction.Size();
	const double Z = FMath::Clamp(N.Z, -1.0, 1.0);

	// Pitch: ±90 exatos nos polos; senão asin da componente vertical.
	OutPitchDegrees = (Z >= 1.0) ? 90.0 : (Z <= -1.0) ? -90.0 : FMath::Asin(Z) * RadToDeg;

	// Yaw: indefinido nos polos (X e Y praticamente zero), vale 0.
	const bool bPole = FMath::Abs(N.X) < 1.0e-12 && FMath::Abs(N.Y) < 1.0e-12;
	OutYawDegrees = bPole ? 0.0 : WrapYaw(FMath::Atan2(N.Y + 0.0, N.X) * RadToDeg);
	return true;
}

bool FDNGlobe::OrbitCameraYawPitch(const FVector& CityDirection, double& OutYawDegrees, double& OutPitchDegrees)
{
	if (!IsUsable(CityDirection))
	{
		return false;
	}
	// "+ 0.0" troca -0.0 por +0.0, para o yaw de +X sair 180 e não -180.
	const FVector Look(-CityDirection.X + 0.0, -CityDirection.Y + 0.0, -CityDirection.Z + 0.0);
	return DirectionToYawPitch(Look, OutYawDegrees, OutPitchDegrees);
}

bool FDNGlobe::GreatCircleDistanceDegrees(double LatA, double LonA, double LatB, double LonB, double& OutDegrees)
{
	if (!IsValidLatLon(LatA, LonA) || !IsValidLatLon(LatB, LonB))
	{
		return false;
	}

	const FVector A = LatLonToDirection(LatA, LonA);
	const FVector B = LatLonToDirection(LatB, LonB);

	// atan2(|A x B|, A . B) é estável para ângulos pequenos e para quase 180 graus.
	const double Angle = FMath::Atan2(FVector::CrossProduct(A, B).Size(), FVector::DotProduct(A, B)) * RadToDeg;
	OutDegrees = FMath::Clamp(Angle, 0.0, 180.0);
	return true;
}
