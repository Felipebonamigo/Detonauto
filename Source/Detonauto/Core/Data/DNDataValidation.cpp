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

		// lote 13b: seguir o carro da frente e rolar depois de acordar
		const bool bMinGap = CheckPositiveFinite(SettingsFile, S.Id, TEXT("FollowMinGapM"), R.FollowMinGapM, Errors);
		const bool bCatchUp = CheckPositiveFinite(SettingsFile, S.Id, TEXT("FollowCatchUpGapM"), R.FollowCatchUpGapM, Errors);
		if (bMinGap && bCatchUp && R.FollowMinGapM >= R.FollowCatchUpGapM)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': FollowMinGapM (%f) precisa ser menor que FollowCatchUpGapM (%f)."), SettingsFile, *S.Id, R.FollowMinGapM, R.FollowCatchUpGapM));
		}
		if (CheckPositiveFinite(SettingsFile, S.Id, TEXT("RollingFriction"), R.RollingFriction, Errors) && R.RollingFriction > 1.0f)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': RollingFriction precisa ser no máximo 1 (veio %f)."), SettingsFile, *S.Id, R.RollingFriction));
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
		// lote 17: alvos de deformação por zona
		CheckRange(DamageFile, Id, TEXT("MorphCurveExponent"), R.MorphCurveExponent, 0.0f, false, 4.0f, true, Errors);
		CheckRange(DamageFile, Id, TEXT("MorphRisePerSecond"), R.MorphRisePerSecond, 0.0f, false, 100.0f, true, Errors);
		// lote 17b: desgaste da pintura
		CheckRange(DamageFile, Id, TEXT("WearChipCoverage"), R.WearChipCoverage, 0.0f, false, 1.0f, true, Errors);
		CheckRange(DamageFile, Id, TEXT("WearChipSizeCm"), R.WearChipSizeCm, 0.0f, false, 200.0f, true, Errors);
		CheckRange(DamageFile, Id, TEXT("WearScratch"), R.WearScratch, 0.0f, true, 1.0f, true, Errors);
		CheckRange(DamageFile, Id, TEXT("WearSoot"), R.WearSoot, 0.0f, true, 1.0f, true, Errors);
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
		Range(TEXT("NitroAccelMps2"), R.NitroAccelMps2, 0.0f, 40.0f);
		Range(TEXT("DriftAssistStartDeg"), R.DriftAssistStartDeg, 0.0f, 80.0f);
		const bool bAssistGain = Range(TEXT("DriftAssistGain"), R.DriftAssistGain, 0.0f, 500.0f);
		const bool bAssistMax = Range(TEXT("DriftAssistMax"), R.DriftAssistMax, 0.0f, 2000.0f);
		// ganho e teto são um par: os dois zero (desligada) ou os dois maiores que zero; um só não faz nada e é erro de digitação
		if (bAssistGain && bAssistMax && (R.DriftAssistGain == 0.0f) != (R.DriftAssistMax == 0.0f))
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': DriftAssistGain (%g) e DriftAssistMax (%g) precisam ser os dois zero (assistência desligada) ou os dois maiores que zero."), CarsFile, *Id, R.DriftAssistGain, R.DriftAssistMax));
		}

		if (bRear && bBrake && R.RearGripFactor * R.HandbrakeRearGrip < MinRearGripProduct)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': RearGripFactor (%.3f) x HandbrakeRearGrip (%.3f) = %.4f, abaixo do mínimo 0,295: com o freio de mão cheio o carro roda (docs/presets-carro.md). Ajuste os dois campos em passos de 0,02."),
				CarsFile, *Id, R.RearGripFactor, R.HandbrakeRearGrip, R.RearGripFactor * R.HandbrakeRearGrip));
		}
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateCameras(const TArray<FDNCameraConfigEntry>& Rows)
{
	constexpr const TCHAR* CamerasFile = TEXT("Cameras.csv");
	TArray<FString> Errors;
	CheckIds(CamerasFile, Rows, Errors);
	if (!Rows.ContainsByPredicate([](const FDNCameraConfigEntry& E) { return E.Id.Equals(TEXT("alta-longe"), ESearchCase::IgnoreCase); }))
	{
		Errors.Add(FString::Printf(TEXT("%s: falta a linha 'alta-longe' (é a câmera padrão do carro)."), CamerasFile));
	}

	for (const FDNCameraConfigEntry& E : Rows)
	{
		const FDNCameraConfigRow& R = E.Row;
		const FString& Id = E.Id;
		auto Range = [&](const TCHAR* Field, float Value, float Min, float Max) { return CheckRange(CamerasFile, Id, Field, Value, Min, true, Max, true, Errors); };
		EDNCameraAnchor Anchor = EDNCameraAnchor::Chase;
		const bool bAnchor = FDNCameraCycle::ParseAnchor(R.Anchor, Anchor);
		if (!bAnchor)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'Anchor' precisa ser Chase ou Car (veio '%s')."), CamerasFile, *Id, *R.Anchor));
		}
		// Chase: o braço tem comprimento; Car: a câmera está presa ao carro e a distância precisa ser 0
		if (bAnchor && Anchor == EDNCameraAnchor::Car)
		{
			if (R.DistanceCm != 0.0f)
			{
				Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'DistanceCm' precisa ser 0 quando Anchor é Car (veio %g)."), CamerasFile, *Id, R.DistanceCm));
			}
		}
		else
		{
			Range(TEXT("DistanceCm"), R.DistanceCm, 150.0f, 3000.0f);
		}
		Range(TEXT("HeightCm"), R.HeightCm, -50.0f, 800.0f);
		Range(TEXT("ForwardCm"), R.ForwardCm, -300.0f, 400.0f);
		Range(TEXT("SideCm"), R.SideCm, -150.0f, 150.0f);
		Range(TEXT("PitchDeg"), R.PitchDeg, -45.0f, 45.0f);
		Range(TEXT("Fov"), R.Fov, 40.0f, 120.0f);
		Range(TEXT("Smoothing"), R.Smoothing, 0.0f, 30.0f);
		Range(TEXT("SpeedFovScale"), R.SpeedFovScale, 0.0f, 2.0f);   // lote 18: quanto do campo de visão por velocidade o modo usa
		Range(TEXT("ShakeScale"), R.ShakeScale, 0.0f, 3.0f);         // lote 18: quanto do tremor o modo usa
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
	Errors.Append(ValidateCameras(Data.CameraModes));
	Errors.Append(ValidateBreakables(Data.Breakables));
	Errors.Append(ValidateBreakableSettings(Data.BreakableSettings));
	Errors.Append(ValidateCameraFx(Data.CameraFx));
	Errors.Append(ValidateCarParts(Data.CarParts));
	Errors.Append(ValidateCarGlass(Data.CarGlass));
	Errors.Append(ValidateEffects(Data.Effects, Data.Breakables));
	Errors.Append(ValidateEffectsSettings(Data.EffectsSettings));
	Errors.Append(ValidateCoursePoints(Data.CoursePoints));
	Errors.Append(ValidateRivals(Data.Rivals));
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateCameraFx(const TArray<FDNCameraFxEntry>& Rows)
{
	constexpr const TCHAR* CameraFxFile = TEXT("CameraFx.csv");
	TArray<FString> Errors;
	if (Rows.Num() != 1)
	{
		Errors.Add(FString::Printf(TEXT("%s: precisa ter exatamente uma linha (veio %d)."), CameraFxFile, Rows.Num()));
	}
	CheckIds(CameraFxFile, Rows, Errors);
	for (const FDNCameraFxEntry& E : Rows)
	{
		const FDNCameraFxRow& R = E.Row;
		auto Range = [&](const TCHAR* Field, float Value, float Min, bool bMinIn, float Max, bool bMaxIn) { CheckRange(CameraFxFile, E.Id, Field, Value, Min, bMinIn, Max, bMaxIn, Errors); };
		// as rampas de velocidade: começo em [0, 400] e fim maior que o começo, até 500
		auto RampCheck = [&](const TCHAR* StartField, float Start, const TCHAR* FullField, float Full)
		{
			Range(StartField, Start, 0.0f, true, 400.0f, true);
			Range(FullField, Full, 0.0f, false, 500.0f, true);
			if (FMath::IsFinite(Start) && FMath::IsFinite(Full) && Full <= Start)
			{
				Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo '%s' (%g) precisa ser maior que '%s' (%g)."), CameraFxFile, *E.Id, FullField, Full, StartField, Start));
			}
		};
		RampCheck(TEXT("SpeedFovStartKmh"), R.SpeedFovStartKmh, TEXT("SpeedFovFullKmh"), R.SpeedFovFullKmh);
		RampCheck(TEXT("ShakeStartKmh"), R.ShakeStartKmh, TEXT("ShakeFullKmh"), R.ShakeFullKmh);
		RampCheck(TEXT("EdgeBlurStartKmh"), R.EdgeBlurStartKmh, TEXT("EdgeBlurFullKmh"), R.EdgeBlurFullKmh);
		Range(TEXT("SpeedFovMaxAddDeg"), R.SpeedFovMaxAddDeg, 0.0f, true, 30.0f, true);
		Range(TEXT("TurboFovAddDeg"), R.TurboFovAddDeg, 0.0f, true, 30.0f, true);
		Range(TEXT("MaxFovDeg"), R.MaxFovDeg, 60.0f, false, 140.0f, true);
		Range(TEXT("FovBlendDegPerS"), R.FovBlendDegPerS, 0.0f, false, 360.0f, true);
		Range(TEXT("ShakeMaxDeg"), R.ShakeMaxDeg, 0.0f, true, 5.0f, true);
		Range(TEXT("ShakeHz"), R.ShakeHz, 0.0f, false, 60.0f, true);
		Range(TEXT("EdgeBlurMax"), R.EdgeBlurMax, 0.0f, true, 1.0f, true);
		Range(TEXT("EdgeBlurInnerRadius"), R.EdgeBlurInnerRadius, 0.0f, true, 1.0f, false);
		Range(TEXT("CrashCamMinDeltaVMs"), R.CrashCamMinDeltaVMs, 0.0f, false, 100.0f, true);
		Range(TEXT("CrashCamTimeScale"), R.CrashCamTimeScale, 0.0f, false, 1.0f, true);
		Range(TEXT("CrashCamEnterS"), R.CrashCamEnterS, 0.0f, false, 5.0f, true);
		Range(TEXT("CrashCamHoldS"), R.CrashCamHoldS, 0.0f, true, 10.0f, true);
		Range(TEXT("CrashCamExitS"), R.CrashCamExitS, 0.0f, false, 5.0f, true);
		Range(TEXT("CrashCamCooldownS"), R.CrashCamCooldownS, 0.0f, true, 30.0f, true);
		Range(TEXT("CrashCamOrbitRadiusM"), R.CrashCamOrbitRadiusM, 0.0f, false, 50.0f, true);
		Range(TEXT("CrashCamOrbitHeightM"), R.CrashCamOrbitHeightM, 0.0f, true, 50.0f, true);
		Range(TEXT("CrashCamOrbitDegPerS"), R.CrashCamOrbitDegPerS, 0.0f, true, 360.0f, true);
		Range(TEXT("CrashCamProbeCm"), R.CrashCamProbeCm, 0.0f, false, 200.0f, true);
		Range(TEXT("CrashCamLeadDeg"), R.CrashCamLeadDeg, 0.0f, true, 180.0f, true);
		Range(TEXT("WreckSteerAccelMs2"), R.WreckSteerAccelMs2, 0.0f, true, 50.0f, true);
		Range(TEXT("WreckSteerMaxDeltaVMs"), R.WreckSteerMaxDeltaVMs, 0.0f, true, 50.0f, true);
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateBreakables(const TArray<FDNBreakableEntry>& Rows)
{
	constexpr const TCHAR* BreakablesFile = TEXT("Breakables.csv");
	TArray<FString> Errors;
	if (Rows.IsEmpty())
	{
		Errors.Add(FString::Printf(TEXT("%s: nenhuma linha (pelo menos um tipo de peça de rua)."), BreakablesFile));
	}
	CheckIds(BreakablesFile, Rows, Errors);
	for (const FDNBreakableEntry& E : Rows)
	{
		const FDNBreakableRow& R = E.Row;
		CheckRange(BreakablesFile, E.Id, TEXT("MassKg"), R.MassKg, 0.0f, false, 5000.0f, true, Errors);
		CheckRange(BreakablesFile, E.Id, TEXT("MinBreakSpeedKmh"), R.MinBreakSpeedKmh, 0.0f, true, 200.0f, true, Errors);
		CheckRange(BreakablesFile, E.Id, TEXT("LaunchFactor"), R.LaunchFactor, 0.0f, true, 3.0f, true, Errors);
		CheckRange(BreakablesFile, E.Id, TEXT("LaunchUpMs"), R.LaunchUpMs, 0.0f, true, 20.0f, true, Errors);
		CheckRange(BreakablesFile, E.Id, TEXT("BreakHeightCm"), R.BreakHeightCm, 0.0f, true, 500.0f, true, Errors);
		if (R.Points < 0)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'Points' não pode ser negativo (veio %d)."), BreakablesFile, *E.Id, R.Points));
		}
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateBreakableSettings(const TArray<FDNBreakableSettingsEntry>& Rows)
{
	constexpr const TCHAR* BreakableSettingsFile = TEXT("BreakableSettings.csv");
	TArray<FString> Errors;
	if (Rows.Num() != 1)
	{
		Errors.Add(FString::Printf(TEXT("%s: precisa ter exatamente uma linha (veio %d)."), BreakableSettingsFile, Rows.Num()));
	}
	CheckIds(BreakableSettingsFile, Rows, Errors);
	for (const FDNBreakableSettingsEntry& E : Rows)
	{
		const FDNBreakableSettingsRow& R = E.Row;
		CheckRange(BreakableSettingsFile, E.Id, TEXT("ArcadeMassFactor"), R.ArcadeMassFactor, 0.0f, false, 10.0f, true, Errors);
		CheckRange(BreakableSettingsFile, E.Id, TEXT("MaxSpeedLossPct"), R.MaxSpeedLossPct, 0.0f, true, 100.0f, true, Errors);
		CheckRange(BreakableSettingsFile, E.Id, TEXT("MaxLaunchMs"), R.MaxLaunchMs, 0.0f, false, 100.0f, true, Errors);
		if (R.MaxActiveProps < 1)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'MaxActiveProps' precisa ser pelo menos 1 (veio %d)."), BreakableSettingsFile, *E.Id, R.MaxActiveProps));
		}
		CheckRange(BreakableSettingsFile, E.Id, TEXT("SleepSpeedMs"), R.SleepSpeedMs, 0.0f, false, 10.0f, true, Errors);
		CheckRange(BreakableSettingsFile, E.Id, TEXT("SleepAfterS"), R.SleepAfterS, 0.0f, false, 60.0f, true, Errors);
		CheckRange(BreakableSettingsFile, E.Id, TEXT("PlowFactor"), R.PlowFactor, 1.0f, true, 3.0f, true, Errors);
		CheckRange(BreakableSettingsFile, E.Id, TEXT("PlowUpMs"), R.PlowUpMs, 0.0f, true, 10.0f, true, Errors);
	}
	return Errors;
}

namespace
{
	/** Zona e secundária em texto: a zona conhecida; a secundária vazia ou conhecida e diferente da zona. */
	void CheckZones(const TCHAR* File, const FString& Id, const FString& Zone, const FString& Secondary, TArray<FString>& Errors)
	{
		EDNDamageZone Z = EDNDamageZone::Front;
		const bool bZone = FDNCarParts::ParseZone(Zone, Z);
		if (!bZone)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'Zone' tem zona desconhecida '%s' (Front, Rear, Left, Right, Roof ou Under)."), File, *Id, *Zone));
		}
		if (Secondary.TrimStartAndEnd().IsEmpty())
		{
			return;
		}
		EDNDamageZone S = EDNDamageZone::Front;
		if (!FDNCarParts::ParseZone(Secondary, S))
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'SecondaryZone' tem zona desconhecida '%s' (vazia = sem secundária)."), File, *Id, *Secondary));
		}
		else if (bZone && S == Z)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'SecondaryZone' (%s) é igual à 'Zone'; deixe vazio."), File, *Id, *Secondary));
		}
	}
}

