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
