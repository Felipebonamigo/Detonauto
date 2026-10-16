# Detonauto

Jogo de corrida arcade com destruição, inspirado nos Burnout clássicos. O foco é
o modo Crash: cruzamentos curtos cheios de tráfego onde o jogador causa o maior
estrago possível. Desenvolvido por uma pessoa, Felipe, com o Claude, usando
Unreal Engine 5.8 e só ferramentas e assets gratuitos.

O design completo está em `docs/design.md`.

## Idioma

Responda sempre em português do Brasil, inclusive em mensagens de commit,
comentários de revisão e documentos. Nomes de código (classes, funções,
variáveis) ficam em inglês, como é o padrão do Unreal.

## Plano e progresso

- Cronograma geral: https://claude.ai/artifact/UxU6zFtkNGTvHEwYH3ZB98
- Sprints com progresso salvo: https://claude.ai/artifact/NkqnCgL7CW5YDtbE5hbSA7
- Estúdio, com os jogos do Felipe, ferramentas, pré-configurações e pendências
  em comum: https://claude.ai/artifact/72KXW2Tve5gB65BHD1CDp6 (documento do
  Claude Docs, lido pelo conector)

No início de uma sessão, descubra em que sprint o Felipe está. Quando houver
acesso às páginas acima, leia o progresso de lá. A cada revisão de escopo
trimestral, quebre o próximo trimestre em sprints de duas semanas.

## Rotinas do Claude

- Rotinas prontas em `.claude/skills/`: `verificar-no-pc`, `jogar-e-capturar`,
  `fechar-lote`, `modelo-3d`, `triar-playtest` e `ligar-unreal-mcp`.
- Mais rotinas (14/10/2026): `revisar-sem-compilar` (confere o C++ antes do commit,
  porque a nuvem não compila), `achar-gargalo`, `ler-crash`, `publicar-na-steam`,
  `declaracao-ia`, `conferir-fato` (cidades e pontos turísticos), `som-livre` e `traduzir`.
- Também servem as skills do Claude que já vêm instaladas: `skill-creator`
  (testa se uma rotina dispara com o jeito de o Felipe pedir),
  `statistical-analysis` (quantas partidas ou rodadas uma comparação precisa
  para valer), `computer-use` (no PC, pelo app do Claude, abre o executável,
  navega pelos menus e tira print; o Felipe liga em Settings > This computer >
  Computer use) e `fewer-permission-prompts` (no PC, libera de vez os comandos
  só de leitura que mais pedem permissão).
- Sessão do PC com o editor aberto: use a skill `unreal-mcp` para montar cena e
  materiais (ligue uma vez com `ligar-unreal-mcp`, porta 8010).
- Problemas de playtest chegam como Issues "[Playtest]" pelo modelo em
  `.github/ISSUE_TEMPLATE/playtest.yml`; o commit que corrige diz "fecha #N".
- Recursos do Unreal propostos e ainda não aprovados: `docs/proposta-recursos-unreal.md`.

## Ambiente

- Unreal Engine 5.8, projeto em C++, Visual Studio Community, Windows.
- O build do Unreal só roda no PC do Felipe. Sessões na nuvem editam código,
  scripts e documentos, mas não compilam nem abrem o editor.
- Git com LFS no GitHub. O plano gratuito tem 10 GiB de armazenamento e 10 GiB
  de download por mês. Se o projeto chegar perto disso, os arquivos grandes
  migram para o Azure DevOps.

## Princípio do estúdio: divertido e bonito

Decisão do Felipe (16/10/2026): **todo jogo nosso tem que ser divertido e bonito, sempre**. Vale para toda tarefa, não só para as de arte.

- **Divertido primeiro.** Se não prende no protótipo feio, arte e polimento não salvam. Antes de chamar um sistema de pronto, pergunte
  se jogar ele dá vontade de "mais uma".
- **Bonito é requisito, aprovado por captura** (nunca só por teste verde), e a captura é olhada antes de se afirmar qualquer coisa visual.
- **Desempenho protege os dois:** quadro travado não é divertido nem bonito.
- Ao fechar um bloco, diga em uma linha o que ele mudou na diversão e no visual. Se não mudou nada, diga isso.
- Na dúvida entre "correto" e "gostoso de jogar", o Felipe decide jogando: leve a versão jogável para ele ver, não só o relatório.

## Regras de trabalho

