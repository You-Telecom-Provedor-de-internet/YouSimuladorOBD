# Mapa Final de GPIO da RevA

## Objetivo

Este documento congela o mapa de GPIO da `RevA` para fechamento do esquematico final.

Ele consolida o mapa de GPIO que deve ser usado como referencia de fechamento para a `PCB RevA`.

## Criterios de fechamento

- manter `CAN`, `K-Line` e `OLED` nos GPIOs ja validados
- remover a UI dos pinos de strapping mais sensiveis do ESP32
- eliminar os LEDs externos redundantes de `CAN` e `K-Line`
- manter apenas `LED_TX` como diagnostico rapido opcional
- preservar `6 botoes`, `encoder`, `DIP` e `OLED`
- simplificar a captura final do `KiCad`

## Mapa final congelado

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
| `GPIO18` | `BTN_PROTOCOL` |

Regra eletrica:

- todos os botoes usam `INPUT_PULLUP` interno
- ligar cada botao entre o GPIO e o `GND`
- nao usar `pull-down` externo nesses botoes

### Encoder

| GPIO | Funcao |
|---|---|
| `GPIO14` | encoder `A / CLK` |
| `GPIO13` | encoder `B / DT` |
| `GPIO19` | encoder `SW` |

Regra eletrica:

- `ENC_SW` segue a mesma logica dos botoes: contato para `GND`
- `GPIO12` e `GPIO15` deixam de ser usados pela UI
- se o modulo `KY-040` tiver `pull-up` onboard em `CLK/DT/SW`, isso e aceitavel nesta revisao porque `GPIO14`, `GPIO13` e `GPIO19` nao sao o problema principal de boot que existia em `GPIO12`

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

| GPIO | LED | Status |
|---|---|---|
| `GPIO23` | `LED_TX` | manter como opcional |
| `GPIO2` | LED onboard | manter |

Remover da `RevA` final:

- `LED_CAN`
- `LED_KLINE`

### GPIOs livres ou reservados

| GPIO | Uso na RevA final | Observacao |
|---|---|---|
| `GPIO12` | nao usar na UI | strapping sensivel no boot |
| `GPIO15` | nao usar na UI | strapping no boot |
| `GPIO39` | livre / reserva | somente entrada; nao necessario nesta revisao |

## Delta em relacao ao rascunho atual do esquema

Aplicar as seguintes trocas para fechar a captura:

1. remover `LED_CAN` de `GPIO19`
2. remover `LED_KLINE` de `GPIO18`
3. mover `BTN_PROTOCOL` para `GPIO18`
4. mover `ENC_SW` para `GPIO19`
5. manter `ENC_A` em `GPIO14`
6. manter `ENC_B` em `GPIO13`
7. recolocar `DIP bit 2` em `GPIO36`
8. deixar `GPIO39` sem obrigacao funcional nesta revisao
9. manter `LED_TX` em `GPIO23` apenas se o LED de trafego rapido continuar desejado

## Justificativa tecnica

### Por que sair de `GPIO12` e `GPIO15`

- `GPIO12` e pino de strapping sensivel no boot do ESP32
- `GPIO15` tambem e pino de strapping
- a UI nao precisa ocupar GPIOs com risco de boot quando existem GPIOs livres apos remover os LEDs redundantes

### Por que remover `LED_CAN` e `LED_KLINE`

- o `OLED` ja mostra protocolo ativo na UI
- os dois LEDs duplicam informacao que ja existe no display
- a tampa de referencia da UI nao reserva abertura especifica para esses LEDs
- liberar `GPIO18` e `GPIO19` resolve a realocacao da UI sem sacrificar barramentos

### Por que manter `LED_TX` opcional

- ele ainda entrega diagnostico visual rapido de trafego
- nao conflita com o mapa final da UI
- pode ser removido depois se a versao mecanica final pedir simplificacao maxima

## Impacto no firmware

O firmware desta branch ja foi alinhado a este mapa nos sinais abaixo:

- `BTN_PROTOCOL`: `GPIO18`
- `ENC_A`: `GPIO14`
- `ENC_SW`: `GPIO19`
- `LED_CAN`: removido do runtime
- `LED_KLINE`: removido do runtime

Nenhuma mudanca adicional de barramento foi necessaria para:

- `CAN`
- `K-Line`
- `OLED`
- `DIP`
- `BTN_PREV`
- `BTN_NEXT`
- `BTN_UP`
- `BTN_DOWN`
- `BTN_SELECT`
- `LED_TX`

## Referencias

- [Pinout canonico](pinout.md)
- [Guia executivo para PCB](engenharia-pcb.md)
- [RevA Carrier para ESP32 DevKit](reva-carrier-esp32-devkit.md)
- [hardware/kicad/revA/ui-panel-reference.csv](../hardware/kicad/revA/ui-panel-reference.csv)
