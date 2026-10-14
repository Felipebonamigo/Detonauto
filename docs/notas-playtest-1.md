# Notas do primeiro playtest no projeto principal (14 out 2026)

Carro do projeto principal (`ADNVehicle`), mapa de teste vazio, sem tráfego.
Respostas do Felipe, sem edição de sentido.

## Ao abrir

- As rodas aparecem **de lado** (eixo errado).

## Por preset

| Preset | Peso | Derrapagem | Curva |
|---|---|---|---|
| 0 Padrão | meio duro | não derrapa nada | dura, não gostou |
| 1 arcade-1 | meio duro | não derrapa nada | dura, não gostou |
| 2 arcade-2 | leve demais | derrapa **sozinho, sem o Espaço**, demais | ok |
| 3 arcade-3 | ok | ok, mas com o Espaço derrapa demais e perde o controle | ok |

## Outros

- **Câmera:** a do preset 0 é mais próxima; as dos presets 1, 2 e 3 (mais longe)
  agradam mais. No jogo final tem que dar para trocar: **alta e longe**,
  **alta e bem longe**, **capô**, **frente (para-choque)** e **motorista**
  (vendo volante e painel).
- **Turbo:** quase não dá diferença; precisa ser bem mais forte.
- **Rodas:** de lado, formato errado e **"embaçadas" ao girar**; ele não quer o efeito.
- **Amassado:** **nada acontece** com `T Y U G H J` nem com as batidas; o carro
  segue intacto. (O HUD mostrava "Amassados: 0 de 8".)

## Leitura

| Achado | Hipótese | Ação |
|---|---|---|
| Rodas de lado e borradas | O `Tick` que copia giro e esterço do Chaos usa o eixo errado; o borrão pode ser material ou desfoque de movimento da roda | Corrigir eixos, remover o borrão |
| Amassado não aparece | Passou nos testes automáticos, mas não no jogo: teclas, material ou aplicação na malha no modo `-Play` | Reproduzir jogando, não por cenário |
| Padrão/arcade-1 duros, curva dura | Curva boa no template; algo na entrada de direção ou no atrito 1,0 do piso | Comparar com o template (suavização do esterço, curva por velocidade) |
| arcade-2 derrapa sozinho | Aderência traseira baixa demais fora do freio de mão | Subir a aderência |
| arcade-3 perde o controle com Espaço | "Precipício" da aderência traseira | Assistência de derrapagem (contraesterço automático) |
| Turbo fraco | Aceleração do turbo baixa | Aumentar e medir |
| Câmeras | Só uma câmera de perseguição | Cinco modos por dados; motorista depende de interior do carro |
