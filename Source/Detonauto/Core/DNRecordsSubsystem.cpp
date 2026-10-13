#include "Core/DNRecordsSubsystem.h"
#include "Core/DNGameDataSubsystem.h"
#include "Detonauto.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void UDNRecordsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	DataSource = Collection.InitializeDependency<UDNGameDataSubsystem>();
	SavePath = DefaultSavePath();
	LoadFromFile(SavePath);
}

FString UDNRecordsSubsystem::DefaultSavePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("Detonauto.json"));
}

bool UDNRecordsSubsystem::LoadFromFile(const FString& Path)
{
	Records = FDNRecords();
	LastCorruptedBackupPath.Reset();

	IFileManager& Files = IFileManager::Get();
	if (!Files.FileExists(*Path))
	{
		UE_LOG(LogDetonauto, Log, TEXT("Registro do jogador: Sem arquivo de registro em '%s': começando vazio."), *Path);
		return true;
	}

	FString Text;
	FDNRecordsLoadResult Result;
	if (FFileHelper::LoadFileToString(Text, *Path))
	{
		Result = FDNRecords::Deserialize(Text);
	}
	else
	{
		Result.bError = true;
		Result.Warnings.Add(TEXT("Não consegui ler o arquivo."));
	}

	if (Result.bError)
	{
		// Nunca sobrescrever: o arquivo sai do caminho com o nome .corrompido-<data> e o jogo começa vazio.
		const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
		FString Backup = FString::Printf(TEXT("%s.corrompido-%s"), *Path, *Stamp);
		for (int32 Suffix = 2; Files.FileExists(*Backup); ++Suffix)
		{
			Backup = FString::Printf(TEXT("%s.corrompido-%s-%d"), *Path, *Stamp, Suffix);
		}
		const bool bMoved = Files.Move(*Backup, *Path, /*Replace*/ false);
		UE_LOG(LogDetonauto, Error, TEXT("Registro do jogador: Arquivo '%s' corrompido (%s). %s"), *Path,
			Result.Warnings.IsEmpty() ? TEXT("sem detalhe") : *Result.Warnings[0],
			bMoved ? *FString::Printf(TEXT("Guardado em '%s'; começando vazio."), *Backup) : TEXT("NÃO consegui renomear; começando vazio."));
		if (bMoved)
		{
			LastCorruptedBackupPath = Backup;
		}
		return false;
	}

	for (const FString& Warning : Result.Warnings)
	{
		UE_LOG(LogDetonauto, Warning, TEXT("Registro do jogador: %s"), *Warning);
	}
	Records = MoveTemp(Result.Records);
	return true;
}

bool UDNRecordsSubsystem::SaveToFile(const FString& Path) const
{
	IFileManager& Files = IFileManager::Get();
	Files.MakeDirectory(*FPaths::GetPath(Path), true);

	// Escrita atômica: grava ao lado e troca. Se qualquer passo falha, o arquivo antigo continua como estava.
	const FString Tmp = Path + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(Records.Serialize(), *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogDetonauto, Error, TEXT("Registro do jogador: Não consegui gravar '%s'; o arquivo anterior ficou como estava."), *Tmp);
		Files.Delete(*Tmp, false, true, true);
		return false;
	}
	if (!Files.Move(*Path, *Tmp, /*Replace*/ true))
	{
		UE_LOG(LogDetonauto, Error, TEXT("Registro do jogador: Não consegui trocar '%s' por '%s'; o arquivo anterior ficou como estava."), *Path, *Tmp);
		Files.Delete(*Tmp, false, true, true);
		return false;
	}
	return true;
}

bool UDNRecordsSubsystem::RegisterResult(const FString& CrossingId, int32 Score)
{
	const bool bNewRecord = Records.RegisterResult(CrossingId, Score);
	if (bNewRecord && !SavePath.IsEmpty())
	{
		Save();
	}
	return bNewRecord;
}

FDNProgressResult UDNRecordsSubsystem::ProgressoParaMenu() const
{
	if (!DataSource)
	{
		UE_LOG(LogDetonauto, Error, TEXT("Registro do jogador: ProgressoParaMenu sem fonte de dados (UDNGameDataSubsystem)."));
		return FDNProgressResult();
	}
	return ProgressoParaMenu(DataSource->GetData());
}

FDNProgressResult UDNRecordsSubsystem::ProgressoParaMenu(const FDNData& Data) const
{
	TArray<FDNCrossingDef> Crossings;
	Crossings.Reserve(Data.Crossings.Num());
	for (const FDNCrossingEntry& Entry : Data.Crossings)
	{
		FDNCrossingDef Def;
		Def.CrossingId = FName(*Entry.Id);
		Def.CityId = FName(*Entry.Row.CityId);
		Def.Thresholds.Bronze = Entry.Row.Bronze;
		Def.Thresholds.Silver = Entry.Row.Silver;
		Def.Thresholds.Gold = Entry.Row.Gold;
		Crossings.Add(Def);
	}

	TArray<FDNCityRegion> Cities;
	Cities.Reserve(Data.Cities.Num());
	for (const FDNCityEntry& Entry : Data.Cities)
	{
		FDNCityRegion City;
		City.CityId = FName(*Entry.Id);
		City.RegionId = FName(*Entry.Row.RegionId);
		City.bPlayable = Entry.Row.Playable;
		Cities.Add(City);
	}
	return FDNProgress::Compute(Records.ToProgressInput(Crossings), Cities);
}
