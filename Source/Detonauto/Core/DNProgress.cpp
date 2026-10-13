#include "Core/DNProgress.h"

namespace
{
	void Add(FDNProgressCount& Into, const FDNProgressCount& From)
	{
		Into.Done += From.Done;
		Into.Total += From.Total;
		Into.Gold += From.Gold;
	}
}

FDNProgressResult FDNProgress::Compute(const TArray<FDNCrossingProgress>& Crossings, const TArray<FDNCityRegion>& Cities)
{
	FDNProgressResult Result;

	// Tabela de cidades. P5: o primeiro id vale; linha sem id de cidade ou de região é ignorada.
	for (const FDNCityRegion& City : Cities)
	{
		if (City.CityId.IsNone())
		{
			Result.Warnings.Add(TEXT("Linha da tabela de cidades sem id de cidade: ignorada."));
			continue;
		}
		if (City.RegionId.IsNone())
		{
			Result.Warnings.Add(FString::Printf(TEXT("A cidade '%s' não tem região: ignorada."), *City.CityId.ToString()));
			continue;
		}
		if (Result.ByCity.Contains(City.CityId))
		{
			Result.Warnings.Add(FString::Printf(TEXT("A cidade '%s' aparece mais de uma vez na tabela: vale a primeira."), *City.CityId.ToString()));
			continue;
		}

		FDNCityProgress& Entry = Result.ByCity.Add(City.CityId);
		Entry.RegionId = City.RegionId;
		Entry.bPlayable = City.bPlayable;
		Result.ByRegion.FindOrAdd(City.RegionId); // a região existe mesmo que só tenha cidades não jogáveis
	}

	// Cruzamentos. P2 e P3.
	TSet<FName> SeenCrossings;
	for (const FDNCrossingProgress& Crossing : Crossings)
	{
		if (Crossing.CrossingId.IsNone())
		{
			Result.Warnings.Add(FString::Printf(TEXT("Cruzamento sem id (cidade '%s'): ignorado."), *Crossing.CityId.ToString()));
			continue;
		}
		if (SeenCrossings.Contains(Crossing.CrossingId))
		{
			Result.Warnings.Add(FString::Printf(TEXT("O cruzamento '%s' aparece mais de uma vez: vale o primeiro."), *Crossing.CrossingId.ToString()));
			continue;
		}
		SeenCrossings.Add(Crossing.CrossingId);

		FDNCityProgress* City = Result.ByCity.Find(Crossing.CityId);
		if (!City)
		{
			Result.Warnings.Add(FString::Printf(TEXT("O cruzamento '%s' é de uma cidade desconhecida ('%s'): ignorado."),
				*Crossing.CrossingId.ToString(), *Crossing.CityId.ToString()));
			continue;
		}
		if (!City->bPlayable)
		{
			continue; // P4: fora do total jogável, sem aviso
		}

		++City->Count.Total;
		if (IsDone(Crossing.Medal))
		{
			++City->Count.Done;
		}
		if (Crossing.Medal == EDNMedal::Gold)
		{
			++City->Count.Gold;
		}
	}

	// Regiões e geral: soma das cidades jogáveis.
	for (const TPair<FName, FDNCityProgress>& Pair : Result.ByCity)
	{
		if (Pair.Value.bPlayable)
		{
			Add(Result.ByRegion.FindOrAdd(Pair.Value.RegionId), Pair.Value.Count);
			Add(Result.Overall, Pair.Value.Count);
		}
	}
	return Result;
}
