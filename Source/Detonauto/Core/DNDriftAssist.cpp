#include "Core/DNDriftAssist.h"

float FDNDriftAssist::SlipAngleDeg(const FVector& LocalVelocityCmS)
{
	if (!FMath::IsFinite(LocalVelocityCmS.X) || !FMath::IsFinite(LocalVelocityCmS.Y))
	{
		return 0.0f;
	}
	if (LocalVelocityCmS.X == 0.0 && LocalVelocityCmS.Y == 0.0)
	{
		return 0.0f;
	}
	return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(LocalVelocityCmS.Y, LocalVelocityCmS.X)));
}

float FDNDriftAssist::YawAcceleration(const FDNDriftAssistConfig& Config, float SlipAngleDeg, float SpeedKmh, float YawRateDegS)
{
	if (!FMath::IsFinite(SlipAngleDeg) || !FMath::IsFinite(SpeedKmh) || !FMath::IsFinite(YawRateDegS)
		|| !FMath::IsFinite(Config.StartDeg) || !FMath::IsFinite(Config.Gain) || !FMath::IsFinite(Config.Max)
		|| !(Config.Gain > 0.0f) || !(Config.Max > 0.0f))
	{
		return 0.0f;
	}
	const float Excess = FMath::Abs(SlipAngleDeg) - Config.StartDeg;
	if (!(Excess > 0.0f))
	{
		return 0.0f;
	}
	const float Direction = SlipAngleDeg > 0.0f ? 1.0f : -1.0f;
	const float Raw = FMath::Min(Config.Gain * Excess, Config.Max);
	const float SpeedFactor = FMath::Clamp((SpeedKmh - MinSpeedKmh) / (FullSpeedKmh - MinSpeedKmh), 0.0f, 1.0f);
	// já girando para a direção certa: empurra menos, e nada a RecoveringYawDegS ou mais
	const float Recovering = FMath::Clamp(YawRateDegS * Direction / RecoveringYawDegS, 0.0f, 1.0f);
	return Direction * Raw * SpeedFactor * (1.0f - Recovering);
}
