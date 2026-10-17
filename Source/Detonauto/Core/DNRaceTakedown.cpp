#include "Core/DNRaceTakedown.h"

bool FDNRaceTakedown::RivalCrashed(float DamageBefore, float DamageNow, bool bDestroyed, float MinGain)
{
	if (bDestroyed)
	{
		return true;
	}
	if (!FMath::IsFinite(DamageBefore) || !FMath::IsFinite(DamageNow) || !FMath::IsFinite(MinGain))
	{
		return false;
	}
	return DamageNow - DamageBefore >= MinGain;
}

bool FDNRaceTakedown::Step(FDNWreckState& State, float DeltaS, bool bWreckNow, float WreckSeconds)
{
	// W3: já destroçado, um novo takedown não reinicia a contagem
	if (!State.bWrecked)
	{
		if (!bWreckNow)
		{
			return false;
		}
		State.bWrecked = true;
		State.LeftS = FMath::IsFinite(WreckSeconds) ? FMath::Max(WreckSeconds, 0.0f) : 0.0f;
		return false;
	}
	// W4: só tempo válido e positivo anda a contagem
	if (FMath::IsFinite(DeltaS) && DeltaS > 0.0f)
	{
		State.LeftS -= DeltaS;
		if (State.LeftS <= 0.0f)
		{
			State = FDNWreckState();
			return true;
		}
	}
	return false;
}

bool FDNRaceTakedown::PlayerDestroyedFirst(float PlayerDestroyedAtS, float RivalCrashedAtS)
{
	if (!FMath::IsFinite(PlayerDestroyedAtS) || !FMath::IsFinite(RivalCrashedAtS) || PlayerDestroyedAtS < 0.0f)
	{
		return false;
	}
	return PlayerDestroyedAtS < RivalCrashedAtS;
}

float FDNRaceTakedown::AngerStep(float LeftS, float DeltaS, bool bTouched, float DurationS)
{
	const float Duration = FMath::IsFinite(DurationS) ? FMath::Max(DurationS, 0.0f) : 0.0f;
	if (bTouched)
	{
		return Duration;
	}
	if (!FMath::IsFinite(LeftS))
	{
		return 0.0f;
	}
	if (FMath::IsFinite(DeltaS) && DeltaS > 0.0f)
	{
		return FMath::Max(LeftS - DeltaS, 0.0f);
	}
	return FMath::Max(LeftS, 0.0f);
}
