#include "Core/DNRace.h"

bool FDNRace::Step(FDNRacerState& Racer, float NewProgressCm, float TimeS, float CourseLengthCm)
{
	// R1 e R2: só sobe; valor inválido não conta
	if (FMath::IsFinite(NewProgressCm) && NewProgressCm >= 0.0f)
	{
		Racer.ProgressCm = FMath::Max(Racer.ProgressCm, NewProgressCm);
	}
	// R3: chegou ao alcançar o comprimento, uma vez só
	if (Racer.bFinished || !FMath::IsFinite(CourseLengthCm) || CourseLengthCm <= 0.0f || Racer.ProgressCm < CourseLengthCm)
	{
		return false;
	}
	Racer.bFinished = true;
	Racer.FinishTimeS = TimeS;
	return true;
}

TArray<int32> FDNRace::Order(TConstArrayView<FDNRacerState> Racers)
{
	// R4: chegou antes de não chegou; chegou por tempo; não chegou por progresso; empate pelo índice (ordenação estável)
	TArray<int32> Out;
	for (int32 i = 0; i < Racers.Num(); ++i)
	{
		Out.Add(i);
	}
	Out.StableSort([&Racers](int32 A, int32 B)
	{
		const FDNRacerState& RA = Racers[A];
		const FDNRacerState& RB = Racers[B];
		if (RA.bFinished != RB.bFinished)
		{
			return RA.bFinished;
		}
		if (RA.bFinished)
		{
			return RA.FinishTimeS < RB.FinishTimeS;
		}
		return RA.ProgressCm > RB.ProgressCm;
	});
	return Out;
}

int32 FDNRace::Position(TConstArrayView<FDNRacerState> Racers, int32 Index)
{
	// R5
	return Racers.IsValidIndex(Index) ? Order(Racers).IndexOfByKey(Index) + 1 : 0;
}
