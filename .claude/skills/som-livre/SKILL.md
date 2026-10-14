---
name: som-livre
description: 'Procura, confere a licença, trata e registra efeito sonoro e música do Detonauto, na mesma linha da skill modelo-3d. Use antes de baixar, gravar ou gerar qualquer som, quando o Felipe pedir "põe um som de...", "troca a música" ou "falta áudio", e sempre que um som novo entrar no jogo.'
---
# Som livre: procurar, conferir, registrar

Regra do Detonauto: Sonniss GDC e Freesound CC0 ou CC BY para motores e impactos; música só de bandas independentes com acordo por
escrito; **música gerada por IA é proibida** (CLAUDE.md, `docs/design.md`). Asset de som com prefixo `SW_`.

## Onde procurar, nesta ordem

1. O que o jogo já tem: ainda nada; o projeto não tem som nem o plugin MetaSounds (é proposta, `docs/proposta-recursos-unreal.md`).
2. **Sonniss GDC Game Audio Bundle**: milhares de efeitos gravados por estúdios, livres para jogo comercial, sem crédito
   obrigatório. A licença proíbe redistribuir o som solto: o repositório é público, então o WAV original fica no PC (com cópia no D:), fora do git. Antes de versionar o asset
   importado, confirme com o Felipe se a licença permite num repositório público; na dúvida, fica fora.
3. **Kenney** (pacotes de áudio CC0) e **Freesound** filtrado por licença **CC0** (ou CC BY, com crédito). O download
   do Freesound em qualidade original exige login: quem baixa é o Felipe.
4. **OpenGameArt**: só itens CC0 ou CC BY; confira a licença item por item, não a do pacote.
5. Gerar ou encomendar: efeito sonoro por IA só com licença comercial clara e o Felipe sabendo; música por IA, não.

Recusa: NC, ND, "uso pessoal", som de outro jogo ou filme, música com letra ou melodia conhecida, a BBC Sound Effects
(licença não comercial), e som de marca (vinheta, buzina registrada).

## Tratar antes de entrar

- Corte o silêncio do começo e do fim e confira se não estala no corte.
- Volume: normalize por categoria com o ffmpeg. Sem alvo escrito no jogo, use -23 LUFS integrado e pico de -1 dBTP:
  ```bash
  ffmpeg -i entrada.wav -af loudnorm=I=-23:TP=-1:LRA=11 -ar 48000 saida.wav
  ```
- Formato: WAV PCM 16 ou 24 bits, 48 kHz, importado como `SW_<nome>`; o `.wav` vai pelo LFS (`.gitattributes`).

## Registrar (no mesmo commit)

Uma linha em `docs/licencas.csv`, com as mesmas colunas dos modelos (origem, url, licença, crédito,
`gerado_por_ia`, usado_em, data).
Som gerado por IA entra marcado e vai para a declaração (skill `declaracao-ia`).
