# 13 - Montagem K-Line com L9637D

## Objetivo

Registrar a montagem final e validada da interface `K-Line` usando `L9637D`, incluindo o que realmente funcionou em bancada.

Esta e a referencia correta para:

- perfboard
- bancada
- captura de esquema para PCB

## Componentes obrigatorios

| RefDes | Valor | Funcao |
|---|---|---|
| `U3` | `L9637D` | transceiver K-Line |
| `RK1` | `510R` | pull-up de `K -> VS`, usar `>= 0.5W` |
| `RLI1` | `10k` | bias de `LI -> VS` |
| `CK1` | `100nF` | `VCC -> GND` |
| `CK2` | `100nF` | `VS -> GND` |

## Pinagem do L9637D

Top view:

```text
         .---U3 L9637D---.
RX    1 -|               |- 8 LI
LO    2 -|               |- 7 VS
VCC   3 -|               |- 6 K
TX    4 -|               |- 5 GND
         '---------------'
```

## Ligacao final validada

| Pin U3 | Nome | Liga em | Estado |
|---|---|---|---|
| `1` | `RX` | `ESP32 GPIO16` | usado |
| `2` | `LO` | `NC` | nao usado |
| `3` | `VCC` | `ESP32 3V3` | usado |
| `4` | `TX` | `ESP32 GPIO17` | usado |
| `5` | `GND` | `GND comum` | usado |
| `6` | `K` | `OBD pin 7` | usado |
| `7` | `VS` | `OBD pin 16 (+12V)` | usado |
| `8` | `LI` | `10k -> VS` | usado |

Passivos:

- `RK1 510R`: entre `pin 6 (K)` e `pin 7 (VS)`
- `CK1 100nF`: entre `pin 3 (VCC)` e `pin 5 (GND)`
- `CK2 100nF`: entre `pin 7 (VS)` e `pin 5 (GND)`

## Resumo visual da ligacao

```text
ESP32 GPIO17  -> U3 pin 4 (TX)
ESP32 GPIO16  <- U3 pin 1 (RX)
ESP32 3V3     -> U3 pin 3 (VCC)
ESP32 GND     -> U3 pin 5 (GND)

OBD pin 7     -> U3 pin 6 (K)
OBD pin 16    -> U3 pin 7 (VS)
OBD pin 4/5   -> GND comum

U3 pin 8 (LI) -> 10k -> U3 pin 7 (VS)
U3 pin 6 (K)  -> 510R -> U3 pin 7 (VS)
```

## Medidas aceitas em repouso

Com a placa ligada, sem trafego:

- `U3 pin 4 (TX) -> GND`: `~3.3V`
- `U3 pin 6 (K) -> GND`: `~12.23V`
- `U3 pin 1 (RX) -> GND`: `~3.317V`

Se isso nao acontecer, revisar primeiro:

- `VS +12V`
- `RK1 510R`
- `GPIO17`
- `GPIO16`
- solda do `L9637D`

## Teste passivo do CI

Com a placa desligada, o CI aprovado mostrou:

- `pin 3 -> pin 5`: alta resistencia
- `pin 7 -> pin 5`: `OL`
- `pin 6 -> pin 5`: `OL`
- `pin 6 -> pin 7`: `OL`

Esse teste ajuda a descartar curto grosseiro.

## Teste funcional isolado do CI

Teste de bancada que aprovou o `L9637D`:

1. alimentar:
   - `pin 3 = 3V3`
   - `pin 5 = GND`
   - `pin 7 = +12V`
   - `pin 8 -> 10k -> pin 7`
   - `pin 6 -> 510R -> pin 7`
2. forcar `pin 4` por resistor de `1k`

Resultado esperado:

- `pin 4` em alto -> `pin 6` perto de `12V`
- `pin 4` em baixo -> `pin 6` perto de `0V`

Esse teste foi o que comprovou que o CI estava funcional e orientado corretamente.

## Problemas reais encontrados na bancada

### 1. Sem `510R` em `K -> VS`

Sintoma:

- `BUS INIT ERROR`
- linha K sem idle alto confiavel

Correcao:

- adicionar `510R` entre `pin 6` e `pin 7`

### 2. Sem os dois `100nF`

Sintoma:

- bring-up mais sensivel a ruido
- init instavel

Correcao:

- adicionar `CK1` e `CK2`

### 3. Protocolo errado no boot

Sintoma:

- placa parecia voltar para `ISO 9141-2` ou outro protocolo apos reset

Correcao:

- persistir o protocolo escolhido em NVS

## Estado final aceito

- `ISO 9141-2`: validado
- `KWP Fast`: validado
- `KWP 5-baud`: stack implementado e pronto para mais validacao externa

## Recomendacao para a placa de circuito impresso

- manter o `L9637D` perto do conector OBD
- manter `CK1` e `CK2` encostados no CI
- usar `RK1` com folga termica
- deixar ponto de teste em `K`, `TX_K`, `RX_K`, `VS` e `GND`
