---
name: revisar-sem-compilar
description: 'Confere o C++ do Unreal do Detonauto antes do commit, porque a nuvem não compila: roda o conferidor (generated.h, GENERATED_BODY, UPROPERTY, *_API, .h × .cpp, includes e Build.cs), revisa à mão o que ele não pega e diz ao PC o que compilar. Use ao mexer em .h ou .cpp de Source/Detonauto, antes de commitar C++, ou quando o Felipe disser "vai compilar?", "confere o C++" ou "o build quebrou".'
---
# Revisar o C++ sem compilar

A nuvem edita o C++ mas não tem o Unreal. Erro bobo só aparece quando o Felipe compila no PC, e custa uma volta inteira.
Esta rotina pega o erro mecânico antes do commit. Ela não substitui a compilação: a palavra final é do PC.

## Passos

1. Rode o conferidor na raiz do repositório (Python 3, sem dependência):
   ```bash
   python3 .claude/skills/revisar-sem-compilar/conferir_cpp.py                    # o que mudou desde o HEAD
   python3 .claude/skills/revisar-sem-compilar/conferir_cpp.py --base origin/main   # tudo o que o branch mudou
   python3 .claude/skills/revisar-sem-compilar/conferir_cpp.py --todos            # todo o C++ (leva segundos)
   ```
   Sai 1 se achou ERRO. ERRO se conserta antes do commit. AVISO se lê e decide. NOTA lembra o que conferir à mão.
   Hoje o código do `main` passa sem erro.
2. Revise à mão a lista "O que o conferidor não pega", abaixo, só nos trechos que você mudou.
3. Siga as regras do Detonauto: lógica em C++ e Blueprint só como cola; `Core/` nunca inclui `Vehicle/`, `Crash/` nem
   `Traffic/` (`docs/arquitetura.md`); prefixos `ADN`, `UDN`, `FDN` (struct pura do Core) e `EDN`; `TObjectPtr` em todo
   ponteiro membro com `UPROPERTY`; log em `LogDetonauto`, sem etiqueta entre colchetes. Regra testável nova vira teste
   Automation Spec em `Source/Detonauto/Core/Tests/<Nome>.spec.cpp`, com o nome `Detonauto.<Grupo>` em português sem acento.
   Armadilhas já registradas (`docs/conceitos-no-codigo.md`, `docs/estado-atual.md`): `AddExpectedError` sem colchetes,
   `FString ==` ignora maiúsculas, nunca indexar array sem conferir o tamanho, subsistema em teste só por
   `DNTest::NewSubsystem<T>()`.
4. Escreva no commit, ou na resposta ao Felipe, a linha "Para o PC:" que o conferidor imprime no fim: ela diz se basta o
   Live Coding ou se é preciso fechar o editor e compilar.
5. Compilar e testar no PC: a skill `verificar-no-pc` roda `Tools\Unreal\build.ps1 -Uproject Detonauto.uproject` (com o editor fechado quando mudou
   cabeçalho) e `Tools\Unreal\test.ps1 -Uproject Detonauto.uproject -Filter Detonauto.<Grupo>`.
6. Se o build do PC quebrar mesmo assim, o erro que escapou vira regra nova no conferidor
   (`.claude/skills/revisar-sem-compilar/conferir_cpp.py`), com o caso real no comentário. Copie a mudança para os outros
   jogos em Unreal (Age of Earth e TDFende): o arquivo é o mesmo nos três.

## O que o conferidor não pega

- Assinatura igual no .h e no .cpp: `const`, referência, tipo de cada parâmetro, `virtual` e `override`.
- `Super::` chamado nos overrides de `BeginPlay`, `EndPlay`, `Tick`, `PostInitializeComponents` e afins.
- Membro novo com valor inicial (na declaração ou no construtor); `bool` e ponteiro sem valor são lixo.
- Texto que o jogador lê é `FText` (`LOCTEXT`/`NSLOCTEXT` ou String Table; skill `traduzir`). `FString` para o resto, `FName` para id.
- No .h, declaração adiantada (`class UFoo;`) em vez de include; o include vai no .cpp, e o .cpp começa pelo próprio .h.
- `check()` só para erro de programação. Dado ruim que o jogador pode causar pede `ensure` ou retorno com `UE_LOG`.
- Remover de `TArray` dentro de um laço: de trás para frente ou `RemoveAll`.
- Tick ligado só quando precisa. Sistema novo de dano, tráfego ou efeito passa pela skill `achar-gargalo` antes de ser aprovado.
- Formato de dado salvo ou trocado entre processos. Save (`Detonauto.json`) e tabelas de `Data/`: o carregador aceita o formato antigo ou migra, com teste.

## Armadilhas

- O conferidor procura módulos pelo `*.Build.cs` a partir da pasta onde você está: rode sempre da raiz do repositório.
- Include de cabeçalho do motor fora da tabela do conferidor não é checado contra o `Build.cs`. Na dúvida, procure o
  módulo na página da classe na documentação da Epic ("Module") e confira o `Build.cs`.
