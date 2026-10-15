---
name: ligar-upscaler
description: 'Instala e mede o DLSS da NVIDIA (Super Resolution, DLAA, Frame Generation, Reflex) e o FSR da AMD no Detonauto, só no projeto e fora do git, a partir do zip que o Felipe já baixou. Use na sessão do PC quando o Felipe pedir "liga o DLSS", "põe o FSR", "frame generation" ou "Reflex", ou quando a meta de fps não fechar por causa da GPU.'
---
# Ligar o upscaler (DLSS e FSR)

O Age of Earth fez primeiro (15/10/2026): FSR 4.1.1 e DLSS 4.5 instalados só na pasta `Plugins/` do projeto, fora do git e sem
mexer no motor, que é o mesmo para todos os jogos. O registro está em `unreal/REVISAO-2026-10-14.md` (§13) e
`unreal/PENDENCIAS.md` do repositório `Felipebonamigo/AgeOfEarth`: leia antes de começar.

## Vale a pena neste jogo?

Corrida com Lumen e destruição, com meta de 60 fps numa placa no nível do PS5 (a 4070 Ti do PC é mais forte). DLSS e FSR dão
folga de GPU para o dano e o tráfego. Decisão do Felipe (14/10/2026, `docs/proposta-recursos-unreal.md`): o TSR basta agora;
DLSS, FSR e XeSS entram nas opções gráficas perto do Acesso Antecipado. Instalar agora serve para medir e decidir.

Upscaler só dá fps quando a GPU é o gargalo. Antes de instalar, rode a skill `achar-gargalo`: se `stat unit` mostra `Game` ou
`Draw` acima de `GPU`, o ganho vai ser pequeno. Foi o caso do Age of Earth, preso na CPU: TSR 80,7 fps, FSR Desempenho
83,6 fps. Mesmo assim o upscaler serve pela qualidade (DLAA) e para quem tem placa mais fraca.

## O que cada plugin faz

| Plugin | Para quê | Cuidado |
| --- | --- | --- |
| `DLSS` + `StreamlineNGXCommon` | Super Resolution (desenha menor e reconstrói) e DLAA (antisserrilhado na resolução cheia) | só placas RTX |
| `StreamlineCore` + `StreamlineDLSSG` | Frame Generation: cria um quadro entre dois e quase dobra o fps visto | soma latência; só RTX 40 ou mais nova; sempre junto com o Reflex |
| `StreamlineReflex` | corta a latência entre o comando e a tela | só GeForce |
| `NIS`, `StreamlineDeepDVC`, `DLSSMoviePipelineSupport` | nitidez, cor e o DLSS no Movie Render Queue (trailer) | opcionais |
| `FSR` (AMD, MIT) | upscaler para qualquer placa, inclusive o Steam Deck | — |
| TSR | o padrão do Unreal, sem plugin | — |

## Passos (PC, editor fechado)

1. Os zips ficam em `C:\Users\Felip\Downloads`: o do DLSS é `2026.09.15_UE5.8_DLSS4.5Plugin_v8.8.0.zip` (a licença já foi
   aceita pelo Felipe; não baixe de novo). O do FSR vem de gpuopen.com/amd-fsr-upscaling (MIT). Se faltar algum ou aparecer
   licença nova, pare e peça ao Felipe.
2. Confira que o `.gitignore` deixa as pastas dos plugins fora do git (desde 15/10/2026): `/.gitignore`, bloco "Upscalers". São perto de 2 GB, o
   repositório é público e o LFS gratuito tem 10 GiB. A licença da NVIDIA também não deixa redistribuir o plugin.
3. Extraia num lugar temporário com `tar -xf` (o `unzip` do Git Bash ignora as subpastas, registrado no Age of Earth).
4. Copie as pastas dos plugins para `Plugins/ (na raiz do repositório, ao lado do Detonauto.uproject)`. Nunca para `C:\Program Files\Epic Games\UE_5.8\Engine\Plugins`, que os jogos
   dividem.
5. Se for preciso ligar algum plugin no `Detonauto.uproject`, use `"Optional": true` na entrada: assim o projeto continua abrindo num
   clone sem a pasta (a sessão da nuvem, outro PC). Confira abrindo um clone limpo.
6. Compile (`Tools\Unreal\build.ps1 -Uproject Detonauto.uproject`) e confira em Edit > Plugins. Os comandos de console de cada plugin (ligar, modo de qualidade, Frame
   Generation, Reflex) estão no guia que vem dentro do zip: use os de lá e anote os que funcionaram. No Age of Earth, o FSR
   liga com `r.FidelityFX.FSR.Enabled 1` e `r.FidelityFX.FSR.QualityMode 0` a `4`.
7. Meça com `Tools\Unreal\lookdev-measure.ps1` e `Tools\Unreal\crash-measure.ps1` (com o perfil `DN_AlvoPS5` quando der): mesma cena, mesma câmera, GPU livre (feche os outros editores; no Age of Earth uma medição com outro
   editor usando 80 % da GPU saiu sem sentido). Compare TSR, DLSS Qualidade, DLSS Desempenho, DLAA, FSR e Frame Generation
   com Reflex. Anote média, p95 e tempo de GPU em `docs/desempenho.md`, com uma captura de cada.
8. Deixe tudo instalado e desligado por padrão até existir a tela de opções gráficas. Registre a licença de cada plugin em
   `docs/licencas.csv` (NVIDIA RTX SDK; AMD MIT) e confira no texto de licença do zip se a NVIDIA pede aviso ou logo na tela de créditos.
9. Commit só do que é texto (`.gitignore`, `.uproject`, `Config/`, documentação). A pasta do plugin nunca.

## Armadilhas

- Frame Generation não conta para a meta de 60 fps: o comando continua no ritmo dos quadros reais. A meta vale sem ele.
- Corrida pede resposta rápida: Frame Generation só junto com o Reflex, e testado no controle e no volante antes de virar opção.
- No Steam Deck e em placa AMD ou Intel, o DLSS não roda: o jogo cai para FSR ou TSR. A tela de opções precisa mostrar só o que
  a placa aceita.
- DLSS e Frame Generation mudam de versão com frequência: anote a versão do zip na linha de licença.
