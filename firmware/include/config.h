#pragma once
#include <cstdint>

// ════════════════════════════════════════════════════════════
//  YouSimuladorOBD — config.h
//  Todos os pinos e constantes em um único lugar.
//  Hardware: ESP32 38-pin + placa expansão com bornes.
// ════════════════════════════════════════════════════════════

// ── CAN / TWAI (usa SN65HVD230 — 3.3V nativo) ───────────────
constexpr int PIN_CAN_TX          = 4;
constexpr int PIN_CAN_RX          = 5;

// ── K-Line (ISO 9141-2 / KWP2000) via UART1 ─────────────────
constexpr int PIN_KLINE_TX        = 17;
constexpr int PIN_KLINE_RX        = 16;
constexpr int KLINE_UART_NUM      = 1;   // UART_NUM_1
constexpr uint32_t KLINE_BAUD     = 10400;

// ── Display OLED SH1107 128x128 (I2C) ───────────────────────
constexpr int PIN_OLED_SDA        = 21;
constexpr int PIN_OLED_SCL        = 22;
constexpr uint8_t OLED_I2C_ADDR   = 0x3C;

// ── Botões (pull-up interno, ativo em LOW) ───────────────────
constexpr int PIN_BTN_PREV        = 32;   // parâmetro anterior
constexpr int PIN_BTN_NEXT        = 33;   // próximo parâmetro
constexpr int PIN_BTN_UP          = 25;   // incrementa valor
constexpr int PIN_BTN_DOWN        = 26;   // decrementa valor
constexpr int PIN_BTN_SELECT      = 27;   // confirma / edita
constexpr int PIN_BTN_PROTOCOL    = 14;   // troca protocolo

// ── Encoder Rotativo KY-040 ──────────────────────────────────
constexpr int PIN_ENC_A           = 12;   // CLK
constexpr int PIN_ENC_B           = 13;   // DT
constexpr int PIN_ENC_SW          = 15;   // SW (botão)

// ── DIP Switch (somente entrada — GPIO 34–39) ────────────────
constexpr int PIN_DIP_0           = 34;   // bit 0 (LSB)
constexpr int PIN_DIP_1           = 35;   // bit 1
constexpr int PIN_DIP_2           = 36;   // bit 2 (MSB)

// ── LEDs de status ───────────────────────────────────────────
constexpr int PIN_LED_BUILTIN     = 2;    // LED azul interno
constexpr int PIN_LED_CAN         = 19;   // Verde  — CAN ativo
constexpr int PIN_LED_KLINE       = 18;   // Amarelo — K-Line ativo
constexpr int PIN_LED_TX          = 23;   // Vermelho — atividade TX

// ════════════════════════════════════════════════════════════
//  Protocolos OBD-II
// ════════════════════════════════════════════════════════════
constexpr uint8_t PROTO_CAN_11B_500K = 0;
constexpr uint8_t PROTO_CAN_11B_250K = 1;
constexpr uint8_t PROTO_CAN_29B_500K = 2;
constexpr uint8_t PROTO_CAN_29B_250K = 3;
constexpr uint8_t PROTO_ISO9141       = 4;
constexpr uint8_t PROTO_KWP_5BAUD     = 5;
constexpr uint8_t PROTO_KWP_FAST      = 6;
constexpr uint8_t PROTO_COUNT         = 7;

inline const char* protoName(uint8_t p) {
    switch (p) {
        case 0: return "CAN 11b 500k";
        case 1: return "CAN 11b 250k";
        case 2: return "CAN 29b 500k";
        case 3: return "CAN 29b 250k";
        case 4: return "ISO 9141-2";
        case 5: return "KWP 5-baud";
        case 6: return "KWP Fast";
        default: return "???";
    }
}

// ════════════════════════════════════════════════════════════
//  IDs CAN OBD-II
// ════════════════════════════════════════════════════════════
constexpr uint32_t OBD_CAN_FUNC_11B   = 0x7DF;       // requisição broadcast 11-bit
constexpr uint32_t OBD_CAN_PHYS_11B   = 0x7E0;       // requisição física ECU1 11-bit
constexpr uint32_t OBD_CAN_RESP_11B   = 0x7E8;       // resposta ECU1 11-bit
constexpr uint32_t OBD_CAN_FUNC_29B   = 0x18DB33F1;  // requisição broadcast 29-bit
constexpr uint32_t OBD_CAN_RESP_29B   = 0x18DAF110;  // resposta ECU1 29-bit

// ════════════════════════════════════════════════════════════
//  Wi-Fi / Web
// ════════════════════════════════════════════════════════════

// ── Modo AP (fallback / standalone) ──────────────────────────
constexpr char AP_SSID[]     = "OBD-Simulator";
constexpr char AP_PASSWORD[] = "obd12345";
constexpr char AP_IP[]       = "192.168.4.1";

// ── Modo STA — conecta na sua rede ───────────────────────────
// Deixe STA_SSID vazio ("") para não tentar conectar (modo AP puro)
constexpr char STA_SSID[]    = "You-Enzo-e-Eloise";
constexpr char STA_PASSWORD[]= "cd1aamss";

// IP fixo na sua rede 192.168.1.x
// Mude STA_STATIC_IP para qualquer .x livre (evite .1 = roteador)
constexpr char STA_STATIC_IP[]  = "192.168.1.150";
constexpr char STA_GATEWAY[]    = "192.168.1.1";
constexpr char STA_SUBNET[]     = "255.255.255.0";
constexpr char STA_DNS1[]       = "192.168.1.1";
constexpr char STA_DNS2[]       = "8.8.8.8";

// ── Geral ─────────────────────────────────────────────────────
constexpr char MDNS_NAME[]   = "obdsim";     // → http://obdsim.local
constexpr uint16_t WEB_PORT  = 80;
constexpr uint32_t WS_BROADCAST_MS = 500;    // intervalo broadcast WebSocket

// ── Modos Wi-Fi (prefixo CFG_ evita conflito com esp_wifi_types.h) ──
constexpr uint8_t CFG_WIFI_AP     = 0;      // só AP  (192.168.4.1)
constexpr uint8_t CFG_WIFI_STA    = 1;      // só STA (192.168.1.150)
constexpr uint8_t CFG_WIFI_AP_STA = 2;      // ambos simultâneos

// ════════════════════════════════════════════════════════════
//  VIN padrão
// ════════════════════════════════════════════════════════════
constexpr char DEFAULT_VIN[] = "YOUSIM00000000001";

// ════════════════════════════════════════════════════════════
//  UI / Botões
// ════════════════════════════════════════════════════════════
constexpr uint32_t BTN_DEBOUNCE_MS   = 50;
constexpr uint32_t BTN_LONG_PRESS_MS = 800;
