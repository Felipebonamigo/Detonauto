#include "Core/DNGameDataSubsystem.h"
#include "Core/Data/DNDataLoader.h"
#include "Core/Data/DNDataValidation.h"
#include "Detonauto.h"

void UDNGameDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Reload();
}

void UDNGameDataSubsystem::Reload()
{
	FDNData Loaded;
	const TArray<FString> LoadErrors = FDNDataLoader::LoadAll(Loaded);
	ApplyData(MoveTemp(Loaded), LoadErrors);
}

void UDNGameDataSubsystem::ApplyData(FDNData NewData, const TArray<FString>& LoadErrors)
{
	Data = MoveTemp(NewData);
	Errors = LoadErrors;
	Errors.Append(FDNDataValidation::ValidateAll(Data));

	bTurboValid = FDNDataValidation::ValidateTurbo(Data.TurboConfig).IsEmpty();
	bCrashValid = FDNDataValidation::ValidateCrash(Data.CrashConfig).IsEmpty();
	bTakedownValid = FDNDataValidation::ValidateTakedown(Data.TakedownConfig).IsEmpty();
	bDamageValid = FDNDataValidation::ValidateDamage(Data.DamageConfig).IsEmpty();
	bCarsValid = FDNDataValidation::ValidateCars(Data.CarConfigs).IsEmpty();
	bCamerasValid = FDNDataValidation::ValidateCameras(Data.CameraModes).IsEmpty();
	bTrafficSettingsValid = FDNDataValidation::ValidateTrafficSettings(Data.TrafficSettings).IsEmpty();
	bTrafficVehiclesValid = FDNDataValidation::ValidateTrafficVehicles(Data.Vehicles).IsEmpty();
	bBreakablesValid = FDNDataValidation::ValidateBreakables(Data.Breakables).IsEmpty();
	bBreakableSettingsValid = FDNDataValidation::ValidateBreakableSettings(Data.BreakableSettings).IsEmpty();
	bCameraFxValid = FDNDataValidation::ValidateCameraFx(Data.CameraFx).IsEmpty();
	bCarPartsValid = FDNDataValidation::ValidateCarParts(Data.CarParts).IsEmpty();
	bCarGlassValid = FDNDataValidation::ValidateCarGlass(Data.CarGlass).IsEmpty();
	bDataValid = Errors.IsEmpty();

	for (const FString& Error : Errors)
	{
		UE_LOG(LogDetonauto, Error, TEXT("Dados do jogo: %s"), *Error);
	}
	if (bDataValid)
	{
		UE_LOG(LogDetonauto, Log, TEXT("Dados do jogo: Tabelas carregadas e válidas."));
	}
	else
	{
		UE_LOG(LogDetonauto, Error, TEXT("Dados do jogo: %d erro(s): o jogo segue, mas os dados estão marcados como inválidos."), Errors.Num());
	}
}

FDNTurboConfig UDNGameDataSubsystem::GetTurboConfig() const
{
	return (bTurboValid && Data.TurboConfig.Num() == 1) ? Data.TurboConfig[0].Row.ToConfig() : FDNTurboConfig();
}

FDNCrashConfig UDNGameDataSubsystem::GetCrashConfig() const
{
	return (bCrashValid && Data.CrashConfig.Num() == 1) ? Data.CrashConfig[0].Row.ToConfig() : FDNCrashConfig();
}

FDNTakedownConfig UDNGameDataSubsystem::GetTakedownConfig() const
{
	return (bTakedownValid && Data.TakedownConfig.Num() == 1) ? Data.TakedownConfig[0].Row.ToConfig() : FDNTakedownConfig();
}

FDNDamageConfig UDNGameDataSubsystem::GetDamageConfig() const
{
	return (bDamageValid && Data.DamageConfig.Num() == 1) ? Data.DamageConfig[0].Row.ToConfig() : FDNDamageConfig();
}

TArray<FDNCameraMode> UDNGameDataSubsystem::GetCameraModes() const
{
	TArray<FDNCameraMode> Modes;
	if (bCamerasValid)
	{
		for (const FDNCameraConfigEntry& E : Data.CameraModes)
		{
			Modes.Add(FDNCameraMode{ E.Id, E.Row.ToConfig() });
		}
	}
	if (Modes.IsEmpty())
	{
		Modes.Add(FDNCameraMode{ TEXT("alta-longe"), FDNCameraConfig() });
	}
	return Modes;
}

