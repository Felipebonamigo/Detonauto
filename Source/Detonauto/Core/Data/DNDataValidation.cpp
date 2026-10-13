#include "Core/Data/DNDataValidation.h"
#include "Core/DNGlobe.h"
#include "Core/DNMedals.h"

namespace
{
	constexpr const TCHAR* RegionsFile = TEXT("Regions.csv");
	constexpr const TCHAR* CitiesFile = TEXT("Cities.csv");
	constexpr const TCHAR* LandmarksFile = TEXT("Landmarks.csv");
	constexpr const TCHAR* CrossingsFile = TEXT("Crossings.csv");
	constexpr const TCHAR* VehiclesFile = TEXT("TrafficVehicles.csv");

	bool IsSafeIdChar(TCHAR C)
	{
		return (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '-' || C == '_';
	}

	/** Ids: não vazio, caracteres seguros e único sem diferenciar a caixa (como o FName). */
	template <typename RowType>
	void CheckIds(const TCHAR* File, const TArray<TDNTableEntry<RowType>>& Entries, TArray<FString>& Errors)
	{
		TMap<FString, FString> FirstByLowerId; // id em minúsculas -> id como foi escrito da primeira vez
		for (const TDNTableEntry<RowType>& Entry : Entries)
		{
			if (Entry.Id.IsEmpty())
			{
				Errors.Add(FString::Printf(TEXT("%s: há uma linha com id vazio."), File));
				continue;
			}
			if (!FDNDataValidation::IsSafeId(Entry.Id))
			{
				Errors.Add(FString::Printf(TEXT("%s, linha '%s': o id tem caractere inválido (use só letras sem acento, números, hífen e sublinhado)."), File, *Entry.Id));
			}

			const FString Lower = Entry.Id.ToLower();
			if (const FString* First = FirstByLowerId.Find(Lower))
			{
				// FString == ignora a caixa; aqui é preciso distinguir "repetido" de "só difere na caixa".
				if (First->Equals(Entry.Id, ESearchCase::CaseSensitive))
				{
					Errors.Add(FString::Printf(TEXT("%s: o id '%s' está repetido."), File, *Entry.Id));
				}
				else
				{
					Errors.Add(FString::Printf(TEXT("%s: os ids '%s' e '%s' são o mesmo id (maiúsculas e minúsculas não diferenciam)."), File, **First, *Entry.Id));
				}
			}
			else
			{
				FirstByLowerId.Add(Lower, Entry.Id);
			}
		}
	}

	/** Order: maior que zero e sem repetição dentro do grupo. GroupLabel descreve o grupo na mensagem (pode ser vazio). */
	void CheckOrder(const TCHAR* File, const FString& RowId, int32 Order, const FString& GroupKey, const FString& GroupLabel,
		TMap<FString, TMap<int32, FString>>& SeenByGroup, TArray<FString>& Errors)
	{
		if (Order <= 0)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': a ordem precisa ser maior que zero (veio %d)."), File, *RowId, Order));
			return;
		}
		TMap<int32, FString>& Seen = SeenByGroup.FindOrAdd(GroupKey);
		if (const FString* Other = Seen.Find(Order))
		{
			Errors.Add(FString::Printf(TEXT("%s: a ordem %d se repete%s (linhas '%s' e '%s')."), File, Order, *GroupLabel, **Other, *RowId));
		}
		else
		{
			Seen.Add(Order, RowId);
		}
	}

	TSet<FString> LowerIds(const TArray<FDNRegionEntry>& Entries)
	{
		TSet<FString> Out;
		for (const FDNRegionEntry& E : Entries) { Out.Add(E.Id.ToLower()); }
		return Out;
	}

	TSet<FString> LowerIds(const TArray<FDNCityEntry>& Entries)
	{
		TSet<FString> Out;
		for (const FDNCityEntry& E : Entries) { Out.Add(E.Id.ToLower()); }
		return Out;
	}
}

bool FDNDataValidation::IsSafeId(const FString& Id)
{
	if (Id.IsEmpty())
	{
		return false;
	}
	for (const TCHAR C : Id)
	{
		if (!IsSafeIdChar(C))
		{
			return false;
		}
	}
	return true;
}

