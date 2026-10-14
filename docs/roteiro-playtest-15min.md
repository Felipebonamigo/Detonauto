# Roteiro de playtest, 15 minutos

Uma sessão só, com tudo o que depende do Felipe jogar. Responda em frases curtas;
o que não conseguir testar, pule. Comparação de referência: como lembra do Burnout.

## Antes (2 min)

1. Feche o editor do Unreal.
2. No PowerShell, na pasta do projeto:
   `.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Play`
3. Se pedir para recompilar, aceite.

Teclas: `1` `2` `3` trocam arcade-1, arcade-2, arcade-3; `0` volta ao Padrão;
Shift = turbo; Espaço = freio de mão; `T` `Y` `U` = amassado na frente (pequeno,
médio, máximo); `G` `H` `J` = lateral esquerda; `V` limpa. A parede fica a ~50 m.

## Carro (6 min)

Para cada preset (`0`, `1`, `2`, `3`), dirija uns 30 segundos e responda:

| Pergunta | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| Peso: leve, na medida ou pesado? | | | | |
| Derrapa com Espaço? Dá para controlar? | | | | |
| Curva: responde bem? | | | | |

Depois, só uma vez:
- A câmera está boa ou ainda perto/longe demais?
- O turbo (Shift) dá a sensação de empurrão? Parece forte, fraco ou na medida?
- As rodas giram e esterçam direito em movimento?

## Amassado (4 min)

1. Aperte `T`, depois `Y`, depois `U` e olhe a frente do carro.
2. Aperte `G` `H` `J` e olhe a lateral. `V` limpa.
3. Bata no muro a ~50 km/h e a ~100 km/h.

Responda: parece batida de carro ou papel amassado? Falta alguma coisa
(vidros, faróis, peças soltando)? A 100 km/h o carro "morrer" no muro faz sentido
ou é cedo/tarde demais?

## Crash com tráfego (5 min, só se o lote 13 já estiver pronto)

Se a sessão local avisar que o modo de teste do Crash está na origin/main:
rodar o modo de teste do Crash (ela informa o comando), entrar no tráfego a
velocidades diferentes e responder:
- O tráfego batendo parece vivo ou parado?
- A pontuação e a medalha na tela fazem sentido?
- O jogo continua fluido (sem engasgos)?

## Como devolver

Cole as respostas aqui no chat. Eu transformo em ajustes de dados e de código.
