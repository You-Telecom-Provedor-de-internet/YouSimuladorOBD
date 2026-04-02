#include "web_server.h"
#include "config.h"
#include "vehicle_profiles.h"
#include "dtc_catalog.h"
#include "elm327_bt.h"
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

// OBD-II DTC encoding: bits 15-14 = system (00=P 01=C 10=B 11=U)
// P0300 → 0x0300, B0001 → 0x8001, C0035 → 0x4035, U0100 → 0xC100
static uint16_t dtcStrToVal(const char* code) {
    uint16_t val = (uint16_t)strtol(code + 1, nullptr, 16);
    switch (code[0]) {
        case 'C': case 'c': val |= 0x4000; break;
        case 'B': case 'b': val |= 0x8000; break;
        case 'U': case 'u': val |= 0xC000; break;
        default: break; // P = 0x0000
    }
    return val;
}

static void dtcValToStr(uint16_t val, char* buf, size_t buflen) {
    static const char PFX[] = "PCBU";
    snprintf(buf, buflen, "%c%04X", PFX[(val >> 14) & 0x03], val & 0x3FFF);
}

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
        dtcValToStr(s_state->dtcs[i], buf, sizeof(buf));
        dtcArr.add(buf);
    }
    doc["vin"]             = s_state->vin;
    doc["profile_id"]      = s_state->profile_id;
    doc["sim_mode"]        = (uint8_t)s_state->sim_mode;
    doc["active_scenario"] = s_state->active_scenario;
    doc["bt_connected"]    = elm327_bt_connected();
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
        char buf[6]; dtcValToStr(s_state->dtcs[i], buf, sizeof(buf));
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
    uint16_t val = dtcStrToVal(code);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_state->dtc_count < 16) s_state->dtcs[s_state->dtc_count++] = val;
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
    uint16_t val = dtcStrToVal(code);
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

