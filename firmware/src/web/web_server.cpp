#include "web_server.h"
#include "config.h"
#include "vehicle_profiles.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// ════════════════════════════════════════════════════════════
//  Wi-Fi + AsyncWebServer + WebSocket + REST API
// ════════════════════════════════════════════════════════════

static AsyncWebServer server(WEB_PORT);
static AsyncWebSocket ws("/ws");
static SimulationState* s_state  = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;

// ── Helpers ───────────────────────────────────────────────────

static String stateToJson() {
    JsonDocument doc;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    doc["protocol"]         = protoName(s_state->active_protocol);
    doc["protocol_id"]      = s_state->active_protocol;
    doc["rpm"]              = s_state->rpm;
    doc["speed"]            = s_state->speed_kmh;
    doc["coolant_temp"]     = s_state->coolant_temp_c;
    doc["intake_temp"]      = s_state->intake_temp_c;
    doc["maf"]              = serialized(String(s_state->maf_gs, 1));
    doc["map"]              = s_state->map_kpa;
    doc["throttle"]         = s_state->throttle_pct;
    doc["ignition_advance"] = serialized(String(s_state->ignition_adv, 1));
    doc["engine_load"]      = s_state->engine_load_pct;
    doc["fuel_level"]       = s_state->fuel_level_pct;
    doc["battery_voltage"]  = serialized(String(s_state->battery_voltage, 1));
    doc["oil_temp"]         = s_state->oil_temp_c;
    doc["stft"]             = serialized(String(s_state->stft_pct, 1));
    doc["ltft"]             = serialized(String(s_state->ltft_pct, 1));
    auto dtcArr = doc.createNestedArray("dtcs");
    for (uint8_t i = 0; i < s_state->dtc_count; i++) {
        char buf[6];
        snprintf(buf, sizeof(buf), "P%04X", s_state->dtcs[i]);
        dtcArr.add(buf);
    }
    doc["vin"]        = s_state->vin;
    doc["profile_id"] = s_state->profile_id;
    xSemaphoreGive(s_mutex);
    String out;
    serializeJson(doc, out);
    return out;
}

// ── Handlers REST ─────────────────────────────────────────────

static void handle_get_status(AsyncWebServerRequest* req) {
    req->send(200, "application/json", stateToJson());
}

static void handle_post_params(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (doc.containsKey("rpm"))              s_state->rpm              = doc["rpm"];
    if (doc.containsKey("speed"))            s_state->speed_kmh        = doc["speed"];
    if (doc.containsKey("coolant_temp"))     s_state->coolant_temp_c   = doc["coolant_temp"];
    if (doc.containsKey("intake_temp"))      s_state->intake_temp_c    = doc["intake_temp"];
    if (doc.containsKey("maf"))              s_state->maf_gs           = doc["maf"];
    if (doc.containsKey("map"))              s_state->map_kpa          = doc["map"];
    if (doc.containsKey("throttle"))         s_state->throttle_pct     = doc["throttle"];
    if (doc.containsKey("ignition_advance")) s_state->ignition_adv     = doc["ignition_advance"];
    if (doc.containsKey("engine_load"))      s_state->engine_load_pct  = doc["engine_load"];
    if (doc.containsKey("fuel_level"))       s_state->fuel_level_pct   = doc["fuel_level"];
    if (doc.containsKey("battery_voltage")) s_state->battery_voltage  = doc["battery_voltage"];
    if (doc.containsKey("oil_temp"))        s_state->oil_temp_c        = doc["oil_temp"];
    if (doc.containsKey("stft"))            s_state->stft_pct          = doc["stft"];
    if (doc.containsKey("ltft"))            s_state->ltft_pct          = doc["ltft"];
    xSemaphoreGive(s_mutex);
    req->send(200, "application/json", "{\"ok\":true}");
}

static void handle_post_protocol(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    uint8_t pid = doc["protocol"] | 0;
    if (pid >= PROTO_COUNT) { req->send(400, "application/json", "{\"error\":\"invalid protocol\"}"); return; }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state->active_protocol = pid;
    xSemaphoreGive(s_mutex);
    String resp = "{\"ok\":true,\"protocol\":\"" + String(protoName(pid)) + "\"}";
    req->send(200, "application/json", resp);
}

