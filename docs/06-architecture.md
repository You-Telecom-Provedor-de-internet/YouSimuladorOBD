# 06 — Arquitetura de Software / Firmware

## Plataforma e Framework

| Item | Escolha | Justificativa |
|------|---------|---------------|
| Plataforma | ESP-IDF v5.x ou Arduino ESP32 | ESP-IDF preferido (TWAI nativo, FreeRTOS, performance) |
| Build System | PlatformIO | Multiplataforma, gerência de deps, CI-friendly |
| Linguagem | C++ (C++17) | Performance, acesso direto a periféricos |
| RTOS | FreeRTOS (incluso no ESP-IDF) | Multitarefa cooperativa/preemptiva |

---

## Estrutura de Arquivos do Firmware

```
firmware/
├── platformio.ini
├── include/
│   ├── config.h              # Defines de pinos e constantes globais
│   ├── simulation_state.h    # Estrutura de estado dos parâmetros
│   └── obd_types.h           # Tipos OBD: PID, DTC, frame, etc.
├── src/
│   ├── main.cpp              # Entry point, init e loop principal
│   │
│   ├── protocols/
│   │   ├── protocol_base.h        # Interface abstrata de protocolo
│   │   ├── can_protocol.cpp/h     # ISO 15765-4 (CAN/TWAI)
│   │   ├── isotp_layer.cpp/h      # ISO-TP segmentação/remontagem
│   │   ├── kline_protocol.cpp/h   # Base K-Line
│   │   ├── iso9141.cpp/h          # ISO 9141-2
│   │   └── kwp2000.cpp/h          # ISO 14230-4 KWP2000
│   │
│   ├── obd/
│   │   ├── obd_dispatcher.cpp/h   # Roteia requisição para handler correto
│   │   ├── mode01_handler.cpp/h   # PIDs de dados correntes
│   │   ├── mode03_handler.cpp/h   # Leitura de DTCs
│   │   ├── mode04_handler.cpp/h   # Limpar DTCs
│   │   └── mode09_handler.cpp/h   # VIN e info do veículo
│   │
│   ├── simulation/
│   │   ├── simulation_manager.cpp/h  # Estado central dos 16 parâmetros
│   │   ├── dynamic_engine.cpp/h      # Motor de física dinâmica (6 modos)
│   │   └── dtc_manager.cpp/h         # Gerenciamento de DTCs
│   │
│   └── ui/
│       ├── display.cpp/h          # Driver OLED SSD1306
│       ├── input_handler.cpp/h    # Botões, encoder, DIP switch
│       └── menu.cpp/h             # Sistema de menus/navegação
└── test/
    └── (testes unitários)
```

---

## Tarefas FreeRTOS

```
┌─────────────────────────────────────────────────────────────┐
│                    FreeRTOS Task Map                         │
├────────────────┬──────────┬───────────┬──────────────────────┤
│ Task           │ Core     │ Prioridade│ Função               │
├────────────────┼──────────┼───────────┼──────────────────────┤
│ task_protocol  │ Core 0   │ Alta (10) │ RX/TX protocolo OBD  │
│ task_can_rx    │ Core 0   │ Alta (10) │ ISR/poll TWAI RX     │
│ task_kline     │ Core 0   │ Alta (10) │ UART K-Line handler  │
│ task_ui        │ Core 1   │ Baixa (3) │ Botões, display      │
│ task_sim       │ Core 1   │ Media (5) │ Atualiza parâmetros  │
│ dyn_engine     │ Core 1   │ Baixa (1) │ Motor dinâmico 10 Hz │
│ task_ws        │ Core 1   │ Media (3) │ Broadcast WebSocket  │
└────────────────┴──────────┴───────────┴──────────────────────┘

Comunicação entre tasks:
- xQueue_obd_request: protocol task → obd_dispatcher
- xQueue_obd_response: obd_dispatcher → protocol task
- xSemaphore_sim_state: protege SimulationState (mutex)
- xQueue_ui_events: input_handler → menu/display
```

