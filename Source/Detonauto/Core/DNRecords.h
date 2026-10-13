// Registro do jogador (save): regra pura. Sem UObject, sem mundo, sem ler nem gravar arquivo: o texto entra e sai por
// FString. Contrato: docs/regras-save.md. Testes: Core/Tests/DNRecords.spec.cpp.
//
// Guarda só a melhor pontuação por cruzamento. A medalha NÃO é guardada: é derivada com os limites atuais.
//
// Decisões de implementação (o contrato não cobre):
//  R1 número JSON "inteiro" = valor sem parte fracionária (4200 e 4200.0 valem; 4200.5 não; -0 vale como 0);
//  R2 "best" ausente ou que não é objeto, numa versão 1: erro (um save assim gravado por cima apagaria tudo);
//  R3 sobra de texto depois do objeto raiz é JSON inválido: erro;
//  R4 a razão do erro vai como primeiro aviso;
//  R5 id com espaços nas pontas não é aparado: vale como está (só o id vazio é ignorado);
//  R6 ToProgressInput devolve um item por cruzamento da lista de entrada, na mesma ordem (repetidos inclusive: quem
//     decide é FDNProgress::Compute, regra P2).
//  R7 a leitura é em fluxo (TJsonReader), não FJsonObject: o objeto da engine junta chaves que só diferem na caixa antes
//     de a gente ver, e o contrato manda valer o MAIOR e avisar. Nomes de campo da raiz ("version", "best") valem só
//     em minúsculas; "version" ou "best" repetidos são erro.

#pragma once

#include "CoreMinimal.h"
#include "Core/DNMedals.h"
#include "Core/DNProgress.h"

/** Um cruzamento do jogo com os limites de medalha atuais (vêm de Crossings.csv). */
struct FDNCrossingDef
{
	FName CrossingId;
	FName CityId;
	FDNMedalThresholds Thresholds;
};

/** Uma linha do registro: o id como foi escrito da primeira vez e a melhor pontuação. */
struct FDNRecordEntry
{
	FString Id;
	int32 Score = 0;
};

struct FDNRecordsLoadResult;

struct FDNRecords
{
	/**
	 *  Guarda max(atual, Score). Devolve verdadeiro se foi NOVO recorde: Score estritamente maior que o atual, ou o
	 *  primeiro resultado do cruzamento (mesmo 0). Score negativo e id vazio são ignorados (falso, nada muda).
	 *  Ids comparam sem diferenciar a caixa; o id guardado é o da primeira vez.
	 */
	bool RegisterResult(const FString& CrossingId, int32 Score);

	/** A melhor pontuação, ou "sem registro" (não setado), que é diferente de 0. */
	TOptional<int32> BestScore(const FString& CrossingId) const;

	int32 Num() const { return Best.Num(); }

	/** Ordem estável: ids em ordem alfabética sem diferenciar a caixa. */
	TArray<FDNRecordEntry> SortedEntries() const;

	/** Um item por cruzamento do jogo, com a medalha derivada dos limites (sem registro ou abaixo do bronze: None). */
	TArray<FDNCrossingProgress> ToProgressInput(const TArray<FDNCrossingDef>& Crossings) const;

	/** JSON versão 1, sempre na mesma ordem: {"version":1,"best":{"id-a":4200,"id-b":0}}. */
	FString Serialize() const;

	/** Lê o texto com tratamento rígido (docs/regras-save.md). Nunca quebra. */
	static FDNRecordsLoadResult Deserialize(const FString& Text);

	/** Igualdade exata: mesmos ids, com a mesma grafia, e as mesmas pontuações. */
	bool operator==(const FDNRecords& Other) const;
	bool operator!=(const FDNRecords& Other) const { return !(*this == Other); }

private:
	/** Chave sem diferenciar a caixa (comportamento do TMap com FString); a chave guarda a grafia da primeira vez. */
	TMap<FString, int32> Best;
};

struct FDNRecordsLoadResult
{
	FDNRecords Records;
	/** Entradas ignoradas ou repetidas, em português, na ordem da entrada. Com bErro, a razão do erro vem primeiro (R4). */
	TArray<FString> Warnings;
	/** Texto inutilizável: o registro vem vazio. */
	bool bError = false;
};
