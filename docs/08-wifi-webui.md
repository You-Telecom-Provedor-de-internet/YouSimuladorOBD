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
  `- OTA online por manifest remoto
```

## Estado Atual do Firmware

- O hostname mDNS atual e `youobd.local`
- O Wi-Fi STA usa DHCP por padrao
- Se a rede STA falhar, o ESP32 sobe AP fallback
- A interface web inteira exige autenticacao
- Hostname, `manifest.json` padrao e credenciais web/OTA podem ser alterados na UI e ficam persistidos na NVS
- O ESP32 pode checar o manifest OTA periodicamente sem atualizar sozinho
- O upload OTA local por arquivo foi removido para reduzir o firmware
- O Bluetooth SPP existe no codigo, mas esta desabilitado por padrao para priorizar estabilidade do Wi-Fi/Web
- O OTA online deste repositorio e exclusivo do `YouSimuladorOBD`
- O futuro `YouAutoTester` deve usar outro `manifest.json` e outro diretório no dominio

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

Credenciais de fabrica:

```text
Usuario: admin
Senha: obd12345
```

Essas credenciais saem de `firmware/include/config.h` no primeiro boot, mas depois podem ser trocadas na pagina `/ota.html` e ficam gravadas na NVS. Para uso em campo, troque a senha padrao antes da entrega.

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
- `GET /api/device/settings`

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
- `POST /api/device/settings`
- `POST /api/ota/check`
- `POST /api/ota/online`

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
  "auth_default": true,
  "current_version": "2026.04.02.1",
  "online_only": true,
  "default_manifest_url": "https://app2.youtelecom.com.br/updates/yousimuladorobd/manifest.json",
  "build_date": "Apr  3 2026",
  "build_time": "19:28:19",
  "hostname": "youobd2.local",
  "hostname_hint": "youobd-ea6be8.local",
  "chip_model": "ESP32-D0WD-V3",
  "sketch_size": 1924528,
  "free_sketch_space": 1966080,
  "fs_total": 131072,
  "fs_used": 90112,
  "auto_check_enabled": true,
  "auto_check_hours": 12,
  "last_check_ms": 24648,
  "running_partition": "app0",
  "last_target": "idle",
  "last_ok": true,
  "last_error": "",
  "last_written": 0,
  "last_total": 0,
  "job_running": false,
  "job_stage": "idle",
  "check_ok": true,
  "check_error": "",
  "checked_manifest_url": "https://app2.youtelecom.com.br/updates/yousimuladorobd/manifest.json",
  "checked_version": "2026.04.02",
  "checked_notes": "Release OTA do YouSimuladorOBD separada por produto no ecossistema You Auto Car",
  "checked_has_firmware": true,
  "checked_has_filesystem": true,
  "update_available": false
}
```

### Exemplo - configuracao persistente

```json
GET /api/device/settings
{
  "hostname": "youobd-01",
  "hostname_hint": "youobd-cfc1c0",
  "manifest_url": "https://app2.youtelecom.com.br/updates/yousimuladorobd/manifest.json",
  "auth_user": "admin",
  "auth_default": false,
  "ota_auto_check": true,
  "ota_auto_check_hours": 12
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

- Checagem de release com `POST /api/ota/check`
- Download remoto de firmware com `POST /api/ota/online`
- Download remoto de filesystem com `POST /api/ota/online`

### Pagina recomendada

Use a pagina:

```text
/ota.html
```

Ela mostra:

- particao ativa
- versao atual do firmware
- tamanho atual do firmware
- espaco livre para OTA
- uso do LittleFS
- ultimo resultado de OTA
- configuracao persistente de hostname, credenciais e manifest OTA
- campo para `manifest.json`
- checagem de versao online
- botoes para baixar firmware ou filesystem diretamente da rede
- estado da checagem automatica do manifest
- indicador de modo `online_only`

### Fluxo online

1. Hospede um `manifest.json` e os arquivos `.bin` em um servidor HTTP/HTTPS acessivel pelo ESP32.

Padrao atual do projeto:

```text
https://app2.youtelecom.com.br/updates/yousimuladorobd/manifest.json
```

2. Estrutura esperada do manifest:

```json
{
  "version": "2026.04.03",
  "notes": "Release de exemplo para OTA online",
  "firmware": {
    "url": "https://app2.youtelecom.com.br/updates/yousimuladorobd/yousimuladorobd-firmware-2026.04.03.bin",
    "md5": "21ec36332fb68787f89b633ea499df12",
    "size": 1931696
  },
  "filesystem": {
    "url": "https://app2.youtelecom.com.br/updates/yousimuladorobd/yousimuladorobd-littlefs-2026.04.03.bin",
    "md5": "d3ad9a3b78c2ab6243133edd777b4346",
    "size": 131072
  }
}
```

3. Na pagina `/ota.html`, informe a URL do manifest e use `Verificar online`.

4. Se houver release disponivel, use `Baixar firmware` ou `Baixar filesystem`.

5. O ESP32 baixa o arquivo diretamente, grava a particao e reinicia automaticamente.

Observacao:

- o firmware atual nao aceita mais upload local por arquivo em `/ota.html`
- a reducao foi feita para recuperar folga no slot OTA e evitar novo overflow do binario

### Processo de release recomendado

1. Atualize `APP_VERSION` em `firmware/include/config.h`.
2. Gere `firmware.bin` e `littlefs.bin` no `YouSimuladorOBD`.
3. Publique os artefatos no projeto web `YouAutoCarvAPP2` em `updates/yousimuladorobd/`.
4. Gere ou atualize o `manifest.json` com a mesma versao.
5. Faça deploy do web e valide:
   - `manifest.json`
   - `firmware.bin`
   - `littlefs.bin`
6. No equipamento, use `Verificar online` para confirmar a nova release antes de iniciar a gravacao.

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
- informe o `manifest.json`
- mande o ESP32 baixar `firmware` ou `filesystem`

Observacao importante:

- o boot nao autoformata mais o LittleFS em caso de falha de montagem
- isso evita apagar a interface web por engano

## Persistencia em NVS

As redes Wi-Fi salvas ficam na NVS.

Agora tambem ficam persistidos na NVS:

- hostname mDNS do equipamento
- URL padrao do `manifest.json`
- usuario e senha da interface web/OTA
- politica de checagem automatica do manifest

Hoje o firmware armazena uma lista de redes conhecidas e tenta:

1. a configuracao padrao do `config.h`
2. o scan das redes salvas
3. fallback para AP se nada conectar

## Seguranca e Operacao

- Troque a senha padrao da interface antes de colocar em campo
- Defina um hostname unico por unidade para evitar conflito em redes com mais de um ESP32
- Troque a senha do AP fallback se o equipamento for sair do laboratorio
- Evite deixar credenciais fixas de Wi-Fi no `config.h` em ambiente final
- Defina `APP_VERSION` para controlar comparacao de release
- Se quiser preload de fabrica, defina `OTA_MANIFEST_URL` no `config.h`
- O `OTA_MANIFEST_URL` deste projeto deve apontar apenas para `updates/yousimuladorobd/`
- A checagem automatica consulta o manifest, mas nao instala update sozinha
- Para automacao via script, use autenticacao HTTP no cliente

## Validacao Atual

Foi validado em hardware:

- acesso web por IP e por `youobd.local`
- autenticacao protegendo a UI
- OTA de firmware trocando a particao ativa
- OTA de filesystem atualizando a UI
- OTA online buscando `firmware.bin` e `littlefs.bin` por manifest remoto
- OTA operando em modo online-only

## Diferencas em Relacao a Documentacao Antiga

Esta versao substitui as informacoes antigas que citavam:

- `obdsim.local`
- `AP+STA` como modo principal
- `index.html.gz`
- `config.json` em LittleFS como base da configuracao

O comportamento atual do projeto esta descrito por este documento e pelo firmware em `firmware/src/web/web_server.cpp`.