static void handle_get_dtcs(AsyncWebServerRequest* req) {
    JsonDocument doc;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    doc["count"] = s_state->dtc_count;
    auto arr = doc.createNestedArray("dtcs");
    for (uint8_t i = 0; i < s_state->dtc_count; i++) {
        char buf[6]; snprintf(buf, sizeof(buf), "P%04X", s_state->dtcs[i]);
        arr.add(buf);
    }
    xSemaphoreGive(s_mutex);
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void handle_add_dtc(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    const char* code = doc["code"] | "";
    if (strlen(code) < 5) { req->send(400); return; }
    uint16_t val = (uint16_t)strtol(code + 1, nullptr, 16); // Pxxx → uint16
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_state->dtc_count < 8) s_state->dtcs[s_state->dtc_count++] = val;
    xSemaphoreGive(s_mutex);
    req->send(200, "application/json", "{\"ok\":true}");
}

static void handle_clear_dtcs(AsyncWebServerRequest* req) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state->dtc_count = 0;
    memset(s_state->dtcs, 0, sizeof(s_state->dtcs));
    xSemaphoreGive(s_mutex);
    req->send(200, "application/json", "{\"ok\":true}");
}

static void handle_remove_dtc(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    const char* code = doc["code"] | "";
    if (strlen(code) < 5) { req->send(400); return; }
    uint16_t val = (uint16_t)strtol(code + 1, nullptr, 16);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (uint8_t i = 0; i < s_state->dtc_count; i++) {
        if (s_state->dtcs[i] == val) {
            // shift para preencher o buraco
            for (uint8_t j = i; j < s_state->dtc_count - 1; j++)
                s_state->dtcs[j] = s_state->dtcs[j + 1];
            s_state->dtc_count--;
            break;
        }
    }
    xSemaphoreGive(s_mutex);
    req->send(200, "application/json", "{\"ok\":true}");
}

static void handle_post_preset(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    const char* name = doc["preset"] | "idle";
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if      (!strcmp(name, "off"))           Preset::applyOff(*s_state);
    else if (!strcmp(name, "idle"))          Preset::applyIdle(*s_state);
    else if (!strcmp(name, "cruise"))        Preset::applyCruise(*s_state);
    else if (!strcmp(name, "fullthrottle"))  Preset::applyFullThrottle(*s_state);
    else if (!strcmp(name, "overheat"))      Preset::applyOverheat(*s_state);
    else if (!strcmp(name, "catalyst_fail")) Preset::applyCatalystFail(*s_state);
    xSemaphoreGive(s_mutex);
    req->send(200, "application/json", "{\"ok\":true}");
}

// ── Wi-Fi config ──────────────────────────────────────────────

// ── Wi-Fi Scan ────────────────────────────────────────────────

static volatile bool s_scan_busy   = false;
static String        s_scan_result = "[]";

static void task_wifi_scan(void*) {
    WiFi.scanDelete();
    // Scan assíncrono: não bloqueia o rádio STA conectado.
    // scanNetworks(async=true) inicia o scan em background do driver Wi-Fi;
    // scanComplete() retorna WIFI_SCAN_RUNNING (-1) até terminar.
    WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/true);

    // Poll a cada 500ms — timeout de 15s para não ficar preso para sempre
    int16_t n = WIFI_SCAN_RUNNING;
    uint32_t deadline = millis() + 15000;
    while (n == WIFI_SCAN_RUNNING && millis() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(500));
        n = WiFi.scanComplete();
    }

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i).isEmpty()) continue;
            JsonObject obj = arr.add<JsonObject>();
            obj["ssid"]    = WiFi.SSID(i);
            obj["rssi"]    = WiFi.RSSI(i);
            obj["secure"]  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            obj["channel"] = WiFi.channel(i);
        }
    }
    WiFi.scanDelete();
    serializeJson(doc, s_scan_result);
    s_scan_busy = false;
    vTaskDelete(nullptr);
}

