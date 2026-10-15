#include "Core/DNCameraFx.h"

namespace
{
	/** Tolerância nas bordas das fases: a soma de passos em float não fecha exato (0,075 + 0,075 pode dar um pouco menos que 0,15). */
	constexpr float PhaseEdgeS = 1.0e-4f;

	float Positive(float Value, float Default)
	{
		return (FMath::IsFinite(Value) && Value > 0.0f) ? Value : Default;
	}

	float NonNegative(float Value, float Default)
	{
		return (FMath::IsFinite(Value) && Value >= 0.0f) ? Value : Default;
	}

	float Smooth01(float U)
	{
		U = FMath::Clamp(U, 0.0f, 1.0f);
		return U * U * (3.0f - 2.0f * U);
	}

	/** Saída suave: rápido no começo, assentando no fim. */
	float EaseOut01(float U)
	{
		U = FMath::Clamp(U, 0.0f, 1.0f);
		return 1.0f - (1.0f - U) * (1.0f - U);
	}

	float PhaseDuration(const FDNCameraFxSettings& S, EDNCrashCamPhase Phase)
	{
		const FDNCameraFxSettings D;
		switch (Phase)
		{
		case EDNCrashCamPhase::Entering: return Positive(S.CrashCamEnterS, D.CrashCamEnterS);
		case EDNCrashCamPhase::Holding: return NonNegative(S.CrashCamHoldS, D.CrashCamHoldS);
		case EDNCrashCamPhase::Exiting: return Positive(S.CrashCamExitS, D.CrashCamExitS);
		case EDNCrashCamPhase::Cooldown: return NonNegative(S.CrashCamCooldownS, D.CrashCamCooldownS);
		default: return 0.0f;
		}
	}

	EDNCrashCamPhase NextPhase(EDNCrashCamPhase Phase)
	{
		switch (Phase)
		{
		case EDNCrashCamPhase::Entering: return EDNCrashCamPhase::Holding;
		case EDNCrashCamPhase::Holding: return EDNCrashCamPhase::Exiting;
		case EDNCrashCamPhase::Exiting: return EDNCrashCamPhase::Cooldown;
		default: return EDNCrashCamPhase::Idle;
		}
	}

	float SlowScale(const FDNCameraFxSettings& S)
	{
		return FMath::Clamp(Positive(S.CrashCamTimeScale, FDNCameraFxSettings().CrashCamTimeScale), 0.01f, 1.0f);
	}

	/** Fração da fase atual (0 a 1). */
	float PhaseFraction(const FDNCameraFxSettings& S, const FDNCrashCamState& State)
	{
		const float Duration = PhaseDuration(S, State.Phase);
		return Duration > 0.0f ? FMath::Clamp(State.PhaseSeconds / Duration, 0.0f, 1.0f) : 1.0f;
	}
}

float FDNCameraFx::Ramp(float Value, float A, float B)
{
	const float V = (FMath::IsFinite(Value) && Value > 0.0f) ? Value : 0.0f;
	if (!FMath::IsFinite(A) || !FMath::IsFinite(B))
	{
		return 0.0f;
	}
	if (A >= B)
	{
		return V >= B ? 1.0f : 0.0f;
	}
	return Smooth01((V - A) / (B - A));
}

float FDNSpeedFov::Target(const FDNCameraFxSettings& S, float BaseFovDeg, float SpeedKmh, float SpeedFovScale, float TurboBlend)
{
	const FDNCameraFxSettings D;
	const float Base = FMath::IsFinite(BaseFovDeg) ? BaseFovDeg : 90.0f;
	const float Scale = NonNegative(SpeedFovScale, 1.0f);
	const float Turbo = FMath::IsFinite(TurboBlend) ? FMath::Clamp(TurboBlend, 0.0f, 1.0f) : 0.0f;
	const float Speed = NonNegative(S.SpeedFovMaxAddDeg, D.SpeedFovMaxAddDeg) * FDNCameraFx::Ramp(SpeedKmh, S.SpeedFovStartKmh, S.SpeedFovFullKmh) * Scale;
	const float Fov = Base + Speed + NonNegative(S.TurboFovAddDeg, D.TurboFovAddDeg) * Turbo;
	return FMath::Min(Fov, Positive(S.MaxFovDeg, D.MaxFovDeg));
}

