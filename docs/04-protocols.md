# 04 - Protocolos OBD-II

## Matriz de protocolos do simulador

| Enum | Protocolo | Fisica | Init | Estado atual |
|---|---|---|---|---|
| `0` | `CAN 11b 500k` | CAN | n/a | Validado |
| `1` | `CAN 11b 250k` | CAN | n/a | Validado |
| `2` | `CAN 29b 500k` | CAN | n/a | Validado |
| `3` | `CAN 29b 250k` | CAN | n/a | Validado |
| `4` | `ISO 9141-2` | K-Line | 5 baud | Validado |
| `5` | `KWP 5-baud` | K-Line | 5 baud | Implementado, ampliar validacao externa |
| `6` | `KWP Fast` | K-Line | fast init | Validado |

## Servicos OBD por protocolo

O simulador entrega o mesmo conjunto logico de servicos OBD em CAN e K-Line, respeitando o encapsulamento de cada transporte.

| Servico | Descricao | Estado |
|---|---|---|
| `Mode 01` | dados correntes, readiness, fuel status, runtime, fuel type | Validado |
| `Mode 02` | freeze frame classico | Validado |
| `Mode 03` | leitura de DTC | Validado |
| `Mode 04` | limpar DTC | Validado |
| `Mode 06` | monitor tests sinteticos | Validado |
| `Mode 09` | VIN | Validado |

## CAN

### IDs usados

#### CAN 11 bits

- requisicao funcional: `0x7DF`
- requisicao fisica: `0x7E0`
- resposta ECU: `0x7E8`

#### CAN 29 bits

- requisicao funcional: `0x18DB33F1`
- requisicao fisica: `0x18DA10F1`
- resposta ECU: `0x18DAF110`

### Camada fisica

- `SN65HVD230`
- `GPIO4` como TX
- `GPIO5` como RX

### Estado validado

- `CAN 11b 500k`: validado
- `CAN 11b 250k`: validado
- `CAN 29b 500k`: validado
- `CAN 29b 250k`: validado

Validado com:

- `Torque Pro`
- `OBDLink MX+`
- `YouAutoCar`

## ISO 9141-2

### Camada fisica

- `L9637D`
- `GPIO17` como TX logico
- `GPIO16` como RX logico
- linha `K` no OBD `pin 7`
- `510R` de `K -> VS`

### Init

Fluxo implementado:

1. linha K em idle alto
2. deteccao do endereco `0x33` em `5 baud`
3. envio de `0x55`
4. envio de key bytes
5. recepcao do complemento do scanner
6. entrada em sessao ativa a `10400 baud`

### Evidencia de aceite

- scanners fecharam em `AUTO, ISO 9141-2`
- resposta real a `0100`: `41 00 1E 3F 80 01`
- VIN por `0902` lido corretamente
- PIDs correntes lidos com `Torque`, `OBDLink` e `YouAutoCar`

## KWP 5-baud

### Objetivo

Suportar scanners que sobem `ISO 14230-4` com slow init pela mesma linha K.

### Estado atual

- handshake e sessao KWP implementados no firmware
- tratamento proprio de key bytes e complemento de endereco
- rearm de sessao por inatividade implementado

### Observacao de status

O stack esta ativo e funcional no firmware, mas a validacao externa ainda precisa ser ampliada com mais um scanner/app alem da rodada principal.

## KWP Fast

### Objetivo

Suportar scanners que usam `fast init` na K-Line em vez de `5 baud init`.

### Estado atual

- deteccao de pulso de fast init implementada
- sessao rearmada por timeout de inatividade
- correcoes de watchdog e parser ja aplicadas
- validado com leitura real no `Torque Pro`

## PIDs de discovery para scanner real

Para aumentar compatibilidade com aplicativos e scanners comerciais, o firmware responde hoje a um conjunto minimo mais realista de discovery:

- `0101`
- `0102`
- `0103`
- `0112`
- `0113`
- `011C`
- `011D`
- `011E`
- `011F`
- `0133`
- `0146`
- `0151`

Isso foi essencial para melhorar aceite de scanners comerciais, principalmente em K-Line.

## Persistencia e troca de protocolo

- o protocolo selecionado pela web/UI e salvo em NVS
- no boot, o firmware carrega primeiro o protocolo salvo
- se nao houver valor salvo valido, usa o DIP switch
- `PROTO_BOOT_OVERRIDE` esta desativado no estado atual (`0xFF`)

## Matriz de validacao externa

| Ferramenta | CAN | ISO 9141-2 | KWP Fast | Observacao |
|---|---|---|---|---|
| `Torque Pro` | OK | OK | OK | leitura real em telefone |
| `OBDLink app` | OK | OK | validar ampliado | conectou em ISO real |
| `YouAutoCar` | OK | OK | n/a | Modes 02/06 validados |
| `ELM327 generico` | CAN validado em bancada | depende da rodada | n/a | usar como regressao adicional |

## Pendencia tecnica conhecida

- ampliar a validacao externa de `KWP 5-baud` com mais uma ferramenta alem da rodada principal