static void handle_post_wifi_scan(AsyncWebServerRequest* req) {
    if (!s_scan_busy) {
        s_scan_busy = true;
        s_scan_result = "[]";
        xTaskCreate(task_wifi_scan, "wifi_scan", 8192, nullptr, 1, nullptr);
    }
    req->send(202, "application/json", "{\"scanning\":true}");
}

static void handle_get_wifi_scan(AsyncWebServerRequest* req) {
    if (s_scan_busy) {
        req->send(202, "application/json", "{\"scanning\":true}");
    } else {
        req->send(200, "application/json", s_scan_result);
    }
}

static void handle_get_wifi(AsyncWebServerRequest* req) {
    JsonDocument doc;
    Preferences prefs;
    prefs.begin("wifi", true);
    doc["mode"]    = prefs.getUChar ("mode",   (strlen(STA_SSID) > 0) ? CFG_WIFI_STA : CFG_WIFI_AP);
    doc["ssid"]    = prefs.getString("ssid",   STA_SSID);
    doc["sta_ip"]  = prefs.getString("sta_ip", STA_STATIC_IP);
    doc["gateway"] = prefs.getString("gw",     STA_GATEWAY);
    prefs.end();
    doc["connected"]  = (WiFi.status() == WL_CONNECTED);
    doc["current_ip"] = WiFi.localIP().toString();
    doc["ap_ip"]      = WiFi.softAPIP().toString();
    doc["ap_ssid"]    = AP_SSID;
    doc["hostname"]   = String(MDNS_NAME) + ".local";
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void handle_post_wifi(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    Preferences prefs;
    prefs.begin("wifi", false);
    if (!doc["ssid"].isNull())    prefs.putString("ssid",   doc["ssid"].as<const char*>());
    if (!doc["sta_ip"].isNull())  prefs.putString("sta_ip", doc["sta_ip"].as<const char*>());
    if (!doc["gateway"].isNull()) prefs.putString("gw",     doc["gateway"].as<const char*>());
    if (!doc["mode"].isNull())    prefs.putUChar ("mode",   doc["mode"].as<uint8_t>());
    // Só salva senha se enviada e não vazia
    const char* pass = doc["password"] | "";
    if (strlen(pass) > 0)         prefs.putString("pass",   pass);
    prefs.end();
    req->send(200, "application/json", "{\"ok\":true}");
    // Reinicia após 1.5s para dar tempo da resposta HTTP ser enviada
    xTaskCreate([](void*){ vTaskDelay(pdMS_TO_TICKS(1500)); ESP.restart(); vTaskDelete(nullptr); },
                "t_reboot", 2048, nullptr, 1, nullptr);
}

static void handle_post_reboot(AsyncWebServerRequest* req) {
    req->send(200, "application/json", "{\"ok\":true}");
    xTaskCreate([](void*){ vTaskDelay(pdMS_TO_TICKS(800)); ESP.restart(); vTaskDelete(nullptr); },
                "t_reboot2", 2048, nullptr, 1, nullptr);
}

static void handle_get_profiles(AsyncWebServerRequest* req) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (uint8_t i = 0; i < VEHICLE_PROFILE_COUNT; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"]       = VEHICLE_PROFILES[i].id;
        obj["brand"]    = VEHICLE_PROFILES[i].brand;
        obj["model"]    = VEHICLE_PROFILES[i].model;
        obj["year"]     = VEHICLE_PROFILES[i].year;
        obj["protocol"] = VEHICLE_PROFILES[i].protocol;
    }
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void handle_post_profile(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    const char* id = doc["id"] | "";
    const VehicleProfile* p = findProfile(id);
    if (!p) { req->send(404, "application/json", "{\"error\":\"perfil nao encontrado\"}"); return; }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    applyVehicleProfile(*s_state, *p);
    xSemaphoreGive(s_mutex);
    String resp = "{\"ok\":true,\"brand\":\"" + String(p->brand) +
                  "\",\"model\":\"" + String(p->model) + "\"}";
    req->send(200, "application/json", resp);
}

// ── WebSocket ─────────────────────────────────────────────────

static void on_ws_event(AsyncWebSocket*, AsyncWebSocketClient* client,
                        AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_DATA) {
        JsonDocument doc;
        if (deserializeJson(doc, data, len)) return;
        const char* t = doc["type"] | "";

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        if (!strcmp(t, "set")) {
            const char* key = doc["key"] | "";
            float val = doc["value"] | 0.0f;
            if      (!strcmp(key, "rpm"))              s_state->rpm             = (uint16_t)val;
            else if (!strcmp(key, "speed"))            s_state->speed_kmh       = (uint8_t)val;
            else if (!strcmp(key, "coolant_temp"))     s_state->coolant_temp_c  = (int16_t)val;
            else if (!strcmp(key, "intake_temp"))      s_state->intake_temp_c   = (int16_t)val;
            else if (!strcmp(key, "maf"))              s_state->maf_gs          = val;
            else if (!strcmp(key, "map"))              s_state->map_kpa         = (uint8_t)val;
            else if (!strcmp(key, "throttle"))         s_state->throttle_pct    = (uint8_t)val;
            else if (!strcmp(key, "ignition_advance")) s_state->ignition_adv    = val;
            else if (!strcmp(key, "engine_load"))      s_state->engine_load_pct = (uint8_t)val;
            else if (!strcmp(key, "fuel_level"))       s_state->fuel_level_pct  = (uint8_t)val;
            else if (!strcmp(key, "battery_voltage"))  s_state->battery_voltage = val;
            else if (!strcmp(key, "oil_temp"))         s_state->oil_temp_c      = (int16_t)val;
            else if (!strcmp(key, "stft"))             s_state->stft_pct        = val;
            else if (!strcmp(key, "ltft"))             s_state->ltft_pct        = val;
        } else if (!strcmp(t, "protocol")) {
            uint8_t pid = doc["id"] | 0;
            if (pid < PROTO_COUNT) s_state->active_protocol = pid;
        } else if (!strcmp(t, "preset")) {
            const char* name = doc["name"] | "idle";
            s_state->profile_id[0] = '\0';  // preset limpa perfil ativo
            if      (!strcmp(name, "off"))           Preset::applyOff(*s_state);
            else if (!strcmp(name, "idle"))          Preset::applyIdle(*s_state);
            else if (!strcmp(name, "cruise"))        Preset::applyCruise(*s_state);
            else if (!strcmp(name, "fullthrottle"))  Preset::applyFullThrottle(*s_state);
            else if (!strcmp(name, "overheat"))      Preset::applyOverheat(*s_state);
            else if (!strcmp(name, "catalyst_fail")) Preset::applyCatalystFail(*s_state);
        } else if (!strcmp(t, "profile")) {
            const char* pid = doc["id"] | "";
            const VehicleProfile* p = findProfile(pid);
            if (p) applyVehicleProfile(*s_state, *p);
        }
        xSemaphoreGive(s_mutex);
    }
}

