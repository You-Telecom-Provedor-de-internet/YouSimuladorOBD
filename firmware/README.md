# Firmware

Este diretorio contem o firmware embarcado do `YouSimuladorOBD`.

## Build

Comandos principais:

```bash
cd firmware
pio run
```

## Upload

```bash
cd firmware
pio run -t upload
pio run -t uploadfs
```

Ferramentas uteis:

```bash
cd firmware
pio device monitor
pio run -t size
```

## Estrutura interna

```text
firmware/
|- include/
|- src/
|  |- obd/
|  |- protocols/
|  |- simulation/
|  `- web/
|- data/
`- platformio.ini
```

## Arquivos mais importantes

- `include/config.h`
  - mapa central de GPIOs e constantes principais
- `src/main.cpp`
  - boot, leitura do DIP, restauracao de protocolo/perfil e subida das tasks
- `src/protocols/can_protocol.cpp`
  - transporte OBD sobre CAN/TWAI
- `src/protocols/kline_protocol.cpp`
  - transporte OBD sobre K-Line/UART1
- `src/web/ui_init.cpp`
  - UI local com OLED, botoes e encoder
- `src/web/web_server.cpp`
  - Wi-Fi, Web UI, API, WebSocket, mDNS e OTA

## Dependencias principais

Declaradas em `platformio.ini`:

- `ESPAsyncWebServer-esphome`
- `ArduinoJson`
- `Adafruit SH110X`
- `Adafruit GFX`
- `Adafruit BusIO`
- `ESP32Encoder`
- `Bounce2`

Tambem usa bibliotecas nativas do ecossistema ESP32:

- `Preferences`
- `Wire`
- `ESPmDNS`
- `LittleFS`
- `Update`
- `WiFi`
- `WiFiClientSecure`

## Arquivos de configuracao eletrica

Para qualquer manutencao ligada a hardware, comecar por:

- `include/config.h`
- `src/main.cpp`
- `src/protocols/can_protocol.cpp`
- `src/protocols/kline_protocol.cpp`
- `src/web/ui_init.cpp`

Se algum GPIO mudar no futuro, atualizar junto:

- `../docs/hardware.md`
- `../docs/pinout.md`

## Observacoes praticas

- a estrutura interna de `firmware/` foi preservada nesta rodada para evitar risco de quebrar build
- esta revisao usa `ESP32 DevKit 38 pinos` como baseline, nao `ESP32-WROOM` integrado
- o firmware usa `Wi-Fi`, mas nao usa `BLE/Bluetooth` de forma ativa nesta revisao
- `SSID`, `hostname` e credenciais default ainda existem no firmware e devem ser tratados como risco operacional/documental

## Dados web em flash

A pasta `data/` contem os arquivos servidos pelo `LittleFS`, como:

- `index.html`
- `ota.html`

Se houver mudanca relevante na UI web:

1. atualizar os arquivos em `data/`
2. reexecutar `pio run`
3. se necessario, subir o filesystem com `pio run -t uploadfs`

## Web UI em bancada

Comportamento esperado da interface web atual:

- `WebSocket` e o caminho preferencial para atualizacao em tempo real
- se o `WebSocket` oscilar ou cair, a UI usa `HTTP` como fallback
- o fallback cobre:
  - status
  - diagnostico
  - cenarios
  - protocolo
  - modo
  - presets
  - sliders e parametros

Autenticacao em bancada:

- todas as rotas `/api/*` exigem autenticacao valida no dispositivo ativo
- as credenciais em `include/config.h` sao apenas o baseline do firmware, nao uma garantia do estado atual salvo no ESP32
- se as credenciais tiverem sido rotacionadas pela UI, automacoes locais, `curl` e fluxos do `YOU OBD Lab` podem receber `401 Unauthorized` mesmo com o firmware atualizado

Checklist rapido antes de culpar a API ou o frontend:

1. abrir `http://youobd2.local` e confirmar login normal na Web UI
2. conferir as credenciais ativas em `Configuracao do Dispositivo`
3. se necessario, rotacionar novamente as credenciais antes de rodar scripts ou smoke tests
4. validar a autenticacao antes de usar `GET /api/status`, `GET /api/diagnostics` ou `GET /api/scenarios` como oracle automatizado

Checklist pratico de validacao apos mexer na UI:

1. abrir `http://youobd2.local`
2. confirmar que o `Perfil do Veiculo` carrega
3. confirmar que a lista de `Camada Diagnostica` carrega
4. testar troca de `Modo de Simulacao`
5. testar aplicacao e limpeza de um cenario
6. testar um slider qualquer e conferir atualizacao de estado

Se o firmware mudou e a UI tambem mudou:

1. `pio run -t upload`
2. `pio run -t uploadfs`
