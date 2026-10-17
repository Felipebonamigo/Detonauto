// Takedown na corrida (marco 1c de docs/fatia-jogavel.md): o que acontece com o rival derrubado. Regras puras, sem UObject. Testes:
// Core/Tests/DNRaceTakedown.spec.cpp. A qualificação e o tipo do takedown são de Core/DNTakedown (FDNTakedown).
//
// Decisões:
//  W1 um rival está "batido" (condição 2 do takedown) quando o dano total dele (FDNDamage::TotalDamage) sobe pelo menos MinGain desde o
//     contato do jogador, ou quando ele fica destruído (D12); dano não finito não conta;
//  W2 o rival derrubado fica parado, destroçado, por WreckSeconds; passado esse tempo volta à pista, inteiro, no ponto da linha onde
//     estava (o Step devolve verdadeiro uma vez, no quadro em que ele volta);
//  W3 um novo derrubar enquanto já está destroçado não reinicia a contagem;
//  W4 tempo de quadro negativo ou não finito não anda a contagem;
//  W5 o jogador "se destruiu primeiro" só se o instante em que ele ficou destruído (>= 0; -1 = não ficou) é estritamente anterior ao em que
//     o rival foi batido; empate vale o detona (a janela de dano fecha os dois carros no mesmo quadro). Só nesse caso a condição 3 do
//     contrato (FDNTakedown::Qualifies) desqualifica;
//  W6 rival tocado pelo jogador reage: fica "bravo" por DurationS (volta a valer DurationS a cada toque), contando para zero com o tempo;
//     tempo de quadro inválido não anda a contagem; duração inválida ou <= 0 nunca deixa bravo.

#pragma once

#include "CoreMinimal.h"

struct FDNWreckState
{
	bool bWrecked = false;
	float LeftS = 0.0f;
};

struct FDNRaceTakedown
{
	/** W1: o dano total do rival subiu MinGain ou mais entre o contato (DamageBefore) e agora (DamageNow). */
	static bool RivalCrashed(float DamageBefore, float DamageNow, bool bDestroyed, float MinGain);

	/** W2 a W4: anda um quadro. bWreckNow marca o instante em que o rival foi derrubado. Devolve verdadeiro no quadro em que ele volta à pista. */
	static bool Step(FDNWreckState& State, float DeltaS, bool bWreckNow, float WreckSeconds);

	/** W5. */
	static bool PlayerDestroyedFirst(float PlayerDestroyedAtS, float RivalCrashedAtS);

	/** W6: o que falta de braveza [s] depois de um quadro. */
	static float AngerStep(float LeftS, float DeltaS, bool bTouched, float DurationS);
};
