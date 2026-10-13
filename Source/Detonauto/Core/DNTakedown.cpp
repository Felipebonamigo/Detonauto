#include "Core/DNTakedown.h"

namespace
{
	/**
	 *  Devolve o vetor com comprimento 1. Vetor zero ou com NaN é inválido. Um vetor que já tem comprimento 1
	 *  (dentro da tolerância de float) passa intacto, para o produto escalar nos limites ficar exato.
	 */
	bool NormalizedSafe(const FVector& V, FVector& Out)
	{
		const double LengthSquared = V.SizeSquared();
		if (!FMath::IsFinite(LengthSquared) || LengthSquared < 1.0e-12)
		{
			return false;
		}
		Out = (FMath::Abs(LengthSquared - 1.0) <= 1.0e-4) ? V : V / FMath::Sqrt(LengthSquared);
		return true;
	}

	/** Passada raspando no cálculo da janela: tempo entre 0 e a janela, inclusive; NaN e negativo ficam fora. */
	bool InWindow(float Seconds, float Window)
	{
		return Seconds >= 0.0f && Seconds <= Window;
	}
}

float FDNTakedown::CosineOf(float Degrees)
{
	return static_cast<float>(FMath::Cos(FMath::DegreesToRadians(static_cast<double>(Degrees))));
}

EDNContactZone FDNTakedown::DeriveZone(const FDNTakedownConfig& Config, const FVector& ContactNormal, const FVector& RivalForward)
{
	FVector N, V;
	if (!NormalizedSafe(ContactNormal, N) || !NormalizedSafe(RivalForward, V))
	{
		return EDNContactZone::Side;
	}

	const double Dot = FVector::DotProduct(N, V);
	const double Cos = CosineOf(Config.ZoneHalfAngleDegrees);
	if (Dot >= Cos)
	{
		return EDNContactZone::Rear;
	}
	if (Dot <= -Cos)
	{
		return EDNContactZone::Front;
	}
	return EDNContactZone::Side;
}

EDNTakedownType FDNTakedown::Classify(const FDNTakedownConfig& Config, const FDNTakedownClassifyInput& Input)
{
	// 1. Vertical: não depende dos vetores.
	if (Input.bPlayerFalling && Input.PlayerHeightAboveRivalCm >= Config.VerticalMinHeightCm)
	{
		return EDNTakedownType::Vertical;
	}

	FVector A, V, N;
	if (!NormalizedSafe(Input.PlayerForward, A) || !NormalizedSafe(Input.RivalForward, V) || !NormalizedSafe(Input.ContactNormal, N))
	{
		return EDNTakedownType::General;
	}

	const double Dot = FVector::DotProduct(A, V);   // cosseno do ângulo entre A e V
	const EDNContactZone Zone = DeriveZone(Config, N, V);

	// 2. Por trás: ângulo menor que 30 (dot maior que cos 30) e zona traseira.
	if (Zone == EDNContactZone::Rear && Dot > CosineOf(Config.BehindMaxAngleDegrees))
	{
		return EDNTakedownType::FromBehind;
	}

	// 3. Em T: ângulo entre 60 e 120, inclusive (dot entre cos 120 e cos 60), e zona lateral.
	if (Zone == EDNContactZone::Side && Dot <= CosineOf(Config.TBoneMinAngleDegrees) && Dot >= CosineOf(Config.TBoneMaxAngleDegrees))
	{
		return EDNTakedownType::TBone;
	}

	// 4. Lateral: zona lateral e ângulo menor que 60 (dot maior que cos 60).
	if (Zone == EDNContactZone::Side && Dot > CosineOf(Config.SideMaxAngleDegrees))
	{
		return EDNTakedownType::Side;
	}

	// 5. Geral.
	return EDNTakedownType::General;
}

bool FDNTakedown::Qualifies(const FDNTakedownConfig& Config, const FDNTakedownContact& Contact)
{
	// Condição 1: velocidade relativa de pelo menos 20 km/h (NaN não qualifica).
	const bool bFastEnough = Contact.RelativeSpeedKmh >= Config.MinRelativeSpeedKmh;

	// Condição 2: o rival bate em até 1,5 s depois do contato (K3: inclusivo, contado do contato).
	const bool bRivalCrashedInTime = Contact.bRivalCrashed && InWindow(Contact.RivalCrashSecondsAfterContact, Config.RivalCrashWindowSeconds);

	// Condição 3: o jogador não se destruiu no mesmo intervalo.
	const bool bPlayerDestroyedInInterval = Contact.bPlayerDestroyed && InWindow(Contact.PlayerDestroyedSecondsAfterContact, Config.RivalCrashWindowSeconds);

	return bFastEnough && bRivalCrashedInTime && !bPlayerDestroyedInInterval;
}

const TCHAR* FDNTakedown::ToString(EDNTakedownType Type)
{
	switch (Type)
	{
	case EDNTakedownType::Vertical: return TEXT("vertical");
	case EDNTakedownType::FromBehind: return TEXT("por tras");
	case EDNTakedownType::TBone: return TEXT("em T");
	case EDNTakedownType::Side: return TEXT("lateral");
	default: return TEXT("geral");
	}
}

const TCHAR* FDNTakedown::ToString(EDNContactZone Zone)
{
	switch (Zone)
	{
	case EDNContactZone::Rear: return TEXT("traseira");
	case EDNContactZone::Front: return TEXT("frontal");
	default: return TEXT("lateral");
	}
}
