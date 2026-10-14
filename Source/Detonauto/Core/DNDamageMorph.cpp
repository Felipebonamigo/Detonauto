#include "Core/DNDamageMorph.h"

namespace
{
	/** M4: configuração inválida usa o padrão. */
	float Valid(float Value, float Default)
	{
		return (FMath::IsFinite(Value) && Value > 0.0f) ? Value : Default;
	}
}

const TCHAR* FDNDamageMorph::TargetName(int32 Zone)
{
	static const TCHAR* Names[ZoneCount] = { TEXT("DN_Front"), TEXT("DN_Rear"), TEXT("DN_Left"), TEXT("DN_Right"), TEXT("DN_Roof") };
	return (Zone >= 0 && Zone < ZoneCount) ? Names[Zone] : TEXT("");
}

float FDNDamageMorph::Weight(const FDNDamageConfig& Config, float ZoneDamage, float Previous, float DeltaTime)
{
	const FDNDamageConfig Defaults;
	const float Exponent = Valid(Config.MorphCurveExponent, Defaults.MorphCurveExponent);
	const float Rise = Valid(Config.MorphRisePerSecond, Defaults.MorphRisePerSecond);

	const float Before = FMath::IsFinite(Previous) ? FMath::Clamp(Previous, 0.0f, 1.0f) : 0.0f; // M3
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f || !FMath::IsFinite(ZoneDamage))
	{
		return Before;
	}
	const float Target = FMath::Pow(FMath::Clamp(ZoneDamage, 0.0f, 1.0f), Exponent); // M1
	// M2: sobe no máximo Rise x Dt, nunca desce
	return FMath::Max(Before, FMath::Min(Target, Before + Rise * DeltaTime));
}

void FDNDamageMorph::Weights(const FDNDamageConfig& Config, const FDNDamageState& State, float (&InOutWeights)[ZoneCount], float DeltaTime)
{
	for (int32 Zone = 0; Zone < ZoneCount; ++Zone)
	{
		InOutWeights[Zone] = Weight(Config, State.ZoneDamage[Zone], InOutWeights[Zone], DeltaTime);
	}
}
