# Pinout Canonico

Documento canonico de pinagem e sinais do `YouSimuladorOBD`.

Regra de interpretacao:

- `confirmado no codigo`: a associacao esta implementada diretamente no firmware atual
- `confirmado na documentacao/EDA existente`: a associacao vem do handoff, docs tecnicas ou bootstrap KiCad
- `inferido / hipotese`: a associacao e plausivel, mas ainda depende de confirmacao fisica ou de escolha final de componente

Fontes principais:

- `firmware/include/config.h`
- `firmware/src/main.cpp`
- `firmware/src/protocols/can_protocol.cpp`
- `firmware/src/protocols/kline_protocol.cpp`
- `firmware/src/web/ui_init.cpp`
- `docs/hardware.md`
- `docs/kline-l9637d-montagem.md`
- `docs/diagrama-eletrico-pcb.md`
- `docs/mapa-gpio-reva.md`
- `hardware/pcb-handoff/netlist-rev-a.csv`

## Sinais do ESP32 usados pelo projeto

| Sinal | GPIO / pino fisico | Funcao | Origem da confirmacao | Observacoes eletricas | Classificacao |
|---|---|---|---|---|---|
| `CAN_TX_LOGIC` | `GPIO4` | `TWAI TX` para `U2 SN65HVD230 TXD` | `config.h` + `can_protocol.cpp` + `hardware/pcb-handoff/netlist-rev-a.csv` | logica em rail `+3V3_AUX`; ativo nos protocolos CAN | confirmado no codigo |
| `CAN_RX_LOGIC` | `GPIO5` | `TWAI RX` vindo de `U2 SN65HVD230 RXD` | `config.h` + `can_protocol.cpp` + `hardware/pcb-handoff/netlist-rev-a.csv` | logica em rail `+3V3_AUX`; ativo nos protocolos CAN | confirmado no codigo |
| `K_TX_LOGIC` | `GPIO17` | `UART1 TX` para `U3 L9637D pin 4` | `config.h` + `kline_protocol.cpp` + `docs/kline-l9637d-montagem.md` | precisa manter idle alto coerente com a interface K-Line | confirmado no codigo |
| `K_RX_LOGIC` | `GPIO16` | `UART1 RX` vindo de `U3 L9637D pin 1` | `config.h` + `kline_protocol.cpp` + `docs/kline-l9637d-montagem.md` | entrada de retorno da K-Line | confirmado no codigo |
| `I2C_SDA` | `GPIO21` | `OLED SH1107 SDA` | `config.h` + `ui_init.cpp` + `hardware/pcb-handoff/netlist-rev-a.csv` | I2C em `400 kHz`; modulo em `+3V3_AUX` | confirmado no codigo |
| `I2C_SCL` | `GPIO22` | `OLED SH1107 SCL` | `config.h` + `ui_init.cpp` + `hardware/pcb-handoff/netlist-rev-a.csv` | I2C em `400 kHz`; modulo em `+3V3_AUX` | confirmado no codigo |
| `BTN_PREV` | `GPIO32` | botao `PREV` | `config.h` + `ui_init.cpp` | `INPUT_PULLUP`; chave para `GND` | confirmado no codigo |
| `BTN_NEXT` | `GPIO33` | botao `NEXT` | `config.h` + `ui_init.cpp` | `INPUT_PULLUP`; chave para `GND` | confirmado no codigo |
| `BTN_UP` | `GPIO25` | botao `UP` | `config.h` + `ui_init.cpp` | `INPUT_PULLUP`; chave para `GND` | confirmado no codigo |
| `BTN_DOWN` | `GPIO26` | botao `DOWN` | `config.h` + `ui_init.cpp` | `INPUT_PULLUP`; chave para `GND` | confirmado no codigo |
| `BTN_SELECT` | `GPIO27` | botao `OK/SELECT` | `config.h` + `ui_init.cpp` | `INPUT_PULLUP`; chave para `GND` | confirmado no codigo |
| `BTN_PROTOCOL` | `GPIO18` | botao de troca rapida de protocolo | `config.h` + `ui_init.cpp` | `INPUT_PULLUP`; chave para `GND` | confirmado no codigo |
| `ENC_A` | `GPIO14` | `KY-040 CLK` | `config.h` + `ui_init.cpp` + `docs/mapa-gpio-reva.md` | logica em `+3V3_AUX`; fora dos pinos mais sensiveis de boot | confirmado no codigo |
| `ENC_B` | `GPIO13` | `KY-040 DT` | `config.h` + `ui_init.cpp` + `wokwi/diagram.json` | logica em `+3V3_AUX` | confirmado no codigo |
| `ENC_SW` | `GPIO19` | botao do encoder | `config.h` + `ui_init.cpp` + `docs/mapa-gpio-reva.md` | `INPUT_PULLUP`; chave para `GND` | confirmado no codigo |
| `DIP_BIT0` | `GPIO34` | bit 0 do protocolo de boot | `config.h` + `main.cpp` + `docs/mapa-gpio-reva.md` | somente entrada; precisa `pull-up` externo `10k` | confirmado no codigo |
| `DIP_BIT1` | `GPIO35` | bit 1 do protocolo de boot | `config.h` + `main.cpp` + `docs/mapa-gpio-reva.md` | somente entrada; precisa `pull-up` externo `10k` | confirmado no codigo |
| `DIP_BIT2` | `GPIO36` | bit 2 do protocolo de boot | `config.h` + `main.cpp` + `docs/mapa-gpio-reva.md` | somente entrada; precisa `pull-up` externo `10k` | confirmado no codigo |
| `LED_TX` | `GPIO23` | LED externo opcional de trafego TX | `config.h` + `main.cpp` + `docs/mapa-gpio-reva.md` | usar resistor serie `330R` se montado | confirmado no codigo |
| `LED_BUILTIN` | `GPIO2` | heartbeat no LED onboard do DevKit | `config.h` + `main.cpp` | LED do modulo comercial; nao aparece como item dedicado no handoff RevA | confirmado no codigo |

