# Arquitetura Canonica do Firmware

Documento canonico de arquitetura embarcada do `YouSimuladorOBD`.

Objetivo:

- explicar como o firmware conversa com o hardware real
- apontar os modulos principais e suas responsabilidades
- separar a arquitetura atual de descricoes historicas mais antigas

## Visao geral

O firmware roda sobre `Arduino-ESP32` com `FreeRTOS` e foi organizado para manter quatro frentes em paralelo:

- barramentos OBD (`CAN` e `K-Line`)
- simulacao e estado do veiculo
- interface local (`OLED`, botoes, encoder, LEDs)
- interface de rede (`Wi-Fi`, `Web UI`, `API`, `WebSocket`, `OTA`)

O ponto central da arquitetura e a estrutura `SimulationState`, compartilhada entre os workers com sincronizacao por `SemaphoreHandle_t`.

## Stack e dependencias

| Camada | Escolha atual |
|---|---|
| MCU | `ESP32 DevKit 38 pinos` |
| Framework | `Arduino` sobre `FreeRTOS` |
| Build | `PlatformIO` |
| Filesystem | `LittleFS` |
| Persistencia | `Preferences / NVS` |
| CAN | `TWAI` nativo do ESP32 |
| K-Line | `UART1` + GPIO |
| UI local | `Adafruit SH110X`, `Adafruit GFX`, `ESP32Encoder`, `Bounce2` |
| Web | `ESPAsyncWebServer-esphome`, `ArduinoJson` |

## Fluxo principal entre hardware e software

```text
Scanner OBD
  -> J1 OBD-II
  -> SN65HVD230 ou L9637D
  -> ESP32 (TWAI ou UART1)
  -> dispatcher OBD
  -> handlers de modo
  -> SimulationState
  -> resposta ao barramento

SimulationState
  <-> UI local
  <-> Web UI / API / WebSocket
  <-> dynamic_engine
  <-> diagnostic_scenario_engine
```

## Web UI e robustez operacional

Arquivos principais:

- `firmware/data/index.html`
- `firmware/data/ota.html`
- `firmware/src/web/web_server.cpp`

Comportamento atual:

- a `Web UI` tenta operar em tempo real via `WebSocket`
- quando o `WebSocket` cai, a interface passa a usar `HTTP` como fallback
- o fallback cobre:
  - leitura de status
  - leitura de diagnostico
  - carga de cenarios
  - troca de protocolo
  - troca de modo
  - aplicacao de presets
  - aplicacao e limpeza de cenarios
  - envio de sliders e parametros manuais

Objetivo pratico:

- evitar que a tela fique "meio viva" quando o navegador perde o `WebSocket`
- manter a bancada utilizavel mesmo com reconexao em andamento
- reduzir casos em que o usuario clica em `Modo de Simulacao` ou `Camada Diagnostica` e nada acontece

Observacao importante:

- se a lista de cenarios vier vazia em uma tentativa inicial, a UI faz nova tentativa automatica
- a API continua protegida por autenticacao; o navegador reaproveita a autenticacao da sessao aberta na mesma origem

## Boot e orquestracao

Arquivo principal:

- `firmware/src/main.cpp`

Responsabilidades:

- iniciar `Serial`
- criar mutex global do estado
- ler o `DIP` no boot
- restaurar protocolo salvo em `NVS`
- restaurar perfil salvo em `NVS`
- configurar LEDs
- subir:
  - `protocol_init()`
  - `ui_init()`
  - `web_init()`
  - `dynamic_init()`

## Mapa de hardware visto pelo firmware

Arquivo base:

- `firmware/include/config.h`

Esse arquivo e a principal fonte de verdade para:

- GPIOs de `CAN`
- GPIOs de `K-Line`
- `I2C` do OLED
- botoes
- encoder
- DIP
- LEDs
- constantes de rede e OTA

Regra pratica:

- se a pinagem mudar um dia, este arquivo muda primeiro
- a documentacao canonica que precisa acompanhar essa mudanca e `docs/hardware.md` e `docs/pinout.md`

## Transporte OBD

Arquivos:

- `firmware/src/protocols/protocol_init.cpp`
- `firmware/src/protocols/can_protocol.cpp`
- `firmware/src/protocols/kline_protocol.cpp`

### `protocol_init.cpp`

- sobe os workers de `CAN` e `K-Line`
- ambos ficam residentes e ativam/desativam o proprio hardware de acordo com `active_protocol`

