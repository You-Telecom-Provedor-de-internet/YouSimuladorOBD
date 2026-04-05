# BOM - Status Consolidado dos Componentes

Atualizado: `2026-04-04`

Este arquivo deixou de ser apenas uma lista de compras e passou a registrar o conjunto realmente usado na bancada validada.

## Conjunto validado

| RefDes | Componente | Estado |
|---|---|---|
| `MCU1` | `ESP32 DevKit` 38 pinos | Validado |
| `U1` | `LM2596` buck 12V -> 5V | Validado |
| `U2` | `SN65HVD230` | Validado |
| `U3` | `L9637D` | Validado |
| `J1` | conector OBD-II femea 16 pinos | Validado |
| `DISP1` | `OLED SH1107 128x128` | Validado |
| `ENC1` | `KY-040` | Validado |
| `SW1` | DIP 3 bits | Validado |
| `SW2-SW7` | 6 botoes | Validado |
| `D1-D3` | LEDs de status | Validado |

## Passivos criticos

| RefDes | Valor | Estado | Observacao |
|---|---|---|---|
| `RK1` | `510R` | Obrigatorio | pull-up de `K -> VS`, usar `>= 0.5W` |
| `RLI1` | `10k` | Obrigatorio | `LI -> VS` no `L9637D` |
| `CK1` | `100nF` | Obrigatorio | `VCC -> GND` no `L9637D` |
| `CK2` | `100nF` | Obrigatorio | `VS -> GND` no `L9637D` |
| `RCAN1` | `120R` | Recomendado | terminacao CAN por jumper ou `DNP` |
| `RDIP1-RDIP3` | `10k` | Obrigatorio | pull-up externo do DIP |
| `RLED1-RLED3` | `330R` | Obrigatorio | LEDs |
| `F1` | `1A` | Obrigatorio | entrada +12V |

## Componentes que ficaram obsoletos

Os itens abaixo nao sao mais a referencia principal do projeto:

- circuito discreto de K-Line com transistor NPN
- qualquer esquema antigo que mostre K-Line direto por transistor
- arquivos antigos em `hardware/schematic/` sem o `L9637D`

## Arquivos corretos para o engenheiro

Use para a PCB:

- [docs/15-diagrama-eletrico-pcb.md](../../docs/15-diagrama-eletrico-pcb.md)
- [hardware/pcb-handoff/README.md](../pcb-handoff/README.md)
