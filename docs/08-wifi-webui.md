# 08 — Wi-Fi e Interface Web de Gestão

## Visão Geral

O ESP32 expõe uma interface web via Wi-Fi para gestão completa do simulador sem necessidade de botões físicos. A interface usa WebSocket para atualização em tempo real dos parâmetros.

```
┌─────────────────────────────────────────────────────────────┐
│                   NAVEGADOR / CELULAR                       │
│         http://192.168.4.1  ou  http://obdsim.local        │
│                 (WebUI SPA — HTML/CSS/JS)                   │
└──────────────────────────┬──────────────────────────────────┘
                           │ HTTP + WebSocket (porta 80)
                           │ Wi-Fi 2.4GHz
                  ┌────────▼────────┐
                  │    ESP32        │
                  │  AsyncWebServer │
                  │  WebSocket      │
                  │  LittleFS       │
                  └─────────────────┘
```

---

## Modos Wi-Fi

### Modo 1: Access Point (AP) — Padrão / Standalone

O ESP32 cria sua própria rede Wi-Fi. Não precisa de roteador.

```
SSID:    OBD-Simulator
Senha:   obd12345
IP:      192.168.4.1
URL:     http://192.168.4.1
```

### Modo 2: Station (STA) — Conecta a rede existente

O ESP32 conecta ao roteador da oficina/laboratório. Mais conveniente para uso contínuo.

```
SSID:    <rede do usuário>
Senha:   <senha do usuário>
IP:      DHCP (exibido no OLED)
mDNS:    http://obdsim.local
```

### Modo 3: AP+STA simultâneo

Cria AP enquanto tenta conectar à rede configurada. Útil durante setup.

---

## API REST

Base URL: `http://<ip>/api`

### GET /api/status

Retorna estado completo do simulador.

```json
GET /api/status
Response 200:
{
  "protocol": "CAN_11B_500K",
  "protocol_id": 0,
  "wifi_mode": "AP",
  "params": {
    "rpm": 1500,
    "speed": 60,
    "coolant_temp": 90,
    "intake_temp": 30,
    "maf": 3.0,
    "map": 35,
    "throttle": 15,
    "ignition_advance": 10.0,
    "engine_load": 25,
    "fuel_level": 75
  },
  "dtcs": ["P0300"],
  "vin": "YOUSIM00000000001"
}
```

### POST /api/params

Atualiza um ou mais parâmetros.

```json
POST /api/params
Content-Type: application/json
Body:
{
  "rpm": 3000,
  "speed": 80,
  "throttle": 45
}
Response 200: { "ok": true }
```

### POST /api/protocol

Troca o protocolo ativo.

```json
POST /api/protocol
Body: { "protocol": 2 }   // 0–6 conforme tabela
Response 200: { "ok": true, "protocol": "CAN_29B_500K" }
```

### GET /api/dtcs

Lista DTCs ativos.

```json
Response: { "count": 1, "dtcs": ["P0300"] }
```

### POST /api/dtcs/add

Adiciona um DTC.

```json
Body: { "code": "P0420" }
Response: { "ok": true }
```

### POST /api/dtcs/remove

Remove um DTC específico sem afetar os demais.

```json
Body: { "code": "P0300" }
Response: { "ok": true }
```

### POST /api/dtcs/clear

Limpa todos os DTCs.

```json
Response: { "ok": true }
```

### POST /api/preset

Carrega um preset de cenário.

```json
Body: { "preset": "idle" }
// Presets: "off", "idle", "cruise", "fullthrottle", "overheat", "catalyst_fail"
Response: { "ok": true }
```

### GET /api/wifi

Retorna configuração Wi-Fi atual (NVS + estado da conexão).

```json
Response 200:
{
  "mode": 1,
  "ssid": "You-Enzo-e-Eloise",
  "sta_ip": "192.168.1.150",
  "gateway": "192.168.1.1",
  "connected": true,
  "current_ip": "192.168.1.150",
  "ap_ip": "192.168.4.1",
  "ap_ssid": "OBD-Simulator",
  "hostname": "obdsim.local"
}
```

### POST /api/wifi

Salva nova configuração na NVS e reinicia o ESP32 em 1.5s.

```json
Body:
{
  "mode": 1,
  "ssid": "MinhaRede",
  "password": "senha123",
  "sta_ip": "192.168.1.200",
  "gateway": "192.168.1.1"
}
Response 200: { "ok": true }
// ESP32 reinicia automaticamente 1.5s após a resposta
```

Modos: `0` = AP, `1` = STA, `2` = AP+STA.
Senha: se omitida ou vazia, mantém a senha anterior (NVS).
IP: se vazio, usa DHCP.

### POST /api/wifi/scan

