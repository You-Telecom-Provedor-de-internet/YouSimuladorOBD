# YouSimuladorOBD - Emulador OBD-II com ESP32

Simulador OBD-II baseado em `ESP32` para validar scanners, adaptadores e aplicativos automotivos reais em bancada.

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
| `5` | `KWP 5-baud` | K-Line | Implementado e em validacao ampliada |
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

## Documentacao canonica

Use estes arquivos como ponto de entrada principal:

- [Arquitetura canonica](docs/arquitetura.md)
- [Hardware consolidado](docs/hardware.md)
- [Pinout canonico](docs/pinout.md)
- [Guia executivo para PCB](docs/engenharia-pcb.md)
- [Firmware - README local](firmware/README.md)
- [Hardware - README local](hardware/README.md)
- [Esquematico conceitual](hardware/schematic/esquematico-conceitual.md)
- [Netlist humana](hardware/schematic/netlist-humana.md)
- [BOM preliminar](hardware/schematic/bom-preliminar.csv)
- [Requisitos de layout PCB](hardware/pcb/requisitos-layout.md)

## Documentacao detalhada e historica

- [01 - Visao Geral](docs/01-overview.md)
- [02 - Hardware e Componentes](docs/02-hardware.md)
- [03 - Pinout e Ligacoes](docs/03-pinout.md)
- [04 - Protocolos OBD-II](docs/04-protocols.md)
- [05 - PIDs OBD-II](docs/05-obd-pids.md)
- [06 - Arquitetura de Firmware](docs/06-architecture.md)
- [07 - Controles e UI](docs/07-ui-controls.md)
- [08 - Wi-Fi, Web e OTA](docs/08-wifi-webui.md)
- [09 - Perfis de Veiculo](docs/09-vehicle-profiles.md)
- [10 - Simulacao Dinamica](docs/10-dynamic-simulation.md)
- [12 - Auditoria de Firmware e Bancada](docs/12-auditoria-firmware-e-bancada.md)
- [13 - Montagem K-Line com L9637D](docs/13-kline-l9637d-montagem.md)
- [14 - Cenarios Diagnosticos](docs/14-diagnostic-scenarios.md)
- [15 - Diagrama Eletrico e Handoff PCB](docs/15-diagrama-eletrico-pcb.md)
- [16 - Pacote para o Engenheiro de PCB](docs/16-pacote-engenheiro-pcb.md)
- [17 - RevA Carrier para ESP32 DevKit](docs/17-revA-carrier-esp32-devkit.md)
- [18 - Codex Plugin YOU OBD Lab](docs/18-codex-plugin-you-obd-lab.md)
- [19 - Manual de Operacao com Imagens](docs/19-manual-operacao.md)
- [19 - Manual de Operacao com Imagens (HTML)](docs/19-manual-operacao.html)
- [19 - Manual de Operacao com Imagens (PDF)](docs/19-manual-operacao.pdf)
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
|  |- engenharia-pcb.md
|  `- docs numerados de apoio
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
- `hardware/schematic/legacy/` guarda o material antigo que nao deve ser usado como fonte principal
- os artefatos de validacao de bancada que estavam soltos na raiz foram movidos para `assets/validation/`
- o laboratorio de validacao com Codex, simulador, app Android e celular real esta documentado em `docs/18-codex-plugin-you-obd-lab.md`
- a operacao diaria do simulador, incluindo OTA e rotacao da senha da API, esta no manual com imagens em `docs/19-manual-operacao.md`, com versoes renderizadas em `docs/19-manual-operacao.html` e `docs/19-manual-operacao.pdf`
