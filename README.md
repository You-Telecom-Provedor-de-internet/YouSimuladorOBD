# YouSimuladorOBD - Emulador OBD-II com ESP32

Simulador OBD-II baseado em ESP32 para validar scanners, adaptadores e aplicativos automotivos reais em bancada.

O projeto hoje entrega:

- 7 protocolos OBD-II selecionaveis em runtime
- barramento CAN validado com scanner real
- barramento K-Line validado com `L9637D`
- interface web protegida com WebSocket e API REST
- OTA online via `manifest.json`
- odometro persistente e camada diagnostica com `Mode 02`, `03`, `04`, `06` e `09`

Importante:

- o simulador nao usa Bluetooth proprio
- a validacao foi feita com adaptadores externos como `OBDLink MX+` e `ELM327`

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

## Modos e servicos OBD

| Servico | Descricao | Estado |
|---|---|---|
| `Mode 01` | PIDs correntes, readiness, fuel status, runtime, fuel type, etc. | Validado |
| `Mode 02` | Freeze Frame classico | Validado |
| `Mode 03` | Leitura de DTCs | Validado |
| `Mode 04` | Limpeza de DTCs | Validado |
| `Mode 06` | Monitor tests sinteticos em formato OBD classico | Validado |
| `Mode 09` | VIN e identificacao | Validado |

## Hardware validado

- `ESP32 DevKit` 38 pinos
- `SN65HVD230` para CAN
- `L9637D` para K-Line
- `LM2596` para 12V -> 5V
- `OLED SH1107 128x128`
- `KY-040`
- 6 botoes + 3 bits de DIP + 3 LEDs de status

## RevA PCB congelada

A revisao `RevA` da placa foi congelada como:

- `carrier para ESP32 DevKit 38 pinos`
- `OLED SH1107` em modulo por header
- `KY-040` em modulo por header
- `LM2596` mantido como modulo ou footprint equivalente de baixo risco

Ou seja, nesta revisao a PCB nao vai integrar `ESP32-WROOM` soldado diretamente. O objetivo e preservar o modulo validado em bancada, reduzir risco da primeira placa e acelerar a ida para layout.

Topologia K-Line validada em bancada:

- `GPIO17` -> `L9637D pin 4 (TX)`
- `GPIO16` <- `L9637D pin 1 (RX)`
- `L9637D pin 6 (K)` -> OBD `pin 7`
- `L9637D pin 7 (VS)` -> OBD `pin 16`
- `L9637D pin 8 (LI)` -> `10k` -> `VS`
- `510R` entre `K` e `VS`
- `100nF` em `VCC -> GND`
- `100nF` em `VS -> GND`

## Web e OTA

- STA com mDNS configuravel, por exemplo `http://youobd.local`
- AP fallback `OBD-Simulator` em `http://192.168.4.1`
- autenticacao web
- WebSocket para atualizacao em tempo real
- API REST para controle, status, cenarios e diagnostico
- OTA online apenas por `manifest.json`
- configuracao persistente em NVS

Credenciais padrao:

```text
Usuario: admin
Senha: obd12345
```

## Estado validado em bancada

- web UI, API e OTA online funcionando
- CAN validado com `Torque Pro`, `OBDLink` e leitura real de PIDs
- `ISO 9141-2` validado com `OBDLink`, `Torque Pro` e `YouAutoCar`
- `KWP Fast` validado com `Torque Pro`
- `Mode 02` e `Mode 06` validados de ponta a ponta no `YouAutoCar`
- `Mode 03`, `Mode 04` e `Mode 09` validados com scanner real
- `VIN` valido de 17 caracteres
- odometro total persistente exposto na web e na API
- protocolo selecionado persistido em NVS
- splash do OLED restaurado com `BOOT_SPLASH_MS = 6000`

## Documentacao

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
- [Hardware - Pacote para PCB](hardware/pcb-handoff/README.md)
- [Hardware - KiCad RevA](hardware/kicad/revA/README.md)

## Build e gravacao

```bash
cd firmware
pio run
pio run -t upload
pio run -t uploadfs
```

## Estrutura

```text
YouSimuladorOBD/
|- README.md
|- docs/
|- firmware/
|  |- include/
|  |- src/
|  |- data/
|  `- platformio.ini
`- hardware/
```

## Observacoes importantes

- o pacote `hardware/pcb-handoff/`, o documento `docs/15-diagrama-eletrico-pcb.md` e a lista curta em `docs/16-pacote-engenheiro-pcb.md` sao a referencia atual para o engenheiro de PCB
- os arquivos antigos em `hardware/schematic/` ficaram marcados como obsoletos
- o projeto agora ja possui bootstrap nativo em `KiCad` em `hardware/kicad/revA/`
- o laboratorio de validacao com Codex, simulador, app Android e celular real esta documentado em `docs/18-codex-plugin-you-obd-lab.md`
