#include "Core/DNCourse.h"

namespace
{
	bool IsValidCourse(TConstArrayView<FVector> Points)
	{
		if (Points.Num() < 2)
		{
			return false;
		}
		for (const FVector& P : Points)
		{
			if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y) || !FMath::IsFinite(P.Z))
			{
				return false;
			}
		}
		return true;
	}
}

float FDNCourse::Length(TConstArrayView<FVector> Points)
{
	if (!IsValidCourse(Points))
	{
		return 0.0f;
	}
	double Sum = 0.0;
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		Sum += FVector::Dist(Points[i - 1], Points[i]);
	}
	return static_cast<float>(Sum);
}

float FDNCourse::Progress(TConstArrayView<FVector> Points, const FVector& Position, float HintCm, float BackCm, float AheadCm)
{
	if (!IsValidCourse(Points) || !FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) || !FMath::IsFinite(Position.Z))
	{
		return HintCm;
	}
	const double Lo = HintCm - FMath::Max(BackCm, 0.0f);
	const double Hi = HintCm + FMath::Max(AheadCm, 0.0f);

	double Best = HintCm;
	double BestDistSq = TNumericLimits<double>::Max();
	double SegStart = 0.0;
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		const FVector A = Points[i - 1];
		const FVector AB = Points[i] - A;
		const double SegLen = AB.Size();
		const double SegEnd = SegStart + SegLen;
		if (SegEnd >= Lo && SegStart <= Hi && SegLen > UE_DOUBLE_SMALL_NUMBER)
		{
			// C4: t preso em 0..1; a janela também corta o trecho, para o atalho não entrar pela ponta do trecho vizinho
			const double TLo = FMath::Clamp((Lo - SegStart) / SegLen, 0.0, 1.0);
			const double THi = FMath::Clamp((Hi - SegStart) / SegLen, 0.0, 1.0);
			const double T = FMath::Clamp(FVector::DotProduct(Position - A, AB) / (SegLen * SegLen), TLo, THi);
			const double DistSq = FVector::DistSquared(Position, A + AB * T);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = SegStart + T * SegLen;
			}
		}
		SegStart = SegEnd;
	}
	return static_cast<float>(Best);
}

FVector FDNCourse::LocationAt(TConstArrayView<FVector> Points, float ProgressCm)
{
	if (!IsValidCourse(Points))
	{
		return FVector::ZeroVector;
	}
	double Left = FMath::Max(static_cast<double>(ProgressCm), 0.0);
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		const double SegLen = FVector::Dist(Points[i - 1], Points[i]);
		if (Left <= SegLen)
		{
			return SegLen > UE_DOUBLE_SMALL_NUMBER ? FMath::Lerp(Points[i - 1], Points[i], Left / SegLen) : Points[i - 1];
		}
		Left -= SegLen;
	}
	return Points.Last();
}
