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

float FDNEffects::SmokeScale(float RequestedSprites, int32 MaxSprites)
{
	// E11: todas as taxas caem na mesma proporção até caber
	if (MaxSprites <= 0)
	{
		return 0.0f;
	}
	if (!FMath::IsFinite(RequestedSprites) || RequestedSprites <= static_cast<float>(MaxSprites))
	{
		return 1.0f;
	}
	return static_cast<float>(MaxSprites) / RequestedSprites;
}

int32 FDNEffectPool::Acquire(int32 Max, double Now, double LifeS, bool& bRecycled)
{
	// E9: vaga livre (o fim já passou) ou, cheio, a que termina primeiro
	const int32 Slots = FMath::Max(1, Max);
	const double End = Now + (FMath::IsFinite(LifeS) && LifeS > 0.0 ? LifeS : 0.0);
	bRecycled = false;
	for (int32 i = 0; i < Ends.Num() && i < Slots; ++i)
	{
		if (Ends[i] <= Now)
		{
			Ends[i] = End;
			return i;
		}
	}
	if (Ends.Num() < Slots)
	{
		return Ends.Add(End);
	}
	int32 First = 0;
	for (int32 i = 1; i < Slots; ++i)
	{
		First = Ends[i] < Ends[First] ? i : First;
	}
	Ends[First] = End;
	bRecycled = true;
	return First;
}

int32 FDNEffectPool::ActiveCount(double Now) const
{
	int32 Count = 0;
	for (const double End : Ends)
	{
		Count += End > Now ? 1 : 0;
	}
	return Count;
}

int32 FDNParticleBudget::Request(int32 Count, int32 Max, double Now, double LifeS)
{
	// E10: o que cabe, contado até o fim da vida do disparo
	Live.RemoveAll([Now](const TPair<double, int32>& L) { return L.Key <= Now; });
	const int32 Granted = FDNEffects::FitBudget(Count, Active(Now), Max);
	if (Granted > 0)
	{
		Live.Add(TPair<double, int32>(Now + (FMath::IsFinite(LifeS) && LifeS > 0.0 ? LifeS : 0.0), Granted));
	}
	return Granted;
}

int32 FDNParticleBudget::Active(double Now) const
{
	int32 Count = 0;
	for (const TPair<double, int32>& L : Live)
	{
		Count += L.Key > Now ? L.Value : 0;
	}
	return Count;
}