Inicia varredura de redes Wi-Fi em background. Retorna imediatamente.

```json
Response 202: { "scanning": true }
```

### GET /api/wifi/scan

Retorna resultado da varredura. Retorna 202 enquanto ainda está buscando.

```json
// Ainda buscando:
Response 202: { "scanning": true }

// Concluído:
Response 200:
[
  { "ssid": "MinhaRede",   "rssi": -45, "secure": true,  "channel": 6  },
  { "ssid": "VizinhoRede", "rssi": -72, "secure": true,  "channel": 11 },
  { "ssid": "RedeAberta",  "rssi": -80, "secure": false, "channel": 1  }
]
```

RSSI (Received Signal Strength Indicator):
- >= -50 dBm → Excelente
- >= -60 dBm → Bom
- >= -70 dBm → Regular
- <  -70 dBm → Fraco

### POST /api/reboot

Reinicia o ESP32 em 800ms sem alterar configurações.

```json
Response 200: { "ok": true }
```

### GET /api/profiles

Lista todos os 22 perfis de veículo disponíveis.

```json
Response 200:
[
  { "id": "fiat_uno_10", "brand": "Fiat", "model": "Uno 1.0 Fire",
    "year": "2004-2013", "protocol": 4 },
  ...
]
```

### POST /api/profile

Aplica um perfil de veículo (ajusta todos os 12 parâmetros + protocolo + VIN).

```json
Body: { "id": "fiat_uno_10" }
Response 200: { "ok": true, "brand": "Fiat", "model": "Uno 1.0 Fire" }
```

---

## WebSocket — Atualização em Tempo Real

Endpoint: `ws://<ip>/ws`

### Mensagens Server → Client

O ESP32 envia estado completo a cada 500ms:

```json
{
  "protocol": "CAN_11B_500K",
  "protocol_id": 0,
  "rpm": 1500,
  "speed": 60,
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
  "dtcs": ["P0300"],
  "vin": "YOUSIM00000000001",
  "profile_id": ""
}
```

### Mensagens Client → Server

```json
// Atualizar parâmetro (qualquer chave do estado)
{ "type": "set", "key": "rpm", "value": 2500 }
{ "type": "set", "key": "battery_voltage", "value": 13.8 }
{ "type": "set", "key": "stft", "value": -3.1 }

// Trocar protocolo
{ "type": "protocol", "id": 1 }

// Carregar preset
{ "type": "preset", "name": "cruise" }
// Presets: "off", "idle", "cruise", "fullthrottle", "overheat", "catalyst_fail"

// Aplicar perfil de veículo
{ "type": "profile", "id": "fiat_palio_14" }
```

---

## Interface Web (SPA)

Arquivo único `index.html` servido pelo LittleFS (SPIFFS).

### Layout da Página

```
┌─────────────────────────────────────────────────────────────┐
│  OBD Simulator  |  Protocolo: [CAN 11b 500k ▼]  |  ● LIVE  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │    RPM        │  │  Velocidade  │  │  Temp. Motor │     │
│  │  [────●────]  │  │  [──●──────] │  │  [──────●──] │     │
│  │   1500 rpm    │  │   60 km/h    │  │    90 °C     │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │  Temp. Ar    │  │     MAF      │  │     MAP      │     │
│  │  [●─────────] │  │  [─●───────] │  │  [─●───────] │     │
│  │    30 °C     │  │   3.0 g/s    │  │   35 kPa     │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │  Acelerador  │  │   Avanço     │  │  Carga Motor │     │
│  │  [─●─────── ] │  │  [───●─────] │  │  [──●──────] │     │
│  │    15 %      │  │   10.0 °     │  │    25 %      │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                             │
│  ┌──────────────┐  ┌─────────────────────────────────┐    │
│  │ Combustível  │  │  DTCs Ativos                    │    │
│  │  [──────●──] │  │  [P0300 ×]  [+ Adicionar]       │    │
│  │    75 %      │  │  [Limpar Todos]                  │    │
│  └──────────────┘  └─────────────────────────────────┘    │
│                                                             │
│  Presets: [Deslig.]  [Idle]  [Cruzeiro]  [Fundo]  [Falha] │
└─────────────────────────────────────────────────────────────┘
```

---

## Bibliotecas Arduino/ESP32 para Web

```ini
; platformio.ini — dependências adicionais para Wi-Fi + Web

lib_deps =
    ; Web server assíncrono (não bloqueia tasks do protocolo)
    esphome/ESPAsyncWebServer-esphome @ ^3.3.0
    ; Suporte a WebSocket (incluso no AsyncWebServer)
    ; mDNS — http://obdsim.local
    ; (incluso no ESP-IDF / arduino-esp32, sem dep. extra)
    ; JSON — para API REST e WebSocket
    bblanchon/ArduinoJson @ ^7.3.0
```

