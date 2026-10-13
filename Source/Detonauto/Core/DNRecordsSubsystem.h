// Cola fina: guarda o FDNRecords do jogador e lê/grava o arquivo Saved/SaveGames/Detonauto.json.
// As regras (melhor pontuação, medalha, texto JSON) estão em Core/DNRecords; aqui só entra o arquivo.
// Testes: Core/Tests/DNSubsystems.spec.cpp.
//
//  - Escrita atômica: grava Detonauto.json.tmp e troca pelo arquivo final. Se a gravação falha, o arquivo antigo fica intacto.
//  - Arquivo corrompido (FDNRecords::Deserialize devolveu erro) NUNCA é sobrescrito: é renomeado para
//    Detonauto.json.corrompido-<data> e o registro começa vazio, com erro no log.
//
// S2: RegisterResult grava o arquivo sozinho quando bate recorde (falha de gravação é logada; o recorde fica na memória).
// S3: arquivo vazio ou ilegível conta como corrompido.
// S4: arquivo que não existe é a primeira execução: registro vazio, sem erro, nada é criado até o primeiro recorde.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/DNRecords.h"
#include "Core/DNProgress.h"
#include "DNRecordsSubsystem.generated.h"

struct FDNData;
class UDNGameDataSubsystem;

UCLASS()
class DETONAUTO_API UDNRecordsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Saved/SaveGames/Detonauto.json */
	static FString DefaultSavePath();

	/** Onde Save() grava. Começa em DefaultSavePath(). */
	void SetSavePath(const FString& Path) { SavePath = Path; }
	const FString& GetSavePath() const { return SavePath; }

	/**
	 *  Lê o arquivo para o registro. Verdadeiro se carregou (inclusive arquivo que não existe: S4). Falso se o arquivo
	 *  estava corrompido: ele foi renomeado (GetLastCorruptedBackupPath) e o registro começou vazio.
	 */
	bool LoadFromFile(const FString& Path);

	/** Grava o registro no arquivo de forma atômica. Falso se não conseguiu (o arquivo antigo fica intacto). */
	bool SaveToFile(const FString& Path) const;
	bool Save() const { return SaveToFile(SavePath); }

	/** Registra um resultado; devolve se foi novo recorde. Bateu recorde: grava o arquivo (S2). */
	bool RegisterResult(const FString& CrossingId, int32 Score);

	const FDNRecords& GetRecords() const { return Records; }
	const FString& GetLastCorruptedBackupPath() const { return LastCorruptedBackupPath; }

	/** De onde ProgressoParaMenu() tira cruzamentos e cidades. Initialize liga ao UDNGameDataSubsystem. */
	void SetDataSource(UDNGameDataSubsystem* Source) { DataSource = Source; }

	/** Contadores do menu: ToProgressInput + FDNProgress::Compute. Sem fonte de dados: resultado vazio e erro logado. */
	FDNProgressResult ProgressoParaMenu() const;
	FDNProgressResult ProgressoParaMenu(const FDNData& Data) const;

private:
	FDNRecords Records;
	FString SavePath;
	FString LastCorruptedBackupPath;

	UPROPERTY()
	TObjectPtr<UDNGameDataSubsystem> DataSource;
};
