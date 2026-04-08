# Pacote de Handoff para PCB

Este diretorio contem o material tecnico consolidado para o engenheiro capturar o esquema da placa.

## Arquivos

- `netlist-rev-a.csv`
  - nets eletricas consolidadas
- `bom-rev-a.csv`
  - lista base de componentes para a revisao A

## Documento mestre

O documento principal do handoff e:

- `docs/diagrama-eletrico-pcb.md`
- `docs/engenharia-pcb.md`

## Captura nativa iniciada

O projeto tambem ja possui um bootstrap nativo no `KiCad`:

- `hardware/kicad/revA/YouSimuladorOBD_RevA.kicad_sch`

Esse conjunto nao substitui o handoff consolidado, mas acelera a continuacao da captura pelo engenheiro.

## Escopo

Este pacote descreve a topologia eletrica validada em bancada:

- `ESP32 DevKit 38 pinos` em carrier RevA
- `LM2596`
- `SN65HVD230`
- `L9637D`
- `OLED`
- botoes, encoder, DIP e LEDs

## Decisao congelada da RevA

A revisao `RevA` fica congelada como:

- `carrier para ESP32 DevKit 38 pinos`
- `OLED` via header
- `KY-040` via header
- `LM2596` por modulo ou footprint equivalente

O pacote desta pasta e o bootstrap em `KiCad` devem ser interpretados nessa arquitetura.

## Observacao importante

Este pacote complementa `hardware/schematic/` e o bootstrap em `hardware/kicad/revA/`; os tres devem permanecer coerentes com os documentos canonicos em `docs/`.
