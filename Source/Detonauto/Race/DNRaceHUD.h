// HUD da corrida (marco 1d de docs/fatia-jogavel.md): posição, tempo, velocidade, turbo, takedowns, progresso de todos, contagem e a
// tela de resultado. Desenhado direto no Canvas, em C++, sem asset (sem UMG, sem Blueprint). Os números vêm do ADNRaceGameMode
// (BuildHudData); nenhuma regra aqui. Os textos vêm de Core/DNRaceFormat.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DNRaceHUD.generated.h"

/** Uma linha da classificação. */
struct FDNRaceHudRow
{
	FString Place;
	FString Name;
	/** Tempo de chegada, ou metros andados de quem ainda corre. */
	FString Value;
	bool bPlayer = false;
};

/** Tudo o que o HUD mostra num quadro. */
struct FDNRaceHudData
{
	int32 Position = 1;
	int32 Total = 1;
	float TimeS = 0.0f;
	float SpeedKmh = 0.0f;
	float TurboBar = 0.0f;
	float TurboMax = 100.0f;
	int32 Takedowns = 0;
	/** Segundos desde o último takedown (grande, para o aviso na tela) e o texto dele. */
	float TakedownAgeS = 1000.0f;
	FString TakedownText;
	/** 0 a 1: quanto do percurso o jogador andou. */
	float PlayerProgress = 0.0f;
	/** O progresso (0 a 1) de cada rival, para as marcas na barra. */
	TArray<float> RivalProgress;
	/** Segundos que faltam para a largada (0 = largou). */
	float CountdownLeftS = 0.0f;
	bool bFinished = false;
	bool bPlayerWrecked = false;
	TArray<FDNRaceHudRow> Rows;
	/** Distância desde o jogador ate o checkpoint seguinte ou a chegada [m]; negativa = não mostrar. */
	float ToNextMarkM = -1.0f;
};

UCLASS()
class DETONAUTO_API ADNRaceHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void Text(const FString& Value, float X, float Y, float Scale, const FLinearColor& Color, bool bCenter = false, bool bRight = false);
};
