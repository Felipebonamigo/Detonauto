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
