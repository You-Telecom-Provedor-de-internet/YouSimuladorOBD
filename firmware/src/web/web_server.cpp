#include "web_server.h"
#include "config.h"
#include "vehicle_profiles.h"
#include "elm327_bt.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <cctype>
#include <cstring>
#include "esp_wifi.h"

// ════════════════════════════════════════════════════════════
//  Wi-Fi + AsyncWebServer + WebSocket + REST API
// ════════════════════════════════════════════════════════════

static AsyncWebServer server(WEB_PORT);
static AsyncWebSocket ws("/ws");
static SimulationState* s_state  = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;
static char s_ota_last_target[16] = "idle";
static char s_ota_last_error[128] = "";
static size_t s_ota_total = 0;
static size_t s_ota_written = 0;
static bool s_ota_last_ok = true;
static volatile bool s_ota_busy = false;
static volatile bool s_ota_job_running = false;
static char s_ota_job_stage[48] = "idle";
static char s_ota_checked_manifest_url[256] = "";
static char s_ota_checked_version[32] = "";
static char s_ota_checked_notes[256] = "";
static char s_ota_last_check_error[128] = "";
static bool s_ota_last_check_ok = false;
static bool s_ota_checked_has_firmware = false;
static bool s_ota_checked_has_filesystem = false;
static bool s_ota_update_available = false;

struct OtaAsset {
    String url;
    String md5;
    size_t size = 0;
};

struct OtaManifest {
    String version;
    String notes;
    OtaAsset firmware;
    OtaAsset filesystem;
};

struct OtaRemoteJob {
    char target[16];
    char version[32];
    char url[256];
    char md5[40];
    size_t size = 0;
    int command = U_FLASH;
};

constexpr uint16_t OTA_AUTO_CHECK_HOURS_DEFAULT = 12;
constexpr uint16_t OTA_AUTO_CHECK_HOURS_MIN = 1;
constexpr uint16_t OTA_AUTO_CHECK_HOURS_MAX = 168;

struct DeviceSettings {
    char hostname[32] = "";
    char manifest_url[256] = "";
    char auth_user[32] = "";
    char auth_password[64] = "";
    bool ota_auto_check = true;
    uint16_t ota_auto_check_hours = OTA_AUTO_CHECK_HOURS_DEFAULT;
};

static DeviceSettings s_device_settings;
static char s_hostname_hint[32] = "";
static uint32_t s_ota_last_check_ms = 0;

static const char* current_hostname();
static const char* current_manifest_url();
static const char* current_auth_user();
static const char* current_auth_password();

// ── Helpers ───────────────────────────────────────────────────

template <typename T>
static T& protect(T& handler) {
    handler.setAuthentication(current_auth_user(), current_auth_password());
    return handler;
}

static void copy_text(char* dst, size_t dst_size, const char* src) {
    if (dst_size == 0) {
        return;
    }
    snprintf(dst, dst_size, "%s", src ? src : "");
}

static void copy_text(char* dst, size_t dst_size, const String& src) {
    copy_text(dst, dst_size, src.c_str());
}

static const char* current_hostname() {
    return s_device_settings.hostname[0] ? s_device_settings.hostname : MDNS_NAME;
}

static const char* current_manifest_url() {
    return s_device_settings.manifest_url[0] ? s_device_settings.manifest_url : OTA_MANIFEST_URL;
}

static const char* current_auth_user() {
    return s_device_settings.auth_user[0] ? s_device_settings.auth_user : WEB_AUTH_USER;
}

static const char* current_auth_password() {
    return s_device_settings.auth_password[0] ? s_device_settings.auth_password : WEB_AUTH_PASSWORD;
}

static bool using_default_auth() {
    return strcmp(current_auth_user(), WEB_AUTH_USER) == 0
        && strcmp(current_auth_password(), WEB_AUTH_PASSWORD) == 0;
}

static void build_hostname_hint() {
    uint64_t mac = ESP.getEfuseMac();
    snprintf(s_hostname_hint, sizeof(s_hostname_hint), "%s-%06llx",
             MDNS_NAME,
             static_cast<unsigned long long>(mac & 0xFFFFFFULL));
}

static bool is_valid_hostname(const String& value) {
    if (value.isEmpty() || value.length() > 31) {
        return false;
    }
    if (value[0] == '-' || value[value.length() - 1] == '-') {
        return false;
    }
    for (size_t i = 0; i < value.length(); i++) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (!(std::isalnum(c) || c == '-')) {
            return false;
        }
    }
    return true;
}

static bool is_valid_auth_user(const String& value) {
    if (value.isEmpty() || value.length() > 24) {
        return false;
    }
    for (size_t i = 0; i < value.length(); i++) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (!(std::isalnum(c) || c == '-' || c == '_' || c == '.')) {
            return false;
        }
    }
    return true;
}

static bool is_valid_manifest_url(const String& value) {
    return value.isEmpty()
        || value.startsWith("http://")
        || value.startsWith("https://");
}

static bool is_valid_auth_password(const String& value) {
    return value.length() >= 8 && value.length() <= 63;
}

