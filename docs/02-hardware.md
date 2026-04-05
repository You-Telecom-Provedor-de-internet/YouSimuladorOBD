# 02 - Hardware e Componentes

## Visao geral da plataforma

O hardware validado do `YouSimuladorOBD` usa uma topologia simples e reproduzivel:

- `ESP32 DevKit` 38 pinos como MCU principal
- `SN65HVD230` para a camada fisica CAN
- `L9637D` para a camada fisica K-Line
- `LM2596` para converter `+12V` do OBD em `+5V`
- `ESP32 3V3` alimentando a logica de `SN65HVD230`, `L9637D` e `OLED`

O objetivo desta revisao e documentar o hardware realmente validado em bancada, e nao a topologia antiga com transistor discreto de K-Line.

## BOM validada

### Componentes principais

| RefDes | Qtd | Componente | Funcao |
|---|---|---|---|
| `U1` | 1 | `LM2596` modulo buck 12V -> 5V | alimentacao do sistema |
| `U2` | 1 | `SN65HVD230` | transceiver CAN 3.3V |
| `U3` | 1 | `L9637D` | transceiver K-Line |
| `MCU1` | 1 | `ESP32 DevKit` 38 pinos | MCU principal |
| `J1` | 1 | conector `OBD-II` femea 16 pinos | interface com scanner |
| `DISP1` | 1 | `OLED SH1107 128x128` I2C | display local |
| `ENC1` | 1 | `KY-040` | encoder rotativo |
| `SW2-SW7` | 6 | botoes tacteis | interface local |
| `SW1` | 1 | DIP switch 3 bits | selecao rapida de protocolo |
| `D1-D3` | 3 | LEDs de status | CAN / K-Line / TX |

### Passivos e apoio

| RefDes | Qtd | Valor | Observacao |
|---|---|---|---|
| `RCAN1` | 1 | `120R` | terminacao CAN, preferencialmente por jumper ou DNP |
| `RK1` | 1 | `510R` | pull-up da linha K para `VS`, usar `>= 0.5W` |
| `RLI1` | 1 | `10k` | bias de `LI -> VS` no `L9637D` |
| `RDIP1-RDIP3` | 3 | `10k` | pull-up externo do DIP para `3V3` |
| `RLED1-RLED3` | 3 | `330R` | limitador dos LEDs |
| `CK1` | 1 | `100nF` | `L9637D VCC -> GND` |
| `CK2` | 1 | `100nF` | `L9637D VS -> GND` |
| `CCAN1` | 1 | `100nF` | desacoplamento local do `SN65HVD230` |
| `F1` | 1 | `1A` | fusivel da entrada `+12V` |

## Alimentacao

### Caminho validado

```text
OBD pin 16 (+12V) -> F1 -> LM2596 IN+
OBD pin 4/5 (GND) -> LM2596 IN-
LM2596 OUT+ -> ESP32 VIN / 5V
LM2596 OUT- -> GND
ESP32 3V3 -> logica de U2, U3 e OLED
```

### Notas para PCB

- manter `+12V`, `+5V` e `3V3` separados por nets claras
- usar plano de GND unico e curto entre `OBD`, `LM2596`, `ESP32`, `SN65HVD230` e `L9637D`
- o `RK1 = 510R` pode dissipar perto de `0.28W` quando a linha K fica baixa; na PCB use componente com folga termica

## CAN - interface fisica

### Topologia validada

```text
ESP32 GPIO4  -> SN65HVD230 TXD
ESP32 GPIO5  <- SN65HVD230 RXD
SN65HVD230 CANH -> OBD pin 6
SN65HVD230 CANL -> OBD pin 14
SN65HVD230 VCC  -> 3V3
SN65HVD230 GND  -> GND
SN65HVD230 RS   -> GND (modo rapido)
```

### Observacoes

- o breakout de bancada aparentou possuir terminacao onboard; na PCB final a melhor pratica e usar `120R` selecionavel por jumper
- para layout, manter o par `CANH/CANL` curto, paralelo e de preferencia roteado junto

## K-Line - interface fisica

### Topologia validada

```text
ESP32 GPIO17 -> L9637D pin 4 (TX)
ESP32 GPIO16 <- L9637D pin 1 (RX)
L9637D pin 3 (VCC) -> 3V3
L9637D pin 5 (GND) -> GND
L9637D pin 6 (K)   -> OBD pin 7
L9637D pin 7 (VS)  -> OBD pin 16 (+12V)
L9637D pin 8 (LI)  -> 10k -> pin 7 (VS)
RK1 510R entre pin 6 (K) e pin 7 (VS)
CK1 100nF entre pin 3 e pin 5
CK2 100nF entre pin 7 e pin 5
L9637D pin 2 (LO)  -> NC
OBD pin 15 (L-Line) -> NC
```

### Medidas validadas em repouso

- `L9637D pin 4 (TX) -> GND`: `~3.3V`
- `L9637D pin 6 (K) -> GND`: `~12.23V`
- `L9637D pin 1 (RX) -> GND`: `~3.317V`

Essas tres medicoes sao a referencia rapida para depuracao.

## Interface local

### OLED

- `GPIO21` -> `SDA`
- `GPIO22` -> `SCL`
- alimentacao em `3V3`

### Botoes

- `GPIO32`, `GPIO33`, `GPIO25`, `GPIO26`, `GPIO27`, `GPIO14`
- modo `INPUT_PULLUP`
- ligar cada botao entre o GPIO e o `GND`
- nao precisa pull-up externo nos botoes

### Encoder

- `GPIO12` -> `CLK`
- `GPIO13` -> `DT`
- `GPIO15` -> `SW`

### DIP

- `GPIO34`, `GPIO35`, `GPIO36`
- esses pinos sao somente entrada e nao possuem pull-up interno
- usar `10k` de cada GPIO para `3V3`
- chave fechada para `GND` = bit `LOW`

### LEDs

- `GPIO19` -> LED CAN
- `GPIO18` -> LED K-Line
- `GPIO23` -> LED TX
- `330R` em serie por LED

## Conector OBD-II usado

| Pino | Sinal | Uso |
|---|---|---|
| `4` | chassis GND | GND comum |
| `5` | signal GND | GND comum |
| `6` | CANH | `SN65HVD230 CANH` |
| `7` | K-Line | `L9637D K` |
| `14` | CANL | `SN65HVD230 CANL` |
| `15` | L-Line | nao usado |
| `16` | +12V bateria | fonte do sistema e `L9637D VS` |

## Recomendacoes para a placa final

### Obrigatorio para reproduzir a bancada validada

- `SN65HVD230`
- `L9637D`
- `510R` entre `K` e `VS`
- `10k` entre `LI` e `VS`
- dois `100nF` no `L9637D`
- `LM2596` ou buck equivalente para `12V -> 5V`

### Recomendado para a revisao de PCB

- `RCAN1 120R` por jumper ou `DNP`
- TVS na entrada `+12V`
- protecao contra inversao de polaridade na entrada
- ponto de teste em `CANH`, `CANL`, `K`, `TX_K`, `RX_K`, `+12V`, `+5V`, `+3V3` e `GND`

## Referencia principal para o engenheiro

Para capturar o esquema de PCB, use junto:

- [15 - Diagrama Eletrico e Handoff PCB](15-diagrama-eletrico-pcb.md)
- [Pacote de handoff CSV](../hardware/pcb-handoff/README.md)
