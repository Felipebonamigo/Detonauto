# Proposta: recursos do Unreal que ainda não usamos

14 out 2026. Proposta da sessão na nuvem. **Decidida em 14 out** (ver o fim), com a delegação do Felipe ("faça tudo você"); ele pode vetar qualquer item. nada aqui muda a
ordem do `docs/plano-visual.md` nem os lotes sem aprovação. Todo item segue a regra 5
(medir antes de aprovar, com o resultado em `docs/desempenho.md`) e a regra 6 (efeito
visual aprovado por captura na cena de aparência e no `-Play`). A lógica continua em
C++ e os ajustes em CSV; nada disso pede Blueprint grosso.

| # | Recurso | Onde encaixa | O que ganha | Como provar |
|---|---|---|---|---|
| 1 | Upscalers: TSR (já na base) e, como opção do jogador, DLSS, FSR e XeSS | Depois do passo 1 do plano visual, já medido no perfil "alvo PS5" | Folga de quadro com Lumen ligado; cada placa usa o seu | `lookdev-measure.ps1` e `crash-measure.ps1` na mesma cena com TSR, DLSS e FSR; p95 do quadro e captura lado a lado |
| 2 | Chaos Destruction (Geometry Collection) | Camada 2 do dano (peças que soltam) e vidros que estilhaçam | Quebra de verdade, o pilar "batida como espetáculo" | `crash-measure.ps1` C-130-40 com e sem fratura; o pool de 24 peças (`FDNPartPool`) continua mandando no limite |
| 3 | Mass, o tráfego do City Sample | Só se o `Rail` não aguentar a densidade que o Crash pedir | Centenas de carros a custo baixo, em sistema feito para cidade | `crash-measure.ps1` P-40 contra a mesma densidade em Mass; se o P-40 já couber no orçamento, fica para depois |
| 4 | MetaSounds | Frente de áudio do `docs/design.md` | Motor que segue rotação, turbo e dano, e batidas em camadas, com as amostras da Sonniss | O Felipe ouve no `-Play`; custo de CPU no Unreal Insights |
| 5 | PCG (geração procedural) | Distrito da fatia vertical | Quarteirões com os prédios do City Sample e detalhes de rua (postes, lixeiras, hidrantes) por regra | Tempo de geração e fps na cena; captura do distrito |
| 6 | Unreal MCP (skill `unreal-mcp`) | Sessão local, já | O Claude monta a cena de aparência e os materiais com o editor aberto, e recompila pelo Live Coding | Ligar uma vez com a skill `ligar-unreal-mcp`; depois, `build.ps1` e `test.ps1` como sempre |

## Ordem sugerida

1. Item 6, porque acelera os outros e não muda o jogo.
2. Item 1, agora que a base de renderização e o perfil alvo PS5 estão medidos.
3. Item 2, depois da camada 1 do dano (morph por zona).
4. Item 3, só depois do lote 13b e da medição do lote 13, e só se o `Rail` não bastar.
5. Itens 4 e 5 quando as frentes de áudio e de distrito abrirem.

## Licenças

Os plugins de upscaler (NVIDIA DLSS, AMD FSR, Intel XeSS) são grátis, mas cada um tem
termos próprios: conferir antes de empacotar e registrar na tela de créditos. O
conteúdo do City Sample segue a licença "só Unreal" já anotada em `docs/licencas.csv`.

## Decisão (14 out 2026)

| # | Recurso | Decisão |
|---|---|---|
| 6 | Unreal MCP | **Aprovado**, depois do lote 13b. Só ligar se os dois plugins existirem no motor 5.8 instalado. Usar só quando o Felipe não estiver usando o PC; os scripts Python versionados continuam sendo a forma oficial de criar cena e material, para tudo ficar reproduzível em texto. |
| 1 | Upscalers | **TSR já basta agora.** DLSS, FSR e XeSS entram na frente de opções gráficas, perto do Acesso Antecipado, com os termos de cada plugin conferidos. |
| 2 | Chaos Destruction | **Aprovado para vidros e peças** (camada 2 do dano), depois da camada 1 (morph por zona) e medido no `crash-measure.ps1`. |
| 3 | Mass | **Só se o `Rail` não aguentar** a densidade do Crash, depois da medição do lote 13. |
| 4 | MetaSounds | **Aprovado** quando a frente de áudio abrir. |
| 5 | PCG | **Aprovado** para o distrito, depois dos prédios do City Sample estarem no projeto. |