static void apply_device_settings_defaults() {
    memset(&s_device_settings, 0, sizeof(s_device_settings));
    build_hostname_hint();
    copy_text(s_device_settings.hostname, sizeof(s_device_settings.hostname), MDNS_NAME);
    copy_text(s_device_settings.manifest_url, sizeof(s_device_settings.manifest_url), OTA_MANIFEST_URL);
    copy_text(s_device_settings.auth_user, sizeof(s_device_settings.auth_user), WEB_AUTH_USER);
    copy_text(s_device_settings.auth_password, sizeof(s_device_settings.auth_password), WEB_AUTH_PASSWORD);
    s_device_settings.ota_auto_check = true;
    s_device_settings.ota_auto_check_hours = OTA_AUTO_CHECK_HOURS_DEFAULT;
}

static void normalize_device_settings() {
    String hostname = s_device_settings.hostname;
    hostname.trim();
    hostname.toLowerCase();
    if (!is_valid_hostname(hostname)) {
        hostname = MDNS_NAME;
    }
    copy_text(s_device_settings.hostname, sizeof(s_device_settings.hostname), hostname);

    String manifest_url = s_device_settings.manifest_url;
    manifest_url.trim();
    if (!is_valid_manifest_url(manifest_url)) {
        manifest_url = OTA_MANIFEST_URL;
    }
    copy_text(s_device_settings.manifest_url, sizeof(s_device_settings.manifest_url), manifest_url);

    String auth_user = s_device_settings.auth_user;
    auth_user.trim();
    if (!is_valid_auth_user(auth_user)) {
        auth_user = WEB_AUTH_USER;
    }
    copy_text(s_device_settings.auth_user, sizeof(s_device_settings.auth_user), auth_user);

    String auth_password = s_device_settings.auth_password;
    auth_password.trim();
    if (!is_valid_auth_password(auth_password)) {
        auth_password = WEB_AUTH_PASSWORD;
    }
    copy_text(s_device_settings.auth_password, sizeof(s_device_settings.auth_password), auth_password);

    if (s_device_settings.ota_auto_check_hours < OTA_AUTO_CHECK_HOURS_MIN
        || s_device_settings.ota_auto_check_hours > OTA_AUTO_CHECK_HOURS_MAX) {
        s_device_settings.ota_auto_check_hours = OTA_AUTO_CHECK_HOURS_DEFAULT;
    }
}

static void load_device_settings() {
    apply_device_settings_defaults();

    Preferences prefs;
    prefs.begin("device", true);
    copy_text(s_device_settings.hostname, sizeof(s_device_settings.hostname), prefs.getString("host", s_device_settings.hostname));
    copy_text(s_device_settings.manifest_url, sizeof(s_device_settings.manifest_url), prefs.getString("manifest", s_device_settings.manifest_url));
    copy_text(s_device_settings.auth_user, sizeof(s_device_settings.auth_user), prefs.getString("user", s_device_settings.auth_user));
    copy_text(s_device_settings.auth_password, sizeof(s_device_settings.auth_password), prefs.getString("pass", s_device_settings.auth_password));
    s_device_settings.ota_auto_check = prefs.getBool("ota_chk", s_device_settings.ota_auto_check);
    s_device_settings.ota_auto_check_hours = prefs.getUShort("ota_hrs", s_device_settings.ota_auto_check_hours);
    prefs.end();

    normalize_device_settings();
}

static void save_device_settings() {
    Preferences prefs;
    prefs.begin("device", false);
    prefs.putString("host", current_hostname());
    prefs.putString("manifest", current_manifest_url());
    prefs.putString("user", current_auth_user());
    prefs.putString("pass", current_auth_password());
    prefs.putBool("ota_chk", s_device_settings.ota_auto_check);
    prefs.putUShort("ota_hrs", s_device_settings.ota_auto_check_hours);
    prefs.end();
}

static void ota_set_stage(const char* stage) {
    copy_text(s_ota_job_stage, sizeof(s_ota_job_stage), stage);
}

static void ota_reset_state(const char* target) {
    s_ota_written = 0;
    s_ota_total = 0;
    s_ota_last_ok = false;
    snprintf(s_ota_last_target, sizeof(s_ota_last_target), "%s", target);
    s_ota_last_error[0] = '\0';
}

static void ota_set_error_text(const char* stage, const char* detail) {
    snprintf(s_ota_last_error, sizeof(s_ota_last_error), "%s: %s",
             stage, detail ? detail : "erro desconhecido");
    s_ota_last_ok = false;
    ota_set_stage("falha");
    Serial.printf("[OTA] %s\n", s_ota_last_error);
}

static void ota_set_error(const char* stage) {
    const char* err = Update.errorString();
    ota_set_error_text(stage, err ? err : "erro desconhecido");
}

static void ota_finish_ok(size_t total) {
    s_ota_total = total;
    s_ota_last_ok = true;
    s_ota_last_error[0] = '\0';
    ota_set_stage("pronto");
    Serial.printf("[OTA] %s concluido (%u bytes)\n", s_ota_last_target, (unsigned)s_ota_total);
}

static void schedule_restart(uint32_t delay_ms = 1200) {
    xTaskCreate([](void* param) {
        uint32_t delay = (uint32_t)(uintptr_t)param;
        vTaskDelay(pdMS_TO_TICKS(delay));
        ESP.restart();
        vTaskDelete(nullptr);
    }, "t_restart", 3072, (void*)(uintptr_t)delay_ms, 1, nullptr);
}

