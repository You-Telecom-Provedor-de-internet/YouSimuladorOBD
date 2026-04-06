# Hardware

Este diretorio concentra os artefatos de hardware, mecanica, handoff e EDA do `YouSimuladorOBD`.

## Baseline da RevA

A `RevA` atual deve ser entendida como:

- `carrier para ESP32 DevKit 38 pinos`
- `SN65HVD230` para CAN
- `L9637D` para K-Line
- `OLED SH1107` via header
- `KY-040` via header
- buck `12V -> 5V` de baixo risco baseado em `LM2596` ou equivalente

## Funcao das pastas

### `kicad/`

- bootstrap EDA nativo da `RevA`
- referencia para continuar a captura do esquematico real

### `pcb-handoff/`

- netlist e BOM de handoff congeladas em CSV
- baseline de transicao entre bancada e captura formal em EDA

### `schematic/`

- material textual canonico desta rodada
- contem:
  - `esquematico-conceitual.md`
  - `netlist-humana.md`
  - `bom-preliminar.csv`
  - `legacy/`

### `pcb/`

- regras e requisitos de layout para a placa

### `cad/`

- referencia mecanica e arquivos de case/painel

### `bom/`

- registros auxiliares de status de BOM

### `schematics/`

- diretorio historico ou residual
- nao e a fonte canonica desta rodada

## Fluxo recomendado para engenharia

1. comecar por `../docs/hardware.md`
2. ler `../docs/pinout.md`
3. usar `schematic/` como base textual atual
4. comparar com `pcb-handoff/`
5. continuar a captura em `kicad/revA/`

## Material legado

Arquivos antigos que nao devem ser usados como fonte principal foram movidos para:

- `schematic/legacy/`

## Observacao importante

O fato de algo existir em `hardware/` nao significa automaticamente que esteja confirmado pelo firmware.

Para classificacao tecnica, usar a regra:

- `confirmado no codigo`
- `confirmado na documentacao/EDA existente`
- `inferido / hipotese`
