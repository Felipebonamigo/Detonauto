---
name: publicar-na-steam
description: 'Empacota o Detonauto (BuildCookRun do Unreal), confere o build e sobe para a Steam pelo SteamPipe, parando onde só o Felipe pode seguir (login, Steam Guard, branch padrão). Use quando o Felipe pedir "sobe para a Steam", "gera o build da loja", "publica no beta" ou "prepara o lançamento".'
---
# Publicar na Steam

Estado hoje: não há App ID, conta Steamworks, script de empacotamento nem a seção de empacotamento no `Config/DefaultGame.ini`.
O plano é Acesso Antecipado na Steam em nov 2028, com 20 cruzamentos, 6 carros e PT/EN/ES (`docs/design.md`).

## O que só o Felipe faz

- Conta Steamworks, taxa do Steam Direct, dados fiscais e bancários, criar o app (sai o **App ID**) e os depots.
- O login no `steamcmd`, com a **conta de build** (nunca a pessoal) e o Steam Guard. O Claude nunca digita senha nem código.
- Pôr o build no branch **padrão** (o que todo mundo baixa) e apertar o botão de lançar. O `steamcmd` só põe no ar
  branches beta; o padrão é sempre no painel, pela mão do Felipe.
- Aceitar termos, pagar ou mudar preço.

## Passos

1. Verifique o jogo (skill `verificar-no-pc`). Nada sobe com teste vermelho.
2. Empacote: ainda não há script; escreva `Tools/Unreal/package.ps1` no primeiro empacotamento, a partir de
   ```
   <UE 5.8>\Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -project=<caminho>\Detonauto.uproject -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory=<pasta> -noP4 -utf8output
   ```
   Antes do primeiro: a pasta `Data/` precisa entrar em `DirectoriesToAlwaysStageAsNonUFS` (pendência 6 de
   `docs/pendente-playtest.md`; `docs/arquitetura.md`). Sem isso o jogo empacotado abre sem as tabelas. O documento pede
   para não mexer no `DefaultGame.ini` antes: combine com o Felipe.
3. Confira o build antes de subir:
   - roda sem o editor e sem a pasta do código (abra o executável empacotado);
   - tamanho total e o que entrou: nada de `.pdb`, log, save de teste, arquivo-fonte de arte ou pasta `Saved/`;
   - versão do jogo visível no menu ou no log, igual à da tag;
   - licenças e créditos no build (skill `declaracao-ia` e o registro de licenças do jogo).
4. Prepare o SteamPipe: não existe ainda. Crie em `Tools/Steam/` o `app_build_<AppID>.vdf` e o `depot_build_<DepotID>.vdf` (com
   `"FileExclusion" "*.pdb"`), com os números que o Felipe passar. No `app_build`, `SetLive` aponta para um branch beta (ex.: `"SetLive" "beta"`).
   Guarde fora do depot os símbolos (`.pdb`) da mesma versão: sem eles, a pilha de um crash vem só com endereços.
5. Peça ao Felipe para rodar o envio, com a conta de build: `steamcmd +login <conta_de_build> +run_app_build <caminho>\Tools\Steam\app_build_<AppID>.vdf +quit`.
6. O Felipe instala pelo cliente Steam, no branch beta, e joga 5 minutos. Só então ele promove ao padrão no painel.
7. Anote a versão, a data, o branch e o tamanho em `docs/estado-atual.md`.

## Armadilhas

- A Valve revisa a página e o build antes do lançamento e exige a página "Em breve" no ar por um tempo mínimo. Prazos
  mudam: confira no Steamworks antes de marcar data.
- Conteúdo de IA: a declaração da página tem que bater com o build (skill `declaracao-ia`).
- Nunca Burnout, EA nem os nomes de modo herdados na página, nas capturas ou no build (CLAUDE.md).
- Upscaler (DLSS, FSR, XeSS) pede licença e crédito na tela de créditos antes de empacotar (`docs/proposta-recursos-unreal.md`).
- Conteúdo da Epic e do City Sample: confira a licença no Fab antes do lançamento (observações em `docs/licencas.csv`).