TArray<FString> FDNDataValidation::ValidateCarParts(const TArray<FDNCarPartEntry>& Rows)
{
	constexpr const TCHAR* File = TEXT("CarParts.csv");
	TArray<FString> Errors;
	if (Rows.IsEmpty())
	{
		Errors.Add(FString::Printf(TEXT("%s: nenhuma linha (pelo menos uma peça de um carro)."), File));
	}
	CheckIds(File, Rows, Errors);
	TMap<FString, FString> SeenPart; // "carro/peça" em minúsculas -> id da primeira linha
	for (const FDNCarPartEntry& E : Rows)
	{
		const FDNCarPartRow& R = E.Row;
		if (R.CarId.TrimStartAndEnd().IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'CarId' está vazio."), File, *E.Id));
		}
		EDNDamagePart Part = EDNDamagePart::Hood;
		if (!FDNCarParts::ParsePart(R.Part, Part))
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'Part' tem peça desconhecida '%s'."), File, *E.Id, *R.Part));
		}
		else
		{
			const FString Key = (R.CarId.TrimStartAndEnd() + TEXT("/") + FDNCarParts::PartName(Part)).ToLower();
			if (const FString* First = SeenPart.Find(Key))
			{
				Errors.Add(FString::Printf(TEXT("%s: a peça %s aparece duas vezes no carro '%s' (linhas '%s' e '%s')."), File, FDNCarParts::PartName(Part), *R.CarId, **First, *E.Id));
			}
			else
			{
				SeenPart.Add(Key, E.Id);
			}
		}
		CheckZones(File, E.Id, R.Zone, R.SecondaryZone, Errors);
		CheckRange(File, E.Id, TEXT("MassKg"), R.MassKg, 0.0f, false, 500.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("DetachDamage"), R.DetachDamage, 0.0f, false, 1.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("DetachDeltaVMs"), R.DetachDeltaVMs, 0.0f, false, 100.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("HingeDamping"), R.HingeDamping, 0.0f, true, 50.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("LaunchMs"), R.LaunchMs, 0.0f, true, 30.0f, true, Errors);
		EDNHingeEdge Edge = EDNHingeEdge::None;
		if (!FDNCarParts::ParseHingeEdge(R.HingeEdge, Edge))
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'HingeEdge' tem borda desconhecida '%s' (None, TopRear, TopFront, SideFront ou Lateral)."),
				File, *E.Id, *R.HingeEdge));
			continue;
		}
		if (Edge == EDNHingeEdge::None)
		{
			// sem dobradiça: pula de presa para solta; ângulo e limiares de pendurar em 0, para ninguém achar que valem
			for (const TPair<const TCHAR*, float>& Field : { TPair<const TCHAR*, float>(TEXT("HingeMaxDeg"), R.HingeMaxDeg),
				TPair<const TCHAR*, float>(TEXT("HangDamage"), R.HangDamage), TPair<const TCHAR*, float>(TEXT("HangDeltaVMs"), R.HangDeltaVMs) })
			{
				if (Field.Value != 0.0f)
				{
					Errors.Add(FString::Printf(TEXT("%s, linha '%s': sem dobradiça (HingeEdge None), o campo '%s' precisa ser 0 (veio %g)."), File, *E.Id, Field.Key, Field.Value));
				}
			}
			continue;
		}
		CheckRange(File, E.Id, TEXT("HingeMaxDeg"), R.HingeMaxDeg, 0.0f, false, 180.0f, true, Errors);
		// pendura antes de soltar: os limiares de pendurar ficam abaixo dos de soltar
		CheckRange(File, E.Id, TEXT("HangDamage"), R.HangDamage, 0.0f, false, FMath::IsFinite(R.DetachDamage) ? R.DetachDamage : 0.0f, false, Errors);
		CheckRange(File, E.Id, TEXT("HangDeltaVMs"), R.HangDeltaVMs, 0.0f, false, FMath::IsFinite(R.DetachDeltaVMs) ? R.DetachDeltaVMs : 0.0f, false, Errors);
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateCarGlass(const TArray<FDNCarGlassEntry>& Rows)
{
	constexpr const TCHAR* File = TEXT("CarGlass.csv");
	TArray<FString> Errors;
	if (Rows.IsEmpty())
	{
		Errors.Add(FString::Printf(TEXT("%s: nenhuma linha (pelo menos um vidro de um carro)."), File));
	}
	CheckIds(File, Rows, Errors);
	TMap<FString, FString> SeenGlass;
	for (const FDNCarGlassEntry& E : Rows)
	{
		const FDNCarGlassRow& R = E.Row;
		if (R.CarId.TrimStartAndEnd().IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'CarId' está vazio."), File, *E.Id));
		}
		EDNGlass Glass = EDNGlass::Windshield;
		if (!FDNGlass::ParseGlass(R.Glass, Glass))
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'Glass' tem vidro desconhecido '%s'."), File, *E.Id, *R.Glass));
		}
		else
		{
			const FString Key = (R.CarId.TrimStartAndEnd() + TEXT("/") + FDNGlass::GlassName(Glass)).ToLower();
			if (const FString* First = SeenGlass.Find(Key))
			{
				Errors.Add(FString::Printf(TEXT("%s: o vidro %s aparece duas vezes no carro '%s' (linhas '%s' e '%s')."), File, FDNGlass::GlassName(Glass), *R.CarId, **First, *E.Id));
			}
			else
			{
				SeenGlass.Add(Key, E.Id);
			}
		}
		CheckZones(File, E.Id, R.Zone, R.SecondaryZone, Errors);
		CheckRange(File, E.Id, TEXT("CrackFrom"), R.CrackFrom, 0.0f, true, 1.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("BurstDamage"), R.BurstDamage, 0.0f, true, 1.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("BurstDeltaVMs"), R.BurstDeltaVMs, 0.0f, true, 100.0f, true, Errors);
	}
	return Errors;
}

