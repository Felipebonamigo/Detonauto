# Carro de teste com presets (lote 8)

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

Para copiar para o projeto: arquivos `.h/.cpp` em `Source/VeiculoTeste/`, `TestCars.csv` em `Data/`, depois
`Tools/Unreal/build.ps1` (com o editor FECHADO: ele trava a DLL) e `Tools/Unreal/test.ps1`.

Como usar, o que cada preset muda e o roteiro de teste: [`docs/presets-carro.md`](../../../docs/presets-carro.md).
Medições sem janela e captura de tela: `Tools/Unreal/car-probe.ps1`.
