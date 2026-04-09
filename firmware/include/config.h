#pragma once

#include <cstdint>

// ============================================================================
// YouSimuladorOBD - config.h
// Todos os pinos e constantes centrais do firmware em um unico lugar.
// Hardware: ESP32 38-pin + placa expansao com bornes.
// ============================================================================

// -- CAN / TWAI (usa SN65HVD230 - 3.3V nativo) -------------------------------
constexpr int PIN_CAN_TX = 4;
constexpr int PIN_CAN_RX = 5;

// -- K-Line (ISO 9141-2 / KWP2000) via UART1 ---------------------------------
constexpr int PIN_KLINE_TX = 17;
constexpr int PIN_KLINE_RX = 16;
constexpr int KLINE_UART_NUM = 1;
constexpr uint32_t KLINE_BAUD = 10400;

// -- Display OLED SH1107 128x128 (I2C) ---------------------------------------
constexpr int PIN_OLED_SDA = 21;
constexpr int PIN_OLED_SCL = 22;
constexpr uint8_t OLED_I2C_ADDR = 0x3C;

// -- Botoes (pull-up interno, ativo em LOW) ----------------------------------
constexpr int PIN_BTN_PREV = 32;
constexpr int PIN_BTN_NEXT = 33;
constexpr int PIN_BTN_UP = 25;
constexpr int PIN_BTN_DOWN = 26;
constexpr int PIN_BTN_SELECT = 27;
constexpr int PIN_BTN_PROTOCOL = 18;

// -- Encoder rotativo KY-040 -------------------------------------------------
constexpr int PIN_ENC_A = 14;
constexpr int PIN_ENC_B = 13;
constexpr int PIN_ENC_SW = 19;

// -- DIP Switch (somente entrada - GPIO 34-39) -------------------------------
constexpr int PIN_DIP_0 = 34;
constexpr int PIN_DIP_1 = 35;
constexpr int PIN_DIP_2 = 36;

// -- LEDs de status ----------------------------------------------------------
constexpr int PIN_LED_BUILTIN = 2;
constexpr int PIN_LED_TX = 23;

// ============================================================================
// Protocolos OBD-II
// ============================================================================
constexpr uint8_t PROTO_CAN_11B_500K = 0;
constexpr uint8_t PROTO_CAN_11B_250K = 1;
constexpr uint8_t PROTO_CAN_29B_500K = 2;
constexpr uint8_t PROTO_CAN_29B_250K = 3;
constexpr uint8_t PROTO_ISO9141 = 4;
constexpr uint8_t PROTO_KWP_5BAUD = 5;
constexpr uint8_t PROTO_KWP_FAST = 6;
constexpr uint8_t PROTO_COUNT = 7;
constexpr uint8_t PROTO_BOOT_OVERRIDE = 0xFF;

inline const char* protoName(uint8_t p) {
    switch (p) {
        case 0:
            return "CAN 11b 500k";
        case 1:
            return "CAN 11b 250k";
        case 2:
            return "CAN 29b 500k";
        case 3:
            return "CAN 29b 250k";
        case 4:
            return "ISO 9141-2";
        case 5:
            return "KWP 5-baud";
        case 6:
            return "KWP Fast";
        default:
            return "???";
    }
}

// ============================================================================
// IDs CAN OBD-II
// ============================================================================
constexpr uint32_t OBD_CAN_FUNC_11B = 0x7DF;
constexpr uint32_t OBD_CAN_PHYS_11B = 0x7E0;
constexpr uint32_t OBD_CAN_RESP_11B = 0x7E8;
constexpr uint32_t OBD_CAN_FUNC_29B = 0x18DB33F1;
constexpr uint32_t OBD_CAN_RESP_29B = 0x18DAF110;

// ============================================================================
// Wi-Fi / Web
// ============================================================================
constexpr uint8_t MAX_WIFI_NETS = 4;

// -- Modo AP (fallback / standalone) -----------------------------------------
constexpr char AP_SSID[] = "OBD-Simulator";
constexpr char AP_PASSWORD[] = "obd12345";
constexpr char AP_IP[] = "192.168.4.1";

// -- Modo STA ----------------------------------------------------------------
constexpr char STA_SSID[] = "YOU_AUTO_CAR_2.4";
constexpr char STA_PASSWORD[] = "cd1aamss";

constexpr char STA_STATIC_IP[] = "";
constexpr char STA_GATEWAY[] = "";
constexpr char STA_SUBNET[] = "";
constexpr char STA_DNS1[] = "";
constexpr char STA_DNS2[] = "";

// -- Geral -------------------------------------------------------------------
// Defaults de fabrica. Hostname e manifesto OTA podem ser sobrescritos via
// interface protegida e ficam salvos na NVS.
// As credenciais Web/API desta revisao sao fixas no firmware.
constexpr char MDNS_NAME[] = "youobd";
constexpr char APP_VERSION[] = "2026.04.09.2";
constexpr char OTA_MANIFEST_URL[] =
    "https://app2.youtelecom.com.br/updates/yousimuladorobd/manifest.json";
constexpr char WEB_AUTH_USER[] = "youobd-core";
constexpr char WEB_AUTH_PASSWORD[] = "YouOBD.RevA@2026#Core";
constexpr char API_AUTH_USER[] = "youobd-core";
constexpr char API_AUTH_PASSWORD[] = "YouOBD.RevA@2026#Core";
constexpr uint16_t WEB_PORT = 80;
constexpr uint32_t WS_BROADCAST_MS = 500;
constexpr uint32_t OTA_HTTP_TIMEOUT_MS = 15000;

// -- Modos Wi-Fi (prefixo CFG_ evita conflito com esp_wifi_types.h) ----------
constexpr uint8_t CFG_WIFI_AP = 0;
constexpr uint8_t CFG_WIFI_STA = 1;
constexpr uint8_t CFG_WIFI_AP_STA = 2;

// ============================================================================
// VIN padrao
// ============================================================================
constexpr char DEFAULT_VIN[] = "9YSMULMS0T1234567";

// ============================================================================
// UI / Botoes
// ============================================================================
constexpr uint32_t BTN_DEBOUNCE_MS = 80;
constexpr uint32_t BTN_LONG_PRESS_MS = 800;