static void handle_post_mode(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    uint8_t m = doc["mode"] | 0;
    if (m >= SIM_MODE_COUNT) { req->send(400, "application/json", "{\"error\":\"invalid mode\"}"); return; }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state->sim_mode = (SimMode)m;
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

// ── Wi-Fi Multi-Network ──────────────────────────────────────
// Armazena até MAX_WIFI_NETS redes na NVS.
// No boot, faz scan e conecta na primeira rede conhecida com
// melhor sinal. Se nenhuma encontrada, sobe AP como fallback.

struct NetConfig {
    String ssid, pass, ip, gw;
};
static NetConfig  s_nets[MAX_WIFI_NETS];
static uint8_t    s_net_count = 0;
static String     s_connected_ssid;

static void load_wifi_nets() {
    Preferences prefs;
    prefs.begin("wifi", true);
    s_net_count = prefs.getUChar("n", 0);
    if (s_net_count > MAX_WIFI_NETS) s_net_count = MAX_WIFI_NETS;

    // Migração: formato antigo (chave "ssid") → novo multi-rede
    if (s_net_count == 0 && prefs.isKey("ssid")) {
        String old_ssid = prefs.getString("ssid", "");
        if (old_ssid.length() > 0) {
            s_nets[0].ssid = old_ssid;
            s_nets[0].pass = prefs.getString("pass", "");
            s_nets[0].ip   = prefs.getString("sta_ip", "");
            s_nets[0].gw   = prefs.getString("gw", "");
            s_net_count = 1;
        }
    } else {
        for (uint8_t i = 0; i < s_net_count; i++) {
            char k[4];
            snprintf(k, 4, "s%d", i); s_nets[i].ssid = prefs.getString(k, "");
            snprintf(k, 4, "p%d", i); s_nets[i].pass = prefs.getString(k, "");
            snprintf(k, 4, "i%d", i); s_nets[i].ip   = prefs.getString(k, "");
            snprintf(k, 4, "g%d", i); s_nets[i].gw   = prefs.getString(k, "");
        }
    }
    prefs.end();

    // Seed com config.h se vazio
    if (s_net_count == 0 && strlen(STA_SSID) > 0) {
        s_nets[0] = { STA_SSID, STA_PASSWORD, STA_STATIC_IP, STA_GATEWAY };
        s_net_count = 1;
    }
}

static void save_wifi_nets() {
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.clear();  // limpa formato antigo e atual
    prefs.putUChar("n", s_net_count);
    for (uint8_t i = 0; i < s_net_count; i++) {
        char k[4];
        snprintf(k, 4, "s%d", i); prefs.putString(k, s_nets[i].ssid);
        snprintf(k, 4, "p%d", i); prefs.putString(k, s_nets[i].pass);
        snprintf(k, 4, "i%d", i); prefs.putString(k, s_nets[i].ip);
        snprintf(k, 4, "g%d", i); prefs.putString(k, s_nets[i].gw);
    }
    prefs.end();
}

// Tenta conectar a uma rede específica. Retorna true se sucesso.
static bool try_connect(const NetConfig& net) {
    Serial.printf("[WiFi] Tentando '%s'", net.ssid.c_str());

    IPAddress ip, gateway, subnet, dns1, dns2;
    if (net.ip.length() > 0 && ip.fromString(net.ip) && gateway.fromString(net.gw)) {
        subnet.fromString(STA_SUBNET);
        dns1.fromString(STA_DNS1);
        dns2.fromString(STA_DNS2);
        WiFi.config(ip, gateway, subnet, dns1, dns2);
        Serial.printf(" (IP fixo: %s)", net.ip.c_str());
    } else {
        // DHCP — limpa config estática anterior
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }

    WiFi.begin(net.ssid.c_str(), net.pass.c_str());

    uint8_t tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries++ < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        s_connected_ssid = net.ssid;
        Serial.printf("[WiFi] Conectado! SSID: '%s'  IP: http://%s\n",
                      net.ssid.c_str(), WiFi.localIP().toString().c_str());
        return true;
    }
    WiFi.disconnect(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    return false;
}

// ── Wi-Fi Scan ────────────────────────────────────────────────

static volatile bool s_scan_busy   = false;
static String        s_scan_result = "[]";

static void task_wifi_scan(void*) {
    WiFi.scanDelete();
    int16_t n = WiFi.scanNetworks(false, true);
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

// ── Wi-Fi REST handlers ──────────────────────────────────────

static void handle_get_wifi(AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["connected"]  = (WiFi.status() == WL_CONNECTED);
    doc["current_ssid"] = s_connected_ssid;
    doc["current_ip"] = WiFi.localIP().toString();
    doc["ap_ip"]      = WiFi.softAPIP().toString();
    doc["ap_ssid"]    = AP_SSID;
    doc["hostname"]   = String(MDNS_NAME) + ".local";
    auto arr = doc["networks"].to<JsonArray>();
    for (uint8_t i = 0; i < s_net_count; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["ssid"] = s_nets[i].ssid;
        obj["ip"]   = s_nets[i].ip;
        obj["gw"]   = s_nets[i].gw;
    }
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
}

// POST /api/wifi — adiciona ou atualiza rede
static void handle_post_wifi(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    const char* ssid = doc["ssid"] | "";
    if (strlen(ssid) == 0) { req->send(400, "application/json", "{\"error\":\"ssid required\"}"); return; }

    // Procura rede existente com mesmo SSID
    int idx = -1;
    for (uint8_t i = 0; i < s_net_count; i++) {
        if (s_nets[i].ssid == ssid) { idx = i; break; }
    }

    // Adiciona nova ou atualiza existente
    if (idx < 0) {
        if (s_net_count >= MAX_WIFI_NETS) {
            req->send(400, "application/json", "{\"error\":\"max networks reached\"}");
            return;
        }
        idx = s_net_count++;
    }
    s_nets[idx].ssid = ssid;
    const char* pass = doc["password"] | "";
    if (strlen(pass) > 0) s_nets[idx].pass = pass;
    if (!doc["sta_ip"].isNull())  s_nets[idx].ip = doc["sta_ip"].as<String>();
    if (!doc["gateway"].isNull()) s_nets[idx].gw = doc["gateway"].as<String>();

    save_wifi_nets();
    req->send(200, "application/json", "{\"ok\":true}");
    xTaskCreate([](void*){ vTaskDelay(pdMS_TO_TICKS(1500)); ESP.restart(); vTaskDelete(nullptr); },
                "t_reboot", 2048, nullptr, 1, nullptr);
}

// POST /api/wifi/remove — remove rede por índice
static void handle_post_wifi_remove(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    uint8_t idx = doc["index"] | 255;
    if (idx >= s_net_count) { req->send(400, "application/json", "{\"error\":\"invalid index\"}"); return; }

    // Shift redes para preencher buraco
    for (uint8_t i = idx; i < s_net_count - 1; i++) s_nets[i] = s_nets[i + 1];
    s_net_count--;
    s_nets[s_net_count] = {};

    save_wifi_nets();
    req->send(200, "application/json", "{\"ok\":true}");
}

static void handle_post_reboot(AsyncWebServerRequest* req) {
    req->send(200, "application/json", "{\"ok\":true}");
    xTaskCreate([](void*){ vTaskDelay(pdMS_TO_TICKS(800)); ESP.restart(); vTaskDelete(nullptr); },
                "t_reboot2", 2048, nullptr, 1, nullptr);
}

// ── Cenários de Falha ─────────────────────────────────────────

// GET /api/scenarios — lista todos os cenários disponíveis
static void handle_get_scenarios(AsyncWebServerRequest* req) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (uint8_t i = 0; i < DTC_SCENARIO_COUNT; i++) {
        const DtcScenario& sc = DTC_SCENARIOS[i];
        JsonObject obj = arr.add<JsonObject>();
        obj["id"]          = sc.id;
        obj["name"]        = sc.name;
        obj["description"] = sc.description;
        obj["dtc_count"]   = sc.dtc_count;
        obj["mil_on"]      = sc.mil_on;
        JsonArray codes = obj["dtcs"].to<JsonArray>();
        for (uint8_t j = 0; j < sc.dtc_count; j++) {
            char buf[6];
            // Reutiliza dtcValToStr para formatar corretamente
            dtcValToStr(sc.dtcs[j], buf, sizeof(buf));
            codes.add(buf);
        }
    }
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
}

// POST /api/scenario — aplica cenário pelo ID
// Body: {"id": 1}  ou  {"id": 0} para limpar
static void handle_post_scenario(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    uint8_t id = doc["id"] | 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool ok = false;
    if (id == 0) {
        Preset::clearScenario(*s_state);
        ok = true;
    } else {
        ok = Preset::applyScenario(*s_state, id);
    }
    xSemaphoreGive(s_mutex);
    if (!ok) {
        req->send(404, "application/json", "{\"error\":\"scenario not found\"}");
        return;
    }
    const DtcScenario* sc = findScenario(id);
    String resp = "{\"ok\":true";
    if (sc) resp += ",\"name\":\"" + String(sc->name) + "\"";
    resp += "}";
    req->send(200, "application/json", resp);
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
        } else if (!strcmp(t, "scenario")) {
            uint8_t sid = doc["id"] | 0;
            if (sid == 0) Preset::clearScenario(*s_state);
            else          Preset::applyScenario(*s_state, sid);
        } else if (!strcmp(t, "profile")) {
            const char* pid = doc["id"] | "";
            const VehicleProfile* p = findProfile(pid);
            if (p) applyVehicleProfile(*s_state, *p);
        } else if (!strcmp(t, "mode")) {
            uint8_t m = doc["id"] | 0;
            if (m < SIM_MODE_COUNT) s_state->sim_mode = (SimMode)m;
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

// ── Wi-Fi init (multi-network) ───────────────────────────────
// 1. Carrega redes salvas da NVS (até MAX_WIFI_NETS)
// 2. Faz scan rápido (~3s) para ver quais estão no ar
// 3. Tenta conectar na melhor rede conhecida (maior RSSI)
// 4. Se falhou, tenta as demais em ordem
// 5. Se nenhuma conectou, sobe AP como fallback

static void wifi_start() {
    load_wifi_nets();

    Serial.printf("[WiFi] %d rede(s) salva(s)\n", s_net_count);
    for (uint8_t i = 0; i < s_net_count; i++)
        Serial.printf("[WiFi]   %d: '%s' %s\n", i, s_nets[i].ssid.c_str(),
                      s_nets[i].ip.length() > 0 ? s_nets[i].ip.c_str() : "(DHCP)");

    bool sta_ok = false;

    if (s_net_count > 0) {
        WiFi.mode(WIFI_STA);

        // Scan rápido para ver quais redes estão no ar
        Serial.println("[WiFi] Escaneando redes...");
        int16_t n = WiFi.scanNetworks(false, true);
        Serial.printf("[WiFi] %d redes encontradas\n", n);

        // Ordena matches por RSSI (melhor sinal primeiro)
        struct Match { uint8_t net_idx; int32_t rssi; };
        Match matches[MAX_WIFI_NETS];
        uint8_t match_count = 0;

        for (int s = 0; s < n && match_count < MAX_WIFI_NETS; s++) {
            for (uint8_t i = 0; i < s_net_count; i++) {
                if (WiFi.SSID(s) == s_nets[i].ssid) {
                    // Evita duplicata (mesmo SSID em dois APs)
                    bool dup = false;
                    for (uint8_t m = 0; m < match_count; m++)
                        if (matches[m].net_idx == i) { dup = true; break; }
                    if (!dup) {
                        matches[match_count++] = { i, WiFi.RSSI(s) };
                    }
                }
            }
        }
        WiFi.scanDelete();

        // Ordena por RSSI (selection sort simples, max 4 itens)
        for (uint8_t i = 0; i < match_count; i++)
            for (uint8_t j = i + 1; j < match_count; j++)
                if (matches[j].rssi > matches[i].rssi) {
                    Match tmp = matches[i]; matches[i] = matches[j]; matches[j] = tmp;
                }

        // Tenta conectar (melhor sinal primeiro)
        for (uint8_t m = 0; m < match_count && !sta_ok; m++) {
            sta_ok = try_connect(s_nets[matches[m].net_idx]);
        }

        // Se scan não encontrou nenhuma rede salva, tenta cada uma cegamente
        if (!sta_ok && match_count == 0) {
            Serial.println("[WiFi] Nenhuma rede conhecida no scan, tentando cada uma...");
            for (uint8_t i = 0; i < s_net_count && !sta_ok; i++) {
                sta_ok = try_connect(s_nets[i]);
            }
        }
    }

    // Se conectou, salva as redes (para gravar seed do config.h no NVS)
    if (sta_ok && s_net_count > 0) {
        save_wifi_nets();
    }

    // Fallback: AP sempre sobe se STA falhou
    if (!sta_ok) {
        Serial.println("[WiFi] Nenhuma rede conectou — iniciando AP...");
        WiFi.disconnect(true);
        vTaskDelay(pdMS_TO_TICKS(200));
        WiFi.mode(WIFI_AP_STA);
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
    server.on("/api/mode",     HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_mode);
    server.on("/api/profiles",  HTTP_GET,  handle_get_profiles);
    server.on("/api/profile",   HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_profile);
    server.on("/api/scenarios", HTTP_GET,  handle_get_scenarios);
    server.on("/api/scenario",  HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_scenario);
    // ⚠️ Registrar /api/wifi/scan ANTES de /api/wifi para evitar prefix-match
    server.on("/api/wifi/scan", HTTP_POST,
        [](AsyncWebServerRequest* r){ handle_post_wifi_scan(r); });
    server.on("/api/wifi/scan", HTTP_GET,  handle_get_wifi_scan);
    server.on("/api/wifi",      HTTP_GET,  handle_get_wifi);
    server.on("/api/wifi/remove", HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_wifi_remove);
    server.on("/api/wifi",      HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_wifi);
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