## Conector OBD-II

| Sinal | GPIO / pino fisico | Funcao | Origem da confirmacao | Observacoes eletricas | Classificacao |
|---|---|---|---|---|---|
| `GND_CHASSIS` | `J1 pin 4` | retorno de terra | `docs/hardware.md` + `docs/diagrama-eletrico-pcb.md` + `netlist-rev-a.csv` | ligado ao `GND` comum da placa | confirmado na documentacao/EDA existente |
| `GND_SIGNAL` | `J1 pin 5` | retorno de terra | `docs/hardware.md` + `docs/diagrama-eletrico-pcb.md` + `netlist-rev-a.csv` | ligado ao `GND` comum da placa | confirmado na documentacao/EDA existente |
| `CANH` | `J1 pin 6` | barramento CAN high | `docs/hardware.md` + `docs/diagrama-eletrico-pcb.md` + `netlist-rev-a.csv` | vai ao `U2 CANH`; roteamento pareado com `CANL` | confirmado na documentacao/EDA existente |
| `KLINE` | `J1 pin 7` | barramento K-Line | `docs/hardware.md` + `docs/kline-l9637d-montagem.md` + `netlist-rev-a.csv` | vai ao `U3 pin 6`; idle alto em `+12V` | confirmado na documentacao/EDA existente |
| `CANL` | `J1 pin 14` | barramento CAN low | `docs/hardware.md` + `docs/diagrama-eletrico-pcb.md` + `netlist-rev-a.csv` | vai ao `U2 CANL`; roteamento pareado com `CANH` | confirmado na documentacao/EDA existente |
| `L_LINE` | `J1 pin 15` | `L-Line` | `docs/hardware.md` + `docs/kline-l9637d-montagem.md` | documentada como `NC` nesta revisao | confirmado na documentacao/EDA existente |
| `+12V_OBD` | `J1 pin 16` | alimentacao de entrada | `docs/hardware.md` + `docs/diagrama-eletrico-pcb.md` + `netlist-rev-a.csv` | fonte primaria do sistema | confirmado na documentacao/EDA existente |

## Alimentacao e rails

| Sinal | GPIO / pino fisico | Funcao | Origem da confirmacao | Observacoes eletricas | Classificacao |
|---|---|---|---|---|---|
| `+12V_OBD` | `J1 pin 16` | entrada principal automotiva | docs + handoff | deve ser protegido antes do buck e antes de distribuir para `U3 VS` | confirmado na documentacao/EDA existente |
| `+12V_PROT` | apos `F1` | rail `+12V` protegido | `docs/diagrama-eletrico-pcb.md` + `netlist-rev-a.csv` + `power_input.kicad_sch` | alimenta `U1`, `U3 VS`, `RK1` e `RLI1` | confirmado na documentacao/EDA existente |
| `+5V_SYS` | saida do buck `U1` | alimenta `VIN/5V` do DevKit | `docs/diagrama-eletrico-pcb.md` + `netlist-rev-a.csv` | depende de `LM2596` ou buck equivalente | confirmado na documentacao/EDA existente |
| `+3V3_AUX` | saida do regulador `3.3V` dedicado | logica dos perifericos | `docs/diagrama-eletrico-pcb.md` + `netlist-rev-a.csv` + `docs/mapa-gpio-reva.md` | rail de `U2`, `U3 VCC`, `OLED`, encoder e pull-ups do DIP; nao interligar ao `3V3` do DevKit | confirmado na documentacao/EDA existente |
| `GND` | `J1 pin 4/5`, `U1`, `MCU1`, `U2`, `U3`, UI | referencia comum | `docs/diagrama-eletrico-pcb.md` + `netlist-rev-a.csv` | manter plano continuo e retorno curto | confirmado na documentacao/EDA existente |