float FDNSpeedFov::Step(const FDNCameraFxSettings& S, float CurrentDeg, float TargetDeg, float RealDt)
{
	if (!FMath::IsFinite(TargetDeg))
	{
		return CurrentDeg;
	}
	if (!FMath::IsFinite(CurrentDeg))
	{
		return TargetDeg;
	}
	if (!FMath::IsFinite(RealDt) || RealDt <= 0.0f)
	{
		return CurrentDeg;
	}
	const float MaxStep = Positive(S.FovBlendDegPerS, FDNCameraFxSettings().FovBlendDegPerS) * RealDt;
	return CurrentDeg + FMath::Clamp(TargetDeg - CurrentDeg, -MaxStep, MaxStep);
}

float FDNSpeedShake::Amplitude(const FDNCameraFxSettings& S, float SpeedKmh, float ShakeScale, float GlobalScale)
{
	return NonNegative(S.ShakeMaxDeg, FDNCameraFxSettings().ShakeMaxDeg) * FDNCameraFx::Ramp(SpeedKmh, S.ShakeStartKmh, S.ShakeFullKmh)
		* NonNegative(ShakeScale, 1.0f) * NonNegative(GlobalScale, 0.0f);
}

FVector2D FDNSpeedShake::Offset(const FDNCameraFxSettings& S, float SpeedKmh, float ShakeScale, float GlobalScale, double TimeS)
{
	const float Amp = Amplitude(S, SpeedKmh, ShakeScale, GlobalScale);
	if (Amp <= 0.0f || !FMath::IsFinite(TimeS))
	{
		return FVector2D::ZeroVector; // exatamente zero abaixo do começo
	}
	// Perlin 1D da engine (determinístico); a fase dá a volta a cada 1000 ciclos para o float não perder precisão em partidas longas
	const double Phase = FMath::Fmod(TimeS * Positive(S.ShakeHz, FDNCameraFxSettings().ShakeHz), 1000.0);
	const float Pitch = FMath::Clamp(FMath::PerlinNoise1D(static_cast<float>(Phase)), -1.0f, 1.0f);
	const float Yaw = FMath::Clamp(FMath::PerlinNoise1D(static_cast<float>(Phase) + 57.31f), -1.0f, 1.0f); // eixo defasado
	return FVector2D(Amp * Pitch, Amp * Yaw);
}

float FDNEdgeBlur::Strength(const FDNCameraFxSettings& S, float SpeedKmh)
{
	return FMath::Clamp(NonNegative(S.EdgeBlurMax, FDNCameraFxSettings().EdgeBlurMax), 0.0f, 1.0f)
		* FDNCameraFx::Ramp(SpeedKmh, S.EdgeBlurStartKmh, S.EdgeBlurFullKmh);
}

bool FDNCrashCam::TryTrigger(const FDNCameraFxSettings& S, FDNCrashCamState& State, float DeltaVMs, EDNImpactSource Source)
{
	if (State.Phase != EDNCrashCamPhase::Idle || !FMath::IsFinite(DeltaVMs) || !FDNImpactPolicy::TriggersCrashCam(Source))
	{
		return false; // durante a câmera lenta e na espera, ignora; mobiliário nunca dispara
	}
	if (DeltaVMs < Positive(S.CrashCamMinDeltaVMs, FDNCameraFxSettings().CrashCamMinDeltaVMs))
	{
		return false;
	}
	State.Phase = EDNCrashCamPhase::Entering;
	State.PhaseSeconds = 0.0f;
	return true;
}