### `can_protocol.cpp`

- usa `TWAI` nativo do ESP32
- aplica configuracao de `250 kbit/s` ou `500 kbit/s`
- trata `11-bit` e `29-bit`
- recebe requests OBD
- monta resposta `ISO-TP` quando necessario
- acende `LED_CAN` e pulsa `LED_TX`

### `kline_protocol.cpp`

- usa `UART1`
- suporta `ISO 9141-2`, `KWP 5-baud` e `KWP Fast`
- executa o handshake de `5-baud init` e `fast init`
- monta e valida frames com checksum
- controla o idle e a sessao do barramento
- pulsa `LED_TX`

## Core OBD

Arquivos:

- `firmware/src/obd/obd_dispatcher.cpp`
- `firmware/src/obd/mode01_handler.cpp`
- `firmware/src/obd/mode02_handler.cpp`
- `firmware/src/obd/mode03_handler.cpp`
- `firmware/src/obd/mode06_handler.cpp`
- `firmware/src/obd/mode09_handler.cpp`

Responsabilidades:

- receber um `OBDRequest`
- despachar por modo
- montar um `OBDResponse`
- retornar resposta positiva ou negativa

## Estado compartilhado e simulacao

Arquivos:

- `firmware/include/simulation_state.h`
- `firmware/src/simulation/dynamic_engine.cpp`
- `firmware/src/simulation/diagnostic_scenario_engine.cpp`
- `firmware/src/simulation/alert_engine.cpp`
- `firmware/src/simulation/fault_catalog.cpp`

Responsabilidades:

- manter dados do "veiculo" simulado
- atualizar dinamica de RPM, velocidade, temperaturas, carga e combustivel
- gerenciar DTCs e cenarios
- persistir elementos de estado como protocolo e odometro

## UI local

Arquivo:

- `firmware/src/web/ui_init.cpp`

Responsabilidades:

- inicializar `Wire` e o `OLED SH1107`
- ler os `6 botoes`
- ler o `encoder`
- permitir navegacao local e troca rapida de protocolo
- refletir o `SimulationState` no display

Dependencias diretas de hardware:

- `GPIO21/22` para `I2C`
- `GPIO32/33/25/26/27/14` para botoes
- `GPIO12/13/15` para encoder
- `GPIO19/18/23` para LEDs externos

## Web, Wi-Fi e OTA

Arquivo:

- `firmware/src/web/web_server.cpp`

Responsabilidades:

- iniciar `Wi-Fi` em `AP`, `STA` ou ambos
- publicar `mDNS`
- servir UI via `LittleFS`
- expor `API REST`
- expor `WebSocket`
- persistir configuracoes de dispositivo e redes
- coordenar `OTA`

Dependencias diretas de software:

- `ESPAsyncWebServer`
- `ArduinoJson`
- `ESPmDNS`
- `LittleFS`
- `Preferences`

## Como o firmware conversa com o hardware

### CAN

- `GPIO4` e `GPIO5`
- `TWAI` ativo apenas em protocolos CAN
- exige `SN65HVD230` ou frontend equivalente em `3V3`

### K-Line

- `GPIO17` e `GPIO16`
- `UART1`
- exige `L9637D` e a rede passiva `RK1`, `RLI1`, `CK1`, `CK2`

### UI local

- display, botoes, encoder, DIP e LEDs sao tratados como hardware de apoio diretamente acoplado ao firmware

### Rede

- usa somente a radio `Wi-Fi` do ESP32
- nao ha uso ativo de `BLE` ou `Bluetooth Classic` no firmware atual

## Conflitos documentais relevantes

1. `docs/07-ui-controls.md` descreve uma experiencia de UI mais ampla que o comportamento implementado em `ui_init.cpp`.
2. `docs/01-overview.md` cita selecao de protocolo por serial USB, o que nao representa o fluxo principal atual.
3. Componentes fisicos do carrier RevA nao podem ser inferidos apenas do firmware; precisam ser cruzados com `docs/15`, `docs/16`, `docs/17`, `hardware/pcb-handoff/` e `hardware/kicad/revA/`.

## Documentos relacionados

- [hardware.md](hardware.md)
- [pinout.md](pinout.md)
- [engenharia-pcb.md](engenharia-pcb.md)
- [06-architecture.md](06-architecture.md)
