# Carro de teste com presets (lote 8)

> **Referência histórica.** Desde o lote 12 o carro vive no projeto principal (`Source/Detonauto/Vehicle/DNVehicle.*`, presets em
> `Data/Cars.csv`, veja `docs/carro.md`). Esta cópia continua aqui só para consulta e para o Sandbox; não é mais atualizada junto.

Cópia de referência dos arquivos do carro de teste que vivem no projeto descartável
`Sandbox/VeiculoTeste` (fora do Git). Estão aqui para ficarem versionados e para quem não enxerga o Sandbox poder ler.
**Se você mudar uma cópia, mude a outra.** A cópia que roda é a do Sandbox.

| Arquivo aqui | Onde vive no projeto de teste | O que é |
|---|---|---|
| `TestCars.csv` | `Data/TestCars.csv` | Uma linha = um preset (`Padrao`, `arcade-1`, `arcade-2`, `arcade-3`, mais `Arcade` e `Pesado` antigos) |
| `DNCarTuning.h` | `Source/VeiculoTeste/` | Linha do CSV, limites, rampa do freio de mão e nitro de teste (lógica pura, sem mundo) |
| `DNCarTuning.spec.cpp` | `Source/VeiculoTeste/` | Testes de Automation da parte pura e dos presets do CSV |
| `DNTestCar.h`, `DNTestCar.cpp` | `Source/VeiculoTeste/` | O carro: Chaos Vehicles em C++, presets, nitro, HUD e os modos de medição |
| `DNTestCarWheels.h` | `Source/VeiculoTeste/` | As duas classes de roda (dianteira e traseira) |
| `DNDentParams.h`, `DNDentParams.spec.cpp` | `Source/VeiculoTeste/` | Protótipo de amassado (lote 10): parâmetros do material, lado da batida e variação de velocidade; lógica pura, com testes |

Para copiar para o projeto: arquivos `.h/.cpp` em `Source/VeiculoTeste/`, `TestCars.csv` em `Data/`, depois
`Tools/Unreal/build.ps1` (com o editor FECHADO: ele trava a DLL) e `Tools/Unreal/test.ps1`.

Como usar, o que cada preset muda e o roteiro de teste: [`docs/presets-carro.md`](../../../docs/presets-carro.md).
Medições sem janela e captura de tela: `Tools/Unreal/car-probe.ps1`.

## Amassado visual (lote 10)

Além dos arquivos acima, o protótipo usa, **no projeto de teste**:

- `Source/VeiculoTeste/Core/DNDamage.h` e `.cpp`: cópia **idêntica** de `Source/Detonauto/Core/DNDamage.*` (não edite aqui; confira com
  `cmp`). O `Build.cs` do projeto de teste ganhou `PublicIncludePaths.Add(ModuleDirectory)` para o include `"Core/DNDamage.h"` valer
  como no projeto principal.
- `Source/VeiculoTeste/DNMeasureGameMode.*`: cópia em `Tools/Unreal/measure-gamemode/` (agora com `-DNStill`).
- O material `Content/Sandbox_Teste/M_DNDentParent`, gerado por `Tools/Unreal/create_dent_material.ps1` (binário, fora do Git).

O que é e o que foi medido: [`docs/prototipo-amassado.md`](../../../docs/prototipo-amassado.md).
