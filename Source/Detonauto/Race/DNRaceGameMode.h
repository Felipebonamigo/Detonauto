// GameMode da corrida ponto a ponto (marco 1 de docs/fatia-jogavel.md, passo a1 de docs/percurso-1.md): monta o percurso
// (ADNCourseActor, a linha vem de Data/CoursePoints.csv), põe o carro do jogador na largada e liga as regras do Core (FDNCourse para o
// progresso, FDNRace para a chegada). Só cola; nenhuma regra nova aqui. Sem .umap e sem Blueprint: abra qualquer mapa vazio.
//
//     UnrealEditor.exe Detonauto.uproject /Engine/Maps/Entry?game=/Script/Detonauto.DNRaceGameMode -game
//
// Linha de comando:
//   -DNCourse=<id>     percurso de Data/CoursePoints.csv (padrão percurso1)
//   -DNCheckpointM=M   metros entre os pontos de verificação (padrão 250)
//   -DNRaceAuto        o carro dirige sozinho pela linha (mira um ponto à frente) e o jogo fecha ao chegar; imprime "DNRace: ..."
//   -DNRaceMaxS=S      tempo máximo da corrida automática (padrão 240 s)
// As teclas e capturas do ADNVehicleTestGameMode (-DNKeys=SHOT@t;QUIT@t) valem aqui.

#pragma once

#include "CoreMinimal.h"
#include "Vehicle/DNVehicleTestGameMode.h"
#include "Core/DNRace.h"
#include "Core/DNRivalDriver.h"
#include "DNRaceGameMode.generated.h"

class ADNCourseActor;
class ADNVehicle;

UCLASS()
class DETONAUTO_API ADNRaceGameMode : public ADNVehicleTestGameMode
{
	GENERATED_BODY()

public:
	ADNRaceGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void StartPlay() override;

private:
	void DriveAuto(ADNVehicle& Vehicle) const;
	void TickRivals(float DeltaSeconds);
	void LogStandings(const TCHAR* Why) const;
	void UpdateHud(const ADNVehicle& Vehicle) const;

	UPROPERTY(Transient)
	TObjectPtr<ADNCourseActor> Course;

	FString CourseId = TEXT("percurso1");
	float CheckpointM = 250.0f;
	bool bAuto = false;
	/** Como o piloto automático dirige (hoje os valores padrão; os níveis dos rivais vêm de Data/Rivals.csv). */
	FDNRivalConfig AutoDriver;
	float MaxSeconds = 240.0f;

	/** Um rival: o carro, o nível do piloto (Data/Rivals.csv) e o estado na corrida. */
	struct FRival
	{
		TWeakObjectPtr<ADNVehicle> Car;
		FString Name;
		FDNRivalConfig Config;
		FDNRacerState State;
	};
	TArray<FRival> Rivals;
	/** Quantos rivais correm (-DNRivals=N, até o número de linhas de Data/Rivals.csv; padrão todos). */
	int32 RivalLimit = 99;
	/** Segundos entre o começo e a largada (-DNRaceCountdownS): ninguém acelera antes. */
	float CountdownS = 3.0f;
	float SinceStartS = 0.0f;

	FDNRacerState Player;
	float RaceTime = 0.0f;
	int32 NextMark = 0;
	bool bFinishedLogged = false;
	float FinishedAt = -1.0f;
	float LastCheckpointAtS = 0.0f;
	/** Último instante em que o progresso subiu pelo menos 1 m, e quanto dele valia: o piloto automático desiste se travar. */
	float LastAdvanceAtS = 0.0f;
	float LastAdvanceCm = 0.0f;
	float MaxSpeedKmh = 0.0f;
	float MaxOffsetCm = 0.0f;
};
