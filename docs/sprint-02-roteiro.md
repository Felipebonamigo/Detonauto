# Sprint 2: primeiros passos no Unreal

De 16 a 27 de novembro de 2026. Objetivo: ter um projeto C++ rodando, entender
a estrutura de pastas e do build, e sentir como um carro do Unreal se comporta.
O projeto desta sprint é descartável e vive em `Sandbox/`, fora do Git.

Os nomes de menus abaixo são os do Unreal 5.x. A interface do 5.8 pode ter
pequenas diferenças. Se algo não bater, mande uma captura de tela.

## Antes de começar: sessão local do Claude (recomendado)

Na nuvem, o Claude só edita arquivos. Numa sessão local, no seu PC, ele também
compila, roda testes e lê os erros do Visual Studio. Para configurar:

1. Instale o Claude Code no Windows seguindo as instruções em
   claude.com/claude-code.
2. Abra o PowerShell na pasta do repositório e rode `claude`.
3. O `CLAUDE.md` do repositório é lido sozinho. Peça "estamos na Sprint 2".

Mesmo assim, continuam com você: jogar e avaliar a sensação, os cliques no
editor do Unreal e o login na Epic. Sem sessão local, o roteiro funciona do
mesmo jeito, só que você cola aqui os erros e as telas.

## Tarefa 1: trilha introdutória da Epic (você)

1. Abra dev.epicgames.com/community/learning e procure uma trilha introdutória
   de Unreal Engine 5, de 1 a 3 horas, em inglês ou português.
2. Faça a trilha inteira, abrindo o editor junto.
3. Anote 3 dúvidas para me perguntar. Elas guiam as explicações da Sprint 3.

Se não achar uma trilha boa, me avise e eu indico o que procurar.

## Tarefa 2: criar o projeto C++ (juntos)

1. No Epic Games Launcher, abra o Unreal Engine 5.8.
2. Em Games, escolha o template de veículo (Vehicle).
3. Em Project Defaults, marque **C++**, não Blueprint.
4. Desmarque Starter Content e Raytracing. Eles só ocupam espaço.
5. Em Location, escolha a pasta `Sandbox` dentro do repositório:
   `C:\Users\Felip\source\repos\Detonauto\Detonauto\Sandbox`
   (crie a pasta `Sandbox` se ela não existir).
6. Em Name, escreva `VeiculoTeste`. Clique em Create.
7. O Unreal abre o Visual Studio e compila. A primeira compilação demora de 5
   a 20 minutos. Se o Visual Studio pedir componentes que faltam, aceite.
8. Quando o editor abrir, aperte Alt+P para jogar.

A pasta `Sandbox/` está no `.gitignore`. Nada dela vai para o GitHub, então
você não gasta a cota do Git LFS.

## Tarefa 3: entender a estrutura (Claude explica)

Abra a pasta `Sandbox\VeiculoTeste`. Para cada item, o que ele é:

| Item | O que é | Vai para o Git no jogo final? |
|---|---|---|
| `VeiculoTeste.uproject` | Descreve o projeto e os módulos | Sim |
| `Source/` | Todo o código C++ | Sim |
| `Content/` | Assets: malhas, materiais, mapas | Sim, pelo LFS |
| `Config/` | Configurações do projeto | Sim |
| `Plugins/` | Plugins do projeto | Sim |
| `Binaries/` | Programas compilados | Não |
| `Intermediate/` | Arquivos temporários do build | Não |
| `Saved/` | Logs, capturas e salvamentos | Não |
| `DerivedDataCache/` | Cache de assets | Não |

Dentro de `Source/VeiculoTeste/`:

- **`VeiculoTeste.Build.cs`**: lista de que outros módulos este módulo usa. É
  aqui que entra o `ChaosVehicles`, por exemplo.
- **`VeiculoTeste.cpp` e `.h`**: o código de entrada do módulo.
- **Classes do carro**: o template traz o carro, o controle e a interface em C++.
- **`VeiculoTeste.Target.cs` e `VeiculoTesteEditor.Target.cs`**: dizem ao build o
  que compilar, o jogo ou o editor.

Um módulo é um pacote de código com suas próprias dependências. No jogo final,
o Detonauto terá os módulos Core, Vehicle, Damage, Crash, Traffic e UI.

### Como compilar

- **Com o editor aberto:** salve o código e aperte Ctrl+Alt+F11 no editor. É o
  Live Coding. Serve para mudanças pequenas em funções.
- **Com o editor fechado:** abra a solução no Visual Studio e aperte Ctrl+B.
  Serve quando você mudou cabeçalhos, criou classes ou adicionou propriedades.
- **Solução não existe ou está quebrada:** clique com o botão direito no
  `.uproject` e escolha "Generate Visual Studio project files".

## Tarefa 4: dirigir e anotar (você)

Jogue de 20 a 30 minutos e anote as respostas:

1. O carro vira rápido demais, devagar demais ou na medida?
2. Dá vontade de derrapar? A derrapada é fácil de controlar?
3. A câmera ajuda a sentir velocidade, ou parece parada?
4. Bater em um muro tem peso, ou o carro parece de plástico?
5. Qual é o fps? Abra o console com a tecla `'` e rode `stat fps`.
6. Rode `stat unit` e anote os tempos de Frame, Game, Draw e GPU.
7. O que parece mais lento ou mais pesado do que o Burnout?

Salve isso em um arquivo `Sandbox/notas-sprint-2.md`. Na Sprint 4 comparamos
essas notas com as do carro do jogo.

## Problemas comuns

- **"Could not be compiled. Try rebuilding from source manually."** Feche o
  editor, abra a solução no Visual Studio e compile com Ctrl+B. A janela de
  saída mostra o erro de verdade.
- **Visual Studio sem o componente de C++ para jogos:** abra o Visual Studio
  Installer, clique em Modificar e marque "Desenvolvimento de jogos com C++".
- **Compilação muito lenta ou travando:** o antivírus pode estar escaneando as
  pastas. Exclua a pasta `Sandbox` e a instalação do Unreal da verificação.
- **Caminho longo demais:** mantenha o projeto perto da raiz do disco se o
  Windows reclamar do tamanho do caminho.
- **O Git mostra arquivos novos de `Sandbox`:** não deveria. Rode
  `git check-ignore Sandbox/VeiculoTeste/Source` para conferir a regra.

## Pronto quando

- O projeto abre e o carro anda, depois de uma compilação sem erros.
- Você sabe dizer para que serve cada pasta da tabela.
- Você sabe compilar pelo Visual Studio e pelo editor.
- As notas da tarefa 4 estão salvas.
- O Claude anotou na planilha de sprints o que cada tarefa deixou.