1. **Lógica em C++.** Blueprint serve só como cola fina: ligar assets e expor
   parâmetros. Blueprint é binário e o Claude não consegue ler nem editar com
   segurança.
2. **Dados em texto.** Ajustes de carros, cruzamentos, pontuação e dificuldade
   ficam em CSV ou JSON na pasta `Data/` e são importados como DataTable.
3. **Commits pequenos**, com mensagem em português no imperativo, por exemplo
   "Adiciona turbo por contramão".
4. **Testes automatizados** para regras de jogo que não dependem de física,
   como pontuação, turbo e medalhas, usando o Automation Spec do Unreal.
5. **Desempenho é requisito.** Meta de 60 fps num PC com placa de vídeo no
   nível do PS5. Qualquer sistema novo de dano, tráfego ou efeitos precisa ser
   medido antes de ser aprovado.
6. **Visual realista é requisito.** Todo efeito visual é aprovado por captura
   de tela na cena de aparência e no modo em que o Felipe joga (`-Play`), não só
   por teste automático. Plano em `docs/plano-visual.md`.
7. **Passagem entre sessões.** O estado do projeto fica em `docs/estado-atual.md`,
   atualizado a cada lote fechado. Uma sessão nova começa por ele.

## Licenças de assets

Nenhum asset entra no projeto sem uma linha em `docs/licencas.csv`.

- **Modelos 3D em comum aos jogos do estúdio:** antes de baixar ou gerar, procure na aba
  "Modelos 3D" do doc do estúdio e no repositório `Felipebonamigo/acervo-3d`.
  Modelo novo entra nos dois no mesmo dia, além da linha em `docs/licencas.csv`.

- **Permitido:** CC0, CC BY com crédito, conteúdo da Epic com licença só para
  Unreal (City Sample e gratuitos do Fab), pacotes GDC da Sonniss, modelos do
  Meshy gratuito com crédito, e o que o Felipe criar.
- **Proibido:** licenças NC ou ND, carros ou logos de marcas reais, silhuetas
  famosas copiadas e música gerada por IA.
- Marque `gerado_por_ia` em todo conteúdo gerado por IA que o jogador vê. A
  Steam exige essa declaração. Código escrito com o Claude não precisa.
- Os pacotes completos do City Sample são enormes. Nunca adicione o pacote
  inteiro: migre para `Content/` só os assets que o jogo usa.

## Marcas de terceiros

Os nomes de modos herdados do Burnout (Road Rage, Crashbreaker, Impact Time,
Aftertouch) são só referência de trabalho. Nunca use esses nomes, nem Burnout
ou EA, em textos, telas ou na página da loja. Os nomes finais serão próprios.

## Estrutura planejada

- `Source/Detonauto/` com os módulos Core, Vehicle, Damage, Crash, Traffic e UI,
  criados na Sprint 9.
- `Content/Detonauto/` com os assets do jogo.
- `Data/` com as tabelas de ajuste em texto.
- `Tools/Blender/` e `Tools/Unreal/` com os scripts Python.
- `docs/` com design, arquitetura e licenças.
- `Sandbox/` com os projetos descartáveis de aprendizado, fora do Git.

## Convenções de nomes, proposta inicial

Revisar na Sprint 7. Prefixos de assets: `BP_` Blueprint, `SM_` malha estática,
`SK_` malha com esqueleto, `M_` material, `MI_` instância de material, `T_`
textura, `NS_` sistema Niagara, `SW_` som, `DT_` DataTable, `WBP_` interface,
`L_` mapa. Classes C++ seguem o padrão do Unreal com o prefixo do jogo, por
exemplo `ADNVehicle` e `UDNDamageComponent`.

## Cidades e pontos turísticos

Decisão do Felipe, 14 out 2026. Detalhes e a ressalva em `docs/menu-mapa.md`.

- Use **apenas o que é público**, como monumentos, praças, vias e obras em
  logradouro público, nos termos da lei brasileira.
- **Nenhuma marca real**, em nenhum lugar: fachadas, placas, outdoors, veículos
  de tráfego, interface.
- Nenhum ponto turístico é destruído no jogo.
- Texto sobre cidades e pontos turísticos só entra depois de conferido em fonte
  confiável (`NeedsReview` em `Data/Landmarks.csv`). Texto gerado por IA entra na
  declaração de conteúdo de IA da Steam.

