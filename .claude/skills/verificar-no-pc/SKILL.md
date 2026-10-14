---
name: verificar-no-pc
description: Compila o Detonauto (build.ps1), roda os testes Automation Spec (test.ps1, com filtro) e faz a prova por mutação, no PC do Felipe. Use para "compilar", "rodar os testes", "testar o lote", "provar por mutação", ou quando um build ou teste falhar.
---
# Verificar no PC: compilar, testar e provar por mutação

## Antes: onde você está
- O build do Unreal só roda no PC do Felipe (Windows, UE 5.8, Visual Studio). A sessão na nuvem **não compila, não roda
  teste e não abre o editor**: ela edita código e deixa os comandos abaixo para a sessão local ou o Felipe.
- Nunca diga "compila" ou "os testes passam" sem a saída do comando, rodado no PC, naquele turno.
- PowerShell, na raiz do repositório. Testes primeiro, como no fluxo de `docs/estado-atual.md`.

## 1. Compilar
Feche o editor do Unreal antes: ele trava a DLL do módulo.
```powershell
.\Tools\Unreal\build.ps1 -Uproject Detonauto.uproject
```
- Alvo `DetonautoEditor` Win64 `Development` (`-Configuration` muda). A versão da engine vem do `.uproject`
  (`EngineAssociation` 5.8); `-EngineVersion` só se não vier. A engine é achada pelo registro ou pelo `LauncherInstalled.dat`.
- Deu certo: `BUILD OK` e saída 0. Falhou: `BUILD FALHOU (código N)` (o 6 já apareceu com erro de sintaxe).
- Live Coding (editor aberto) só para corpo de função. `.h`, `UPROPERTY`, classe nova ou `Build.cs`: editor fechado e `build.ps1`.

## 2. Rodar os testes
```powershell
.\Tools\Unreal\test.ps1 -Uproject Detonauto.uproject
.\Tools\Unreal\test.ps1 -Uproject Detonauto.uproject -Filter Detonauto.Turbo
.\Tools\Unreal\test.ps1 -Uproject Detonauto.uproject -Filter "Detonauto.Carro+Detonauto.Dados+Detonauto.Camera+Detonauto.Subsistemas"
```
- Compile antes: o runner (`UnrealEditor-Cmd`, sem janela, `-nullrhi`) carrega a DLL já compilada.
- Sem `-Filter` roda tudo que começa com `Detonauto`. Grupos de hoje (prefixo `Detonauto.`): `Amassado`, `Camera`, `Carro`,
  `Componentes`, `Crash`, `Dados`, `Dano`, `Globo`, `Medalhas`, `Pontuacao`, `Progresso`, `Registro`, `Subsistemas`,
  `Takedown`, `Trafego`, `Turbo`. Vários juntos com `+`, como no lote 14. O exemplo `Detonauto.Score` do cabeçalho do
  script não existe: a pontuação é `Detonauto.Pontuacao`.
- `-TimeoutSeconds` (padrão 600).
- Deu certo: `RESUMO: N aprovados, 0 reprovados, 0 não rodaram` e saída 0. Saída 1: algum `[FALHOU]` (a mensagem vem
  logo abaixo), algum não rodou, nenhum teste no filtro, ou timeout.
- Relatório: `Saved\AutomationReport\index.json`. Log: `Saved\Logs\test-automation.log`.

## 3. Prova por mutação (como no lote 14)
Não há script: é à mão. Um teste só vale se fica vermelho quando o código está errado.
1. Controle: compile o código limpo, rode o filtro do lote e veja tudo verde.
2. Para cada mutante: troque um trecho (`>` por `>=`, tire uma guarda, mude um limite), compile, rode o filtro,
   anote morto (vermelho) ou vivo, e restaure.
3. Mutantes em `Data/*.csv` não recompilam: rode-os depois de compilar o código **limpo**, senão o teste roda contra o
   binário do último mutante de código.
4. Controle no fim, sem mutação: tudo verde de novo.
5. Mutante vivo: escreva o teste que falta, ou documente por que é equivalente (no lote 14: `>=` no lugar de `>` onde o
   excesso já é zero ali).

## Quando falha
- Build não grava a DLL: editor aberto. Feche e compile de novo. Ao abrir o editor, se ele pedir para recompilar, aceite.
- `Sem relatório em ...` ou `TIMEOUT`: leia `Saved\Logs\test-automation.log`. Já aconteceu de um teste indexar array sem
  conferir o tamanho e derrubar o runner inteiro: confira o tamanho antes de indexar.
- `Nenhum teste encontrado`: o filtro está errado ou o spec não se chama `Detonauto.Algo`.
- Verde que não testa: `AddExpectedError` com `[..]` vira classe de caracteres e engole falhas; use texto literal.
- Outras armadilhas (`docs/estado-atual.md`): `bUseUnity = false` no `Detonauto.Build.cs` (helpers de testes colidiam);
  `FString ==` ignora caixa (use `Equals(..., ESearchCase::CaseSensitive)`); `FJsonObject` junta chaves que diferem só na
  caixa (leia em fluxo com `TJsonReader`); subsistema de `GameInstance` em teste só com `DNTest::NewSubsystem<T>()`
  (`Core/Tests/DNTestData.h`).
- Teste verde não prova efeito visível: use a skill `jogar-e-capturar`.

## Depois
- Registre no documento do lote: comando, filtro, número de testes, a linha `RESUMO`, mutantes mortos/total no código e
  nos CSV e os equivalentes com o motivo (modelo: `docs/lote-14.md`, seção "Prova por mutação").
- Armadilha nova que custou retrabalho vai para "Armadilhas já descobertas" em `docs/estado-atual.md` (skill `fechar-lote`).