static int ota_next_version_part(const char*& text) {
    int value = 0;
    while (*text && (*text < '0' || *text > '9')) {
        if (*text == '.') {
            text++;
            break;
        }
        text++;
    }
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (*text - '0');
        text++;
    }
    if (*text == '.') {
        text++;
    }
    return value;
}

static int ota_compare_versions(const char* current, const char* remote) {
    const char* lhs = current ? current : "";
    const char* rhs = remote ? remote : "";
    const char* lhs_scan = lhs;
    const char* rhs_scan = rhs;
    while (*lhs_scan || *rhs_scan) {
        int left_part = ota_next_version_part(lhs_scan);
        int right_part = ota_next_version_part(rhs_scan);
        if (left_part != right_part) {
            return left_part < right_part ? -1 : 1;
        }
    }
    return strcmp(lhs, rhs);
}

static void ota_clear_manifest_state(const char* manifest_url = nullptr) {
    s_ota_last_check_ok = false;
    copy_text(s_ota_checked_manifest_url, sizeof(s_ota_checked_manifest_url), manifest_url);
    s_ota_checked_version[0] = '\0';
    s_ota_checked_notes[0] = '\0';
    s_ota_last_check_error[0] = '\0';
    s_ota_checked_has_firmware = false;
    s_ota_checked_has_filesystem = false;
    s_ota_update_available = false;
}

static void ota_store_manifest_state(const char* manifest_url, const OtaManifest& manifest, bool ok,
                                     const char* error_message = nullptr) {
    s_ota_last_check_ms = millis();
    s_ota_last_check_ok = ok;
    copy_text(s_ota_checked_manifest_url, sizeof(s_ota_checked_manifest_url), manifest_url);
    if (ok) {
        copy_text(s_ota_checked_version, sizeof(s_ota_checked_version), manifest.version);
        copy_text(s_ota_checked_notes, sizeof(s_ota_checked_notes), manifest.notes);
        s_ota_checked_has_firmware = !manifest.firmware.url.isEmpty();
        s_ota_checked_has_filesystem = !manifest.filesystem.url.isEmpty();
        s_ota_update_available = ota_compare_versions(APP_VERSION, manifest.version.c_str()) < 0;
        s_ota_last_check_error[0] = '\0';
    } else {
        s_ota_checked_version[0] = '\0';
        s_ota_checked_notes[0] = '\0';
        s_ota_checked_has_firmware = false;
        s_ota_checked_has_filesystem = false;
        s_ota_update_available = false;
        copy_text(s_ota_last_check_error, sizeof(s_ota_last_check_error), error_message);
    }
}

static bool ota_http_begin(HTTPClient& http, WiFiClient& plain, WiFiClientSecure& secure,
                           const String& url) {
    if (url.startsWith("https://")) {
        secure.setInsecure();
        return http.begin(secure, url);
    }
    return http.begin(plain, url);
}

static bool ota_fetch_text(const String& url, String& payload, char* error_buf, size_t error_buf_len) {
    HTTPClient http;
    WiFiClient plain;
    WiFiClientSecure secure;
    if (!ota_http_begin(http, plain, secure, url)) {
        copy_text(error_buf, error_buf_len, "nao foi possivel abrir a URL");
        return false;
    }
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        copy_text(error_buf, error_buf_len, HTTPClient::errorToString(code));
        http.end();
        return false;
    }

    payload = http.getString();
    http.end();

    if (payload.isEmpty()) {
        copy_text(error_buf, error_buf_len, "manifest vazio");
        return false;
    }
    return true;
}

static bool ota_parse_manifest(const String& payload, OtaManifest& manifest,
                               char* error_buf, size_t error_buf_len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        copy_text(error_buf, error_buf_len, err.c_str());
        return false;
    }

    manifest.version = doc["version"] | "";
    manifest.notes = doc["notes"] | "";
    manifest.firmware.url = doc["firmware"]["url"] | "";
    if (manifest.firmware.url.isEmpty()) {
        manifest.firmware.url = doc["firmware_url"] | "";
    }
    manifest.firmware.md5 = doc["firmware"]["md5"] | "";
    if (manifest.firmware.md5.isEmpty()) {
        manifest.firmware.md5 = doc["firmware_md5"] | "";
    }
    manifest.firmware.size = doc["firmware"]["size"] | 0;
    if (manifest.firmware.size == 0) {
        manifest.firmware.size = doc["firmware_size"] | 0;
    }

    manifest.filesystem.url = doc["filesystem"]["url"] | "";
    if (manifest.filesystem.url.isEmpty()) {
        manifest.filesystem.url = doc["filesystem_url"] | "";
    }
    manifest.filesystem.md5 = doc["filesystem"]["md5"] | "";
    if (manifest.filesystem.md5.isEmpty()) {
        manifest.filesystem.md5 = doc["filesystem_md5"] | "";
    }
    manifest.filesystem.size = doc["filesystem"]["size"] | 0;
    if (manifest.filesystem.size == 0) {
        manifest.filesystem.size = doc["filesystem_size"] | 0;
    }

    if (manifest.version.isEmpty()) {
        copy_text(error_buf, error_buf_len, "manifest sem campo version");
        return false;
    }
    if (manifest.firmware.url.isEmpty() && manifest.filesystem.url.isEmpty()) {
        copy_text(error_buf, error_buf_len, "manifest sem URLs de firmware/filesystem");
        return false;
    }
    return true;
}