### Por que ESPAsyncWebServer?

- Não bloqueia o loop principal — usa tasks FreeRTOS internamente
- Suporte nativo a WebSocket
- Serve arquivos direto do LittleFS (sem carregar na RAM)
- Suporte a gzip — `index.html.gz` reduz uso de flash

---

## Sistema de Arquivos LittleFS

```
LittleFS (partição 1MB na flash):
/data/
├── index.html.gz    (~15KB comprimido — toda a UI)
├── config.json      (configurações Wi-Fi e protocolo padrão)
└── favicon.ico
```

### Upload dos arquivos web

```bash
# PlatformIO: coloca arquivos em /data/ e executa:
pio run --target uploadfs

# Ou via Arduino IDE com plugin LittleFS uploader
```

---

## Configuração NVS (Persistência)

Salvo na NVS (Non-Volatile Storage) do ESP32:

| Chave NVS | Tipo | Valor padrão |
|-----------|------|-------------|
| `wifi_ssid` | string | — |
| `wifi_pass` | string | — |
| `wifi_mode` | uint8 | 0 (AP) |
| `def_protocol` | uint8 | 0 (CAN 11b 500k) |
| `def_rpm` | uint16 | 800 |
| `def_speed` | uint8 | 0 |
| `def_coolant` | int16 | 90 |
| `ap_ssid` | string | OBD-Simulator |
| `ap_pass` | string | obd12345 |

---

## Integração com Firmware — Estrutura de Arquivos Atualizada

```
firmware/src/
├── main.cpp
├── protocols/      (igual ao doc anterior)
├── obd/            (igual)
├── simulation/     (igual)
├── ui/             (botões + OLED)
└── web/                            ← NOVO
    ├── web_server.cpp/h            # Inicialização AsyncWebServer
    ├── api_handlers.cpp/h          # Handlers das rotas REST
    ├── ws_handler.cpp/h            # Handler WebSocket + broadcast
    └── wifi_manager.cpp/h          # AP/STA, reconexão, mDNS

firmware/data/                      ← NOVO (LittleFS)
    ├── index.html                  # UI completa (ou .gz)
    └── config.json
```

---

## Fluxo de Inicialização Wi-Fi

```
Boot ESP32
    │
    ▼
Lê NVS: wifi_mode, ssid, pass
    │
    ├── mode = AP ──────────────────► Inicia AP "OBD-Simulator"
    │                                  IP: 192.168.4.1
    │
    ├── mode = STA ─► Conecta à rede
    │                      │
    │               ┌──────▼──────┐
    │               │  Conectou?  │
    │               └──────┬──────┘
    │                 Não  │ Sim
    │                  ▼   ▼
    │         Fallback AP+STA  Inicia mDNS
    │         (rádio STA ativo  obdsim.local
    │          para scan Wi-Fi)
    │
    └── mode = AP+STA ──► Ambos simultaneamente
    │
    ▼
Inicia AsyncWebServer na porta 80
Monta rotas REST + WebSocket /ws
Inicia task de broadcast WS (500ms)
```

---

## Tarefa WebSocket Broadcast

```cpp
// ws_handler.cpp — task FreeRTOS Core 1

void task_ws_broadcast(void* param) {
    while (true) {
        if (ws.count() > 0) {           // só se houver clientes
            StaticJsonDocument<512> doc;
            doc["type"]             = "update";
            doc["rpm"]              = sim.rpm;
            doc["speed"]            = sim.speed_kmh;
            doc["coolant_temp"]     = sim.coolant_temp_c;
            doc["intake_temp"]      = sim.intake_temp_c;
            doc["maf"]              = sim.maf_gs;
            doc["map"]              = sim.map_kpa;
            doc["throttle"]         = sim.throttle_pct;
            doc["ignition_advance"] = sim.ignition_advance;
            doc["engine_load"]      = sim.engine_load_pct;
            doc["fuel_level"]       = sim.fuel_level_pct;
            doc["obd_activity"]     = obd_activity_flag;

            String msg;
            serializeJson(doc, msg);
            ws.textAll(msg);
            obd_activity_flag = false;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

---

## Exibição de IP no OLED

Quando conectado em modo STA, o OLED exibe:

```
┌──────────────────────┐
│ Wi-Fi: SUA_REDE      │
│ IP: 192.168.1.45     │
│ obdsim.local         │
│ Proto: CAN 11b 500k  │
└──────────────────────┘
```

Em modo AP:

```
┌──────────────────────┐
│ AP: OBD-Simulator    │
│ IP: 192.168.4.1      │
│ Senha: obd12345      │
│ Proto: CAN 11b 500k  │
└──────────────────────┘
```
