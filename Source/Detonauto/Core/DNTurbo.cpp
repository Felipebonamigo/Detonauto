#include "Core/DNTurbo.h"

namespace
{
	/** Tempo inválido (negativo, zero ou NaN) não ganha nem gasta. */
	float SanitizeDelta(float DeltaTime)
	{
		return (FMath::IsFinite(DeltaTime) && DeltaTime > 0.0f) ? DeltaTime : 0.0f;
	}
}

float FDNTurbo::WrongWayGain(const FDNTurboConfig& Config, float SpeedKmh, bool bWrongWay, float DeltaTime)
{
	if (!bWrongWay || !(SpeedKmh > Config.WrongWayMinSpeedKmh))
	{
		return 0.0f;
	}
	return Config.WrongWayGainPerSecond * SanitizeDelta(DeltaTime);
}

float FDNTurbo::DriftGain(const FDNTurboConfig& Config, float DriftAngleDeg, float SpeedKmh, float DeltaTime)
{
	const float Angle = FMath::Abs(DriftAngleDeg);
	if (!(Angle >= Config.DriftMinAngleDeg && Angle <= Config.DriftMaxAngleDeg) || !(SpeedKmh > Config.DriftMinSpeedKmh))
	{
		return 0.0f;
	}
	return Config.DriftGainPerSecond * SanitizeDelta(DeltaTime);
}

float FDNTurbo::NearMissGain(const FDNTurboConfig& Config, FDNTurboState& State, const FDNNearMissEvent& Event, float SpeedKmh)
{
	const bool bQualifies = Event.bVehicleMoving
		&& Event.LateralDistanceCm < Config.NearMissMaxLateralCm
		&& SpeedKmh > Config.NearMissMinSpeedKmh
		&& !State.NearMissVehiclesCounted.Contains(Event.VehicleId);
	if (!bQualifies || Config.NearMissChainMultipliers.IsEmpty())
	{
		return 0.0f;
	}

	// T4: exatamente na janela continua a cadeia; só "mais de" reinicia.
	if (State.NearMissChainLength > 0 && State.SecondsSinceNearMiss <= Config.NearMissChainWindowSeconds)
	{
		++State.NearMissChainLength;
	}
	else
	{
		State.NearMissChainLength = 1;
	}
	State.SecondsSinceNearMiss = 0.0f;
	State.NearMissVehiclesCounted.Add(Event.VehicleId);

	// T3: depois da última posição, vale o último multiplicador.
	const int32 Index = FMath::Min(State.NearMissChainLength - 1, Config.NearMissChainMultipliers.Num() - 1);
	return Config.NearMissGain * Config.NearMissChainMultipliers[Index];
}

float FDNTurbo::AirGain(const FDNTurboConfig& Config, FDNTurboState& State, bool bAirborne, float DeltaTime)
{
	if (!bAirborne)
	{
		State.AirTimeSeconds = 0.0f;
		State.AirGainThisJump = 0.0f;
		return 0.0f;
	}

	const float Dt = SanitizeDelta(DeltaTime);
	const float Before = State.AirTimeSeconds;
	State.AirTimeSeconds += Dt;

	// só conta o tempo que passou do limiar
	const float Counted = FMath::Max(0.0f, State.AirTimeSeconds - FMath::Max(Before, Config.AirTimeThresholdSeconds));
	const float Room = FMath::Max(0.0f, Config.AirMaxGainPerJump - State.AirGainThisJump);
	const float Gain = FMath::Min(Counted * Config.AirGainPerSecond, Room);
	State.AirGainThisJump += Gain;
	return Gain;
}

void FDNTurbo::ApplyTakedown(const FDNTurboConfig& Config, FDNTurboState& State)
{
	State.Bar = FMath::Min(Config.TakedownSetsBarTo, Config.MaxBar);
}

void FDNTurbo::Tick(const FDNTurboConfig& Config, FDNTurboState& State, const FDNTurboFrameInput& Input)
{
	const float Dt = SanitizeDelta(Input.DeltaTime);

	// O tempo da cadeia anda sempre, com o turbo ligado ou não.
	State.SecondsSinceNearMiss = FMath::Min(State.SecondsSinceNearMiss + Dt, 1.0e9f);

	// Ganhos: só com o turbo desligado no começo do quadro; só o takedown ganha com ele ligado.
	// O salto sempre acompanha o tempo no ar (para o limite por salto), mas o ganho só vale desligado.
	const bool bWasActive = State.bActive;
	float Gain = 0.0f;
	const float AirGainValue = AirGain(Config, State, Input.bAirborne, Dt);
	if (!bWasActive)
	{
		Gain += WrongWayGain(Config, Input.SpeedKmh, Input.bWrongWay, Dt);   // T1: as fontes somam
		Gain += DriftGain(Config, Input.DriftAngleDeg, Input.SpeedKmh, Dt);
		Gain += AirGainValue;
		for (const FDNNearMissEvent& Event : Input.NearMisses)
		{
			Gain += NearMissGain(Config, State, Event, Input.SpeedKmh);
		}
	}
	State.Bar = FMath::Min(State.Bar + Gain, Config.MaxBar);

	if (Input.bTakedown)
	{
		ApplyTakedown(Config, State);
	}

	// Ligar (O1): só no aperto, isto é, na borda de subida do botão (solto no quadro anterior e apertado
	// agora), e se a barra tiver mais de 0 neste quadro (já com os ganhos e o takedown dele). Segurar o
	// botão com a barra vazia não liga nada. Por consequência, depois de zerar com o botão apertado,
	// só um novo aperto liga de novo (T6).
	const bool bPressed = Input.bButtonHeld && !State.bButtonWasHeld;
	if (!State.bActive && bPressed && State.Bar > 0.0f)
	{
		State.bActive = true;
		State.bStartedAtFull = State.Bar >= Config.MaxBar;
		State.bExtendedAwarded = false;
	}

	// Usar: consome, desliga ao soltar ou ao zerar, e dá o turbo prolongado (T7).
	if (State.bActive)
	{
		if (!Input.bButtonHeld)
		{
			State.bActive = false;
		}
		else
		{
			State.Bar -= Config.ConsumptionPerSecond * Dt;
			if (State.Bar <= 0.0f)
			{
				State.Bar = 0.0f;
				if (State.bStartedAtFull && !State.bExtendedAwarded)
				{
					State.Bar = FMath::Min(Config.ExtendedBonus, Config.MaxBar);
					State.bExtendedAwarded = true;
				}
				else
				{
					State.bActive = false;
				}
			}
		}
	}

	State.bButtonWasHeld = Input.bButtonHeld;
}