static bool ota_fetch_manifest(const String& manifest_url, OtaManifest& manifest,
                               char* error_buf, size_t error_buf_len) {
    String payload;
    if (!ota_fetch_text(manifest_url, payload, error_buf, error_buf_len)) {
        return false;
    }
    return ota_parse_manifest(payload, manifest, error_buf, error_buf_len);
}

static bool ota_download_and_apply(const OtaRemoteJob& job) {
    ota_set_stage("baixando");
    Serial.printf("[OTA] Baixando %s online: %s\n", job.target, job.url);

    HTTPClient http;
    WiFiClient plain;
    WiFiClientSecure secure;
    if (!ota_http_begin(http, plain, secure, job.url)) {
        ota_set_error_text("http begin", "nao foi possivel abrir a URL");
        return false;
    }
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        ota_set_error_text("download", HTTPClient::errorToString(code).c_str());
        http.end();
        return false;
    }

    int remote_size = http.getSize();
    size_t expected_size = job.size > 0 ? job.size
                                        : (remote_size > 0 ? static_cast<size_t>(remote_size)
                                                           : UPDATE_SIZE_UNKNOWN);
    s_ota_total = expected_size == UPDATE_SIZE_UNKNOWN
        ? (remote_size > 0 ? static_cast<size_t>(remote_size) : 0)
        : expected_size;
    ota_set_stage("gravando");

    if (!Update.begin(expected_size, job.command)) {
        ota_set_error("begin");
        http.end();
        return false;
    }
    if (job.md5[0] != '\0' && !Update.setMD5(job.md5)) {
        Update.abort();
        ota_set_error_text("md5", "manifest com md5 invalido");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t written = 0;
    size_t remaining = remote_size > 0 ? static_cast<size_t>(remote_size) : 0;
    uint32_t last_activity = millis();

    while (http.connected() && (remaining > 0 || remote_size <= 0)) {
        size_t available = stream->available();
        if (available > 0) {
            size_t chunk = available > sizeof(buffer) ? sizeof(buffer) : available;
            int read = stream->readBytes(buffer, chunk);
            if (read <= 0) {
                break;
            }
            if (Update.write(buffer, static_cast<size_t>(read)) != static_cast<size_t>(read)) {
                Update.abort();
                ota_set_error("write");
                http.end();
                return false;
            }
            written += static_cast<size_t>(read);
            s_ota_written = written;
            if (remaining > 0) {
                remaining -= static_cast<size_t>(read);
            }
            last_activity = millis();
            continue;
        }

        if (!http.connected()) {
            break;
        }
        if (millis() - last_activity > OTA_HTTP_TIMEOUT_MS) {
            Update.abort();
            ota_set_error_text("download", "timeout lendo o stream");
            http.end();
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    bool even_if_remaining = (expected_size == UPDATE_SIZE_UNKNOWN);
    if (!Update.end(even_if_remaining)) {
        ota_set_error("end");
        http.end();
        return false;
    }

    http.end();
    ota_finish_ok(written);
    return true;
}

static void task_ota_online(void* param) {
    OtaRemoteJob* job = reinterpret_cast<OtaRemoteJob*>(param);
    bool ok = ota_download_and_apply(*job);
    delete job;
    s_ota_job_running = false;
    s_ota_busy = false;
    if (ok) {
        schedule_restart();
    }
    vTaskDelete(nullptr);
}

static void ota_upload_chunk(const String& filename, size_t index, uint8_t* data, size_t len,
                             bool final, int command, const char* target) {
    if (index == 0) {
        if (s_ota_busy) {
            ota_set_error_text("busy", "outra atualizacao ja esta em andamento");
            return;
        }
        s_ota_busy = true;
        s_ota_job_running = true;
        ota_reset_state(target);
        ota_set_stage("upload");
        Serial.printf("[OTA] Iniciando %s: %s\n", target, filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, command)) {
            ota_set_error("begin");
            return;
        }
    }

    if (Update.hasError()) {
        return;
    }

    if (Update.write(data, len) != len) {
        ota_set_error("write");
        return;
    }

    s_ota_written = index + len;

    if (final) {
        if (!Update.end(true)) {
            ota_set_error("end");
            return;
        }
        ota_finish_ok(index + len);
    }
}

static void handle_get_ota_info(AsyncWebServerRequest* req) {
    JsonDocument doc;
    const esp_partition_t* running = esp_ota_get_running_partition();
    doc["enabled"] = true;
    doc["auth_user"] = current_auth_user();
    doc["auth_default"] = using_default_auth();
    doc["current_version"] = APP_VERSION;
    doc["default_manifest_url"] = current_manifest_url();
    doc["build_date"] = __DATE__;
    doc["build_time"] = __TIME__;
    doc["hostname"] = String(current_hostname()) + ".local";
    doc["hostname_hint"] = String(s_hostname_hint) + ".local";
    doc["chip_model"] = ESP.getChipModel();
    doc["sketch_size"] = ESP.getSketchSize();
    doc["free_sketch_space"] = ESP.getFreeSketchSpace();
    doc["fs_total"] = LittleFS.totalBytes();
    doc["fs_used"] = LittleFS.usedBytes();
    doc["auto_check_enabled"] = s_device_settings.ota_auto_check;
    doc["auto_check_hours"] = s_device_settings.ota_auto_check_hours;
    doc["last_check_ms"] = s_ota_last_check_ms;
    if (running) {
        doc["running_partition"] = running->label;
    }
    doc["last_target"] = s_ota_last_target;
    doc["last_ok"] = s_ota_last_ok;
    doc["last_error"] = s_ota_last_error;
    doc["last_written"] = s_ota_written;
    doc["last_total"] = s_ota_total;
    doc["job_running"] = s_ota_job_running;
    doc["job_stage"] = s_ota_job_stage;
    doc["check_ok"] = s_ota_last_check_ok;
    doc["check_error"] = s_ota_last_check_error;
    doc["checked_manifest_url"] = s_ota_checked_manifest_url;
    doc["checked_version"] = s_ota_checked_version;
    doc["checked_notes"] = s_ota_checked_notes;
    doc["checked_has_firmware"] = s_ota_checked_has_firmware;
    doc["checked_has_filesystem"] = s_ota_checked_has_filesystem;
    doc["update_available"] = s_ota_update_available;
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void handle_post_ota_result(AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["ok"] = s_ota_last_ok;
    doc["target"] = s_ota_last_target;
    doc["written"] = s_ota_written;
    doc["total"] = s_ota_total;
    if (s_ota_last_error[0] != '\0') {
        doc["error"] = s_ota_last_error;
    }
    String out;
    serializeJson(doc, out);
    req->send(s_ota_last_ok ? 200 : 500, "application/json", out);
    s_ota_job_running = false;
    s_ota_busy = false;
    if (s_ota_last_ok) {
        schedule_restart();
    }
}

static void handle_post_ota_check(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"json invalido\"}");
        return;
    }

    String manifest_url = doc["manifest_url"] | current_manifest_url();
    manifest_url.trim();
    if (manifest_url.isEmpty()) {
        ota_store_manifest_state("", OtaManifest{}, false, "manifest_url vazio");
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"manifest_url vazio\"}");
        return;
    }

    OtaManifest manifest;
    char error_buf[128] = "";
    if (!ota_fetch_manifest(manifest_url, manifest, error_buf, sizeof(error_buf))) {
        ota_store_manifest_state(manifest_url.c_str(), manifest, false, error_buf);
        String out = "{\"ok\":false,\"error\":\"" + String(error_buf) + "\"}";
        req->send(502, "application/json", out);
        return;
    }

    ota_store_manifest_state(manifest_url.c_str(), manifest, true);

    JsonDocument resp;
    resp["ok"] = true;
    resp["manifest_url"] = manifest_url;
    resp["current_version"] = APP_VERSION;
    resp["available_version"] = manifest.version;
    resp["update_available"] = s_ota_update_available;
    resp["notes"] = manifest.notes;
    resp["has_firmware"] = !manifest.firmware.url.isEmpty();
    resp["has_filesystem"] = !manifest.filesystem.url.isEmpty();
    resp["firmware_url"] = manifest.firmware.url;
    resp["filesystem_url"] = manifest.filesystem.url;
    resp["firmware_size"] = manifest.firmware.size;
    resp["filesystem_size"] = manifest.filesystem.size;
    String out;
    serializeJson(resp, out);
    req->send(200, "application/json", out);
}

