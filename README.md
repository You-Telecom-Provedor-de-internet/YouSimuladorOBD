# YouSimuladorOBD - Emulador OBD-II com ESP32

Simulador OBD-II baseado em `ESP32` para validar scanners, adaptadores e aplicativos automotivos reais em bancada e evoluir para uma placa dedicada orientada a produto.

O projeto atual entrega:

- `7` protocolos OBD-II selecionaveis em runtime
- barramento `CAN` validado com scanner real
- barramento `K-Line` validado com `L9637D`
- `Web UI`, `API REST`, `WebSocket`, `mDNS` e `OTA`
- odometro persistente e camada diagnostica com `Mode 02`, `03`, `04`, `06` e `09`

Importante:

- o simulador nao usa Bluetooth proprio nesta revisao
- a validacao de bancada foi feita com adaptadores externos, como `OBDLink MX+` e `ELM327`

## Protocolos suportados

| Enum | Protocolo | Fisica | Estado |
|---|---|---|---|
| `0` | `CAN 11b 500k` | CAN | Validado |
| `1` | `CAN 11b 250k` | CAN | Validado |
| `2` | `CAN 29b 500k` | CAN | Validado |
| `3` | `CAN 29b 250k` | CAN | Validado |
| `4` | `ISO 9141-2` | K-Line | Validado |
| `5` | `KWP 5-baud` | K-Line | Validado |
| `6` | `KWP Fast` | K-Line | Validado |

## Hardware validado

- `ESP32 DevKit` 38 pinos
- `SN65HVD230` para CAN
- `L9637D` para K-Line
- `LM2596` para `12V -> 5V`
- `OLED SH1107 128x128`
- `KY-040`
- `6` botoes + `3` bits de DIP + `3` LEDs de status

## RevA PCB congelada

A `RevA` da placa foi congelada como:

- `carrier para ESP32 DevKit 38 pinos`
- `OLED SH1107` em modulo por header
- `KY-040` em modulo por header
- `LM2596` mantido como modulo ou footprint equivalente de baixo risco

Ou seja, nesta revisao a PCB nao integra `ESP32-WROOM` soldado diretamente. O objetivo e preservar o modulo validado em bancada, reduzir risco da primeira placa e acelerar a ida para layout.

## Fase atual do projeto

Estado atual consolidado:

- prototipo fisico montado e testado com `ESP32 DevKit`, `L9637D`, `SN65HVD230`, `OLED` e interface local
- `7` protocolos suportados com aceite operacional, incluindo `KWP 5-baud`
- arquitetura `RevA` mantida como `carrier para ESP32 DevKit 38 pinos`

Objetivo desta proxima fase:

- capturar e fechar a `PCB RevA`
- reduzir improvisos de bancada e transformar a arquitetura atual em hardware repetivel
- preparar a plataforma para virar produto com foco em robustez eletrica, montagem, teste e manutencao

## Documentacao canonica

Use estes arquivos como ponto de entrada principal:

- [Arquitetura canonica](docs/arquitetura.md)
- [Hardware consolidado](docs/hardware.md)
- [Pinout canonico](docs/pinout.md)
- [Manual de operacao](docs/manual-operacao.md)
- [Montagem K-Line com L9637D](docs/kline-l9637d-montagem.md)
- [Diagrama eletrico e handoff PCB](docs/diagrama-eletrico-pcb.md)
- [RevA carrier para ESP32 DevKit](docs/reva-carrier-esp32-devkit.md)
- [Mapa final de GPIO da RevA](docs/mapa-gpio-reva.md)
- [Guia executivo para PCB](docs/engenharia-pcb.md)
- [Firmware - README local](firmware/README.md)
- [Hardware - README local](hardware/README.md)
- [Esquematico conceitual](hardware/schematic/esquematico-conceitual.md)
- [Netlist humana](hardware/schematic/netlist-humana.md)
- [BOM preliminar](hardware/schematic/bom-preliminar.csv)
- [Requisitos de layout PCB](hardware/pcb/requisitos-layout.md)
- [Hardware - Pacote para PCB](hardware/pcb-handoff/README.md)
- [Hardware - KiCad RevA](hardware/kicad/revA/README.md)

## Build e gravacao

```bash
cd firmware
pio run
pio run -t upload
pio run -t uploadfs
```

Para detalhes do firmware e das dependencias:

- [firmware/README.md](firmware/README.md)

## Como evoluir a RevA para esquematico/PCB real

1. Ler [docs/hardware.md](docs/hardware.md) e [docs/pinout.md](docs/pinout.md).
2. Capturar ou revisar a conectividade com base em:
   - [hardware/schematic/esquematico-conceitual.md](hardware/schematic/esquematico-conceitual.md)
   - [hardware/schematic/netlist-humana.md](hardware/schematic/netlist-humana.md)
   - [hardware/schematic/bom-preliminar.csv](hardware/schematic/bom-preliminar.csv)
3. Aplicar as regras de layout em [hardware/pcb/requisitos-layout.md](hardware/pcb/requisitos-layout.md).
4. Comparar com o handoff congelado em [hardware/pcb-handoff/](hardware/pcb-handoff/).
5. Continuar a captura nativa a partir de [hardware/kicad/revA/](hardware/kicad/revA/).
6. Fechar a `RevA` com criterio de produto:
   - protecao automotiva definitiva
   - conectividade mecanica repetivel
   - pontos de teste
   - montagem e manutencao previsiveis

## Estrutura de pastas

```text
YouSimuladorOBD/
|- README.md
|- assets/
|  `- validation/
|- docs/
|  |- arquitetura.md
|  |- hardware.md
|  |- pinout.md
|  |- manual-operacao.md
|  |- kline-l9637d-montagem.md
|  |- diagrama-eletrico-pcb.md
|  |- reva-carrier-esp32-devkit.md
|  |- mapa-gpio-reva.md
|  |- engenharia-pcb.md
|  `- ota-manifest.example.json
|- firmware/
|  |- README.md
|  |- include/
|  |- src/
|  |- data/
|  `- platformio.ini
|- hardware/
|  |- README.md
|  |- bom/
|  |- cad/
|  |- kicad/revA/
|  |- pcb/
|  |- pcb-handoff/
|  `- schematic/
|- tools/
`- wokwi/
```

## Observacoes importantes

- os documentos canonicos desta rodada sao `docs/hardware.md`, `docs/pinout.md`, `docs/engenharia-pcb.md` e os arquivos em `hardware/schematic/`
- `hardware/pcb-handoff/` e `hardware/kicad/revA/` continuam sendo baseline tecnico importante para o engenheiro de PCB
- os artefatos de validacao de bancada que estavam soltos na raiz foram movidos para `assets/validation/`
- a integracao com o laboratorio Codex permanece no repositorio do plugin `YOU OBD Lab`, separado deste repositorio
- a operacao diaria do simulador, incluindo OTA e credencial fixa da API, esta em `docs/manual-operacao.md`
