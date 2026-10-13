#include "Core/DNMedals.h"

bool FDNMedals::Validate(const FDNMedalThresholds& Thresholds, FString* OutError)
{
	auto Fail = [OutError](const TCHAR* Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	};

	if (Thresholds.Bronze <= 0)
	{
		return Fail(TEXT("O limite do bronze precisa ser maior que zero."));
	}
	if (Thresholds.Silver <= Thresholds.Bronze)
	{
		return Fail(TEXT("O limite da prata precisa ser maior que o do bronze."));
	}
	if (Thresholds.Gold <= Thresholds.Silver)
	{
		return Fail(TEXT("O limite do ouro precisa ser maior que o da prata."));
	}
	return true;
}

EDNMedal FDNMedals::Evaluate(const FDNMedalThresholds& Thresholds, int32 Score)
{
	if (!Validate(Thresholds))
	{
		return EDNMedal::None;
	}
	if (Score >= Thresholds.Gold)
	{
		return EDNMedal::Gold;
	}
	if (Score >= Thresholds.Silver)
	{
		return EDNMedal::Silver;
	}
	if (Score >= Thresholds.Bronze)
	{
		return EDNMedal::Bronze;
	}
	return EDNMedal::None;
}

const TCHAR* FDNMedals::ToString(EDNMedal Medal)
{
	switch (Medal)
	{
	case EDNMedal::Bronze: return TEXT("bronze");
	case EDNMedal::Silver: return TEXT("prata");
	case EDNMedal::Gold: return TEXT("ouro");
	default: return TEXT("nenhuma");
	}
}