static void handle_post_ota_online(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    if (s_ota_busy) {
        req->send(409, "application/json", "{\"ok\":false,\"error\":\"outra atualizacao ja esta em andamento\"}");
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"json invalido\"}");
        return;
    }

    String manifest_url = doc["manifest_url"] | current_manifest_url();
    String target = doc["target"] | "firmware";
    manifest_url.trim();
    target.trim();
    if (manifest_url.isEmpty()) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"manifest_url vazio\"}");
        return;
    }
    if (target != "firmware" && target != "filesystem") {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"target invalido\"}");
        return;
    }

    OtaManifest manifest;
    char error_buf[128] = "";
    if (!ota_fetch_manifest(manifest_url, manifest, error_buf, sizeof(error_buf))) {
        ota_store_manifest_state(manifest_url.c_str(), manifest, false, error_buf);
        String out = "{\"ok\":false,\"error\":\"" + String(error_buf) + "\"}";
        req->send(502, "application/json", out);
        return;
    }
    ota_store_manifest_state(manifest_url.c_str(), manifest, true);

    const OtaAsset* asset = target == "filesystem" ? &manifest.filesystem : &manifest.firmware;
    int command = target == "filesystem" ? U_SPIFFS : U_FLASH;
    if (asset->url.isEmpty()) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"manifest sem URL para o alvo pedido\"}");
        return;
    }

    OtaRemoteJob* job = new OtaRemoteJob();
    copy_text(job->target, sizeof(job->target), target);
    copy_text(job->version, sizeof(job->version), manifest.version);
    copy_text(job->url, sizeof(job->url), asset->url);
    copy_text(job->md5, sizeof(job->md5), asset->md5);
    job->size = asset->size;
    job->command = command;

    s_ota_busy = true;
    s_ota_job_running = true;
    ota_reset_state(job->target);
    ota_set_stage("fila");

    if (xTaskCreate(task_ota_online, "ota_online", 10240, job, 1, nullptr) != pdPASS) {
        delete job;
        s_ota_busy = false;
        s_ota_job_running = false;
        ota_set_error_text("task", "nao foi possivel iniciar a task OTA");
        req->send(500, "application/json", "{\"ok\":false,\"error\":\"falha ao iniciar task OTA\"}");
        return;
    }

    JsonDocument resp;
    resp["ok"] = true;
    resp["started"] = true;
    resp["target"] = target;
    resp["version"] = manifest.version;
    resp["manifest_url"] = manifest_url;
    String out;
    serializeJson(resp, out);
    req->send(202, "application/json", out);
}

