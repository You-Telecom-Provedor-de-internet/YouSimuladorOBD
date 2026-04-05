# Pacote de Handoff para PCB

Este diretorio contem o material tecnico consolidado para o engenheiro capturar o esquema da placa.

## Arquivos

- `netlist-rev-a.csv`
  - nets eletricas consolidadas
- `bom-rev-a.csv`
  - lista base de componentes para a revisao A

## Documento mestre

O documento principal do handoff e:

- `docs/15-diagrama-eletrico-pcb.md`
- `docs/16-pacote-engenheiro-pcb.md`

## Captura nativa iniciada

O projeto tambem ja possui um bootstrap nativo no `KiCad`:

- `hardware/kicad/revA/YouSimuladorOBD_RevA.kicad_sch`

Esse conjunto nao substitui o handoff consolidado, mas acelera a continuacao da captura pelo engenheiro.

## Escopo

Este pacote descreve a topologia eletrica validada em bancada:

- `ESP32`
- `LM2596`
- `SN65HVD230`
- `L9637D`
- `OLED`
- botoes, encoder, DIP e LEDs

## Observacao importante

Os arquivos antigos em `hardware/schematic/` nao sao mais a referencia de engenharia. Eles ficam apenas como historico e foram supersedidos por este pacote.
