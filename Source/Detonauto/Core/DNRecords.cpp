#include "Core/DNRecords.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr int32 SaveVersion = 1;

	FDNRecordsLoadResult Failure(const FString& Reason)
	{
		FDNRecordsLoadResult R;
		R.bError = true;
		R.Warnings.Add(Reason);
		return R;
	}

	/** R1: número sem parte fracionária (aceita -0). */
	bool IsWhole(double N)
	{
		return FMath::IsFinite(N) && FMath::FloorToDouble(N) == N;
	}

	/** Depois de ler o início de um objeto ou array, consome tudo até o fim correspondente. Falso se o texto acabar ou estiver malformado. */
	bool SkipNested(TJsonReader<TCHAR>& Reader)
	{
		int32 Depth = 1;
		EJsonNotation N = EJsonNotation::Error;
		while (Depth > 0)
		{
			if (!Reader.ReadNext(N) || N == EJsonNotation::Error)
			{
				return false;
			}
			if (N == EJsonNotation::ObjectStart || N == EJsonNotation::ArrayStart)
			{
				++Depth;
			}
			else if (N == EJsonNotation::ObjectEnd || N == EJsonNotation::ArrayEnd)
			{
				--Depth;
			}
		}
		return true;
	}

	/** Lê o conteúdo de "best" (o ObjectStart já foi consumido). Falso se o texto estiver malformado. */
	bool ReadBest(TJsonReader<TCHAR>& Reader, FDNRecordsLoadResult& Result)
	{
		EJsonNotation N = EJsonNotation::Error;
		while (true)
		{
			if (!Reader.ReadNext(N) || N == EJsonNotation::Error)
			{
				return false;
			}
			if (N == EJsonNotation::ObjectEnd)
			{
				return true;
			}
			const FString Id = Reader.GetIdentifier();
			bool bValid = false;
			double Number = 0.0;
			if (N == EJsonNotation::Number)
			{
				Number = Reader.GetValueAsNumber();
				bValid = IsWhole(Number) && Number >= 0.0 && Number <= static_cast<double>(MAX_int32);
			}
			else if ((N == EJsonNotation::ObjectStart || N == EJsonNotation::ArrayStart) && !SkipNested(Reader))
			{
				return false;
			}

			if (Id.IsEmpty())
			{
				Result.Warnings.Add(TEXT("Entrada ignorada: o id está vazio."));
			}
			else if (!bValid)
			{
				Result.Warnings.Add(FString::Printf(TEXT("Entrada '%s' ignorada: a pontuação precisa ser um número inteiro de 0 a %d."), *Id, MAX_int32));
			}
			else
			{
				if (Result.Records.BestScore(Id).IsSet())
				{
					Result.Warnings.Add(FString::Printf(TEXT("O id '%s' está repetido (maiúsculas e minúsculas não diferenciam): vale a maior pontuação."), *Id));
				}
				Result.Records.RegisterResult(Id, static_cast<int32>(Number));
			}
		}
	}
}

bool FDNRecords::RegisterResult(const FString& CrossingId, int32 Score)
{
	if (CrossingId.IsEmpty() || Score < 0)
	{
		return false;
	}
	// Find + alterar o valor: Add numa chave que já existe só diferente na caixa trocaria a grafia guardada.
	if (int32* Current = Best.Find(CrossingId))
	{
		if (Score > *Current)
		{
			*Current = Score;
			return true;
		}
		return false;
	}
	Best.Add(CrossingId, Score);
	return true;
}

TOptional<int32> FDNRecords::BestScore(const FString& CrossingId) const
{
	if (const int32* Found = CrossingId.IsEmpty() ? nullptr : Best.Find(CrossingId))
	{
		return *Found;
	}
	return TOptional<int32>();
}

TArray<FDNRecordEntry> FDNRecords::SortedEntries() const
{
	TArray<FDNRecordEntry> Entries;
	Entries.Reserve(Best.Num());
	for (const TPair<FString, int32>& Pair : Best)
	{
		FDNRecordEntry& E = Entries.AddDefaulted_GetRef();
		E.Id = Pair.Key;
		E.Score = Pair.Value;
	}
	Entries.Sort([](const FDNRecordEntry& A, const FDNRecordEntry& B)
	{
		return A.Id.ToLower().Compare(B.Id.ToLower(), ESearchCase::CaseSensitive) < 0;
	});
	return Entries;
}

TArray<FDNCrossingProgress> FDNRecords::ToProgressInput(const TArray<FDNCrossingDef>& Crossings) const
{
	TArray<FDNCrossingProgress> Result;
	Result.Reserve(Crossings.Num());
	for (const FDNCrossingDef& Def : Crossings)
	{
		FDNCrossingProgress P;
		P.CrossingId = Def.CrossingId;
		P.CityId = Def.CityId;
		const TOptional<int32> Score = BestScore(Def.CrossingId.ToString());
		P.Medal = Score.IsSet() ? FDNMedals::Evaluate(Def.Thresholds, Score.GetValue()) : EDNMedal::None;
		Result.Add(P);
	}
	return Result;
}