static void handle_get_device_settings(AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["hostname"] = current_hostname();
    doc["hostname_hint"] = s_hostname_hint;
    doc["manifest_url"] = current_manifest_url();
    doc["auth_user"] = current_auth_user();
    doc["auth_default"] = using_default_auth();
    doc["ota_auto_check"] = s_device_settings.ota_auto_check;
    doc["ota_auto_check_hours"] = s_device_settings.ota_auto_check_hours;
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void handle_post_device_settings(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"json invalido\"}");
        return;
    }

    DeviceSettings next = s_device_settings;
    bool changed = false;

    if (doc["hostname"].is<String>()) {
        String hostname = doc["hostname"].as<String>();
        hostname.trim();
        hostname.toLowerCase();
        if (!is_valid_hostname(hostname)) {
            req->send(400, "application/json", "{\"ok\":false,\"error\":\"hostname invalido\"}");
            return;
        }
        if (strcmp(next.hostname, hostname.c_str()) != 0) {
            copy_text(next.hostname, sizeof(next.hostname), hostname);
            changed = true;
        }
    }

    if (doc["manifest_url"].is<String>()) {
        String manifest_url = doc["manifest_url"].as<String>();
        manifest_url.trim();
        if (!is_valid_manifest_url(manifest_url)) {
            req->send(400, "application/json", "{\"ok\":false,\"error\":\"manifest_url invalido\"}");
            return;
        }
        if (strcmp(next.manifest_url, manifest_url.c_str()) != 0) {
            copy_text(next.manifest_url, sizeof(next.manifest_url), manifest_url);
            changed = true;
        }
    }

    if (doc["auth_user"].is<String>()) {
        String auth_user = doc["auth_user"].as<String>();
        auth_user.trim();
        if (!is_valid_auth_user(auth_user)) {
            req->send(400, "application/json", "{\"ok\":false,\"error\":\"usuario invalido\"}");
            return;
        }
        if (strcmp(next.auth_user, auth_user.c_str()) != 0) {
            copy_text(next.auth_user, sizeof(next.auth_user), auth_user);
            changed = true;
        }
    }

    if (doc["auth_password"].is<String>()) {
        String auth_password = doc["auth_password"].as<String>();
        auth_password.trim();
        if (!auth_password.isEmpty()) {
            if (!is_valid_auth_password(auth_password)) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"senha deve ter entre 8 e 63 caracteres\"}");
                return;
            }
            if (strcmp(next.auth_password, auth_password.c_str()) != 0) {
                copy_text(next.auth_password, sizeof(next.auth_password), auth_password);
                changed = true;
            }
        }
    }

    if (!doc["ota_auto_check"].isNull()) {
        bool ota_auto_check = doc["ota_auto_check"] | next.ota_auto_check;
        if (next.ota_auto_check != ota_auto_check) {
            next.ota_auto_check = ota_auto_check;
            changed = true;
        }
    }

    if (!doc["ota_auto_check_hours"].isNull()) {
        int ota_auto_check_hours = doc["ota_auto_check_hours"] | next.ota_auto_check_hours;
        if (ota_auto_check_hours < OTA_AUTO_CHECK_HOURS_MIN
            || ota_auto_check_hours > OTA_AUTO_CHECK_HOURS_MAX) {
            req->send(400, "application/json", "{\"ok\":false,\"error\":\"intervalo deve ficar entre 1 e 168 horas\"}");
            return;
        }
        if (next.ota_auto_check_hours != static_cast<uint16_t>(ota_auto_check_hours)) {
            next.ota_auto_check_hours = static_cast<uint16_t>(ota_auto_check_hours);
            changed = true;
        }
    }

    if (!changed) {
        req->send(200, "application/json", "{\"ok\":true,\"changed\":false}");
        return;
    }

    s_device_settings = next;
    save_device_settings();
    ota_clear_manifest_state(current_manifest_url());

    JsonDocument resp;
    resp["ok"] = true;
    resp["changed"] = true;
    resp["restart_required"] = true;
    resp["hostname"] = String(current_hostname()) + ".local";
    resp["manifest_url"] = current_manifest_url();
    resp["auth_default"] = using_default_auth();
    String out;
    serializeJson(resp, out);
    req->send(200, "application/json", out);
    schedule_restart();
}

static void handle_head_page(AsyncWebServerRequest* req, const char* content_type) {
    AsyncWebServerResponse* resp = req->beginResponse(200, content_type, "");
    req->send(resp);
}

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
    doc["vin"]        = s_state->vin;
    doc["profile_id"] = s_state->profile_id;
    doc["sim_mode"]   = (uint8_t)s_state->sim_mode;
    doc["bt_connected"] = elm327_bt_connected();
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

