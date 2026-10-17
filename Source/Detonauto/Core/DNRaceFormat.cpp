#include "Core/DNRaceFormat.h"

FString FDNRaceFormat::Time(float Seconds)
{
	if (!FMath::IsFinite(Seconds) || Seconds < 0.0f)
	{
		return TEXT("--:--.-");
	}
	// décimos inteiros, truncados (F1), presos em 99:59.9 (F2)
	constexpr int64 MaxTenths = 99 * 600 + 59 * 10 + 9;
	const int64 Tenths = FMath::Min(static_cast<int64>(FMath::FloorToDouble(static_cast<double>(Seconds) * 10.0)), MaxTenths);
	const int64 Minutes = Tenths / 600;
	const int64 Rest = Tenths % 600;
	return FString::Printf(TEXT("%lld:%02lld.%lld"), Minutes, Rest / 10, Rest % 10);
}

FString FDNRaceFormat::Place(int32 Position)
{
	if (Position < 1 || Position > 99)
	{
		return TEXT("-");
	}
	return FString::Printf(TEXT("%dº"), Position);
}