void FDNCrashCam::Step(const FDNCameraFxSettings& S, FDNCrashCamState& State, float RealDt)
{
	if (!FMath::IsFinite(RealDt) || RealDt <= 0.0f || State.Phase == EDNCrashCamPhase::Idle)
	{
		return;
	}
	float Left = RealDt;
	for (int32 Guard = 0; Guard < 5 && State.Phase != EDNCrashCamPhase::Idle; ++Guard)
	{
		const float Duration = PhaseDuration(S, State.Phase);
		if (State.PhaseSeconds + Left < Duration - PhaseEdgeS)
		{
			State.PhaseSeconds += Left;
			return;
		}
		Left = FMath::Max(0.0f, Left - FMath::Max(0.0f, Duration - State.PhaseSeconds)); // a sobra passa para a fase seguinte
		State.Phase = NextPhase(State.Phase);
		State.PhaseSeconds = 0.0f;
		if (Left <= 0.0f)
		{
			return;
		}
	}
	if (State.Phase == EDNCrashCamPhase::Idle)
	{
		State.PhaseSeconds = 0.0f;
	}
}

float FDNCrashCam::TimeScale(const FDNCameraFxSettings& S, const FDNCrashCamState& State)
{
	const float Slow = SlowScale(S);
	switch (State.Phase)
	{
	case EDNCrashCamPhase::Entering: return 1.0f + (Slow - 1.0f) * EaseOut01(PhaseFraction(S, State));
	case EDNCrashCamPhase::Holding: return Slow;
	case EDNCrashCamPhase::Exiting: return Slow + (1.0f - Slow) * Smooth01(PhaseFraction(S, State));
	default: return 1.0f;
	}
}

float FDNCrashCam::BlendWeight(const FDNCameraFxSettings& S, const FDNCrashCamState& State)
{
	switch (State.Phase)
	{
	case EDNCrashCamPhase::Entering: return EaseOut01(PhaseFraction(S, State));
	case EDNCrashCamPhase::Holding: return 1.0f;
	case EDNCrashCamPhase::Exiting: return 1.0f - Smooth01(PhaseFraction(S, State));
	default: return 0.0f;
	}
}

bool FDNCrashCam::SteerActive(const FDNCrashCamState& State)
{
	return State.Phase == EDNCrashCamPhase::Entering || State.Phase == EDNCrashCamPhase::Holding;
}

FVector FDNWreckSteer::DeltaVelocity(const FDNCameraFxSettings& S, bool bActive, float SteerInput, const FVector& CarVelocityCmS,
	const FVector& ScreenRight, float RealDt, float& InOutUsedMs)
{
	if (!bActive || !FMath::IsFinite(SteerInput) || SteerInput == 0.0f || !FMath::IsFinite(RealDt) || RealDt <= 0.0f)
	{
		return FVector::ZeroVector;
	}
	const FDNCameraFxSettings D;
	const float Used = NonNegative(InOutUsedMs, 0.0f);
	const float Left = NonNegative(S.WreckSteerMaxDeltaVMs, D.WreckSteerMaxDeltaVMs) - Used;
	if (Left <= 0.0f)
	{
		return FVector::ZeroVector;
	}
	const float Amount = FMath::Min(NonNegative(S.WreckSteerAccelMs2, D.WreckSteerAccelMs2) * FMath::Min(FMath::Abs(SteerInput), 1.0f) * RealDt, Left);
	const FVector Right = FVector(ScreenRight.X, ScreenRight.Y, 0.0).GetSafeNormal();
	const FVector Horizontal(CarVelocityCmS.X, CarVelocityCmS.Y, 0.0);
	FVector Dir = Right;
	if (!Horizontal.ContainsNaN() && Horizontal.Size() > 1.0)
	{
		Dir = FVector::CrossProduct(FVector::UpVector, Horizontal.GetSafeNormal()); // à direita da velocidade
		if (FVector::DotProduct(Dir, Right) < 0.0)
		{
			Dir = -Dir; // o lado direito da TELA
		}
	}
	if (Dir.IsNearlyZero() || Dir.ContainsNaN())
	{
		return FVector::ZeroVector;
	}
	InOutUsedMs = Used + Amount;
	return Dir * (FMath::Sign(SteerInput) * Amount * 100.0f);
}
