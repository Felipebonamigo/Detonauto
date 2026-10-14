// Leitura dos CSVs de Data/ como DataTable. Devolve as linhas na ordem do arquivo e a lista de problemas
// (coluna faltando, número inválido, id repetido, arquivo inexistente...). Lista vazia = leitura sem problemas.
//
// Caminho: FPaths::ProjectDir() é a pasta do Detonauto.uproject, que é a raiz do repositório; Data/ fica ali.
// O test.ps1 abre o projeto pelo caminho absoluto do .uproject, então nada precisa ser configurado.
// Para um jogo empacotado, Data/ precisa ser listada em DirectoriesToAlwaysStageAsNonUFS (ainda não foi).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"
#include "Misc/DefaultValueHelper.h"
#include "UObject/UnrealType.h"
#include "Core/Data/DNDataRows.h"

struct FDNDataLoader
{
	/**
	 *  O importador de DataTable da engine NÃO reclama de texto que não é número (por exemplo "abc" em um campo
	 *  inteiro vira 0 em silêncio). Esta checagem estrita olha cada célula de campo numérico ou booleano.
	 */
	template <typename RowType>
	static void CheckCells(const FString& CsvText, const FString& Label, TArray<FString>& Errors)
	{
		const FCsvParser Parser(CsvText);
		const TArray<TArray<const TCHAR*>>& Rows = Parser.GetRows();
		if (Rows.IsEmpty())
		{
			return;
		}

		// coluna -> propriedade (a coluna 0 é o id e fica fora da struct)
		TArray<const FProperty*> ColumnProperty;
		for (int32 Column = 0; Column < Rows[0].Num(); ++Column)
		{
			const FProperty* Found = nullptr;
			if (Column > 0)
			{
				for (TFieldIterator<FProperty> It(RowType::StaticStruct()); It; ++It)
				{
					if (It->GetName().Equals(Rows[0][Column], ESearchCase::IgnoreCase))
					{
						Found = *It;
						break;
					}
				}
			}
			ColumnProperty.Add(Found);
		}

		for (int32 Row = 1; Row < Rows.Num(); ++Row)
		{
			const FString RowId = Rows[Row].IsEmpty() ? FString() : FString(Rows[Row][0]);
			for (int32 Column = 1; Column < Rows[Row].Num() && Column < ColumnProperty.Num(); ++Column)
			{
				const FProperty* Property = ColumnProperty[Column];
				if (!Property)
				{
					continue;
				}
				const FString Cell = FString(Rows[Row][Column]).TrimStartAndEnd();

				if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
				{
					const bool bOk = Numeric->IsInteger() ? FDefaultValueHelper::IsStringValidInteger(Cell) : FDefaultValueHelper::IsStringValidFloat(Cell);
					if (!bOk)
					{
						Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo '%s' tem o valor '%s', que não é um número %s."),
							*Label, *RowId, *Property->GetName(), *Cell, Numeric->IsInteger() ? TEXT("inteiro") : TEXT("decimal")));
					}
				}
				else if (CastField<FBoolProperty>(Property))
				{
					const bool bOk = Cell.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Cell.Equals(TEXT("false"), ESearchCase::IgnoreCase)
						|| Cell == TEXT("1") || Cell == TEXT("0");
					if (!bOk)
					{
						Errors.Add(FString::Printf(TEXT("%s, linha '%s': o campo '%s' tem o valor '%s'; use true ou false."), *Label, *RowId, *Property->GetName(), *Cell));
					}
				}
			}
		}
	}

	/** Caminho absoluto de um arquivo em Data/. */
	static FString DataPath(const FString& FileName)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), FileName));
	}

	/** Lê o texto de um CSV. Label aparece nas mensagens de erro (por exemplo "Regions.csv"). */
	template <typename RowType>
	static TArray<FString> ParseEntries(const FString& CsvText, const FString& Label, TArray<TDNTableEntry<RowType>>& OutEntries)
	{
		TArray<FString> Errors;
		OutEntries.Reset();

		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
		Table->RowStruct = RowType::StaticStruct();
		for (const FString& Problem : Table->CreateTableFromCSVString(CsvText))
		{
			Errors.Add(FString::Printf(TEXT("%s: %s"), *Label, *Problem));
		}

		CheckCells<RowType>(CsvText, Label, Errors);

		for (const FName& RowName : Table->GetRowNames())
		{
			if (const RowType* Row = Table->FindRow<RowType>(RowName, TEXT("DNDataLoader"), false))
			{
				TDNTableEntry<RowType>& Entry = OutEntries.AddDefaulted_GetRef();
				Entry.Id = RowName.ToString();
				Entry.Row = *Row;
			}
		}
		return Errors;
	}

	/** Lê um arquivo. RelativePath é relativo à pasta do projeto (por exemplo "Data/Regions.csv"). */
	template <typename RowType>
	static TArray<FString> LoadEntries(const FString& RelativePath, TArray<TDNTableEntry<RowType>>& OutEntries)
	{
		OutEntries.Reset();
		const FString Absolute = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), RelativePath));
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *Absolute))
		{
			return { FString::Printf(TEXT("%s: não consegui ler o arquivo (%s)."), *FPaths::GetCleanFilename(RelativePath), *Absolute) };
		}
		return ParseEntries<RowType>(Csv, FPaths::GetCleanFilename(RelativePath), OutEntries);
	}

	/** Lê as catorze tabelas de Data/. */
	static TArray<FString> LoadAll(FDNData& OutData)
	{
		TArray<FString> Errors;
		Errors.Append(LoadEntries<FDNRegionRow>(TEXT("Data/Regions.csv"), OutData.Regions));
		Errors.Append(LoadEntries<FDNCityRow>(TEXT("Data/Cities.csv"), OutData.Cities));
		Errors.Append(LoadEntries<FDNLandmarkRow>(TEXT("Data/Landmarks.csv"), OutData.Landmarks));
		Errors.Append(LoadEntries<FDNCrossingRow>(TEXT("Data/Crossings.csv"), OutData.Crossings));
		Errors.Append(LoadEntries<FDNTrafficVehicleRow>(TEXT("Data/TrafficVehicles.csv"), OutData.Vehicles));
		Errors.Append(LoadEntries<FDNTrafficSettingsRow>(TEXT("Data/TrafficSettings.csv"), OutData.TrafficSettings));
		Errors.Append(LoadEntries<FDNTurboConfigRow>(TEXT("Data/Turbo.csv"), OutData.TurboConfig));
		Errors.Append(LoadEntries<FDNCrashConfigRow>(TEXT("Data/Crash.csv"), OutData.CrashConfig));
		Errors.Append(LoadEntries<FDNTakedownConfigRow>(TEXT("Data/Takedown.csv"), OutData.TakedownConfig));
		Errors.Append(LoadEntries<FDNDamageConfigRow>(TEXT("Data/Damage.csv"), OutData.DamageConfig));
		Errors.Append(LoadEntries<FDNCarConfigRow>(TEXT("Data/Cars.csv"), OutData.CarConfigs));
		Errors.Append(LoadEntries<FDNCameraConfigRow>(TEXT("Data/Cameras.csv"), OutData.CameraModes));
		Errors.Append(LoadEntries<FDNBreakableRow>(TEXT("Data/Breakables.csv"), OutData.Breakables));
		Errors.Append(LoadEntries<FDNBreakableSettingsRow>(TEXT("Data/BreakableSettings.csv"), OutData.BreakableSettings));
		return Errors;
	}
};
