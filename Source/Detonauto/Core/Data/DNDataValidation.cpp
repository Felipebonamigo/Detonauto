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
	constexpr const TCHAR* SettingsFile = TEXT("TrafficSettings.csv");

	/** Decimal finito e maior que zero. Devolve verdadeiro se está ok; senão registra o erro. */
	bool CheckPositiveFinite(const TCHAR* File, const FString& RowId, const TCHAR* Field, float Value, TArray<FString>& Errors)
	{
		if (!FMath::IsFinite(Value))
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo '%s' precisa ser um número finito (veio %f)."), File, *RowId, Field, Value));
			return false;
		}
		if (Value <= 0.0f)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo '%s' precisa ser maior que zero (veio %f)."), File, *RowId, Field, Value));
			return false;
		}
		return true;
	}

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

TArray<FString> FDNDataValidation::ValidateTrafficSettings(const TArray<FDNTrafficSettingsEntry>& Settings)
{
	TArray<FString> Errors;
	CheckIds(SettingsFile, Settings, Errors);

	if (Settings.Num() != 1)
	{
		Errors.Add(FString::Printf(TEXT("%s: precisa ter exatamente uma linha (veio %d)."), SettingsFile, Settings.Num()));
	}

	for (const FDNTrafficSettingsEntry& S : Settings)
	{
		const FDNTrafficSettingsRow& R = S.Row;
		const bool bWake = CheckPositiveFinite(SettingsFile, S.Id, TEXT("WakeDistanceM"), R.WakeDistanceM, Errors);
		const bool bAhead = CheckPositiveFinite(SettingsFile, S.Id, TEXT("WakeAheadM"), R.WakeAheadM, Errors);
		CheckPositiveFinite(SettingsFile, S.Id, TEXT("WakeAheadMinClosingKmh"), R.WakeAheadMinClosingKmh, Errors);
		const bool bDespawn = CheckPositiveFinite(SettingsFile, S.Id, TEXT("DespawnDistanceM"), R.DespawnDistanceM, Errors);
		CheckPositiveFinite(SettingsFile, S.Id, TEXT("StillSpeedMs"), R.StillSpeedMs, Errors);
		if (R.MaxSpawnPerFrame <= 0)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'MaxSpawnPerFrame' precisa ser maior que zero (veio %d)."), SettingsFile, *S.Id, R.MaxSpawnPerFrame));
		}

		if (bWake && bAhead && R.WakeDistanceM >= R.WakeAheadM)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': WakeDistanceM (%f) precisa ser menor que WakeAheadM (%f)."), SettingsFile, *S.Id, R.WakeDistanceM, R.WakeAheadM));
		}
		if (bAhead && bDespawn && R.WakeAheadM >= R.DespawnDistanceM)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': WakeAheadM (%f) precisa ser menor que DespawnDistanceM (%f)."), SettingsFile, *S.Id, R.WakeAheadM, R.DespawnDistanceM));
		}
	}
	return Errors;
}

namespace
{
	constexpr const TCHAR* TurboFile = TEXT("Turbo.csv");
	constexpr const TCHAR* CrashFile = TEXT("Crash.csv");
	constexpr const TCHAR* TakedownFile = TEXT("Takedown.csv");
	constexpr const TCHAR* DamageFile = TEXT("Damage.csv");

