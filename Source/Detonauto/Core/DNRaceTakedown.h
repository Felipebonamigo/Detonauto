// Takedown na corrida (marco 1c de docs/fatia-jogavel.md): o que acontece com o rival derrubado. Regras puras, sem UObject. Testes:
// Core/Tests/DNRaceTakedown.spec.cpp. A qualificação e o tipo do takedown são de Core/DNTakedown (FDNTakedown).
//
// Decisões:
//  W1 um rival está "batido" (condição 2 do takedown) quando o dano total dele (FDNDamage::TotalDamage) sobe pelo menos MinGain desde o
//     contato do jogador, ou quando ele fica destruído (D12); dano não finito não conta;
//  W2 o rival derrubado fica parado, destroçado, por WreckSeconds; passado esse tempo volta à pista, inteiro, no ponto da linha onde
//     estava (o Step devolve verdadeiro uma vez, no quadro em que ele volta);
//  W3 um novo derrubar enquanto já está destroçado não reinicia a contagem;
//  W4 tempo de quadro negativo ou não finito não anda a contagem.

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
};
