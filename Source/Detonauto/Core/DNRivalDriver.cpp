#include "Core/DNRivalDriver.h"

FDNDriveCommand FDNRivalDriver::Command(float NearErrorDeg, float FarErrorDeg, float SpeedKmh, const FDNRivalConfig& Config)
{
	FDNDriveCommand Out;
	if (!FMath::IsFinite(NearErrorDeg) || !FMath::IsFinite(FarErrorDeg) || !FMath::IsFinite(SpeedKmh))
	{
		return Out;
	}
	const float Kmh = FMath::Max(SpeedKmh, 0.0f);
	const float Gain = FMath::Clamp(15.0f / FMath::Max(Kmh / 3.6f, 1.0f), 0.2f, 1.0f);
	Out.Steer = FMath::Clamp(NearErrorDeg / Config.SteerFullDeg * Gain, -1.0f, 1.0f);

	const float Corner = FMath::Clamp(FMath::Abs(FarErrorDeg) / Config.CornerFullDeg, 0.0f, 1.0f);
	const float Target = FMath::Lerp(Config.StraightKmh, Config.CornerKmh, Corner);
	Out.Throttle = FMath::Clamp(0.4f + (Target - Kmh) * 0.08f, 0.0f, 1.0f);
	Out.Brake = Kmh > Target + 10.0f ? FMath::Clamp((Kmh - Target) * 0.04f, 0.0f, 1.0f) : 0.0f;
	return Out;
}

float FDNRivalDriver::LookAheadCm(float SpeedKmh, const FDNRivalConfig& Config)
{
	const float Kmh = FMath::IsFinite(SpeedKmh) ? FMath::Max(SpeedKmh, 0.0f) : 0.0f;
	return Config.LookAheadMinCm + Kmh * Config.LookAheadPerKmhCm;
}
