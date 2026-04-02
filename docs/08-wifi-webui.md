# 08 - Wi-Fi, Interface Web e OTA

## Visao Geral

O ESP32 expoe uma interface web para controle completo do simulador e para atualizacao OTA do firmware e do filesystem.

Fluxo atual:

```text
Navegador / Celular
  |- http://192.168.1.5/
  |- http://youobd.local/
  `- http://192.168.4.1/    (fallback AP)
              |
              v
ESP32
  |- Wi-Fi STA com DHCP
  |- AP fallback "OBD-Simulator"
  |- AsyncWebServer
  |- WebSocket /ws
  |- LittleFS
  `- OTA web para firmware e filesystem
```

## Estado Atual do Firmware

- O hostname mDNS atual e `youobd.local`
- O Wi-Fi STA usa DHCP por padrao
- Se a rede STA falhar, o ESP32 sobe AP fallback
- A interface web inteira exige autenticacao
- O Bluetooth SPP existe no codigo, mas esta desabilitado por padrao para priorizar estabilidade do Wi-Fi/Web

## Modos Wi-Fi

### STA - rede existente

Quando consegue conectar na rede configurada:

```text
SSID padrao: YOU_AUTO_CAR_2.4
IP: DHCP
Hostname: youobd.local
Exemplo: http://192.168.1.5/
```

### AP fallback

Quando nao consegue conectar em STA:

```text
SSID: OBD-Simulator
Senha: obd12345
IP: 192.168.4.1
URL: http://192.168.4.1/
```

## Autenticacao Web

Toda a interface web protegida e as rotas sensiveis usam autenticacao HTTP com desafio Digest por padrao.

Credenciais padrao atuais:

```text
Usuario: admin
Senha: obd12345
```

Essas credenciais estao em `firmware/include/config.h` e devem ser trocadas antes de uso em producao.

## URLs Importantes

Painel principal:

```text
http://youobd.local/
http://<ip-do-esp>/
```

Pagina OTA:

```text
http://youobd.local/ota.html
http://<ip-do-esp>/ota.html
```

Health check:

```text
GET /ping
GET /ping-json
```

## API REST

Base:

```text
http://<ip>/api
```

Todas as rotas abaixo exigem autenticacao.

### Leitura de estado

- `GET /api/status`
- `GET /api/dtcs`
- `GET /api/profiles`
- `GET /api/wifi`
- `GET /api/wifi/scan`
- `GET /api/ota/info`

### Escrita e controle

- `POST /api/params`
- `POST /api/protocol`
- `POST /api/preset`
- `POST /api/mode`
- `POST /api/profile`
- `POST /api/dtcs/add`
- `POST /api/dtcs/remove`
- `POST /api/dtcs/clear`
- `POST /api/wifi`
- `POST /api/wifi/remove`
- `POST /api/wifi/scan`
- `POST /api/reboot`
- `POST /api/ota/firmware`
- `POST /api/ota/filesystem`

### Exemplo - status

```json
GET /api/status
{
  "protocol": "CAN 11b 500k",
  "protocol_id": 0,
  "rpm": 800,
  "speed": 0,
  "coolant_temp": 90,
  "intake_temp": 30,
  "maf": "3.0",
  "map": 35,
  "throttle": 15,
  "ignition_advance": "10.0",
  "engine_load": 25,
  "fuel_level": 75,
  "battery_voltage": "14.2",
  "oil_temp": 85,
  "stft": "0.0",
  "ltft": "0.0",
  "dtcs": [],
  "vin": "YOUSIM00000000001",
  "profile_id": "",
  "sim_mode": 0,
  "bt_connected": false
}
```

### Exemplo - Wi-Fi

```json
GET /api/wifi
{
  "connected": true,
  "current_ssid": "YOU_AUTO_CAR_2.4",
  "current_ip": "192.168.1.5",
  "ap_ip": "192.168.4.1",
  "ap_ssid": "OBD-Simulator",
  "hostname": "youobd.local",
  "networks": [
    {
      "ssid": "YOU_AUTO_CAR_2.4",
      "ip": "",
      "gw": ""
    }
  ]
}
```

### Exemplo - OTA info

```json
GET /api/ota/info
{
  "enabled": true,
  "auth_user": "admin",
  "build_date": "Apr  2 2026",
  "build_time": "15:56:44",
  "hostname": "youobd.local",
  "chip_model": "ESP32-D0WD-V3",
  "sketch_size": 1751216,
  "free_sketch_space": 1966080,
  "fs_total": 131072,
  "fs_used": 69632,
  "running_partition": "app0",
  "last_target": "idle",
  "last_ok": true,
  "last_error": "",
  "last_written": 0,
  "last_total": 0
}
```

## WebSocket

Endpoint:

```text
ws://<host>/ws
```

O WebSocket tambem exige autenticacao e envia o estado completo do simulador a cada `500 ms`.

## OTA Web

O projeto usa a propria stack web do ESP32 para OTA.

### O que pode ser atualizado

- Firmware principal com `POST /api/ota/firmware`
- Filesystem da interface web com `POST /api/ota/filesystem`

### Pagina recomendada

Use a pagina:

```text
/ota.html
```

Ela mostra:

- particao ativa
- tamanho atual do firmware
- espaco livre para OTA
- uso do LittleFS
- ultimo resultado de OTA

### Fluxo de uso

1. Build do firmware:

```bash
pio run
```

2. Para atualizar o firmware via web, envie:

```text
.pio/build/esp32dev/firmware.bin
```

3. Para atualizar a interface web via web, gere e envie:

```bash
pio run --target buildfs
```

Arquivo esperado:

```text
.pio/build/esp32dev/littlefs.bin
```

4. O ESP32 reinicia automaticamente apos OTA bem-sucedido.

## Particionamento Atual

O projeto usa `board_build.partitions = min_spiffs.csv`.

Mapa relevante:

```text
nvs      0x5000
otadata  0x2000
app0     0x1E0000
app1     0x1E0000
spiffs   0x20000   -> usado pelo LittleFS
```

Isso permite:

- OTA A/B do firmware entre `app0` e `app1`
- filesystem separado para a UI web

## LittleFS

Arquivos atuais em `firmware/data/`:

```text
index.html
ota.html
```

Upload por cabo:

```bash
pio run --target uploadfs
```

Upload remoto:

- use `/ota.html`
- envie `littlefs.bin`

Observacao importante:

- o boot nao autoformata mais o LittleFS em caso de falha de montagem
- isso evita apagar a interface web por engano

## Persistencia em NVS

As redes Wi-Fi salvas ficam na NVS.

Hoje o firmware armazena uma lista de redes conhecidas e tenta:

1. a configuracao padrao do `config.h`
2. o scan das redes salvas
3. fallback para AP se nada conectar

## Seguranca e Operacao

- Troque `WEB_AUTH_PASSWORD` antes de colocar em campo
- Troque a senha do AP fallback se o equipamento for sair do laboratorio
- Evite deixar credenciais fixas de Wi-Fi no `config.h` em ambiente final
- Para automacao via script, use autenticacao HTTP no cliente

## Validacao Atual

Foi validado em hardware:

- acesso web por IP e por `youobd.local`
- autenticacao protegendo a UI
- OTA de firmware trocando a particao ativa
- OTA de filesystem atualizando a UI

## Diferencas em Relacao a Documentacao Antiga

Esta versao substitui as informacoes antigas que citavam:

- `obdsim.local`
- `AP+STA` como modo principal
- `index.html.gz`
- `config.json` em LittleFS como base da configuracao

O comportamento atual do projeto esta descrito por este documento e pelo firmware em `firmware/src/web/web_server.cpp`.
