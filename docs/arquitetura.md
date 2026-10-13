# Arquitetura do Detonauto (rascunho)

Rascunho v0, outubro de 2026. Antecipa o documento de arquitetura da Sprint 8.
Será revisado com o que o Felipe aprender nas Sprints 3 a 7 e travado antes de
o projeto definitivo ser criado, na Sprint 9. Nada aqui é decisão final.

O que sustenta tudo: uma pessoa com o Claude, Unreal Engine 5.8, C++ para a
lógica, dados em texto, testes automáticos e 60 fps num PC no nível do PS5.

## Princípios

1. **Lógica em C++, Blueprint só como cola.** Blueprint é binário e o Claude
   não edita com segurança.
2. **Regras separadas do mundo.** Pontuação, turbo e medalhas são funções puras,
   sem física nem Actors, para serem testadas sem abrir o jogo.
3. **Dados em tabelas de texto.** O que se ajusta mora em CSV ou JSON em `Data/`,
   importado como DataTable. Ajustar não exige recompilar.
4. **Desempenho medido antes de aprovado.** Todo sistema que roda a cada
   quadro tem orçamento e medição.
5. **Um módulo, uma responsabilidade.** Dependências sempre apontam para baixo.

## Módulos

```
          UI
           |
 Crash   Traffic
     \     /
      Vehicle    Damage
          \       /
            Core
```

| Módulo | Responsabilidade | Depende de |
|---|---|---|
| `Core` | Tipos e regras puras: pontuação, turbo, medalhas, leitura de tabelas, log | Só a engine |
| `Vehicle` | O carro: física sobre o Chaos Vehicles, entrada, câmera, turbo | Core |
| `Damage` | Deformação, peças soltas, explosão, vidro | Core |
| `Traffic` | Carros de tráfego em trilhas, reaproveitamento, reação a batidas | Core, Vehicle, Damage |
| `Crash` | Regras do modo Crash: largada, valor destruído, multiplicadores, medalhas | Core, Vehicle, Damage, Traffic |
| `UI` | HUD, menus, resultados | Core |

Duas regras de dependência:

- `Core` nunca depende de outro módulo do jogo. É o que torna testável.
- A `UI` lê o estado por interfaces e eventos. Nenhum outro módulo conhece a `UI`.

**Suposição a validar na Sprint 9:** seis módulos podem ser demais para uma
pessoa. Cada módulo tem custo de manutenção (um Build.cs, dependências, tempo de
compilação). Alternativa: começar com três (`Core`, `Gameplay`, `UI`) e dividir
só quando um deles passar de uns 5 mil linhas. Decidir olhando o tempo de build
do projeto de teste.

## Dados

| Tabela | Conteúdo | Quando entra |
|---|---|---|
| `Data/Cars.csv` | Massa, aderência, potência, turbo, resistência de cada carro | Sprint 10 |
| `Data/Crossings.csv` | Cruzamentos: mapa, tráfego, valores, metas das medalhas | Sprint 17 |
| `Data/Scoring.csv` | Multiplicadores e valores do modo Crash | Sprint 17 |
| `Data/TrafficVehicles.csv` | Carros de tráfego, valor em dinheiro, peso | Sprint 16 |
| `Data/Strings.csv` | Textos da interface, por idioma | Sprint 22 |

Cada CSV tem uma `struct` em C++ que herda de `FTableRowBase`. Um teste lê a
tabela e confere se todas as linhas fazem sentido, por exemplo massa maior que
zero.

**Pendência antes do primeiro build empacotado:** `Data/` fica na raiz do
projeto, fora de `Content/`, e a engine só copia para o jogo empacotado as
pastas que a configuração manda. Adicionar `Data/` em `DirectoriesToAlwaysStageAsNonUFS`
(seção `[/Script/UnrealEd.ProjectPackagingSettings]` do `DefaultGame.ini`). O
caminho dessa propriedade é **relativo à pasta `Content/`** (confirmado no
cabeçalho `ProjectPackagingSettings.h` da engine 5.8), então para `Data/` na
raiz seria `../Data`. Falta decidir também entre copiar solto (`NonUFS`) e
colocar dentro do pacote (`DirectoriesToAlwaysStageAsUFS`); os carregadores
atuais leem com `FFileHelper`, e **não foi testado** qual das duas opções
funciona num build empacotado. Não alterar o `DefaultGame.ini` antes disso.