## Transceivers e sinais externos

| Sinal | GPIO / pino fisico | Funcao | Origem da confirmacao | Observacoes eletricas | Classificacao |
|---|---|---|---|---|---|
| `U2 TXD` | `SN65HVD230 pin 1` | entrada de TX do transceiver CAN | `docs/diagrama-eletrico-pcb.md` + `netlist-rev-a.csv` | ligado a `GPIO4` | confirmado na documentacao/EDA existente |
| `U2 RXD` | `SN65HVD230 pin 4` | saida de RX do transceiver CAN | `docs/diagrama-eletrico-pcb.md` + `netlist-rev-a.csv` | ligado a `GPIO5` | confirmado na documentacao/EDA existente |
| `U2 VCC` | `SN65HVD230 pin 3` | alimentacao `+3V3_AUX` | `docs/hardware.md` + `netlist-rev-a.csv` | transceiver CAN nativo no rail auxiliar | confirmado na documentacao/EDA existente |
| `U2 RS` | `SN65HVD230 pin 8` | configuracao de slope/mode | `docs/hardware.md` + `netlist-rev-a.csv` | amarrado a `GND` para modo rapido | confirmado na documentacao/EDA existente |
| `U3 RX` | `L9637D pin 1` | retorno K-Line para o ESP32 | `docs/kline-l9637d-montagem.md` + `netlist-rev-a.csv` | ligado a `GPIO16` | confirmado na documentacao/EDA existente |
| `U3 TX` | `L9637D pin 4` | comando K-Line vindo do ESP32 | `docs/kline-l9637d-montagem.md` + `netlist-rev-a.csv` | ligado a `GPIO17` | confirmado na documentacao/EDA existente |
| `U3 VCC` | `L9637D pin 3` | logica `+3V3_AUX` | `docs/kline-l9637d-montagem.md` + `netlist-rev-a.csv` | desacoplado por `CK1` | confirmado na documentacao/EDA existente |
| `U3 VS` | `L9637D pin 7` | dominio automotivo `+12V` | `docs/kline-l9637d-montagem.md` + `netlist-rev-a.csv` | desacoplado por `CK2` | confirmado na documentacao/EDA existente |
| `U3 K` | `L9637D pin 6` | barramento K-Line externo | `docs/kline-l9637d-montagem.md` + `netlist-rev-a.csv` | puxado para `VS` por `RK1 510R` | confirmado na documentacao/EDA existente |
| `U3 LI` | `L9637D pin 8` | bias de linha | `docs/kline-l9637d-montagem.md` + `netlist-rev-a.csv` | ligado a `VS` via `RLI1 10k` | confirmado na documentacao/EDA existente |
| `U3 LO` | `L9637D pin 2` | saida nao usada | `docs/kline-l9637d-montagem.md` | `NC` nesta revisao | confirmado na documentacao/EDA existente |

## Programacao e debug

| Sinal | GPIO / pino fisico | Funcao | Origem da confirmacao | Observacoes eletricas | Classificacao |
|---|---|---|---|---|---|
| `USB_DEVKIT` | conector USB do modulo | gravacao do firmware e serial monitor | `docs/reva-carrier-esp32-devkit.md` + `README.md` | precisa ficar acessivel na borda da carrier | confirmado na documentacao/EDA existente |
| `UART0 debug` | serial do DevKit | `Serial.begin(115200)` no boot | `main.cpp` | depende do modulo comercial e da interface USB serial do DevKit | confirmado no codigo |

## Restricoes relevantes do ESP32

| Item | Observacao | Origem |
|---|---|---|
| `GPIO34-39` | somente entrada, sem `pull-up` interno | comportamento do ESP32 refletido em `docs/mapa-gpio-reva.md` e no uso real do firmware |
| `GPIO12` | pino sensivel de boot; fora da UI final | registrado em `docs/mapa-gpio-reva.md` |
| `GPIO15` | pino sensivel de boot; fora da UI final | registrado em `docs/mapa-gpio-reva.md` |
| `GPIO6-11` | reservados para flash interna | registrados no mapeamento do DevKit; nao usados no projeto |
| `GPIO1/GPIO3` | associados ao caminho USB/serial do DevKit | documentacao historica + uso de `Serial` no firmware |

## Conflitos e observacoes de engenharia

1. A definicao de comportamento da UI local fica no firmware atual, especialmente em `ui_init.cpp`.
   - para pinagem e comportamento real, priorizar `ui_init.cpp`

2. O pinout do carrier RevA depende do `ESP32 DevKit` comercial exato.
   - o mapeamento de GPIO esta congelado
   - a geometria final dos headers ainda depende da escolha do modulo

3. `L-Line` permanece `NC` nesta consolidacao.
   - se uma revisao futura exigir `L-Line`, isso sera mudanca de hardware e de documentacao, nao desta rodada