---

## Estrutura de Estado da Simulação

```cpp
// simulation_state.h

struct SimulationState {
    // Parâmetros simulados (16 total)
    uint16_t rpm;               // 0–16000 RPM
    uint8_t  speed_kmh;         // 0–255 km/h
    int16_t  coolant_temp_c;    // -40 a +215 °C
    int16_t  intake_temp_c;     // -40 a +80 °C
    float    maf_gs;            // 0.0–655.35 g/s
    uint8_t  map_kpa;           // 0–255 kPa
    uint8_t  throttle_pct;      // 0–100 %
    float    ignition_advance;  // -64.0 a +63.5 °
    uint8_t  engine_load_pct;   // 0–100 %
    uint8_t  fuel_level_pct;    // 0–100 %
    float    battery_voltage;   // 0.0–65.5 V
    int16_t  oil_temp_c;        // -40 a +210 °C
    float    stft_pct;          // -30 a +30 %
    float    ltft_pct;          // -30 a +30 %

    // DTCs
    uint8_t  dtc_count;
    uint16_t dtcs[8];           // lista de DTCs (2 bytes cada, encoding P/C/B/U)

    // VIN
    char vin[18];               // 17 chars + null terminator

    // Estado do protocolo e simulação
    uint8_t  active_protocol;   // 0–6
    SimMode  sim_mode;          // STATIC, IDLE, URBAN, HIGHWAY, WARMUP, FAULT
};
```

---

## Fluxo de Processamento de Requisição OBD

```
[Recebe frame físico via TWAI ou UART]
            │
            ▼
    ┌───────────────┐
    │ Protocol Layer│  decodifica frame (CAN/K-Line)
    │ extrai payload│  extrai: Mode + PID + dados
    └───────┬───────┘
            │ OBDRequest { mode, pid, data[] }
            ▼
    ┌───────────────┐
    │ OBD Dispatcher│  switch(request.mode)
    │               │  mode 01 → Mode01Handler
    │               │  mode 03 → Mode03Handler
    │               │  mode 04 → Mode04Handler
    │               │  mode 09 → Mode09Handler
    └───────┬───────┘
            │ OBDResponse { data[], len }
            ▼
    ┌───────────────┐
    │ Protocol Layer│  monta frame resposta
    │               │  CAN: 0x7E8 + ISO-TP
    │               │  K-Line: header + checksum
    └───────┬───────┘
            │
            ▼
    [Transmite frame físico]
```

---

## Mode01Handler — Lógica de Resposta por PID

```cpp
// Pseudo-código do Mode01Handler

OBDResponse Mode01Handler::handle(uint8_t pid, SimulationState& state) {
    switch (pid) {
        case 0x00:  // PIDs suportados 01-20
            return buildResponse({0x18, 0x3F, 0x80, 0x01});

        case 0x04:  // Engine load
            return buildResponse({(uint8_t)(state.engine_load_pct * 255 / 100)});

        case 0x05:  // Coolant temp
            return buildResponse({(uint8_t)(state.coolant_temp_c + 40)});

        case 0x0B:  // MAP
            return buildResponse({state.map_kpa});

        case 0x0C: { // RPM
            uint16_t raw = state.rpm * 4;
            return buildResponse({(uint8_t)(raw >> 8), (uint8_t)(raw & 0xFF)});
        }

        case 0x0D:  // Speed
            return buildResponse({state.speed_kmh});

        case 0x0E: { // Ignition advance
            uint8_t a = (uint8_t)((state.ignition_advance + 64.0f) * 2.0f);
            return buildResponse({a});
        }

        case 0x0F:  // IAT
            return buildResponse({(uint8_t)(state.intake_temp_c + 40)});

        case 0x10: { // MAF
            uint16_t raw = (uint16_t)(state.maf_gs * 100.0f);
            return buildResponse({(uint8_t)(raw >> 8), (uint8_t)(raw & 0xFF)});
        }

        case 0x11:  // TPS
            return buildResponse({(uint8_t)(state.throttle_pct * 255 / 100)});

        case 0x20:  // PIDs suportados 21-40
            return buildResponse({0x00, 0x02, 0x00, 0x01});

        case 0x2F:  // Fuel level
            return buildResponse({(uint8_t)(state.fuel_level_pct * 255 / 100)});

        default:
            return buildNegativeResponse(0x12); // PID não suportado
    }
}
```