	/** Decimal finito e dentro da faixa (extremos inclusivos ou não). Devolve verdadeiro se está ok; senão registra o erro. */
	bool CheckRange(const TCHAR* File, const FString& RowId, const TCHAR* Field, float Value,
		float Min, bool bMinInclusive, float Max, bool bMaxInclusive, TArray<FString>& Errors)
	{
		const bool bFinite = FMath::IsFinite(Value);
		const bool bAboveMin = bMinInclusive ? Value >= Min : Value > Min;
		const bool bBelowMax = bMaxInclusive ? Value <= Max : Value < Max;
		if (bFinite && bAboveMin && bBelowMax)
		{
			return true;
		}
		Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo '%s' precisa ser um número finito em %c%g, %g%c (veio %f)."),
			File, *RowId, Field, bMinInclusive ? TEXT('[') : TEXT('('), Min, Max, bMaxInclusive ? TEXT(']') : TEXT(')'), Value));
		return false;
	}

	constexpr float Huge = 3.0e38f;

	/** Maior que zero. */
	bool CheckAboveZero(const TCHAR* File, const FString& Id, const TCHAR* Field, float Value, TArray<FString>& Errors)
	{
		return CheckRange(File, Id, Field, Value, 0.0f, false, Huge, true, Errors);
	}

	/** Maior ou igual a zero. */
	bool CheckNotNegative(const TCHAR* File, const FString& Id, const TCHAR* Field, float Value, TArray<FString>& Errors)
	{
		return CheckRange(File, Id, Field, Value, 0.0f, true, Huge, true, Errors);
	}

	/** Mensagem comum de "uma linha só". */
	template <typename RowType>
	void CheckSingleRow(const TCHAR* File, const TArray<TDNTableEntry<RowType>>& Rows, TArray<FString>& Errors)
	{
		CheckIds(File, Rows, Errors);
		if (Rows.Num() != 1)
		{
			Errors.Add(FString::Printf(TEXT("%s: precisa ter exatamente uma linha (veio %d)."), File, Rows.Num()));
		}
	}
}

