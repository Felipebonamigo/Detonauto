#include "Core/DNCrash.h"

namespace
{
	/** Tempo inválido (negativo, zero ou NaN) não anda o relógio. */
	float SanitizeDelta(float DeltaTime)
	{
		return (FMath::IsFinite(DeltaTime) && DeltaTime > 0.0f) ? DeltaTime : 0.0f;
	}
}

int32 FDNCrash::ComputePoints(const FDNCrashConfig& Config, int32 Value, int32 Multiplier, int32 ChainLinks)
{
	if (Value <= 0)
	{
		return 0;
	}

	const int64 Mult = FMath::Clamp<int64>(Multiplier, 1, FMath::Max(1, Config.MaxMultiplier));
	const int64 Links = FMath::Max<int64>(0, ChainLinks);
	// O bônus nunca é negativo (percentual negativo na configuração vale como sem bônus). Cada fator cabe em 32 bits,
	// então Links * percentual cabe em int64.
	const int64 BonusPercent = FMath::Clamp<int64>(Links * Config.ChainBonusPercentPerLink, 0, FMath::Max(0, Config.ChainBonusMaxPercent));

	// C2: valor x (100 + bônus) / 100, truncado, e só depois vezes o multiplicador. Tudo em int64.
	// Valor < 2^31 e (100 + bônus) < 2^31 + 100: o produto cabe em int64.
	const int64 WithChain = static_cast<int64>(Value) * (100 + BonusPercent) / 100;
	if (WithChain >= MAX_int32)
	{
		return MAX_int32; // o multiplicador é >= 1: já saturou, e multiplicar de novo poderia estourar o int64
	}
	const int64 Points = WithChain * Mult; // < 2^31 x 2^31: cabe em int64
	return Points > MAX_int32 ? MAX_int32 : static_cast<int32>(Points);
}

void FDNCrash::RegisterImpact(const FDNCrashConfig& Config, FDNCrashState& State)
{
	if (State.bEnded || State.bFirstImpactHappened)
	{
		return;
	}
	State.bFirstImpactHappened = true;
	State.SecondsSinceFirstImpact = 0.0f;
	State.QuietSeconds = 0.0f;
}

int32 FDNCrash::RegisterDestruction(const FDNCrashConfig& Config, FDNCrashState& State, int32 Value)
{
	if (State.bEnded || Value <= 0)
	{
		return 0;
	}

	RegisterImpact(Config, State);

	// Cadeia: dentro da janela (inclusive) soma um elo; senão recomeça. O 1º veículo não tem bônus.
	if (State.DestroyedCount > 0 && State.SecondsSinceLastDestruction <= Config.ChainWindowSeconds)
	{
		++State.ChainLinks;
	}
	else
	{
		State.ChainLinks = 0;
	}

	const int32 Points = ComputePoints(Config, Value, State.Multiplier, State.ChainLinks);
	const int32 Before = State.Score.Total;
	State.Score.Add(Points);

	State.SecondsSinceLastDestruction = 0.0f;
	++State.DestroyedCount;
	return State.Score.Total - Before;
}

void FDNCrash::CollectMultiplier(const FDNCrashConfig& Config, FDNCrashState& State)
{
	if (State.bEnded)
	{
		return;
	}
	State.Multiplier = FMath::Min(State.Multiplier + 1, FMath::Max(1, Config.MaxMultiplier));
}

void FDNCrash::Tick(const FDNCrashConfig& Config, FDNCrashState& State, float DeltaTime, float MaxVehicleSpeedMps)
{
	if (State.bEnded)
	{
		return;
	}

	const float Dt = SanitizeDelta(DeltaTime);
	State.SecondsSinceLastDestruction = FMath::Min(State.SecondsSinceLastDestruction + Dt, 1.0e9f);

	// C3: os relógios do fim da rodada só andam depois do primeiro impacto.
	if (!State.bFirstImpactHappened)
	{
		return;
	}

	State.SecondsSinceFirstImpact += Dt;

	// Velocidade NaN conta como em movimento: não encerra a rodada por engano.
	const bool bQuiet = FMath::IsFinite(MaxVehicleSpeedMps) && MaxVehicleSpeedMps <= Config.QuietSpeedMetersPerSecond;
	State.QuietSeconds = bQuiet ? State.QuietSeconds + Dt : 0.0f;

	if (State.QuietSeconds >= Config.QuietSecondsToEnd || State.SecondsSinceFirstImpact >= Config.MaxSecondsAfterFirstImpact)
	{
		State.bEnded = true;
	}
}