FString FDNRecords::Serialize() const
{
	FString Text;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("version"), SaveVersion);
	Writer->WriteObjectStart(TEXT("best"));
	for (const FDNRecordEntry& E : SortedEntries())
	{
		Writer->WriteValue(E.Id, E.Score);
	}
	Writer->WriteObjectEnd();
	Writer->WriteObjectEnd();
	Writer->Close();
	return Text;
}

FDNRecordsLoadResult FDNRecords::Deserialize(const FString& Text)
{
	if (Text.TrimStartAndEnd().IsEmpty())
	{
		return Failure(TEXT("O texto do registro está vazio."));
	}

	// Leitura em fluxo, não FJsonObject: o objeto da engine junta chaves que só diferem na caixa (e as repetidas)
	// antes de a gente ver, e o contrato manda valer o MAIOR e avisar.
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Text);
	auto Invalid = [&Reader]()
	{
		return Failure(FString::Printf(TEXT("O texto do registro não é um JSON válido (%s)."), *Reader->GetErrorMessage()));
	};

	EJsonNotation N = EJsonNotation::Error;
	if (!Reader->ReadNext(N) || N == EJsonNotation::Error)
	{
		return Invalid();
	}
	if (N != EJsonNotation::ObjectStart)
	{
		return Failure(TEXT("A raiz do registro precisa ser um objeto JSON."));
	}

	FDNRecordsLoadResult Result;
	bool bHasVersion = false;
	bool bVersionWhole = false;
	double Version = 0.0;
	bool bHasBest = false;
	bool bBestIsObject = false;

	while (true)
	{
		if (!Reader->ReadNext(N) || N == EJsonNotation::Error)
		{
			return Invalid();
		}
		if (N == EJsonNotation::ObjectEnd)
		{
			break;
		}
		const FString Key = Reader->GetIdentifier();
		const bool bNested = N == EJsonNotation::ObjectStart || N == EJsonNotation::ArrayStart;

		if (Key.Equals(TEXT("version"), ESearchCase::CaseSensitive))
		{
			if (bHasVersion)
			{
				return Failure(TEXT("O campo 'version' do registro aparece mais de uma vez."));
			}
			bHasVersion = true;
			if (N == EJsonNotation::Number)
			{
				Version = Reader->GetValueAsNumber();
				bVersionWhole = IsWhole(Version);
			}
			else if (bNested && !SkipNested(*Reader))
			{
				return Invalid();
			}
		}
		else if (Key.Equals(TEXT("best"), ESearchCase::CaseSensitive))
		{
			if (bHasBest)
			{
				return Failure(TEXT("O campo 'best' do registro aparece mais de uma vez."));
			}
			bHasBest = true;
			if (N == EJsonNotation::ObjectStart)
			{
				bBestIsObject = true;
				if (!ReadBest(*Reader, Result))
				{
					return Invalid();
				}
			}
			else if (bNested && !SkipNested(*Reader))
			{
				return Invalid();
			}
		}
		else if (bNested && !SkipNested(*Reader))
		{
			return Invalid(); // campo desconhecido: ignorado (compatível para frente), mas precisa estar bem formado
		}
	}

	// R3: depois do objeto raiz só pode sobrar espaço em branco.
	if (Reader->ReadNext(N) || !Reader->GetErrorMessage().IsEmpty())
	{
		return Failure(TEXT("O texto do registro tem conteúdo depois do objeto raiz."));
	}

	// Versão: obrigatória, número inteiro, e só a 1 é lida. Versão maior nunca é lida "no escuro".
	if (!bHasVersion || !bVersionWhole)
	{
		return Failure(TEXT("O campo 'version' do registro falta ou não é um número inteiro."));
	}
	if (Version != SaveVersion)
	{
		return Failure(FString::Printf(TEXT("A versão %g do registro não é suportada (só a %d)."), Version, SaveVersion));
	}
	// R2.
	if (!bBestIsObject)
	{
		return Failure(TEXT("O campo 'best' do registro falta ou não é um objeto."));
	}
	return Result;
}

bool FDNRecords::operator==(const FDNRecords& Other) const
{
	const TArray<FDNRecordEntry> A = SortedEntries();
	const TArray<FDNRecordEntry> B = Other.SortedEntries();
	if (A.Num() != B.Num())
	{
		return false;
	}
	for (int32 i = 0; i < A.Num(); ++i)
	{
		if (A[i].Score != B[i].Score || !A[i].Id.Equals(B[i].Id, ESearchCase::CaseSensitive))
		{
			return false;
		}
	}
	return true;
}
