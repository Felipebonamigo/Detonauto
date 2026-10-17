# Roteiro de playtest 3, 15 minutos

A primeira corrida do jogo, de ponta a ponta: largada, 3 rivais, detona, chegada com classificação. Só o Felipe pode julgar se dá vontade de "mais uma".
Responda em frases curtas; o que não conseguir testar, pule. Capturas do que você vai ver: `docs/img/corrida-1/`.

## Antes (2 min)

1. Feche o editor do Unreal **e os outros projetos Unreal** (TDFende, Age of Earth). Eles pesam no PC e mudam a sensação.
2. No PowerShell, na pasta do projeto:
   `.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -GameMode DNRaceGameMode -AlvoPS5 -Play -ResX 1920 -ResY 1080`
   (abre a corrida no percurso 1, uma rua de cidade de ~2 km com 3 curvas, no preset arcade-3, com os 3 rivais Veloz, Firme e Cauteloso)
3. Se pedir para recompilar, aceite. Na primeira vez a abertura demora (os shaders compilam).

**Teclas:**

| Tecla | O que faz |
|---|---|
| W / S | acelera / freia |
| A / D | esterça |
| Espaço | freio de mão |
| Shift | turbo |
| C | troca a câmera |
| R | recomeça a corrida inteira (grade, rivais, contagem) |

**Como a corrida funciona:** você larga em último, atrás dos 3 rivais, depois de uma contagem de 3 s. Bater num rival com força (e sem destruir o seu carro) é um **detona**:
aparece "DETONOU!", o seu turbo enche e o rival fica parado uns 3 s e volta à pista. Ao cruzar a chegada aparece a classificação; R recomeça.

## 1. Largar e correr (5 min)

Corra duas ou três vezes.

- A contagem (3, 2, 1, VAI!) e o HUD são legíveis? Posição e tempo (em cima à esquerda), velocidade (à direita), turbo (embaixo), barra de progresso com os rivais. O que está pequeno, grande demais ou sobrando?
- Os rivais estão na dificuldade certa? Veloz (~70 s), Firme (~75 s) e Cauteloso (~80 s) para fechar o percurso. Dá para alcançar o Cauteloso? E o Veloz?
- O percurso é bom de dirigir? As 3 curvas são fáceis demais, difíceis demais, ou boas? A rua é larga ou estreita demais?
- A corrida dá vontade de **"mais uma"**? Se não, o que falta?

## 2. Detonar (4 min)

Chegue por trás num rival e bata com força, a 40 ou 50 km/h a mais que ele.

- O detona é **satisfatório**? O aviso "DETONOU!", o rival destroçado e o turbo cheio vendem a ideia?
- O turbo **recompensa**? Dá vontade de caçar o próximo rival com ele?
- Os 3 s do rival parado são pouco ou muito? Ele devia voltar mais longe, ou ficar fora de vez?
- Depois de um detona, **o seu carro ficou destruído também**? Aconteceu muito? Se sim, é frustrante ou parte da graça?
- Faltou alguma coisa: o rival bater de volta, desviar, ou ser empurrado para a parede?

## 3. Chegada e resultado (2 min)

- A tela de resultado mostra o que você queria ver (posição, tempos, detonas)? Falta algo (pontos, medalha, melhor tempo)?
- R para recomeçar é rápido o bastante?

## 4. Visual (2 min)

- A rua está **bonita**? Prédios, asfalto molhado, calçada, céu. O que mais pesa contra: calçada branca demais, prédios iguais, falta de postes e árvores, céu liso?
- Os 4 carros são todos vermelhos. Atrapalha saber quem é quem?
- O jogo ficou fluido o tempo todo? Algum engasgo?

## 5. Geral (1 min)

- O que mais incomodou?
- O que mais gostou?
- Falta o som (ainda não há nenhum). Sem ele, a corrida perde muito?

## Como devolver

Cole as respostas no chat da nuvem. Viram ajustes de dados (CSV) e de código.