// ── Task de broadcast WebSocket (500ms) ───────────────────────

static void task_ws_broadcast(void*) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(WS_BROADCAST_MS));
        if (ws.count() > 0) {
            ws.textAll(stateToJson());
        }
        ws.cleanupClients(); // remove clientes desconectados
    }
}

// ── Wi-Fi init ────────────────────────────────────────────────
// Prioridade de configuração:
//   1. NVS (salvo pela Web UI via POST /api/wifi)
//   2. config.h (defaults em tempo de compilação)

static void wifi_start() {
    Preferences prefs;
    prefs.begin("wifi", true);
    // Lê NVS — se vazio, cai para os defaults do config.h
    String ssid   = prefs.getString("ssid",   STA_SSID);
    String pass   = prefs.getString("pass",   STA_PASSWORD);
    String sta_ip = prefs.getString("sta_ip", STA_STATIC_IP);
    String gw     = prefs.getString("gw",     STA_GATEWAY);
    String sn     = prefs.getString("sn",     STA_SUBNET);
    uint8_t mode  = prefs.getUChar ("mode",
                        (strlen(STA_SSID) > 0) ? CFG_WIFI_STA : CFG_WIFI_AP);
    prefs.end();

    bool sta_ok = false;

    if ((mode == CFG_WIFI_STA || mode == CFG_WIFI_AP_STA) && ssid.length() > 0) {
        WiFi.mode(mode == CFG_WIFI_AP_STA ? WIFI_AP_STA : WIFI_STA);

        // Configura IP fixo antes de conectar
        IPAddress ip, gateway, subnet, dns1, dns2;
        if (ip.fromString(sta_ip) && gateway.fromString(gw) && subnet.fromString(sn)) {
            dns1.fromString(STA_DNS1);
            dns2.fromString(STA_DNS2);
            WiFi.config(ip, gateway, subnet, dns1, dns2);
            Serial.printf("[WiFi] IP fixo configurado: %s\n", sta_ip.c_str());
        }

        WiFi.begin(ssid.c_str(), pass.c_str());
        Serial.printf("[WiFi] Conectando a '%s'", ssid.c_str());

        uint8_t tries = 0;
        while (WiFi.status() != WL_CONNECTED && tries++ < 30) {
            vTaskDelay(pdMS_TO_TICKS(500));
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            sta_ok = true;
            Serial.printf("[WiFi] Conectado! IP: http://%s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[WiFi] mDNS:         http://%s.local\n", MDNS_NAME);
        } else {
            Serial.println("[WiFi] Falha na conexao STA — iniciando AP...");
        }
    }

    // AP: sempre sobe se modo AP, AP+STA, ou se STA falhou
    if (mode == CFG_WIFI_AP || mode == CFG_WIFI_AP_STA || !sta_ok) {
        // Mantém AP+STA (não AP puro) para que o rádio STA fique ativo e o
        // scan de redes funcione mesmo sem conexão STA estabelecida.
        if (mode == CFG_WIFI_STA && !sta_ok) WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(AP_SSID, AP_PASSWORD);
        Serial.printf("[WiFi] AP: '%s'  senha: '%s'  IP: http://%s\n",
                      AP_SSID, AP_PASSWORD, WiFi.softAPIP().toString().c_str());
    }

    if (MDNS.begin(MDNS_NAME)) {
        MDNS.addService("http", "tcp", WEB_PORT);
        Serial.printf("[mDNS] http://%s.local registrado\n", MDNS_NAME);
    }
}