TArray<FString> FDNDataValidation::ValidateTurbo(const TArray<FDNTurboConfigEntry>& Rows)
{
	TArray<FString> Errors;
	CheckSingleRow(TurboFile, Rows, Errors);

	for (const FDNTurboConfigEntry& E : Rows)
	{
		const FDNTurboConfigRow& R = E.Row;
		const FString& Id = E.Id;
		const bool bMaxBar = CheckAboveZero(TurboFile, Id, TEXT("MaxBar"), R.MaxBar, Errors);
		CheckNotNegative(TurboFile, Id, TEXT("WrongWayMinSpeedKmh"), R.WrongWayMinSpeedKmh, Errors);
		CheckNotNegative(TurboFile, Id, TEXT("WrongWayGainPerSecond"), R.WrongWayGainPerSecond, Errors);
		CheckAboveZero(TurboFile, Id, TEXT("NearMissMaxLateralCm"), R.NearMissMaxLateralCm, Errors);
		CheckNotNegative(TurboFile, Id, TEXT("NearMissMinSpeedKmh"), R.NearMissMinSpeedKmh, Errors);
		CheckNotNegative(TurboFile, Id, TEXT("NearMissGain"), R.NearMissGain, Errors);
		CheckAboveZero(TurboFile, Id, TEXT("NearMissChainWindowSeconds"), R.NearMissChainWindowSeconds, Errors);

		if (R.NearMissChainMultipliers.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': NearMissChainMultipliers não pode ser vazio."), TurboFile, *Id));
		}
		for (int32 i = 0; i < R.NearMissChainMultipliers.Num(); ++i)
		{
			const float M = R.NearMissChainMultipliers[i];
			if (!FMath::IsFinite(M) || M <= 0.0f)
			{
				Errors.Add(FString::Printf(TEXT("%s, linha '%s': NearMissChainMultipliers[%d] precisa ser um número finito maior que zero (veio %f)."), TurboFile, *Id, i, M));
			}
		}

		// Derrapada: 0 <= mínimo < máximo <= 90 (acima de 90 o carro anda de ré, não derrapa).
		const bool bDriftMin = CheckNotNegative(TurboFile, Id, TEXT("DriftMinAngleDeg"), R.DriftMinAngleDeg, Errors);
		const bool bDriftMax = CheckRange(TurboFile, Id, TEXT("DriftMaxAngleDeg"), R.DriftMaxAngleDeg, 0.0f, false, 90.0f, true, Errors);
		if (bDriftMin && bDriftMax && R.DriftMinAngleDeg >= R.DriftMaxAngleDeg)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': DriftMinAngleDeg (%f) precisa ser menor que DriftMaxAngleDeg (%f)."), TurboFile, *Id, R.DriftMinAngleDeg, R.DriftMaxAngleDeg));
		}
		CheckNotNegative(TurboFile, Id, TEXT("DriftMinSpeedKmh"), R.DriftMinSpeedKmh, Errors);
		CheckNotNegative(TurboFile, Id, TEXT("DriftGainPerSecond"), R.DriftGainPerSecond, Errors);
		CheckNotNegative(TurboFile, Id, TEXT("AirTimeThresholdSeconds"), R.AirTimeThresholdSeconds, Errors);
		CheckNotNegative(TurboFile, Id, TEXT("AirGainPerSecond"), R.AirGainPerSecond, Errors);
		CheckNotNegative(TurboFile, Id, TEXT("AirMaxGainPerJump"), R.AirMaxGainPerJump, Errors);

		// O Takedown enche a barra até este valor: dentro de (0, MaxBar].
		const bool bSets = CheckAboveZero(TurboFile, Id, TEXT("TakedownSetsBarTo"), R.TakedownSetsBarTo, Errors);
		if (bMaxBar && bSets && R.TakedownSetsBarTo > R.MaxBar)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': TakedownSetsBarTo (%f) não pode passar de MaxBar (%f)."), TurboFile, *Id, R.TakedownSetsBarTo, R.MaxBar));
		}
		CheckAboveZero(TurboFile, Id, TEXT("ConsumptionPerSecond"), R.ConsumptionPerSecond, Errors);
		CheckNotNegative(TurboFile, Id, TEXT("ExtendedBonus"), R.ExtendedBonus, Errors);
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateCrash(const TArray<FDNCrashConfigEntry>& Rows)
{
	TArray<FString> Errors;
	CheckSingleRow(CrashFile, Rows, Errors);

	for (const FDNCrashConfigEntry& E : Rows)
	{
		const FDNCrashConfigRow& R = E.Row;
		if (R.MaxMultiplier < 1)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'MaxMultiplier' precisa ser pelo menos 1 (veio %d)."), CrashFile, *E.Id, R.MaxMultiplier));
		}
		CheckAboveZero(CrashFile, E.Id, TEXT("ChainWindowSeconds"), R.ChainWindowSeconds, Errors);
		if (R.ChainBonusPercentPerLink < 0)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'ChainBonusPercentPerLink' não pode ser negativo (veio %d)."), CrashFile, *E.Id, R.ChainBonusPercentPerLink));
		}
		if (R.ChainBonusMaxPercent < 0)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'ChainBonusMaxPercent' não pode ser negativo (veio %d)."), CrashFile, *E.Id, R.ChainBonusMaxPercent));
		}
		CheckNotNegative(CrashFile, E.Id, TEXT("QuietSpeedMetersPerSecond"), R.QuietSpeedMetersPerSecond, Errors);
		const bool bQuiet = CheckAboveZero(CrashFile, E.Id, TEXT("QuietSecondsToEnd"), R.QuietSecondsToEnd, Errors);
		const bool bMaxAfter = CheckAboveZero(CrashFile, E.Id, TEXT("MaxSecondsAfterFirstImpact"), R.MaxSecondsAfterFirstImpact, Errors);
		// Senão a regra do silêncio nunca encerraria a rodada: o limite de tempo sempre chegaria antes.
		if (bQuiet && bMaxAfter && R.QuietSecondsToEnd > R.MaxSecondsAfterFirstImpact)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': QuietSecondsToEnd (%f) não pode passar de MaxSecondsAfterFirstImpact (%f)."), CrashFile, *E.Id, R.QuietSecondsToEnd, R.MaxSecondsAfterFirstImpact));
		}
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateTakedown(const TArray<FDNTakedownConfigEntry>& Rows)
{
	TArray<FString> Errors;
	CheckSingleRow(TakedownFile, Rows, Errors);

	for (const FDNTakedownConfigEntry& E : Rows)
	{
		const FDNTakedownConfigRow& R = E.Row;
		const FString& Id = E.Id;
		CheckNotNegative(TakedownFile, Id, TEXT("MinRelativeSpeedKmh"), R.MinRelativeSpeedKmh, Errors);
		CheckAboveZero(TakedownFile, Id, TEXT("RivalCrashWindowSeconds"), R.RivalCrashWindowSeconds, Errors);
		CheckAboveZero(TakedownFile, Id, TEXT("VerticalMinHeightCm"), R.VerticalMinHeightCm, Errors);
		CheckRange(TakedownFile, Id, TEXT("BehindMaxAngleDegrees"), R.BehindMaxAngleDegrees, 0.0f, false, 180.0f, true, Errors);
		const bool bSide = CheckRange(TakedownFile, Id, TEXT("SideMaxAngleDegrees"), R.SideMaxAngleDegrees, 0.0f, false, 180.0f, true, Errors);

		const bool bTMin = CheckNotNegative(TakedownFile, Id, TEXT("TBoneMinAngleDegrees"), R.TBoneMinAngleDegrees, Errors);
		const bool bTMax = CheckRange(TakedownFile, Id, TEXT("TBoneMaxAngleDegrees"), R.TBoneMaxAngleDegrees, 0.0f, false, 180.0f, true, Errors);
		if (bTMin && bTMax && R.TBoneMinAngleDegrees >= R.TBoneMaxAngleDegrees)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': TBoneMinAngleDegrees (%f) precisa ser menor que TBoneMaxAngleDegrees (%f)."), TakedownFile, *Id, R.TBoneMinAngleDegrees, R.TBoneMaxAngleDegrees));
		}

		// A lateral termina onde (ou antes de) o T começa: sem sobreposição; um vão entre as duas é permitido.
		if (bSide && bTMin && R.SideMaxAngleDegrees > R.TBoneMinAngleDegrees)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': SideMaxAngleDegrees (%f) não pode passar de TBoneMinAngleDegrees (%f)."), TakedownFile, *Id, R.SideMaxAngleDegrees, R.TBoneMinAngleDegrees));
		}

		// Quatro zonas de 2 x meio ângulo cabem em 360 graus sem se sobrepor só com meio ângulo < 90.
		CheckRange(TakedownFile, Id, TEXT("ZoneHalfAngleDegrees"), R.ZoneHalfAngleDegrees, 0.0f, false, 90.0f, false, Errors);
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateDamage(const TArray<FDNDamageConfigEntry>& Rows)
{
	TArray<FString> Errors;
	CheckSingleRow(DamageFile, Rows, Errors);

	for (const FDNDamageConfigEntry& E : Rows)
	{
		const FDNDamageConfigRow& R = E.Row;
		const FString& Id = E.Id;
		CheckAboveZero(DamageFile, Id, TEXT("DentMinDeltaVMs"), R.DentMinDeltaVMs, Errors);
		CheckAboveZero(DamageFile, Id, TEXT("ZoneDestroyDeltaVMs"), R.ZoneDestroyDeltaVMs, Errors);
		CheckAboveZero(DamageFile, Id, TEXT("DentMaxDepthCm"), R.DentMaxDepthCm, Errors);
		CheckAboveZero(DamageFile, Id, TEXT("DentDepthPerMs"), R.DentDepthPerMs, Errors);
		CheckAboveZero(DamageFile, Id, TEXT("DentBaseRadiusCm"), R.DentBaseRadiusCm, Errors);
		if (R.MaxDents < 1)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'MaxDents' precisa ser pelo menos 1 (veio %d)."), DamageFile, *Id, R.MaxDents));
		}
		CheckAboveZero(DamageFile, Id, TEXT("PartDetachDeltaVMs"), R.PartDetachDeltaVMs, Errors);
		CheckRange(DamageFile, Id, TEXT("PartDetachZoneDamage"), R.PartDetachZoneDamage, 0.0f, false, 1.0f, true, Errors);
		CheckRange(DamageFile, Id, TEXT("DestroyedTotalDamage"), R.DestroyedTotalDamage, 0.0f, false, 1.0f, true, Errors);
		if (R.MaxActive < 1)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'MaxActive' precisa ser pelo menos 1 (veio %d)."), DamageFile, *Id, R.MaxActive));
		}
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateCars(const TArray<FDNCarConfigEntry>& Rows)
{
	constexpr const TCHAR* CarsFile = TEXT("Cars.csv");
	// Faixas fechadas: as mesmas que o protótipo de teste limitava em silêncio (Sandbox, lote 8). Aqui são recusadas.
	// Produto mínimo da aderência traseira com o freio de mão cheio: medido no Chaos 5.8 com o freio a 3000
	// (docs/presets-carro.md): 0,32 = 12 graus de deslizamento; 0,30 = 37 a 41; 0,294 = 77; 0,28 = 90 (o carro roda).
	constexpr float MinRearGripProduct = 0.295f;

	TArray<FString> Errors;
	CheckIds(CarsFile, Rows, Errors);
	if (!Rows.ContainsByPredicate([](const FDNCarConfigEntry& E) { return E.Id.Equals(TEXT("Padrao"), ESearchCase::IgnoreCase); }))
	{
		Errors.Add(FString::Printf(TEXT("%s: falta a linha 'Padrao' (é o carro do template e o usado quando o preset pedido não existe)."), CarsFile));
	}

	for (const FDNCarConfigEntry& E : Rows)
	{
		const FDNCarConfigRow& R = E.Row;
		const FString& Id = E.Id;
		auto Range = [&](const TCHAR* Field, float Value, float Min, float Max) { return CheckRange(CarsFile, Id, Field, Value, Min, true, Max, true, Errors); };
		Range(TEXT("MassKg"), R.MassKg, 100.0f, 10000.0f);
		Range(TEXT("MaxTorque"), R.MaxTorque, 50.0f, 5000.0f);
		Range(TEXT("LateralGrip"), R.LateralGrip, 0.1f, 5.0f);
		Range(TEXT("SuspensionStiffness"), R.SuspensionStiffness, 10.0f, 2000.0f);
		Range(TEXT("SteeringGain"), R.SteeringGain, 0.1f, 3.0f);
		Range(TEXT("CenterOfMassDropCm"), R.CenterOfMassDropCm, 0.0f, 60.0f);
		Range(TEXT("SuspensionDampingRatio"), R.SuspensionDampingRatio, 0.05f, 1.0f);
		Range(TEXT("SuspensionTravelCm"), R.SuspensionTravelCm, 2.0f, 40.0f);
		Range(TEXT("RollbarScaling"), R.RollbarScaling, 0.0f, 1.0f);
		const bool bRear = Range(TEXT("RearGripFactor"), R.RearGripFactor, 0.2f, 2.0f);
		const bool bBrake = Range(TEXT("HandbrakeRearGrip"), R.HandbrakeRearGrip, 0.05f, 1.5f);
		Range(TEXT("HandbrakeRampInSeconds"), R.HandbrakeRampInSeconds, 0.01f, 3.0f);
		Range(TEXT("HandbrakeRampOutSeconds"), R.HandbrakeRampOutSeconds, 0.01f, 3.0f);
		Range(TEXT("HandbrakeTorque"), R.HandbrakeTorque, 0.0f, 20000.0f);
		Range(TEXT("CameraDistanceCm"), R.CameraDistanceCm, 150.0f, 2000.0f);
		Range(TEXT("CameraHeightCm"), R.CameraHeightCm, 0.0f, 600.0f);
		Range(TEXT("CameraFov"), R.CameraFov, 60.0f, 120.0f);
		Range(TEXT("NitroAccelMps2"), R.NitroAccelMps2, 0.0f, 40.0f);

		if (bRear && bBrake && R.RearGripFactor * R.HandbrakeRearGrip < MinRearGripProduct)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': RearGripFactor (%.3f) x HandbrakeRearGrip (%.3f) = %.4f, abaixo do mínimo 0,295: com o freio de mão cheio o carro roda (docs/presets-carro.md). Ajuste os dois campos em passos de 0,02."),
				CarsFile, *Id, R.RearGripFactor, R.HandbrakeRearGrip, R.RearGripFactor * R.HandbrakeRearGrip));
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
	Errors.Append(ValidateTrafficSettings(Data.TrafficSettings));
	Errors.Append(ValidateTurbo(Data.TurboConfig));
	Errors.Append(ValidateCrash(Data.CrashConfig));
	Errors.Append(ValidateTakedown(Data.TakedownConfig));
	Errors.Append(ValidateDamage(Data.DamageConfig));
	Errors.Append(ValidateCars(Data.CarConfigs));
	return Errors;
}