static NetConfig default_wifi_net() {
    return { STA_SSID, STA_PASSWORD, STA_STATIC_IP, STA_GATEWAY };
}

static int find_saved_net_index(const String& ssid) {
    for (uint8_t i = 0; i < s_net_count; i++) {
        if (s_nets[i].ssid == ssid) {
            return i;
        }
    }
    return -1;
}

static void upsert_wifi_net(const NetConfig& net) {
    if (net.ssid.isEmpty()) {
        return;
    }

    int idx = find_saved_net_index(net.ssid);
    if (idx < 0) {
        if (s_net_count >= MAX_WIFI_NETS) {
            return;
        }
        idx = s_net_count++;
    }

    s_nets[idx] = net;
}

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

static void start_access_point() {
    IPAddress ap_ip, ap_gw, ap_subnet;
    if (ap_ip.fromString(AP_IP)) {
        ap_gw = ap_ip;
        ap_subnet.fromString("255.255.255.0");
        WiFi.softAPConfig(ap_ip, ap_gw, ap_subnet);
    }

    WiFi.softAP(AP_SSID, AP_PASSWORD, 6);
    vTaskDelay(pdMS_TO_TICKS(500));

    wifi_config_t ap_cfg = {};
    esp_wifi_get_config(WIFI_IF_AP, &ap_cfg);
    ap_cfg.ap.authmode        = WIFI_AUTH_WPA2_PSK;
    ap_cfg.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);

    Serial.printf("[WiFi] AP: '%s'  senha: '%s'  IP: http://%s\n",
                  AP_SSID, AP_PASSWORD, WiFi.softAPIP().toString().c_str());
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
    Serial.println("[HTTP] GET /api/wifi");
    JsonDocument doc;
    doc["connected"]  = (WiFi.status() == WL_CONNECTED);
    doc["current_ssid"] = s_connected_ssid;
    doc["current_ip"] = WiFi.localIP().toString();
    doc["ap_ip"]      = WiFi.softAPIP().toString();
    doc["ap_ssid"]    = AP_SSID;
    doc["hostname"]   = String(current_hostname()) + ".local";
    doc["hostname_hint"] = String(s_hostname_hint) + ".local";
    auto arr = doc["networks"].to<JsonArray>();
    for (uint8_t i = 0; i < s_net_count; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["ssid"] = s_nets[i].ssid;
        obj["ip"]   = s_nets[i].ip;
        obj["gw"]   = s_nets[i].gw;
    }
    String out; serializeJson(doc, out);
    Serial.printf("[HTTP] /api/wifi json len:%u\n", out.length());
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

