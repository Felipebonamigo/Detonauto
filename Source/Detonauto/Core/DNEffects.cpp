#include "Core/DNEffects.h"

namespace
{
	const TCHAR* const EffectNames[] = { TEXT("SparkImpact"), TEXT("SparkScrape"), TEXT("GlassShards"), TEXT("PlasticChips"), TEXT("TireSmoke"),
		TEXT("EngineSmoke"), TEXT("EngineFire"), TEXT("PropDust"), TEXT("PropLeaves") };
	static_assert(UE_ARRAY_COUNT(EffectNames) == static_cast<int32>(EDNEffect::PropLeaves) + 1, "um nome por efeito");

	/** Valor finito no limiar ou acima (inclusivo). */
	bool Reaches(float Value, float Threshold)
	{
		return FMath::IsFinite(Value) && Value >= Threshold;
	}

	int32 ClampCount(float Value, int32 Min, int32 Max)
	{
		return FMath::Clamp(FMath::RoundToInt(Value), Min, FMath::Max(Min, Max));
	}
}

const TCHAR* FDNEffects::EffectName(EDNEffect Effect)
{
	const int32 Index = static_cast<int32>(Effect);
	return Index >= 0 && Index < UE_ARRAY_COUNT(EffectNames) ? EffectNames[Index] : TEXT("?");
}

bool FDNEffects::ParseEffect(const FString& Text, EDNEffect& Out)
{
	for (int32 i = 0; i < UE_ARRAY_COUNT(EffectNames); ++i)
	{
		if (Text.TrimStartAndEnd().Equals(EffectNames[i], ESearchCase::IgnoreCase))
		{
			Out = static_cast<EDNEffect>(i);
			return true;
		}
	}
	return false;
}

int32 FDNEffects::ImpactSparks(const FDNEffectSpec& Spec, float DeltaVMs)
{
	// E2: sem o limiar, nada; com ele, CountPerUnit por m/s entre o piso e o teto
	if (!Reaches(DeltaVMs, Spec.TriggerMin) || !FMath::IsFinite(Spec.CountPerUnit))
	{
		return 0;
	}
	return ClampCount(Spec.CountPerUnit * DeltaVMs, Spec.CountMin, Spec.CountMax);
}

float FDNEffects::ScrapeSparkRate(const FDNEffectSpec& Spec, float SpeedMs)
{
	// E3: CountPerUnit partículas por segundo por m/s, até CountMax por segundo
	if (!Reaches(SpeedMs, Spec.TriggerMin) || !FMath::IsFinite(Spec.CountPerUnit))
	{
		return 0.0f;
	}
	return FMath::Clamp(Spec.CountPerUnit * SpeedMs, 0.0f, static_cast<float>(FMath::Max(0, Spec.CountMax)));
}

int32 FDNEffects::GlassShards(const FDNEffectSpec& Spec, float AreaM2)
{
	// E4: pela área do vidro; área inválida fica no mínimo (um vidro que estoura sempre solta cacos)
	const float Area = FMath::IsFinite(AreaM2) && AreaM2 > 0.0f ? AreaM2 : 0.0f;
	return ClampCount(Spec.CountPerUnit * Area, Spec.CountMin, Spec.CountMax);
}

int32 FDNEffects::PlasticChips(const FDNEffectSpec& Spec, float DeltaVMs)
{
	// E5: linear do piso (no limiar) ao teto (em TriggerFull); TriggerFull no limiar ou abaixo: o teto direto
	if (!Reaches(DeltaVMs, Spec.TriggerMin))
	{
		return 0;
	}
	const float Span = Spec.TriggerFull - Spec.TriggerMin;
	const float T = Span > 0.0f ? FMath::Clamp((DeltaVMs - Spec.TriggerMin) / Span, 0.0f, 1.0f) : 1.0f;
	return ClampCount(FMath::Lerp(static_cast<float>(Spec.CountMin), static_cast<float>(Spec.CountMax), T), Spec.CountMin, Spec.CountMax);
}

float FDNEffects::TireSmokeRate(const FDNEffectSpec& Spec, float SlipDeg, float SpeedKmh, bool bWheelSpin)
{
	// E6: abaixo da velocidade mínima, nada; a taxa é o deslizamento sobre o da taxa 1, a partir do limiar; patinando, pelo menos Extra
	if (!Reaches(SpeedKmh, Spec.MinSpeedKmh))
	{
		return 0.0f;
	}
	const float Slip = FMath::IsFinite(SlipDeg) ? FMath::Abs(SlipDeg) : 0.0f;
	float Rate = 0.0f;
	if (Slip >= Spec.TriggerMin && Spec.TriggerFull > 0.0f)
	{
		Rate = FMath::Clamp(Slip / Spec.TriggerFull, 0.0f, 1.0f);
	}
	if (bWheelSpin && FMath::IsFinite(Spec.Extra))
	{
		Rate = FMath::Max(Rate, FMath::Clamp(Spec.Extra, 0.0f, 1.0f));
	}
	return Rate;
}

FDNEngineSmoke FDNEffects::EngineSmoke(const FDNEffectSpec& Smoke, const FDNEffectSpec& Fire, float FrontDamage, bool bDestroyed)
{
	// E7: clara no limiar, escura em TriggerFull, fogo no limiar do fogo ou, destruído, em Extra
	FDNEngineSmoke Out;
	if (!FMath::IsFinite(FrontDamage))
	{
		return Out;
	}
	Out.bSmoke = FrontDamage >= Smoke.TriggerMin;
	Out.bDark = Out.bSmoke && FrontDamage >= Smoke.TriggerFull;
	if (Out.bSmoke)
	{
		const float Span = Smoke.TriggerFull - Smoke.TriggerMin;
		Out.Intensity = Span > 0.0f ? FMath::Clamp((FrontDamage - Smoke.TriggerMin) / Span, 0.0f, 1.0f) : 1.0f;
	}
	Out.bFire = FrontDamage >= Fire.TriggerMin || (bDestroyed && FrontDamage >= Fire.Extra);
	return Out;
}

int32 FDNEffects::FitBudget(int32 Requested, int32 Active, int32 Max)
{
	// E8: o que cabe até o teto
	return FMath::Clamp(Max - Active, 0, FMath::Max(0, Requested));
}