## Testes

- **Automation Spec** para as regras puras de `Core`. Rodam sem janela pelo
  `Tools/Unreal/test.ps1`, com filtro pelo nome do projeto.
- **Validação de dados:** um teste por tabela.
- **Teste de desempenho:** um mapa de teste com 6 carros e 40 de tráfego, medido
  com `stat unit` e Unreal Insights. Resultado anotado em `docs/desempenho.md`.
- **O que não se testa por automação:** a sensação de dirigir. É decisão do
  Felipe jogando.

Meta: toda regra do jogo que não depende de física tem pelo menos um teste.

## Desempenho

Orçamento por quadro para 60 fps: 16,6 ms no total. Divisão inicial, a ajustar
com medições reais no protótipo:

| Parte | Orçamento |
|---|---|
| Game thread (lógica, física, tráfego, dano) | 8 ms |
| Render thread | 8 ms |
| GPU | 14 ms |

Regras de código que seguem desse orçamento:

- Desligar o `Tick` nos Actors que não precisam dele.
- Reaproveitar (pooling) peças soltas, carros de tráfego e efeitos.
- Limite de peças soltas ativas ao mesmo tempo, decidido no teste de dano.
- Dano por deformação e peças destacáveis, não malhas totalmente quebráveis.

Os valores da tabela acima são **suposição**: foram escolhidos para somar com
folga, não medidos.

## Fluxo de uma batida (alto nível)

1. O `Vehicle` detecta o impacto e calcula a força.
2. Se a força passa de um limite, o `Crash` entra em câmera lenta e passa o
   controle dos destroços ao jogador.
3. O `Damage` aplica a deformação e solta as peças.
4. Se o jogador aciona a explosão, o `Damage` aplica um impulso radial, e o
   `Crash` soma o valor de cada veículo atingido, pelas regras do `Core`.
5. O `Traffic` reage: carros atingidos viram corpos físicos e saem do pool.
6. A `UI` mostra a pontuação por evento, sem conhecer os módulos acima.

## Conteúdo e assets

- Carros: veículos do City Sample, modificados no Blender. Prefixos conforme o
  `CLAUDE.md`.
- Toda entrada de asset passa pela planilha `docs/licencas.csv`.
- Scripts: `Tools/Blender/` e `Tools/Unreal/`, em Python e PowerShell.

## Perguntas em aberto

1. ~~Seis módulos ou três no início?~~ **Decidido em 13 out 2026:** um único
   módulo, `Detonauto`, com subpastas que espelham os módulos planejados
   (`Core`, `Vehicle`, `Damage`, `Crash`, `Traffic`, `UI`). As regras de
   dependência valem por convenção de pasta: `Core` nunca inclui as outras. Um
   módulo só tem build mais simples para uma pessoa. Dividir depois é barato se
   alguma subpasta passar de uns 5 mil linhas ou se o tempo de build pedir.
2. Técnica de dano: deformação por vértices, por física ou por malha
   pré-quebrada. Decide o teste das Sprints 12 e 13.
3. Física do carro: Chaos Vehicles com camada própria de forças ou um modelo
   arcade próprio. Primeiro diagnóstico no fim deste documento. Falta o
   Felipe jogar o carro de teste antes de decidir.
4. Como guardar o replay do takedown sem custo alto de memória.
5. Se o tráfego usa física completa ou um modelo simplificado até a batida.
   A primeira medição aponta para o modelo simplificado (ver a seção abaixo).

## Diagnóstico do Chaos Vehicles para o estilo arcade (Sprint 4)

