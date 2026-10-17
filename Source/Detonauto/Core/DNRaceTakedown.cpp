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