namespace
{
	/** Inteiro na faixa fechada [Min, Max]; senão registra o erro. */
	void CheckIntRange(const TCHAR* File, const FString& Id, const TCHAR* Field, int32 Value, int32 Min, int32 Max, TArray<FString>& Errors)
	{
		if (Value < Min || Value > Max)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo '%s' precisa ser um inteiro em [%d, %d] (veio %d)."), File, *Id, Field, Min, Max, Value));
		}
	}
}

TArray<FString> FDNDataValidation::ValidateEffects(const TArray<FDNEffectEntry>& Rows, const TArray<FDNBreakableEntry>& Breakables)
{
	constexpr const TCHAR* File = TEXT("Effects.csv");
	TArray<FString> Errors;
	CheckIds(File, Rows, Errors);
	TMap<EDNEffect, FString> Seen; // efeito -> id da primeira linha
	for (const FDNEffectEntry& E : Rows)
	{
		const FDNEffectRow& R = E.Row;
		EDNEffect Effect = EDNEffect::SparkImpact;
		if (!FDNEffects::ParseEffect(R.Effect, Effect))
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'Effect' tem efeito desconhecido '%s'."), File, *E.Id, *R.Effect));
			continue;
		}
		if (const FString* First = Seen.Find(Effect))
		{
			Errors.Add(FString::Printf(TEXT("%s: o efeito %s aparece duas vezes (linhas '%s' e '%s')."), File, FDNEffects::EffectName(Effect), **First, *E.Id));
		}
		else
		{
			Seen.Add(Effect, E.Id);
		}
		// limiares: dano (motor e fogo) em [0, 1]; o resto em [0, 100]; a rampa termina depois de começar
		const bool bDamage = Effect == EDNEffect::EngineSmoke || Effect == EDNEffect::EngineFire;
		const float TriggerTop = bDamage ? 1.0f : 100.0f;
		CheckRange(File, E.Id, TEXT("TriggerMin"), R.TriggerMin, 0.0f, true, TriggerTop, true, Errors);
		const bool bRamp = Effect == EDNEffect::PlasticChips || Effect == EDNEffect::TireSmoke || Effect == EDNEffect::EngineSmoke;
		const float FullMin = bRamp && FMath::IsFinite(R.TriggerMin) ? R.TriggerMin : 0.0f;
		CheckRange(File, E.Id, TEXT("TriggerFull"), R.TriggerFull, FullMin, !bRamp, TriggerTop, true, Errors);
		CheckRange(File, E.Id, TEXT("MinSpeedKmh"), R.MinSpeedKmh, 0.0f, true, 300.0f, true, Errors);
		CheckIntRange(File, E.Id, TEXT("CountMin"), R.CountMin, 0, 5000, Errors);
		CheckIntRange(File, E.Id, TEXT("CountMax"), R.CountMax, FMath::Max(0, R.CountMin), 5000, Errors);
		CheckRange(File, E.Id, TEXT("CountPerUnit"), R.CountPerUnit, 0.0f, true, 10000.0f, true, Errors);
		const bool bFraction = Effect == EDNEffect::TireSmoke || Effect == EDNEffect::EngineFire;
		CheckRange(File, E.Id, TEXT("Extra"), R.Extra, 0.0f, true, bFraction ? 1.0f : 1000.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("LifeMinS"), R.LifeMinS, 0.0f, false, 30.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("LifeMaxS"), R.LifeMaxS, FMath::IsFinite(R.LifeMinS) ? R.LifeMinS : 0.0f, true, 30.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("SpeedMinMs"), R.SpeedMinMs, 0.0f, true, 100.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("SpeedMaxMs"), R.SpeedMaxMs, FMath::IsFinite(R.SpeedMinMs) ? R.SpeedMinMs : 0.0f, true, 100.0f, true, Errors);
		CheckIntRange(File, E.Id, TEXT("PoolMax"), R.PoolMax, 1, 64, Errors);
		// mobiliário: só no pó e nas folhas, obrigatório neles, e cada tipo existe em Breakables.csv
		const TArray<FString> Kinds = R.PropKinds();
		const bool bProp = Effect == EDNEffect::PropDust || Effect == EDNEffect::PropLeaves;
		if (!bProp && !Kinds.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'Props' só vale no PropDust e no PropLeaves (veio '%s')."), File, *E.Id, *R.Props));
		}
		if (bProp && Kinds.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'Props' precisa dos tipos de peça de rua que disparam o efeito."), File, *E.Id));
		}
		for (const FString& Kind : Kinds)
		{
			if (!Breakables.ContainsByPredicate([&Kind](const FDNBreakableEntry& B) { return B.Id.Equals(Kind, ESearchCase::IgnoreCase); }))
			{
				Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'Props' cita o tipo '%s', que não existe em Breakables.csv."), File, *E.Id, *Kind));
			}
		}
	}
	for (int32 i = 0; i <= static_cast<int32>(EDNEffect::PropLeaves); ++i)
	{
		const EDNEffect Effect = static_cast<EDNEffect>(i);
		if (!Seen.Contains(Effect))
		{
			Errors.Add(FString::Printf(TEXT("%s: falta a linha do efeito %s (cada efeito precisa de uma)."), File, FDNEffects::EffectName(Effect)));
		}
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateEffectsSettings(const TArray<FDNEffectsSettingsEntry>& Rows)
{
	constexpr const TCHAR* File = TEXT("EffectsSettings.csv");
	TArray<FString> Errors;
	CheckSingleRow(File, Rows, Errors);
	for (const FDNEffectsSettingsEntry& E : Rows)
	{
		const FDNEffectsSettingsRow& R = E.Row;
		CheckIntRange(File, E.Id, TEXT("MaxGpuParticles"), R.MaxGpuParticles, 1, 100000, Errors);
		CheckIntRange(File, E.Id, TEXT("MaxSmokeSprites"), R.MaxSmokeSprites, 1, 10000, Errors);
		CheckRange(File, E.Id, TEXT("SmokeFadeNearCm"), R.SmokeFadeNearCm, 0.0f, true, 2000.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("SmokeFadeFarCm"), R.SmokeFadeFarCm, FMath::IsFinite(R.SmokeFadeNearCm) ? R.SmokeFadeNearCm : 0.0f, false, 5000.0f, true, Errors);
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateCoursePoints(const TArray<FDNCoursePointEntry>& Rows)
{
	constexpr const TCHAR* File = TEXT("CoursePoints.csv");
	TArray<FString> Errors;
	if (Rows.IsEmpty())
	{
		Errors.Add(FString::Printf(TEXT("%s: nenhuma linha (pelo menos um percurso com 2 pontos)."), File));
	}
	CheckIds(File, Rows, Errors);

	constexpr float MaxCoordCm = 10000000.0f;
	TMap<FString, TArray<const FDNCoursePointEntry*>> ByCourse;
	for (const FDNCoursePointEntry& E : Rows)
	{
		const FDNCoursePointRow& R = E.Row;
		const FString Course = R.Course.TrimStartAndEnd();
		if (Course.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'Course' está vazio."), File, *E.Id));
			continue;
		}
		if (R.Order < 0)
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'Order' precisa ser um inteiro >= 0 (veio %d)."), File, *E.Id, R.Order));
		}
		CheckRange(File, E.Id, TEXT("XCm"), R.XCm, -MaxCoordCm, true, MaxCoordCm, true, Errors);
		CheckRange(File, E.Id, TEXT("YCm"), R.YCm, -MaxCoordCm, true, MaxCoordCm, true, Errors);
		CheckRange(File, E.Id, TEXT("ZCm"), R.ZCm, -MaxCoordCm, true, MaxCoordCm, true, Errors);
		CheckRange(File, E.Id, TEXT("WidthCm"), R.WidthCm, 400.0f, true, 3000.0f, true, Errors);
		ByCourse.FindOrAdd(Course.ToLower()).Add(&E);
	}

	for (TPair<FString, TArray<const FDNCoursePointEntry*>>& Pair : ByCourse)
	{
		TArray<const FDNCoursePointEntry*>& Points = Pair.Value;
		if (Points.Num() < 2)
		{
			Errors.Add(FString::Printf(TEXT("%s: o percurso '%s' precisa de pelo menos 2 pontos (veio %d)."), File, *Pair.Key, Points.Num()));
			continue;
		}
		Points.StableSort([](const FDNCoursePointEntry& A, const FDNCoursePointEntry& B) { return A.Row.Order < B.Row.Order; });
		for (int32 i = 1; i < Points.Num(); ++i)
		{
			const FDNCoursePointRow& Prev = Points[i - 1]->Row;
			const FDNCoursePointRow& Cur = Points[i]->Row;
			if (Cur.Order == Prev.Order)
			{
				Errors.Add(FString::Printf(TEXT("%s: o campo 'Order' %d aparece duas vezes no percurso '%s' (linhas '%s' e '%s')."), File, Cur.Order, *Pair.Key, *Points[i - 1]->Id, *Points[i]->Id));
			}
			if (Cur.WidthCm != Points[0]->Row.WidthCm)
			{
				Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'WidthCm' (%g) precisa ser igual ao do resto do percurso '%s' (%g)."), File, *Points[i]->Id, Cur.WidthCm, *Pair.Key, Points[0]->Row.WidthCm));
			}
			const FVector A(Prev.XCm, Prev.YCm, Prev.ZCm);
			const FVector B(Cur.XCm, Cur.YCm, Cur.ZCm);
			if (FMath::IsFinite(A.X + A.Y + A.Z + B.X + B.Y + B.Z) && FVector::Dist(A, B) < 100.0)
			{
				Errors.Add(FString::Printf(TEXT("%s, linha '%s': o ponto fica a menos de 100 cm do anterior no percurso '%s'."), File, *Points[i]->Id, *Pair.Key));
			}
		}
	}
	return Errors;
}

TArray<FString> FDNDataValidation::ValidateRivals(const TArray<FDNRivalEntry>& Rows)
{
	constexpr const TCHAR* File = TEXT("Rivals.csv");
	TArray<FString> Errors;
	if (Rows.IsEmpty())
	{
		Errors.Add(FString::Printf(TEXT("%s: nenhuma linha (pelo menos um rival)."), File));
	}
	else if (Rows.Num() > 7)
	{
		Errors.Add(FString::Printf(TEXT("%s: no maximo 7 rivais (veio %d)."), File, Rows.Num()));
	}
	CheckIds(File, Rows, Errors);
	for (const FDNRivalEntry& E : Rows)
	{
		const FDNRivalRow& R = E.Row;
		if (R.Name.TrimStartAndEnd().IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo 'Name' está vazio."), File, *E.Id));
		}
		CheckRange(File, E.Id, TEXT("StraightKmh"), R.StraightKmh, 20.0f, false, 400.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("CornerKmh"), R.CornerKmh, 5.0f, false, FMath::IsFinite(R.StraightKmh) ? R.StraightKmh : 400.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("CornerFullDeg"), R.CornerFullDeg, 1.0f, false, 180.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("SteerFullDeg"), R.SteerFullDeg, 1.0f, false, 180.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("LookAheadMinCm"), R.LookAheadMinCm, 100.0f, true, 10000.0f, true, Errors);
		CheckRange(File, E.Id, TEXT("LookAheadPerKmhCm"), R.LookAheadPerKmhCm, 0.0f, true, 200.0f, true, Errors);
	}
	return Errors;
}
