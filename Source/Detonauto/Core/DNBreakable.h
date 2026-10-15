// Mobiliário destrutível (postes, árvores, bancos, lixeiras, hidrantes, placas, paraciclos): o carro derruba a peça e segue,
// perdendo um pouco de velocidade. Regra pura, sem mundo e sem física. Contrato: docs/regras-mobiliario.md (lote 17c). Testes:
// Core/Tests/DNBreakable.spec.cpp (decisões B1 a B8 lá). Dados: Data/Breakables.csv (por tipo) e Data/BreakableSettings.csv
// (uma linha). Quem aplica no mundo: Crash/DNBreakableProp e Crash/DNBreakableSubsystem.

#pragma once

#include "CoreMinimal.h"

/** Um tipo de peça (uma linha de Breakables.csv). */
struct FDNBreakableKind
{
	float MassKg = 100.0f;
	/** Abaixo desta velocidade horizontal do carro a peça é sólida. */
	float MinBreakSpeedKmh = 10.0f;
	/** A peça sai com a velocidade do carro vezes isto, na direção do carro. */
	float LaunchFactor = 1.0f;
	/** Mais esta velocidade para cima [m/s]. */
	float LaunchUpMs = 2.0f;
	/** Tomba girando em torno da base (poste, árvore, placa) em vez de voar inteira. */
	bool bTopplesAtBase = false;
	/** Altura onde a peça parte [cm]; o que fica abaixo (toco, base) não sai do lugar. 0 = sai inteira. */
	float BreakHeightCm = 0.0f;
	int32 Points = 0;
};

/** Os valores globais (a linha de BreakableSettings.csv). */
struct FDNBreakableSettings
{
	/** Peso da massa da peça na conta de momento arcade (k). */
	float ArcadeMassFactor = 1.0f;
	/** Teto da perda de velocidade do carro, em porcentagem. */
	float MaxSpeedLossPct = 25.0f;
	/** Teto da velocidade de lançamento da peça [m/s]. */
	float MaxLaunchMs = 35.0f;
	/** Quantas peças soltas simulam ao mesmo tempo; passando disso, a mais antiga congela onde está. */
	int32 MaxActiveProps = 16;
	/** A peça solta dorme depois de SleepAfterS seguidos abaixo desta velocidade [m/s]. */
	float SleepSpeedMs = 0.2f;
	float SleepAfterS = 1.0f;
	/** B9: destroço que o carro alcança sai da frente com a velocidade horizontal do carro vezes isto (>= 1). */
	float PlowFactor = 1.1f;
	/** B9: e com pelo menos esta velocidade para cima [m/s] (um pulo, para não sair arrastando no asfalto). */
	float PlowUpMs = 1.5f;
};

struct FDNBreakInput
{
	/** Velocidade do carro ANTES do contato (cm/s, no mundo, +Z para cima). */
	FVector CarVelocityCmS = FVector::ZeroVector;
	float CarMassKg = 0.0f;
	/** Direção do centro do carro ao centro da peça (só a parte horizontal conta; não precisa ser unitária). */
	FVector CarToProp = FVector::ZeroVector;
	/** Do pé da parte que quebra (o corte) ao centro de massa dela [cm]. Zero ou desconhecido: braço de tombo de ToppleArmM. */
	FVector PivotToComCm = FVector::ZeroVector;
};

struct FDNBreakResult
{
	bool bBreaks = false;
	/** Velocidade do carro depois (só o módulo muda). Sem quebra, a de antes: quem para o carro é a física. */
	FVector CarVelocityCmS = FVector::ZeroVector;
	/** |v| - |v'| [m/s], para o dano do carro. */
	float CarDeltaVMs = 0.0f;
	FVector PropVelocityCmS = FVector::ZeroVector;
	FVector PropAngularRadS = FVector::ZeroVector;
	int32 Points = 0;
};

/** B10: uma peça solta candidata a congelar quando o orçamento estoura. */
struct FDNFreezeCandidate
{
	float SpeedMs = 0.0f;
	/** Encostada no chão (ou em outra coisa embaixo dela); no ar, congelar a deixaria flutuando. */
	bool bGrounded = true;
};

struct FDNBreakable
{
	/** Braço do tombo [m] e teto do giro [rad/s] (B3, escolha deste lote). */
	static constexpr float ToppleArmM = 2.0f;
	static constexpr float MaxToppleRadS = 8.0f;

	static FDNBreakResult Hit(const FDNBreakableKind& Kind, const FDNBreakableSettings& Settings, const FDNBreakInput& Input);

	/** B8: soma o tempo parado (abaixo de SleepSpeedMs); verdadeiro quando chega a SleepAfterS. Mexeu, zera. */
	static bool StepSleep(const FDNBreakableSettings& Settings, float SpeedMs, float DeltaSeconds, float& InOutStillSeconds);

	/**
	 *  B9: o carro varre o destroço que alcança (cm/s, no mundo). Devolve a velocidade nova do destroço; o carro não muda (destroço não
	 *  segura carro). Carro parado ou velocidade inválida: o destroço segue como está.
	 */
	static FVector Plow(const FDNBreakableSettings& Settings, const FVector& CarVelocityCmS, const FVector& DebrisVelocityCmS);

	/**
	 *  B10: qual peça congelar quando o orçamento de peças simulando estoura (índice na lista, da mais antiga para a mais nova). A mais
	 *  LENTA, e nunca uma no ar se houver outra no chão; empate fica com a mais antiga; velocidade inválida conta como a mais rápida.
	 *  Lista vazia: INDEX_NONE.
	 */
	static int32 PickToFreeze(TConstArrayView<FDNFreezeCandidate> Candidates);
};

/** De onde veio uma batida do carro do jogador. */
enum class EDNImpactSource : uint8
{
	Traffic,  // carro do tráfego
	World,    // parede, prédio, barreira
	Prop      // peça de rua destrutível
};

/** B7: o que cada batida dispara. */
struct FDNImpactPolicy
{
	/** Conta como a batida do jogador que abre a rodada de Crash: só no tráfego. */
	static bool CountsForCrashRound(EDNImpactSource Source) { return Source == EDNImpactSource::Traffic; }
	/** Pode disparar a câmera de batida (lote 18): tudo menos peça de rua. */
	static bool TriggersCrashCam(EDNImpactSource Source) { return Source != EDNImpactSource::Prop; }
};
