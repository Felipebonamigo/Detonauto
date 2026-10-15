#include "Core/DNBreakable.h"
#include "Core/DNCar.h"

namespace
{
	float Positive(float Value, float Default)
	{
		return (FMath::IsFinite(Value) && Value > 0.0f) ? Value : Default;
	}

	FVector Flat(const FVector& V)
	{
		return FVector(V.X, V.Y, 0.0);
	}
}

FDNBreakResult FDNBreakable::Hit(const FDNBreakableKind& Kind, const FDNBreakableSettings& Settings, const FDNBreakInput& Input)
{
	FDNBreakResult R;
	const FVector V = Input.CarVelocityCmS;
	if (V.ContainsNaN() || !FMath::IsFinite(V.SizeSquared()))
	{
		return R; // B6: velocidade inválida não quebra
	}
	R.CarVelocityCmS = V;

	// B1: limiar pela velocidade horizontal
	const FVector Horizontal = Flat(V);
	const float HorizontalKmh = static_cast<float>(Horizontal.Size()) * 0.036f;
	const float MinKmh = FMath::IsFinite(Kind.MinBreakSpeedKmh) ? FMath::Max(Kind.MinBreakSpeedKmh, 0.0f) : FDNBreakableKind().MinBreakSpeedKmh;
	if (HorizontalKmh < MinKmh)
	{
		return R;
	}
	R.bBreaks = true;
	R.Points = FMath::Max(Kind.Points, 0);

	// B2: momento arcade, com teto, vezes a frontalidade; só o módulo muda
	const FDNBreakableSettings Def;
	const float CarMass = Positive(Input.CarMassKg, FDNCarConfig().MassKg);
	const float PropMass = Positive(Kind.MassKg, FDNBreakableKind().MassKg);
	const float K = Positive(Settings.ArcadeMassFactor, Def.ArcadeMassFactor);
	const float Cap = FMath::Clamp(Positive(Settings.MaxSpeedLossPct, Def.MaxSpeedLossPct), 0.0f, 100.0f) / 100.0f;
	const float Loss = FMath::Min(1.0f - CarMass / (CarMass + K * PropMass), Cap);
	const FVector Dir = Flat(Input.CarToProp).GetSafeNormal();
	const float Front = (Dir.IsNearlyZero() || Horizontal.IsNearlyZero()) ? 1.0f : FMath::Abs(static_cast<float>(FVector::DotProduct(Horizontal.GetSafeNormal(), Dir)));
	R.CarVelocityCmS = V * (1.0f - Loss * Front);
	R.CarDeltaVMs = static_cast<float>(V.Size() - R.CarVelocityCmS.Size()) / 100.0f; // B4

	// B3: lançamento na direção do carro, com teto; quem tomba gira em torno da base
	const float MaxLaunch = Positive(Settings.MaxLaunchMs, Def.MaxLaunchMs) * 100.0f;
	const float LaunchFactor = FMath::IsFinite(Kind.LaunchFactor) ? FMath::Max(Kind.LaunchFactor, 0.0f) : FDNBreakableKind().LaunchFactor;
	const float LaunchUp = FMath::IsFinite(Kind.LaunchUpMs) ? FMath::Max(Kind.LaunchUpMs, 0.0f) : FDNBreakableKind().LaunchUpMs;
	R.PropVelocityCmS = (V * LaunchFactor + FVector::UpVector * (LaunchUp * 100.0f)).GetClampedToMaxSize(MaxLaunch);
	if (Kind.bTopplesAtBase && !Horizontal.IsNearlyZero())
	{
		const float LaunchHorizontalMs = static_cast<float>(Flat(R.PropVelocityCmS).Size()) / 100.0f;
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, Horizontal.GetSafeNormal());
		R.PropAngularRadS = Axis * FMath::Min(LaunchHorizontalMs / ToppleArmM, MaxToppleRadS);
	}
	return R;
}

FVector FDNBreakable::Plow(const FDNBreakableSettings& Settings, const FVector& CarVelocityCmS, const FVector& DebrisVelocityCmS)
{
	const FVector Debris = (DebrisVelocityCmS.ContainsNaN() || !FMath::IsFinite(DebrisVelocityCmS.SizeSquared())) ? FVector::ZeroVector : DebrisVelocityCmS;
	const FVector Car = Flat(CarVelocityCmS);
	if (Car.ContainsNaN() || !FMath::IsFinite(Car.SizeSquared()) || Car.Size() < 100.0)
	{
		return Debris; // carro parado (abaixo de 1 m/s) ou inválido não varre
	}
	const FDNBreakableSettings Def;
	const float Factor = (FMath::IsFinite(Settings.PlowFactor) && Settings.PlowFactor >= 1.0f) ? Settings.PlowFactor : Def.PlowFactor;
	const float UpMs = (FMath::IsFinite(Settings.PlowUpMs) && Settings.PlowUpMs >= 0.0f) ? Settings.PlowUpMs : Def.PlowUpMs;
	const FVector Dir = Car.GetSafeNormal();
	const double Along = FVector::DotProduct(Debris, Dir);
	const double Target = Car.Size() * Factor;
	if (Along >= Target && Debris.Z >= UpMs * 100.0f)
	{
		return Debris; // já sai da frente sozinho
	}
	FVector Out = Debris + Dir * FMath::Max(Target - Along, 0.0); // só a parte na direção do carro muda; o lado fica
	Out.Z = FMath::Max(Out.Z, static_cast<double>(UpMs) * 100.0);
	return Out.GetClampedToMaxSize(Positive(Settings.MaxLaunchMs, Def.MaxLaunchMs) * 100.0f);
}

bool FDNBreakable::StepSleep(const FDNBreakableSettings& Settings, float SpeedMs, float DeltaSeconds, float& InOutStillSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return false;
	}
	const FDNBreakableSettings Def;
	const float SleepSpeed = Positive(Settings.SleepSpeedMs, Def.SleepSpeedMs);
	const float SleepAfter = Positive(Settings.SleepAfterS, Def.SleepAfterS);
	if (!FMath::IsFinite(SpeedMs) || SpeedMs >= SleepSpeed)
	{
		InOutStillSeconds = 0.0f;
		return false;
	}
	InOutStillSeconds += DeltaSeconds;
	return InOutStillSeconds >= SleepAfter - KINDA_SMALL_NUMBER;
}