static void task_ota_auto_check(void*) {
    vTaskDelay(pdMS_TO_TICKS(15000));

    while (true) {
        if (s_device_settings.ota_auto_check
            && current_manifest_url()[0] != '\0'
            && WiFi.status() == WL_CONNECTED
            && !s_ota_busy
            && !s_ota_job_running) {
            uint32_t interval_ms = static_cast<uint32_t>(s_device_settings.ota_auto_check_hours) * 3600000UL;
            if (s_ota_last_check_ms == 0 || millis() - s_ota_last_check_ms >= interval_ms) {
                OtaManifest manifest;
                char error_buf[128] = "";
                String manifest_url = current_manifest_url();
                if (ota_fetch_manifest(manifest_url, manifest, error_buf, sizeof(error_buf))) {
                    ota_store_manifest_state(manifest_url.c_str(), manifest, true);
                    Serial.printf("[OTA] Check automatico OK: %s\n", manifest.version.c_str());
                } else {
                    ota_store_manifest_state(manifest_url.c_str(), manifest, false, error_buf);
                    Serial.printf("[OTA] Check automatico falhou: %s\n", error_buf);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

// ── Wi-Fi init (multi-network) ───────────────────────────────
// 1. Carrega redes salvas da NVS (até MAX_WIFI_NETS)
// 2. Faz scan rápido (~3s) para ver quais estão no ar
// 3. Tenta conectar na melhor rede conhecida (maior RSSI)
// 4. Se falhou, tenta as demais em ordem
// 5. Se nenhuma conectou, sobe AP como fallback

static void wifi_start() {
    load_device_settings();
    load_wifi_nets();
    NetConfig factory_net = default_wifi_net();
    int factory_idx = factory_net.ssid.length() > 0
        ? find_saved_net_index(factory_net.ssid)
        : -1;
    bool prefer_factory_net = factory_net.ssid.length() > 0
        && (factory_idx < 0
            || (factory_net.ip.length() == 0 && factory_idx >= 0 && s_nets[factory_idx].ip.length() > 0));

    Serial.printf("[WiFi] %d rede(s) salva(s)\n", s_net_count);
    for (uint8_t i = 0; i < s_net_count; i++)
        Serial.printf("[WiFi]   %d: '%s' %s\n", i, s_nets[i].ssid.c_str(),
                      s_nets[i].ip.length() > 0 ? s_nets[i].ip.c_str() : "(DHCP)");

    bool sta_ok = false;

    if (s_net_count > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.setHostname(current_hostname());

        if (prefer_factory_net) {
            Serial.println("[WiFi] Tentando configuracao padrao antes das redes salvas...");
            sta_ok = try_connect(factory_net);
            if (sta_ok) {
                upsert_wifi_net(factory_net);
            }
        }

        // Scan rápido para ver quais redes estão no ar
        if (!sta_ok) {
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
    }

    // Se conectou, salva as redes (para gravar seed do config.h no NVS)
    if (sta_ok && s_net_count > 0) {
        save_wifi_nets();
    }

    // Fallback: AP sempre sobe se STA falhou
    if (!sta_ok) {
        Serial.println("[WiFi] Nenhuma rede conectou — iniciando AP...");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        vTaskDelay(pdMS_TO_TICKS(500));
        WiFi.mode(WIFI_AP);
        vTaskDelay(pdMS_TO_TICKS(100));
        start_access_point();
    }

    if (MDNS.begin(current_hostname())) {
        MDNS.addService("http", "tcp", WEB_PORT);
        Serial.printf("[mDNS] http://%s.local registrado\n", current_hostname());
    }
}

// ── API pública ───────────────────────────────────────────────

void web_init(SimulationState* state, SemaphoreHandle_t mutex) {
    s_state = state;
    s_mutex = mutex;

    wifi_start();

    // LittleFS — serve index.html da flash
    if (!LittleFS.begin()) {
        Serial.println("[LittleFS] Mount falhou!");
    } else if (!LittleFS.exists("/index.html")) {
        Serial.println("[LittleFS] /index.html nao encontrado");
    } else {
        Serial.println("[LittleFS] /index.html pronto para servir");
    }

    // WebSocket
    protect(ws);
    ws.onEvent(on_ws_event);
    server.addHandler(&ws);

    // REST API
    protect(server.on("/api/status",   HTTP_GET,  handle_get_status));
    protect(server.on("/api/dtcs",     HTTP_GET,  handle_get_dtcs));
    protect(server.on("/api/dtcs/clear", HTTP_POST,
        [](AsyncWebServerRequest* r){ handle_clear_dtcs(r); }));

    protect(server.on("/api/params",   HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_params));
    protect(server.on("/api/protocol", HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_protocol));
    protect(server.on("/api/dtcs/add", HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_add_dtc));
    protect(server.on("/api/dtcs/remove", HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_remove_dtc));
    protect(server.on("/api/preset",   HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_preset));
    protect(server.on("/api/mode",     HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_mode));
    protect(server.on("/api/profiles", HTTP_GET,  handle_get_profiles));
    protect(server.on("/api/profile",  HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_profile));
    // ⚠️ Registrar /api/wifi/scan ANTES de /api/wifi para evitar prefix-match
    protect(server.on("/api/wifi/scan", HTTP_POST,
        [](AsyncWebServerRequest* r){ handle_post_wifi_scan(r); }));
    protect(server.on("/api/wifi/scan", HTTP_GET,  handle_get_wifi_scan));
    protect(server.on("/api/wifi",      HTTP_GET,  handle_get_wifi));
    protect(server.on("/api/wifi/remove", HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_wifi_remove));
    protect(server.on("/api/wifi",      HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_wifi));
    protect(server.on("/api/reboot",   HTTP_POST,
        [](AsyncWebServerRequest* r){ handle_post_reboot(r); }));
    protect(server.on("/api/device/settings", HTTP_GET, handle_get_device_settings));
    protect(server.on("/api/device/settings", HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_device_settings));
    protect(server.on("/api/ota/info", HTTP_GET, handle_get_ota_info));
    protect(server.on("/api/ota/check", HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_ota_check));
    protect(server.on("/api/ota/online", HTTP_POST, [](AsyncWebServerRequest*){},
        nullptr, handle_post_ota_online));
    protect(server.on("/api/ota/firmware", HTTP_POST,
        [](AsyncWebServerRequest* req) { handle_post_ota_result(req); },
        [](AsyncWebServerRequest*, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
            ota_upload_chunk(filename, index, data, len, final, U_FLASH, "firmware");
        }));
    protect(server.on("/api/ota/filesystem", HTTP_POST,
        [](AsyncWebServerRequest* req) { handle_post_ota_result(req); },
        [](AsyncWebServerRequest*, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
            ota_upload_chunk(filename, index, data, len, final, U_SPIFFS, "filesystem");
        }));
    server.on("/ping", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/plain", "pong");
    });
    server.on("/ping-json", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", "{\"ok\":true}");
    });
    protect(server.on("/", HTTP_HEAD,
        [](AsyncWebServerRequest* req) { handle_head_page(req, "text/html"); }));
    protect(server.on("/index.html", HTTP_HEAD,
        [](AsyncWebServerRequest* req) { handle_head_page(req, "text/html"); }));
    protect(server.on("/ota.html", HTTP_HEAD,
        [](AsyncWebServerRequest* req) { handle_head_page(req, "text/html"); }));
    // Serve arquivos do LittleFS (UI web)
    protect(server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html"));

    // CORS para desenvolvimento local
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    server.begin();
    Serial.printf("[WebServer] Porta %d iniciada\n", WEB_PORT);

    // Task broadcast WebSocket — Core 1
    xTaskCreatePinnedToCore(task_ws_broadcast, "task_ws", 4096, nullptr, 3, nullptr, 1);
    xTaskCreatePinnedToCore(task_ota_auto_check, "task_ota_chk", 6144, nullptr, 1, nullptr, 1);
}