// ── API pública ───────────────────────────────────────────────

void web_init(SimulationState* state, SemaphoreHandle_t mutex) {
    s_state = state;
    s_mutex = mutex;

    wifi_start();

    // LittleFS — serve index.html da flash
    if (!LittleFS.begin(true)) {
        Serial.println("[LittleFS] Mount falhou!");
    }

    // WebSocket
    ws.onEvent(on_ws_event);
    server.addHandler(&ws);

    // REST API
    server.on("/api/status",   HTTP_GET,  handle_get_status);
    server.on("/api/dtcs",     HTTP_GET,  handle_get_dtcs);
    server.on("/api/dtcs/clear", HTTP_POST,
        [](AsyncWebServerRequest* r){ handle_clear_dtcs(r); });

    server.on("/api/params",   HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_params);
    server.on("/api/protocol", HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_protocol);
    server.on("/api/dtcs/add", HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_add_dtc);
    server.on("/api/dtcs/remove", HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_remove_dtc);
    server.on("/api/preset",   HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_preset);
    server.on("/api/profiles", HTTP_GET,  handle_get_profiles);
    server.on("/api/profile",  HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_profile);
    server.on("/api/wifi",      HTTP_GET,  handle_get_wifi);
    server.on("/api/wifi",      HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_wifi);
    server.on("/api/wifi/scan", HTTP_POST,
        [](AsyncWebServerRequest* r){ handle_post_wifi_scan(r); });
    server.on("/api/wifi/scan", HTTP_GET,  handle_get_wifi_scan);
    server.on("/api/reboot",   HTTP_POST,
        [](AsyncWebServerRequest* r){ handle_post_reboot(r); });

    // Serve arquivos do LittleFS (UI web)
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // CORS para desenvolvimento local
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    server.begin();
    Serial.printf("[WebServer] Porta %d iniciada\n", WEB_PORT);

    // Task broadcast WebSocket — Core 1
    xTaskCreatePinnedToCore(task_ws_broadcast, "task_ws", 4096, nullptr, 3, nullptr, 1);
}
