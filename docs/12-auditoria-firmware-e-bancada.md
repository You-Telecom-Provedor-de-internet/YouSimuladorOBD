# 12 - Auditoria do Firmware e Bancada

## Objetivo

Consolidar o estado real do `YouSimuladorOBD` apos as rodadas de bring-up, reducao de firmware, camada diagnostica e validacao de scanners reais.

Data de consolidacao desta versao: `2026-04-06`

## Correcao estrutural do transporte

### 1. Troca de protocolo em runtime

Problema antigo:

- a UI podia mostrar um protocolo enquanto o hardware ainda estava no barramento anterior

Estado atual:

- `protocol_init.cpp` sobe CAN e K-Line
- cada worker ativa ou repousa conforme `active_protocol`
- o protocolo selecionado passa a ser persistido em NVS

### 2. Persistencia de protocolo

Problema antigo:

- reset podia voltar para outro protocolo e sabotar a bancada

Estado atual:

- `main.cpp` carrega protocolo salvo
- DIP fica como fallback
- `PROTO_BOOT_OVERRIDE` voltou para estado neutro

## Correcao do CAN

Conjunto de correcoes consolidadas:

- troca de protocolo reinicializando o barramento certo
- filtro de IDs OBD esperado
- suporte a respostas longas em CAN
- `Mode 03` e `Mode 04` encapsulados corretamente

Resultado:

- `CAN 11b 500k` validado
- `CAN 11b 250k` validado
- `CAN 29b 500k` validado
- `CAN 29b 250k` validado

Ferramentas usadas:

- `ELM327`
- `OBDLink MX+`
- `Torque Pro`

## Correcao do K-Line

### Causa principal da rodada de bancada

O bring-up de K-Line so fechou quando a topologia real ficou igual a abaixo:

- `L9637D`
- `510R` entre `K` e `VS`
- `10k` entre `LI` e `VS`
- `100nF` em `VCC -> GND`
- `100nF` em `VS -> GND`

Medidas de repouso aceitas:

- `TX`: `~3.3V`
- `K`: `~12V`
- `RX`: `~3.3V`

### Correcao de firmware

Foram corrigidos:

- detector de `5 baud init`
- fluxo de key bytes
- parser de eco e dados residuais
- rearm de sessao por inatividade
- espera de `fast init` sem travar watchdog

## Compatibilidade com scanners reais

### OBDLink e apps proprios

Aceite consolidado:

- `ISO 9141-2`: `OK`
- leitura de `0100`, `0902`, `010C`, `010D`, `0105`, `0104`, `010B`, `010F`, `0111`, `0106`: `OK`
- `Mode 03`: `OK`

### Torque Pro

Aceite consolidado:

- `ISO 9141-2`: `OK`
- `KWP Fast`: `OK`
- gauges com leitura real de RPM, TPS, coolant, MAF, MAP e bateria: `OK`

### YouAutoCar

Aceite consolidado:

- CAN: `OK`
- `ISO 9141-2`: `OK`
- `Mode 02`: `OK`
- `Mode 06`: `OK`
- `Mode 09`: `OK`

## Compatibilidade OBD melhorada no firmware

Para ficar mais proximo de uma ECU real de bancada, o firmware passou a responder discovery adicional em `Mode 01`:

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

Isso foi importante para melhorar aceite de scanners comerciais.

## Camada diagnostica

Estado consolidado:

- `Mode 02` classico validado
- `Mode 03` validado
- `Mode 04` validado
- `Mode 06` validado
- `Mode 09` validado
- `/api/diagnostics` com `freeze_frame` e `freeze_frames[]`
- decoder do app corrigido para DTC de freeze frame

## OTA e tamanho de firmware

Rodadas anteriores reduziram o firmware para sair do limite perigoso do slot OTA:

- upload local de OTA removido
- OTA mantida apenas online
- `CORE_DEBUG_LEVEL` reduzido

Estado operacional:

- OTA online funcional
- firmware gravado por `COM3`
- filesystem gravado por `COM3`

## Matriz de aceite resumida

| Area | Estado |
|---|---|
| CAN | Validado |
| ISO 9141-2 | Validado |
| KWP Fast | Validado |
| KWP 5-baud | Validado |
| Web / API / WebSocket | Validado |
| OTA online | Validado |
| OLED / UI local | Validado |
| Mode 02 | Validado |
| Mode 03 | Validado |
| Mode 04 | Validado |
| Mode 06 | Validado |
| Mode 09 | Validado |

## Pendencias honestas

- transformar a validacao fisica consolidada em captura nativa de `KiCad` ou `EasyEDA`
- fechar os detalhes de `PCB` orientados a produto: protecao, mecanica, pontos de teste e repetibilidade de montagem