---

## Gerenciamento de Protocolo — Máquina de Estados

### Estados K-Line

```
┌──────────┐  5-baud addr   ┌─────────────┐  sync OK   ┌──────────┐
│  IDLE    ├───────────────►│ WAIT_SYNC   ├───────────►│ WAIT_KB  │
└──────────┘                └─────────────┘            └─────┬────┘
     ▲                                                        │ KB OK
     │ timeout/error                                          ▼
     │                                                  ┌──────────┐
     └──────────────────────────────────────────────────│  ACTIVE  │
                                                        │ (loop)   │
                                                        └──────────┘

Estados CAN:
┌──────────┐  frame 7DF rx  ┌──────────────┐  resp sent  ┌──────────┐
│  LISTEN  ├───────────────►│  PROCESSING  ├────────────►│  LISTEN  │
└──────────┘                └──────────────┘             └──────────┘
```

---

## platformio.ini

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf

; ou para Arduino framework:
; framework = arduino

build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DCONFIG_TWAI_ISR_IN_IRAM=1

lib_deps =
    ; display OLED
    adafruit/Adafruit SSD1306 @ ^2.5.7
    adafruit/Adafruit GFX Library @ ^1.11.9
    ; encoder
    madhephaestus/ESP32Encoder @ ^0.10.1
    ; debounce botões
    thomasfredericks/Bounce2 @ ^2.71

monitor_speed = 115200
monitor_filters = esp32_exception_decoder
```

---

## config.h — Defines Centrais

```cpp
// config.h — todos os defines em um único lugar

// === CAN / TWAI ===
#define PIN_CAN_TX          GPIO_NUM_4
#define PIN_CAN_RX          GPIO_NUM_5

// === K-LINE ===
#define PIN_KLINE_TX        17
#define PIN_KLINE_RX        16
#define KLINE_UART_NUM      UART_NUM_1
#define KLINE_BAUD          10400

// === DISPLAY OLED ===
#define PIN_OLED_SDA        21
#define PIN_OLED_SCL        22
#define OLED_I2C_ADDR       0x3C

// === BOTÕES ===
#define PIN_BTN_PREV        32
#define PIN_BTN_NEXT        33
#define PIN_BTN_UP          25
#define PIN_BTN_DOWN        26
#define PIN_BTN_SELECT      27
#define PIN_BTN_PROTOCOL    14

// === ENCODER ===
#define PIN_ENC_A           12
#define PIN_ENC_B           13
#define PIN_ENC_SW          15

// === DIP SWITCH ===
#define PIN_DIP_0           34
#define PIN_DIP_1           35
#define PIN_DIP_2           36

// === LEDS ===
#define PIN_LED_CAN         19
#define PIN_LED_KLINE       18
#define PIN_LED_TX          23

// === PROTOCOLOS ===
#define PROTO_CAN_11B_500K  0
#define PROTO_CAN_11B_250K  1
#define PROTO_CAN_29B_500K  2
#define PROTO_CAN_29B_250K  3
#define PROTO_ISO9141       4
#define PROTO_KWP_5BAUD     5
#define PROTO_KWP_FAST      6

// === OBD IDs ===
#define OBD_CAN_FUNC_ID_11B     0x7DF
#define OBD_CAN_RESP_ID_11B     0x7E8
#define OBD_CAN_FUNC_ID_29B     0x18DB33F1
#define OBD_CAN_RESP_ID_29B     0x18DAF110

// === VIN ===
#define DEFAULT_VIN         "9YSMULMS0T1234567"
```
