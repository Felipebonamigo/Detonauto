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
//   -DNRaceAutoKmh=K   velocidade do piloto automático na reta (padrão 100)
//   -DNRaceMaxS=S      tempo máximo da corrida automática (padrão 240 s)
// As teclas e capturas do ADNVehicleTestGameMode (-DNKeys=SHOT@t;QUIT@t) valem aqui.

#pragma once

#include "CoreMinimal.h"
#include "Vehicle/DNVehicleTestGameMode.h"
#include "Core/DNRace.h"
#include "Core/DNRivalDriver.h"
#include "Core/DNRaceTakedown.h"
#include "Core/DNTakedown.h"
#include "Race/DNRaceHUD.h"
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

	/** Os números do HUD deste quadro (ADNRaceHUD). */
	FDNRaceHudData BuildHudData() const;

protected:
	virtual void StartPlay() override;

private:
	void DriveAuto(ADNVehicle& Vehicle) const;
	void TickRivals(float DeltaSeconds);

	/** Poste, árvore e banco pela calçada, a cada ~22 m, alternando os lados, em ritmo fixo: peças destrutíveis do mobiliário (Data/Breakables.csv). */
	void SpawnStreetLife();
	void TickTakedowns(ADNVehicle& Vehicle);

	/** O jogador tocou em algo: se foi num rival, abre uma janela de takedown (FDNTakedown). */
	UFUNCTION()
	void OnPlayerHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	void LogStandings(const TCHAR* Why) const;
	/** Tecla R: a corrida recomeça (a grade, os estados, os rivais); o carro do jogador volta pelo próprio DNRestart. */
	void ResetRace();

	UPROPERTY(Transient)
	TObjectPtr<ADNCourseActor> Course;

	FString CourseId = TEXT("percurso1");
	float CheckpointM = 250.0f;
	bool bAuto = false;
	bool bHandbrakeReleased = false;
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
		FDNWreckState Wreck;
		/** Um takedown acabou de derrubá-lo: o próximo quadro o põe em destroço. */
		bool bWreckNow = false;
		/** O jogador tocou nele neste quadro: liga a braveza (W6). */
		bool bTouched = false;
		/** Quanto falta para ele deixar de revidar [s]; enquanto > 0 ele mira no jogador. */
		float AngerLeftS = 0.0f;
	};

	/** Um toque do jogador num rival, esperando ele ser derrubado dentro da janela (condição 2 do takedown). */
	struct FPendingContact
	{
		int32 RivalIndex = -1;
		double Time = 0.0;
		float RelativeSpeedKmh = 0.0f;
		/** Dano total do rival no instante do toque: o takedown pede que ele suba MinCrashDamageGain dentro da janela. */
		float RivalDamageBefore = 0.0f;
		FDNTakedownClassifyInput Classify;
	};
	TArray<FPendingContact> Pending;
	FDNTakedownConfig TakedownConfig;
	/** Velocidades do quadro anterior: no contato a física já mexeu nas atuais. */
	FVector PrevPlayerVelocity = FVector::ZeroVector;
	TArray<FVector> PrevRivalVelocity;
	bool bPlayerHitBound = false;
	int32 Takedowns = 0;
	double LastTakedownAt = -100.0;
	FString LastTakedownText;
	/** Quanto tempo o rival derrubado fica destroçado antes de voltar (W2). */
	float WreckSeconds = 3.0f;
	/** O jogador destruído vira naufrágio por este tempo e renasce na linha, inteiro, com o turbo zerado; a corrida só acaba na chegada. */
	float PlayerWreckSeconds = 2.5f;
	FDNWreckState PlayerWreck;
	/** Instante do jogo em que o jogador ficou destruído (-1 = inteiro): a regra W5 compara com o do rival. */
	double PlayerDestroyedAtS = -1.0;
	/** Quanto tempo o rival tocado revida [s] (W6). */
	float AngerSeconds = 2.0f;
	// atalho: o dano que "bate" o rival (W1) é uma constante; vai para Data/Takedown.csv se o playtest pedir ajuste
	float MinCrashDamageGain = 0.05f; // dano total é a média de 5 zonas: 0,05 é uma zona com ~0,25 de dano
	/** -DNRaceHunt: o piloto automático do jogador corre mais que os rivais (200 km/h na reta), para bater neles e provar o takedown. */
	bool bHunt = false;
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