TArray<FString> FDNDataValidation::ValidateRegions(const TArray<FDNRegionEntry>& Regions)
{
	TArray<FString> Errors;
	CheckIds(RegionsFile, Regions, Errors);

	TMap<FString, TMap<int32, FString>> SeenOrders;
	for (const FDNRegionEntry& R : Regions)
	{
		if (R.Row.Name.TrimStartAndEnd().IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o nome está vazio."), RegionsFile, *R.Id));
		}
		CheckOrder(RegionsFile, R.Id, R.Row.Order, TEXT(""), TEXT(""), SeenOrders, Errors);
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateCities(const TArray<FDNCityEntry>& Cities, const TArray<FDNRegionEntry>& Regions)
{
	TArray<FString> Errors;
	CheckIds(CitiesFile, Cities, Errors);

	const TSet<FString> RegionIds = LowerIds(Regions);
	TMap<FString, TMap<int32, FString>> SeenOrders;
	for (const FDNCityEntry& C : Cities)
	{
		if (C.Row.Name.TrimStartAndEnd().IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o nome está vazio."), CitiesFile, *C.Id));
		}

		if (C.Row.RegionId.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': falta a região (RegionId)."), CitiesFile, *C.Id));
		}
		else if (!RegionIds.Contains(C.Row.RegionId.ToLower()))
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': a região '%s' não existe em %s."), CitiesFile, *C.Id, *C.Row.RegionId, RegionsFile));
		}

		if (!FMath::IsFinite(C.Row.Latitude) || C.Row.Latitude < -90.0 || C.Row.Latitude > 90.0)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': a latitude %f precisa ser um número em [-90, 90]."), CitiesFile, *C.Id, C.Row.Latitude));
		}
		if (!FMath::IsFinite(C.Row.Longitude) || C.Row.Longitude < -180.0 || C.Row.Longitude > 180.0)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': a longitude %f precisa ser um número em [-180, 180]."), CitiesFile, *C.Id, C.Row.Longitude));
		}

		if (C.Row.Playable && C.Row.District.TrimStartAndEnd().IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': cidade jogável precisa de distrito (District)."), CitiesFile, *C.Id));
		}

		const FString Region = C.Row.RegionId.ToLower();
		CheckOrder(CitiesFile, C.Id, C.Row.Order, Region, FString::Printf(TEXT(" na região '%s'"), *C.Row.RegionId), SeenOrders, Errors);
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateLandmarks(const TArray<FDNLandmarkEntry>& Landmarks, const TArray<FDNCityEntry>& Cities)
{
	TArray<FString> Errors;
	CheckIds(LandmarksFile, Landmarks, Errors);

	const TSet<FString> CityIds = LowerIds(Cities);
	for (const FDNLandmarkEntry& L : Landmarks)
	{
		if (L.Row.Name.TrimStartAndEnd().IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o nome está vazio."), LandmarksFile, *L.Id));
		}

		if (L.Row.CityId.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': falta a cidade (CityId)."), LandmarksFile, *L.Id));
		}
		else if (!CityIds.Contains(L.Row.CityId.ToLower()))
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': a cidade '%s' não existe em %s."), LandmarksFile, *L.Id, *L.Row.CityId, CitiesFile));
		}

		const bool bHasText = !L.Row.ShortText.TrimStartAndEnd().IsEmpty();
		if (bHasText && L.Row.NeedsReview)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o texto ainda está marcado para revisão (NeedsReview=true); só entra depois de conferido em fonte confiável."), LandmarksFile, *L.Id));
		}
		if (bHasText && L.Row.SourceNote.TrimStartAndEnd().IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': texto sem fonte (SourceNote vazio)."), LandmarksFile, *L.Id));
		}
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateCrossings(const TArray<FDNCrossingEntry>& Crossings, const TArray<FDNCityEntry>& Cities)
{
	TArray<FString> Errors;
	CheckIds(CrossingsFile, Crossings, Errors);

	const TSet<FString> CityIds = LowerIds(Cities);
	TMap<FString, TMap<int32, FString>> SeenOrders;
	for (const FDNCrossingEntry& X : Crossings)
	{
		if (X.Row.CityId.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': falta a cidade (CityId)."), CrossingsFile, *X.Id));
		}
		else if (!CityIds.Contains(X.Row.CityId.ToLower()))
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': a cidade '%s' não existe em %s."), CrossingsFile, *X.Id, *X.Row.CityId, CitiesFile));
		}

		CheckOrder(CrossingsFile, X.Id, X.Row.Order, X.Row.CityId.ToLower(), FString::Printf(TEXT(" na cidade '%s'"), *X.Row.CityId), SeenOrders, Errors);

		FDNMedalThresholds Limits;
		Limits.Bronze = X.Row.Bronze;
		Limits.Silver = X.Row.Silver;
		Limits.Gold = X.Row.Gold;
		FString Why;
		if (!FDNMedals::Validate(Limits, &Why))
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': limites de medalha inválidos. %s"), CrossingsFile, *X.Id, *Why));
		}
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateTrafficVehicles(const TArray<FDNTrafficVehicleEntry>& Vehicles)
{
	TArray<FString> Errors;
	CheckIds(VehiclesFile, Vehicles, Errors);

	for (const FDNTrafficVehicleEntry& V : Vehicles)
	{
		if (V.Row.Name.TrimStartAndEnd().IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o nome está vazio."), VehiclesFile, *V.Id));
		}
		if (V.Row.Class.TrimStartAndEnd().IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': a classe (Class) está vazia."), VehiclesFile, *V.Id));
		}
		if (V.Row.ValueCash <= 0)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o valor em dinheiro (ValueCash) precisa ser maior que zero (veio %d)."), VehiclesFile, *V.Id, V.Row.ValueCash));
		}
		if (!FMath::IsFinite(V.Row.MassKg) || V.Row.MassKg <= 0.0f)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': a massa (MassKg) precisa ser um número maior que zero (veio %f)."), VehiclesFile, *V.Id, V.Row.MassKg));
		}
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateAll(const FDNData& Data)
{
	TArray<FString> Errors;
	Errors.Append(ValidateRegions(Data.Regions));
	Errors.Append(ValidateCities(Data.Cities, Data.Regions));
	Errors.Append(ValidateLandmarks(Data.Landmarks, Data.Cities));
	Errors.Append(ValidateCrossings(Data.Crossings, Data.Cities));
	Errors.Append(ValidateTrafficVehicles(Data.Vehicles));
	return Errors;
}
