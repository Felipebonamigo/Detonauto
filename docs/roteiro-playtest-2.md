# Roteiro de playtest 2, 20 minutos

Tudo o que entrou desde o primeiro playtest e só o Felipe pode julgar jogando.
Responda em frases curtas; o que não conseguir testar, pule. Referências: Burnout
(sensação arcade, câmera de batida) e Wreckfest (dano e peças).

## Antes (2 min)

1. Feche o editor do Unreal **e os outros projetos Unreal** (TDFende, Age of
   Earth). Eles pesam no PC e mudam a sensação.
2. No PowerShell, na pasta do projeto:
   `.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Map /Game/Detonauto/Maps/L_Lookdev -AlvoPS5 -Play`
3. Se pedir para recompilar, aceite.

**Teclas:**

| Tecla | O que faz |
|---|---|
| W / S | acelera / freia |
| A / D | esterça |
| Espaço | freio de mão |
| Shift | turbo |
| C | troca a câmera |
| 1 2 3 0 | presets de carro |
| R | coloca o carro inteiro de novo na largada (a sessão local confirma) |

A rua da cena de aparência tem postes, árvores, bancos, lixeiras e hidrantes que
quebram, e um muro no fim.

## 1. Dirigir (4 min), no preset padrão (arcade-3)

- Peso: leve, na medida ou pesado?
- A derrapagem com Espaço dá para controlar, ou ainda perde o carro?
- O turbo (Shift) agora empurra de verdade?
- A sensação de velocidade a 150–220 km/h melhorou? O campo de visão abre, a
  câmera treme de leve e as bordas da tela borram. As rodas ficam nítidas?
- Das 5 câmeras (C), qual você usaria? Alguma está ruim?

## 2. Destruir a rua (4 min)

Passe por cima de postes, árvores, bancos, lixeiras e hidrantes, a 60 e a
100 km/h.

- Parece destruição? Cai, voa e tomba de um jeito convincente?
- O carro perde a velocidade certa? (Poste tira uns 15%, árvore uns 23%,
  lixeira quase nada.)
- Empurrar um poste ou um tronco já caído sem frear parece leve demais?
- Devagar (10 km/h) o poste segura o carro. Faz sentido?

## 3. Bater (6 min)

Bata no muro a ~100 km/h, de frente. Depois bata de lado num poste grosso ou no
muro, para pegar a porta.

- **Câmera lenta:** vende a batida? Dura muito ou pouco (hoje ~1,5 s a 1/4 da
  velocidade)? Mostra o lado amassado?
- **Controle do destroço:** durante a câmera lenta, A e D empurram o carro uns
  1,3 m. É pouco, muito, ou não deu para perceber?
- **Peças:**
  - para-choque e capô pendurados e depois saindo;
  - porta abrindo e balançando;
  - para-lamas e aerofólio voando.
  Parece o Wreckfest? Saem cedo ou tarde demais? Voam devagar demais na câmera
  lenta?
- **Vidros:** o para-brisa trincando e o vidro lateral estourando estão bons?
- **Amassado:** a frente destruída e as lascas de tinta nas dobras parecem
  batida de verdade?

## 4. Geral (2 min)

- O jogo ficou fluido o tempo todo? Algum engasgo na batida?
- O que mais incomodou?
- O que mais gostou?
- O que falta para parecer um jogo de batida "de verdade"?

## Como devolver

Cole as respostas no chat da nuvem. Viram ajustes de dados (CSV) e de código.