FDNTrafficSettings UDNGameDataSubsystem::GetTrafficSettings() const
{
	return (bTrafficSettingsValid && Data.TrafficSettings.Num() == 1) ? Data.TrafficSettings[0].Row.ToSettings() : FDNTrafficSettings();
}

const TArray<FDNTrafficVehicleEntry>& UDNGameDataSubsystem::GetTrafficVehicles() const
{
	static const TArray<FDNTrafficVehicleEntry> None;
	return bTrafficVehiclesValid ? Data.Vehicles : None;
}

bool UDNGameDataSubsystem::TryGetMedalThresholds(const FString& CrossingId, FDNMedalThresholds& Out) const
{
	const FDNCrossingEntry* Entry = Data.Crossings.FindByPredicate([&CrossingId](const FDNCrossingEntry& E) { return E.Id.Equals(CrossingId, ESearchCase::IgnoreCase); });
	if (!Entry)
	{
		return false;
	}
	FDNMedalThresholds Thresholds;
	Thresholds.Bronze = Entry->Row.Bronze;
	Thresholds.Silver = Entry->Row.Silver;
	Thresholds.Gold = Entry->Row.Gold;
	if (!FDNMedals::Validate(Thresholds))
	{
		return false;
	}
	Out = Thresholds;
	return true;
}

bool UDNGameDataSubsystem::TryGetBreakableKind(const FString& Kind, FDNBreakableKind& Out) const
{
	if (!bBreakablesValid)
	{
		return false;
	}
	const FDNBreakableEntry* Entry = Data.Breakables.FindByPredicate([&Kind](const FDNBreakableEntry& E) { return E.Id.Equals(Kind, ESearchCase::IgnoreCase); });
	if (!Entry)
	{
		return false;
	}
	Out = Entry->Row.ToKind();
	return true;
}

FDNBreakableSettings UDNGameDataSubsystem::GetBreakableSettings() const
{
	return (bBreakableSettingsValid && Data.BreakableSettings.Num() == 1) ? Data.BreakableSettings[0].Row.ToSettings() : FDNBreakableSettings();
}

FDNCameraFxSettings UDNGameDataSubsystem::GetCameraFxSettings() const
{
	return (bCameraFxValid && Data.CameraFx.Num() == 1) ? Data.CameraFx[0].Row.ToSettings() : FDNCameraFxSettings();
}

bool UDNGameDataSubsystem::TryGetCarConfig(const FString& Id, FDNCarConfig& Out) const
{
	if (!bCarsValid)
	{
		return false;
	}
	const FDNCarConfigEntry* Entry = Data.CarConfigs.FindByPredicate([&Id](const FDNCarConfigEntry& E) { return E.Id.Equals(Id, ESearchCase::IgnoreCase); });
	if (!Entry)
	{
		return false;
	}
	Out = Entry->Row.ToConfig();
	return true;
}

TArray<FDNCarPartSpec> UDNGameDataSubsystem::GetCarParts(const FString& CarId) const
{
	TArray<FDNCarPartSpec> Out;
	if (!bCarPartsValid)
	{
		return Out;
	}
	for (const FDNCarPartEntry& E : Data.CarParts)
	{
		FDNCarPartSpec Spec;
		if (E.Row.CarId.TrimStartAndEnd().Equals(CarId, ESearchCase::IgnoreCase) && E.Row.ToSpec(Spec))
		{
			Out.Add(Spec);
		}
	}
	return Out;
}

TArray<FDNGlassSpec> UDNGameDataSubsystem::GetCarGlass(const FString& CarId) const
{
	TArray<FDNGlassSpec> Out;
	if (!bCarGlassValid)
	{
		return Out;
	}
	for (const FDNCarGlassEntry& E : Data.CarGlass)
	{
		FDNGlassSpec Spec;
		if (E.Row.CarId.TrimStartAndEnd().Equals(CarId, ESearchCase::IgnoreCase) && E.Row.ToSpec(Spec))
		{
			Out.Add(Spec);
		}
	}
	return Out;
}
