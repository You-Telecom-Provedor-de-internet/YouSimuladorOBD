# 03 - Pinout ESP32 e Ligacoes

## ESP32 DevKit 38 pinos - mapa usado no projeto

### Comunicacao OBD

| GPIO | Funcao | Destino |
|---|---|---|
| `GPIO4` | CAN TX | `SN65HVD230 TXD` |
| `GPIO5` | CAN RX | `SN65HVD230 RXD` |
| `GPIO17` | K-Line TX | `L9637D pin 4 (TX)` |
| `GPIO16` | K-Line RX | `L9637D pin 1 (RX)` |

### OLED I2C

| GPIO | Funcao | Destino |
|---|---|---|
| `GPIO21` | I2C SDA | `OLED SDA` |
| `GPIO22` | I2C SCL | `OLED SCL` |

### Botoes

| GPIO | Funcao |
|---|---|
| `GPIO32` | `BTN_PREV` |
| `GPIO33` | `BTN_NEXT` |
| `GPIO25` | `BTN_UP` |
| `GPIO26` | `BTN_DOWN` |
| `GPIO27` | `BTN_SELECT` |
| `GPIO14` | `BTN_PROTOCOL` |

Todos os botoes usam `INPUT_PULLUP` interno e devem ser ligados entre o GPIO e o `GND`.

### Encoder

| GPIO | Funcao |
|---|---|
| `GPIO12` | encoder `A / CLK` |
| `GPIO13` | encoder `B / DT` |
| `GPIO15` | encoder `SW` |

### DIP de protocolo

| GPIO | Bit | Observacao |
|---|---|---|
| `GPIO34` | bit 0 | somente entrada, usar `10k` externo para `3V3` |
| `GPIO35` | bit 1 | somente entrada, usar `10k` externo para `3V3` |
| `GPIO36` | bit 2 | somente entrada, usar `10k` externo para `3V3` |

Leitura:

- chave aberta -> `HIGH`
- chave fechada para `GND` -> `LOW`

### LEDs

| GPIO | LED | Cor sugerida |
|---|---|---|
| `GPIO19` | atividade CAN | verde |
| `GPIO18` | atividade K-Line | amarelo |
| `GPIO23` | TX / trafego | vermelho |
| `GPIO2` | heartbeat interno | LED onboard |

## Tabela de protocolos por enum

| Valor | Protocolo |
|---|---|
| `0` | `CAN 11b 500k` |
| `1` | `CAN 11b 250k` |
| `2` | `CAN 29b 500k` |
| `3` | `CAN 29b 250k` |
| `4` | `ISO 9141-2` |
| `5` | `KWP 5-baud` |
| `6` | `KWP Fast` |

## Conector OBD-II

| Pino OBD | Sinal | Ligacao no projeto |
|---|---|---|
| `4` | chassis GND | `GND comum` |
| `5` | signal GND | `GND comum` |
| `6` | CAN High | `SN65HVD230 CANH` |
| `7` | K-Line | `L9637D pin 6 (K)` |
| `14` | CAN Low | `SN65HVD230 CANL` |
| `15` | L-Line | `NC` |
| `16` | +12V bateria | `LM2596 IN+` e `L9637D pin 7 (VS)` |

## Ligacoes completas por bloco

### CAN

| Origem | Destino |
|---|---|
| `GPIO4` | `SN65HVD230 TXD` |
| `GPIO5` | `SN65HVD230 RXD` |
| `3V3` | `SN65HVD230 VCC` |
| `GND` | `SN65HVD230 GND` |
| `GND` | `SN65HVD230 RS` |
| `OBD pin 6` | `SN65HVD230 CANH` |
| `OBD pin 14` | `SN65HVD230 CANL` |

### K-Line

| Origem | Destino |
|---|---|
| `GPIO17` | `L9637D pin 4 (TX)` |
| `GPIO16` | `L9637D pin 1 (RX)` |
| `3V3` | `L9637D pin 3 (VCC)` |
| `GND` | `L9637D pin 5 (GND)` |
| `OBD pin 7` | `L9637D pin 6 (K)` |
| `OBD pin 16` | `L9637D pin 7 (VS)` |
| `RLI1 10k` | `L9637D pin 8 (LI)` -> `pin 7 (VS)` |
| `RK1 510R` | `L9637D pin 6 (K)` -> `pin 7 (VS)` |
| `CK1 100nF` | `L9637D pin 3` -> `pin 5` |
| `CK2 100nF` | `L9637D pin 7` -> `pin 5` |

## Restricoes importantes do ESP32

| GPIO | Restricao |
|---|---|
| `GPIO0` | boot strap, evitar uso geral |
| `GPIO1` / `GPIO3` | serial USB debug |
| `GPIO6-11` | flash interna, nunca usar |
| `GPIO34-39` | somente entrada, sem pull-up interno |
| `GPIO12` | pino sensivel em boot, evitar pull-up externo incorreto |

## Referencia rapida de diagnostico

Quando a K-Line estiver montada corretamente e o protocolo ativo for K-Line:

- `L9637D pin 4 (TX) -> GND`: `~3.3V` em repouso
- `L9637D pin 6 (K) -> GND`: `~12V` em repouso
- `L9637D pin 1 (RX) -> GND`: `~3.3V` em repouso

Se a linha `K` estiver perto de `0V` em repouso, revisar:

- `RK1 510R`
- `VS +12V`
- `GPIO17`
- `GPIO16`
- solda do `L9637D`
