# KiCad RevA

Bootstrap nativo do projeto no `KiCad`, criado a partir do handoff validado em bancada.

## Escopo desta captura inicial

Esta revisao ainda nao e o esquematico completo final. Ela entrega:

- folha raiz nativa do `KiCad`
- separacao por blocos funcionais
- pontos de entrada claros para o engenheiro continuar a captura
- referencia mecanica do frontal de `OLED + KY-040 + 6 botoes`

## Arquivos

- `YouSimuladorOBD_RevA.kicad_sch`
  - folha raiz
- `power_input.kicad_sch`
  - entrada OBD, fusivel e buck
- `esp32_core.kicad_sch`
  - MCU, GPIOs e mapa funcional
- `obd_transceivers.kicad_sch`
  - `SN65HVD230` e `L9637D`
- `ui_local.kicad_sch`
  - `OLED SH1107`, `KY-040`, botoes, DIP e LEDs
- `ui-panel-reference.csv`
  - coordenadas base do painel frontal da tampa de teste
- `erc-report.txt`
  - validacao inicial do bootstrap com `0` erros e `0` warnings

## Fonte tecnica

Os dados eletricos consolidados continuam sendo:

- `docs/15-diagrama-eletrico-pcb.md`
- `hardware/pcb-handoff/netlist-rev-a.csv`
- `hardware/pcb-handoff/bom-rev-a.csv`

## Observacao

Esta captura foi iniciada para que o engenheiro ja possa trabalhar em arquivo EDA nativo sem depender apenas do handoff em `Markdown + CSV`.