Resultado do projeto descartável 2, escrito pela sessão local a partir do código
da engine e de testes sem janela. O Felipe ainda não jogou o carro. Tudo que não
foi verificado está marcado como suposição.

**Verificado no código:**

- O Chaos Vehicles é uma simulação de veículo com pneus: curva de deslizamento
  lateral, suspensão por raycast, motor e câmbio.
- Em C++ dá para configurar rodas (raio, largura, atrito, freio, esterço),
  massa, motor, transmissão, direção, arrasto, câmera e entrada.
- Em tempo de execução existem setters para torque do motor, multiplicador de
  atrito e de deslizamento das rodas, parâmetros de suspensão, ângulo máximo
  de esterço, arrasto, downforce e divisão de tração.
- Depende de asset, e criar exige o editor visual: a malha esquelética com ossos
  de roda, o Physics Asset e o Anim Blueprint. O teste reaproveitou os do
  template por caminho.

**Leitura provisória, a validar jogando:**

- Dá para chegar a um carro "arcade" só apertando parâmetros: aderência lateral
  alta, centro de massa baixo, arrasto e ganho de direção.
- Derrapada exagerada e giro no ar depois da batida, o que o Burnout faz, não se
  resolvem com parâmetros de pneu. Vão exigir forças próprias sobre o chassi,
  aplicadas a cada quadro, por cima do Chaos.

**Suposições não testadas:**

- Que a configuração de torque no chassi do Chaos serve para o aftertouch.
- Que os setters em tempo de execução aguentam mudanças frequentes.
- O custo por carro com 40 de tráfego.
- A estabilidade com massa e aderência extremas.
- Que a curva de deslizamento lateral só vale na criação do veículo.

**Medição, sem valor de desempenho real:** média de cerca de 1 ms por quadro no
Game thread, em execução sem janela e sem tráfego, sem render e sem dano. Não é
o custo real nem prova de 60 fps.

**Direção provisória:** começar com o Chaos Vehicles configurado por tabela e
adicionar uma camada própria de forças só onde o parâmetro não chegar. Não
construir um modelo de física do zero antes de o Felipe sentir o carro.

## Primeira evidência de desempenho (13 out 2026)

Medição em modo Standalone, 1920x1080, num PC bem mais forte que o alvo "nível
PS5" (RTX 4070 Ti e Ryzen 7 5800X3D). Detalhes e ressalvas em
`docs/desempenho.md`. Os carros extras são cópias do mesmo carro, usadas como
substitutos de custo, e não o tráfego nem o dano reais.

| Cenário | Frame médio | Game thread médio |
|---|---|---|
| 1 carro | 6,7 ms | 1,6 ms |
| 1 mais 20 parados | 7,2 ms | 4,7 ms |
| 1 mais 40 parados | 7,7 ms | 7,0 ms |
| 1 mais 40 em movimento | 9,1 ms | 9,1 ms |

**O que sai daí:**

- A GPU quase não muda (4,7 a 6,4 ms). O que cresce com o número de carros é o
  Game thread, com cerca de 0,13 a 0,19 ms por carro com física completa.
- Com 40 carros em movimento, o Game thread já passa os 8 ms que o orçamento
  deste documento reservou, só com a física, e ainda sem IA, dano ou destroços.
  Num hardware mais fraco, a conta fica pior.
- **Consequência para o desenho:** não é viável dar física completa do Chaos a
  todo carro de tráfego o tempo todo. O tráfego deve seguir trilhas de forma
  simplificada (sem simulação de pneu e suspensão) e só virar corpo físico
  completo quando for atingido. Isso responde a pergunta em aberto nº 5 como
  hipótese de trabalho, a validar no protótipo com tráfego real.
- Diferenças menores que cerca de 1 ms entre cenários não são confiáveis: houve
  ruído entre rodadas iguais.

**Ainda não medido:** tráfego real, dano, destroços, explosão, áudio, 4K, memória
de vídeo, jogo empacotado e um hardware próximo do alvo.

