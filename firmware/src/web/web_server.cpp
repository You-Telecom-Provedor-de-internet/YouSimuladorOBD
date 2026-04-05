#include "web_server.h"
#include "config.h"
#include "diagnostic_scenario_engine.h"
#include "dynamic_engine.h"
#include "simulation_precedence.h"
#include "vehicle_profiles.h"
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
    char api_auth_user[32] = "";
    char api_auth_password[64] = "";
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
static const char* current_api_auth_user();
static const char* current_api_auth_password();

static void persist_active_protocol(uint8_t protocol) {
    if (protocol >= PROTO_COUNT) {
        return;
    }
    Preferences prefs;
    prefs.begin("obd", false);
    prefs.putUChar("proto", protocol);
    prefs.end();
}

static void persist_active_profile(const char* profile_id) {
    Preferences prefs;
    prefs.begin("obd", false);
    if (profile_id && profile_id[0]) {
        prefs.putString("profile", profile_id);
    } else {
        prefs.remove("profile");
    }
    prefs.end();
}

static void clear_persisted_profile() {
    persist_active_profile(nullptr);
}

// ── Helpers ───────────────────────────────────────────────────

template <typename T>
static T& protect(T& handler) {
    handler.setAuthentication(current_auth_user(), current_auth_password());
    return handler;
}

static bool authenticate_api_request(AsyncWebServerRequest* request) {
    if (request->authenticate(current_api_auth_user(), current_api_auth_password())) {
        return true;
    }

    if (strcmp(current_api_auth_user(), current_auth_user()) != 0
        || strcmp(current_api_auth_password(), current_auth_password()) != 0) {
        if (request->authenticate(current_auth_user(), current_auth_password())) {
            return true;
        }
    }

    return false;
}

template <typename Handler>
static auto protect_api_get(Handler handler) {
    return [handler](AsyncWebServerRequest* request) {
        if (!authenticate_api_request(request)) {
            return request->requestAuthentication("youobd-api", false);
        }
        handler(request);
    };
}

template <typename Handler>
static auto protect_api_body(Handler handler) {
    return [handler](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        if (!authenticate_api_request(request)) {
            return request->requestAuthentication("youobd-api", false);
        }
        handler(request, data, len, index, total);
    };
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

static const char* current_api_auth_user() {
    return s_device_settings.api_auth_user[0] ? s_device_settings.api_auth_user : API_AUTH_USER;
}

static const char* current_api_auth_password() {
    return s_device_settings.api_auth_password[0] ? s_device_settings.api_auth_password : API_AUTH_PASSWORD;
}

static bool using_default_auth() {
    return strcmp(current_auth_user(), WEB_AUTH_USER) == 0
        && strcmp(current_auth_password(), WEB_AUTH_PASSWORD) == 0;
}

static bool using_default_api_auth() {
    return strcmp(current_api_auth_user(), API_AUTH_USER) == 0
        && strcmp(current_api_auth_password(), API_AUTH_PASSWORD) == 0;
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
    copy_text(s_device_settings.api_auth_user, sizeof(s_device_settings.api_auth_user), API_AUTH_USER);
    copy_text(s_device_settings.api_auth_password, sizeof(s_device_settings.api_auth_password), API_AUTH_PASSWORD);
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

    String api_auth_user = s_device_settings.api_auth_user;
    api_auth_user.trim();
    if (!is_valid_auth_user(api_auth_user)) {
        api_auth_user = API_AUTH_USER;
    }
    copy_text(s_device_settings.api_auth_user, sizeof(s_device_settings.api_auth_user), api_auth_user);

    String api_auth_password = s_device_settings.api_auth_password;
    api_auth_password.trim();
    if (!is_valid_auth_password(api_auth_password)) {
        api_auth_password = API_AUTH_PASSWORD;
    }
    copy_text(s_device_settings.api_auth_password, sizeof(s_device_settings.api_auth_password), api_auth_password);

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
    copy_text(s_device_settings.api_auth_user, sizeof(s_device_settings.api_auth_user), prefs.getString("api_user", s_device_settings.api_auth_user));
    copy_text(s_device_settings.api_auth_password, sizeof(s_device_settings.api_auth_password), prefs.getString("api_pass", s_device_settings.api_auth_password));
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
    prefs.putString("api_user", current_api_auth_user());
    prefs.putString("api_pass", current_api_auth_password());
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

static void handle_get_ota_info(AsyncWebServerRequest* req) {
    JsonDocument doc;
    const esp_partition_t* running = esp_ota_get_running_partition();
    doc["enabled"] = true;
    doc["auth_user"] = current_auth_user();
    doc["auth_default"] = using_default_auth();
    doc["current_version"] = APP_VERSION;
    doc["online_only"] = true;
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
    doc["api_auth_user"] = current_api_auth_user();
    doc["api_auth_default"] = using_default_api_auth();
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
    bool restart_required = false;

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
            restart_required = true;
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
            restart_required = true;
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
                restart_required = true;
            }
        }
    }

    if (doc["api_auth_user"].is<String>()) {
        String api_auth_user = doc["api_auth_user"].as<String>();
        api_auth_user.trim();
        if (!is_valid_auth_user(api_auth_user)) {
            req->send(400, "application/json", "{\"ok\":false,\"error\":\"usuario api invalido\"}");
            return;
        }
        if (strcmp(next.api_auth_user, api_auth_user.c_str()) != 0) {
            copy_text(next.api_auth_user, sizeof(next.api_auth_user), api_auth_user);
            changed = true;
            restart_required = true;
        }
    }

    if (doc["api_auth_password"].is<String>()) {
        String api_auth_password = doc["api_auth_password"].as<String>();
        api_auth_password.trim();
        if (!api_auth_password.isEmpty()) {
            if (!is_valid_auth_password(api_auth_password)) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"senha api deve ter entre 8 e 63 caracteres\"}");
                return;
            }
            if (strcmp(next.api_auth_password, api_auth_password.c_str()) != 0) {
                copy_text(next.api_auth_password, sizeof(next.api_auth_password), api_auth_password);
                changed = true;
                restart_required = true;
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
    resp["restart_required"] = restart_required;
    resp["hostname"] = String(current_hostname()) + ".local";
    resp["manifest_url"] = current_manifest_url();
    resp["auth_default"] = using_default_auth();
    resp["api_auth_default"] = using_default_api_auth();
    String out;
    serializeJson(resp, out);
    req->send(200, "application/json", out);
    if (restart_required) {
        schedule_restart();
    }
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

static const char* driveContextSlug(uint8_t sim_mode) {
    switch (sim_mode) {
        case SIM_IDLE: return "idle";
        case SIM_URBAN: return "urban";
        case SIM_HIGHWAY: return "highway";
        case SIM_WARMUP: return "warmup";
        case SIM_FAULT: return "fault";
        default: return "static";
    }
}

static const char* freezeFrameSourceSlug(uint8_t source) {
    switch (source) {
        case DIAG_FREEZE_SOURCE_SCENARIO: return "scenario";
        case DIAG_FREEZE_SOURCE_MANUAL: return "manual";
        case DIAG_FREEZE_SOURCE_EFFECTIVE: return "effective";
        default: return "none";
    }
}

static const char* odometerSourceSlug(uint8_t source) {
    switch (source) {
        case ODOMETER_SOURCE_CLUSTER: return "cluster";
        case ODOMETER_SOURCE_BCM: return "bcm";
        case ODOMETER_SOURCE_ABS: return "abs";
        case ODOMETER_SOURCE_ECM: return "ecm";
        default: return "unknown";
    }
}

static const char* simModeLabel(uint8_t sim_mode) {
    switch (sim_mode) {
        case SIM_IDLE: return "Marcha lenta";
        case SIM_URBAN: return "Urbano";
        case SIM_HIGHWAY: return "Rodovia";
        case SIM_WARMUP: return "Aquecimento";
        case SIM_FAULT: return "Falha DTC";
        default: return "Manual";
    }
}

static const char* simModeSourceSlug(uint8_t source) {
    switch (source) {
        case SIM_MODE_SOURCE_USER: return "user";
        case SIM_MODE_SOURCE_SCENARIO: return "scenario";
        case SIM_MODE_SOURCE_PRESET: return "preset";
        default: return "boot";
    }
}

static const char* simModeSourceLabel(uint8_t source) {
    switch (source) {
        case SIM_MODE_SOURCE_USER: return "Escolha do usuario";
        case SIM_MODE_SOURCE_SCENARIO: return "Forcado pela camada diagnostica";
        case SIM_MODE_SOURCE_PRESET: return "Definido por preset";
        default: return "Restaurado no boot";
    }
}

static const char* precedenceNoticeSlug(uint8_t code) {
    switch (code) {
        case SIM_NOTICE_PROFILE_APPLIED: return "profile_applied";
        case SIM_NOTICE_PROFILE_RESTORED: return "profile_restored";
        case SIM_NOTICE_PROFILE_CLEARED_BY_PROTOCOL: return "profile_cleared_by_protocol";
        case SIM_NOTICE_PROTOCOL_CHANGED: return "protocol_changed";
        case SIM_NOTICE_PRESET_APPLIED: return "preset_applied";
        case SIM_NOTICE_SCENARIO_APPLIED: return "scenario_applied";
        case SIM_NOTICE_SCENARIO_FORCED_MODE: return "scenario_forced_mode";
        case SIM_NOTICE_SCENARIO_CLEARED_BY_MODE: return "scenario_cleared_by_mode";
        case SIM_NOTICE_SCENARIO_CLEARED_BY_MANUAL_EDIT: return "scenario_cleared_by_manual_edit";
        case SIM_NOTICE_SCENARIO_CLEARED: return "scenario_cleared";
        default: return "none";
    }
}

static const char* precedenceNoticeLevel(uint8_t code) {
    switch (code) {
        case SIM_NOTICE_PROFILE_CLEARED_BY_PROTOCOL:
        case SIM_NOTICE_PRESET_APPLIED:
        case SIM_NOTICE_SCENARIO_FORCED_MODE:
        case SIM_NOTICE_SCENARIO_CLEARED_BY_MODE:
        case SIM_NOTICE_SCENARIO_CLEARED_BY_MANUAL_EDIT:
            return "warn";
        case SIM_NOTICE_NONE:
            return "neutral";
        default:
            return "info";
    }
}

static bool hasActiveScenario(const SimulationState& snap) {
    return static_cast<DiagnosticScenarioId>(snap.scenario_id) != DIAG_SCENARIO_NONE;
}

static const char* effectiveDtcSourceSlug(const SimulationState& snap) {
    const bool manual = snap.manual_dtc_count > 0;
    const bool scenario = hasActiveScenario(snap) && snap.dtc_count > snap.manual_dtc_count;
    if (manual && scenario) return "mixed";
    if (scenario) return "scenario";
    if (manual) return "manual";
    return "none";
}

static const char* effectiveDtcSourceLabel(const SimulationState& snap) {
    const char* slug = effectiveDtcSourceSlug(snap);
    if (strcmp(slug, "mixed") == 0) return "Mescla de cenario e injecao manual";
    if (strcmp(slug, "scenario") == 0) return "Derivados da camada diagnostica";
    if (strcmp(slug, "manual") == 0) return "Injetados manualmente";
    return "Sem DTCs efetivos";
}

static String profileSummaryText(const VehicleProfile* profile, const SimulationState& snap) {
    if (profile) {
        return String(profile->brand) + " " + profile->model + " (" + profile->year + ")";
    }
    if (snap.profile_id[0]) {
        return String(snap.profile_id) + " (nao catalogado)";
    }
    return "Nenhum perfil ativo";
}

static String precedenceNoticeMessage(const SimulationState& snap) {
    switch (simulation_precedence_snapshot().notice_code) {
        case SIM_NOTICE_PROFILE_APPLIED:
            return "Perfil aplicado como base do veiculo; camada diagnostica e DTCs manuais foram limpos.";
        case SIM_NOTICE_PROFILE_RESTORED:
            return "Perfil persistido foi restaurado automaticamente no boot.";
        case SIM_NOTICE_PROFILE_CLEARED_BY_PROTOCOL:
            return "Protocolo alterado manualmente; o perfil ativo foi desassociado para evitar ambiguidade.";
        case SIM_NOTICE_PROTOCOL_CHANGED:
            return "Protocolo alterado manualmente sem mexer na camada diagnostica.";
        case SIM_NOTICE_PRESET_APPLIED:
            return "Preset rapido aplicado; perfil ativo e camada diagnostica foram limpos.";
        case SIM_NOTICE_SCENARIO_APPLIED:
            return "Camada diagnostica aplicada como overlay sobre o perfil e o modo atuais.";
        case SIM_NOTICE_SCENARIO_FORCED_MODE:
            return String("Cenario ativo forçou o modo para ") + simModeLabel(static_cast<uint8_t>(snap.sim_mode)) + ".";
        case SIM_NOTICE_SCENARIO_CLEARED_BY_MODE:
            return "O modo escolhido pelo usuario limpou o cenario ativo por incompatibilidade.";
        case SIM_NOTICE_SCENARIO_CLEARED_BY_MANUAL_EDIT:
            return "Edicao manual de sensores limpou o cenario ativo e preservou DTCs manuais.";
        case SIM_NOTICE_SCENARIO_CLEARED:
            return "Camada diagnostica limpa; perfil e DTCs manuais preservados.";
        default:
            return "Hierarquia estavel: perfil define a base, modo define a dinamica, cenario atua como overlay e DTC manual entra na mescla final.";
    }
}

static void appendControlLayers(JsonDocument& doc, const SimulationState& snap, const VehicleProfile* profile) {
    const SimulationPrecedenceState precedence = simulation_precedence_snapshot();
    JsonObject layers = doc["control_layers"].to<JsonObject>();

    JsonObject profile_layer = layers["profile"].to<JsonObject>();
    const bool profile_bound = snap.profile_id[0] != '\0';
    profile_layer["active"] = profile_bound;
    profile_layer["id"] = profile_bound ? snap.profile_id : nullptr;
    profile_layer["label"] = profileSummaryText(profile, snap);
    profile_layer["persistent"] = profile_bound;
    profile_layer["protocol_linked"] = profile && profile->protocol == snap.active_protocol;

    JsonObject mode_layer = layers["simulation_mode"].to<JsonObject>();
    mode_layer["id"] = static_cast<uint8_t>(snap.sim_mode);
    mode_layer["slug"] = driveContextSlug(static_cast<uint8_t>(snap.sim_mode));
    mode_layer["label"] = simModeLabel(static_cast<uint8_t>(snap.sim_mode));
    mode_layer["source"] = simModeSourceSlug(precedence.sim_mode_source);
    mode_layer["source_label"] = simModeSourceLabel(precedence.sim_mode_source);

    JsonObject diag_layer = layers["diagnostic_layer"].to<JsonObject>();
    diag_layer["active"] = hasActiveScenario(snap);
    diag_layer["id"] = diagnostic_scenario_slug(static_cast<DiagnosticScenarioId>(snap.scenario_id));
    diag_layer["label"] = diagnostic_scenario_label(static_cast<DiagnosticScenarioId>(snap.scenario_id));
    diag_layer["forced_mode"] = hasActiveScenario(snap) && precedence.sim_mode_source == SIM_MODE_SOURCE_SCENARIO;

    JsonObject dtc_layer = layers["dtcs"].to<JsonObject>();
    dtc_layer["manual_count"] = snap.manual_dtc_count;
    dtc_layer["effective_count"] = snap.dtc_count;
    dtc_layer["source"] = effectiveDtcSourceSlug(snap);
    dtc_layer["source_label"] = effectiveDtcSourceLabel(snap);

    JsonObject precedence_obj = doc["precedence"].to<JsonObject>();
    precedence_obj["notice_code"] = precedenceNoticeSlug(precedence.notice_code);
    precedence_obj["notice_level"] = precedenceNoticeLevel(precedence.notice_code);
    precedence_obj["message"] = precedenceNoticeMessage(snap);
}

static void clear_profile_binding(SimulationState& state) {
    state.profile_id[0] = '\0';
}

static void clear_scenario_for_manual_edit(SimulationState& state) {
    if (!hasActiveScenario(state)) {
        return;
    }
    diagnostic_engine_clear_scenario(state, true);
    simulation_precedence_set_notice(SIM_NOTICE_SCENARIO_CLEARED_BY_MANUAL_EDIT);
}

static void apply_manual_mode_selection(SimulationState& state, SimMode mode) {
    const DiagnosticScenarioId active_scenario = static_cast<DiagnosticScenarioId>(state.scenario_id);
    if (active_scenario != DIAG_SCENARIO_NONE &&
        (mode == SIM_FAULT || !diagnostic_scenario_allows_mode(active_scenario, mode))) {
        diagnostic_engine_clear_scenario(state, true);
        simulation_precedence_set_notice(SIM_NOTICE_SCENARIO_CLEARED_BY_MODE);
    }
    state.sim_mode = mode;
    simulation_precedence_set_mode_source(SIM_MODE_SOURCE_USER);
}

static void apply_profile_selection(SimulationState& state, const VehicleProfile& profile) {
    diagnostic_engine_clear_scenario(state, false);
    applyVehicleProfile(state, profile);
    diagnostic_engine_rebuild_effective_dtcs(state);
    if (simulation_precedence_snapshot().sim_mode_source == SIM_MODE_SOURCE_SCENARIO) {
        simulation_precedence_set_mode_source(SIM_MODE_SOURCE_USER);
    }
    simulation_precedence_set_notice(SIM_NOTICE_PROFILE_APPLIED);
}

static bool apply_protocol_selection(SimulationState& state, uint8_t protocol) {
    const bool profile_bound = state.profile_id[0] != '\0';
    const VehicleProfile* profile = activeVehicleProfile(state);
    state.active_protocol = protocol;
    state.pid_a6_supported = defaultPidA6SupportForProtocol(protocol) ? 1 : 0;

    const bool keep_profile = profile_bound && profile && profile->protocol == protocol;
    if (!keep_profile && profile_bound) {
        clear_profile_binding(state);
        simulation_precedence_set_notice(SIM_NOTICE_PROFILE_CLEARED_BY_PROTOCOL);
        return true;
    }

    simulation_precedence_set_notice(SIM_NOTICE_PROTOCOL_CHANGED);
    return false;
}

static void apply_preset_selection(SimulationState& state, const char* name) {
    diagnostic_engine_clear_scenario(state, false);
    clear_profile_binding(state);
    state.pid_a6_supported = defaultPidA6SupportForProtocol(state.active_protocol) ? 1 : 0;
    state.odometer_source = ODOMETER_SOURCE_CLUSTER;
    if      (!strcmp(name, "off"))           Preset::applyOff(state);
    else if (!strcmp(name, "idle"))          Preset::applyIdle(state);
    else if (!strcmp(name, "cruise"))        Preset::applyCruise(state);
    else if (!strcmp(name, "fullthrottle"))  Preset::applyFullThrottle(state);
    else if (!strcmp(name, "overheat"))      Preset::applyOverheat(state);
    else if (!strcmp(name, "catalyst_fail")) Preset::applyCatalystFail(state);
    simulation_precedence_set_mode_source(SIM_MODE_SOURCE_PRESET);
    simulation_precedence_set_notice(SIM_NOTICE_PRESET_APPLIED);
}

static void appendDtcArray(JsonArray arr, const uint16_t* dtcs, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        char buf[6] = {};
        dtcValToStr(dtcs[i], buf, sizeof(buf));
        arr.add(buf);
    }
}

static void appendPrimaryAlert(JsonDocument& doc, const SimulationState& snap) {
    const DiagnosticAlert* primary = diagnostic_get_primary_alert(snap);
    if (!primary) {
        doc["primary_alert"] = nullptr;
        return;
    }

    JsonObject obj = doc["primary_alert"].to<JsonObject>();
    obj["type"] = primary->type;
    obj["severity"] = primary->severity;
    obj["system"] = primary->system;
    obj["compartment"] = primary->compartment;
    obj["fault_id"] = primary->fault_id;
}

static String stateToJson() {
    SimulationState snap = {};
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    snap = *s_state;
    xSemaphoreGive(s_mutex);
    const VehicleProfile* profile = activeVehicleProfile(snap);

    JsonDocument doc;
    doc["protocol"]         = protoName(snap.active_protocol);
    doc["protocol_id"]      = snap.active_protocol;
    doc["rpm"]              = snap.rpm;
    doc["speed"]            = snap.speed_kmh;
    doc["coolant_temp"]     = snap.coolant_temp_c;
    doc["intake_temp"]      = snap.intake_temp_c;
    doc["maf"]              = serialized(String(snap.maf_gs, 1));
    doc["map"]              = snap.map_kpa;
    doc["throttle"]         = snap.throttle_pct;
    doc["ignition_advance"] = serialized(String(snap.ignition_adv, 1));
    doc["engine_load"]      = snap.engine_load_pct;
    doc["fuel_level"]       = snap.fuel_level_pct;
    doc["battery_voltage"]  = serialized(String(snap.battery_voltage, 1));
    doc["oil_temp"]         = snap.oil_temp_c;
    doc["stft"]             = serialized(String(snap.stft_pct, 1));
    doc["ltft"]             = serialized(String(snap.ltft_pct, 1));
    appendDtcArray(doc.createNestedArray("dtcs"), snap.dtcs, snap.dtc_count);
    doc["vin"]              = snap.vin;
    doc["profile_id"]       = snap.profile_id;
    doc["profile_selected"] = profile != nullptr;
    if (profile) {
        doc["profile_brand"] = profile->brand;
        doc["profile_model"] = profile->model;
        doc["profile_year"] = profile->year;
        doc["profile_turbo"] = vehicleProfileIsTurbo(*profile);
    }
    doc["sim_mode"]         = static_cast<uint8_t>(snap.sim_mode);
    doc["scenario_id"]      = snap.scenario_id;
    doc["health_score"]     = snap.health_score;
    doc["limp_mode"]        = snap.limp_mode != 0;
    doc["active_faults_count"] = snap.active_fault_count;
    doc["alert_count"]      = snap.alert_count;
    doc["odometer_total_km"] = static_cast<float>(snap.odometer_total_km_x10) / 10.0f;
    appendPrimaryAlert(doc, snap);
    appendControlLayers(doc, snap, profile);
    String out;
    serializeJson(doc, out);
    return out;
}

// ── Handlers REST ─────────────────────────────────────────────

static String scenariosToJson() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    size_t count = 0;
    const ScenarioDefinition* definitions = diagnostic_scenario_all(count);
    for (size_t i = 1; i < count; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["scenario_id"] = static_cast<uint8_t>(definitions[i].id);
        obj["id"] = definitions[i].slug;
        obj["label"] = definitions[i].label;
        obj["summary"] = definitions[i].summary;
        obj["default_mode"] = static_cast<uint8_t>(definitions[i].default_mode);
    }
    String out;
    serializeJson(doc, out);
    return out;
}

static void appendActiveFaults(JsonArray arr, const SimulationState& snap) {
    for (uint8_t i = 0; i < snap.active_fault_count; i++) {
        const ActiveFault& fault = snap.active_faults[i];
        const FaultCatalogEntry* entry = fault_catalog_get(fault.fault_id);
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = entry->slug;
        obj["label"] = entry->label;
        if (fault.dtc != 0) {
            char dtc_buf[6] = {};
            dtcValToStr(fault.dtc, dtc_buf, sizeof(dtc_buf));
            obj["dtc"] = dtc_buf;
        } else {
            obj["dtc"] = nullptr;
        }
        obj["severity"] = diagnostic_severity_slug(fault.severity);
        obj["system"] = diagnostic_system_slug(fault.system);
        obj["compartment"] = diagnostic_compartment_slug(fault.compartment);
        obj["stage"] = fault_stage_slug(fault.stage);
        obj["intensity_pct"] = fault.intensity_pct;
        obj["root_cause"] = entry->is_root_cause != 0;
    }
}

static void appendAlerts(JsonArray arr, const SimulationState& snap) {
    for (uint8_t i = 0; i < snap.alert_count; i++) {
        const DiagnosticAlert& alert = snap.alerts[i];
        const FaultCatalogEntry* entry = fault_catalog_get(alert.fault_id);
        JsonObject obj = arr.add<JsonObject>();
        obj["type"] = diagnostic_alert_type_slug(alert.type);
        obj["label"] = diagnostic_alert_type_label(alert.type);
        obj["severity"] = diagnostic_severity_slug(alert.severity);
        obj["system"] = diagnostic_system_slug(alert.system);
        obj["compartment"] = diagnostic_compartment_slug(alert.compartment);
        obj["fault_id"] = entry->slug;
        obj["primary"] = (alert.flags & 0x01) != 0;
    }
}

static void appendAnomalies(JsonArray arr, const SimulationState& snap) {
    for (uint8_t i = 0; i < snap.active_fault_count; i++) {
        const ActiveFault& fault = snap.active_faults[i];
        JsonObject obj = arr.add<JsonObject>();
        switch (fault.fault_id) {
            case FAULT_ID_MISFIRE_MULTI:
                obj["metric"] = "rpm_stability";
                obj["severity"] = "warning";
                obj["description"] = "Oscilacao de RPM incompatível com carga e TPS em trajeto urbano.";
                break;
            case FAULT_ID_MISFIRE_CYL_1:
            case FAULT_ID_MISFIRE_CYL_4:
                obj["metric"] = "combustion_balance";
                obj["severity"] = "warning";
                obj["description"] = "Falha derivada por cilindro mantendo torque irregular sob carga parcial.";
                break;
            case FAULT_ID_FAN_DEGRADED:
            case FAULT_ID_COOLING_OVERTEMP:
                obj["metric"] = "thermal_rise_low_speed";
                obj["severity"] = fault.severity >= DIAG_SEV_HIGH ? "critical" : "warning";
                obj["description"] = "Temperaturas sobem mais em baixa velocidade e aliviam com maior fluxo de ar.";
                break;
            case FAULT_ID_AIR_LEAK_IDLE:
                obj["metric"] = "fuel_trim_positive";
                obj["severity"] = "warning";
                obj["description"] = "STFT/LTFT positivos com MAP elevado em marcha lenta.";
                break;
            case FAULT_ID_CATALYST_EFF_DROP:
                obj["metric"] = "emissions_efficiency";
                obj["severity"] = "warning";
                obj["description"] = "Eficiencia de emissao degradada com pouca perda de dirigibilidade.";
                break;
            case FAULT_ID_VOLTAGE_UNSTABLE:
                obj["metric"] = "supply_voltage";
                obj["severity"] = "warning";
                obj["description"] = "Tensao oscilando de forma plausivel para alternador ou bateria instavel.";
                break;
            case FAULT_ID_MODULE_SUPPLY_INSTABILITY:
                obj["metric"] = "module_signal_jitter";
                obj["severity"] = "warning";
                obj["description"] = "Sensores secundarios mostram jitter por alimentacao eletrica instavel.";
                break;
            case FAULT_ID_ABS_WHEEL_SPEED_INCONSISTENT:
                obj["metric"] = "wheel_speed_context";
                obj["severity"] = "warning";
                obj["description"] = "Inconsistencia interna de roda/ABS pronta para integracao futura.";
                break;
            default:
                arr.remove(arr.size() - 1);
                continue;
        }
    }
}

static String diagnosticsToJson() {
    SimulationState snap = {};
    DiagnosticFreezeFrame freeze_frame = {};
    DiagnosticFreezeFrame freeze_frames[DIAG_FREEZE_FRAME_HISTORY] = {};
    bool has_freeze_frame = false;
    uint8_t freeze_frame_count = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    snap = *s_state;
    has_freeze_frame = diagnostic_get_freeze_frame(freeze_frame);
    freeze_frame_count = diagnostic_get_freeze_frames(freeze_frames, DIAG_FREEZE_FRAME_HISTORY);
    xSemaphoreGive(s_mutex);
    const VehicleProfile* profile = activeVehicleProfile(snap);

    JsonDocument doc;
    const DiagnosticScenarioId scenario_id = static_cast<DiagnosticScenarioId>(snap.scenario_id);
    const uint8_t probable_root_id = diagnostic_get_probable_root_fault_id(snap);
    const FaultCatalogEntry* probable_root = fault_catalog_get(probable_root_id);

    doc["scenario_id"] = diagnostic_scenario_slug(scenario_id);
    doc["drive_context"] = driveContextSlug(static_cast<uint8_t>(snap.sim_mode));
    doc["health_score"] = snap.health_score;
    doc["limp_mode"] = snap.limp_mode != 0;

    JsonObject vehicle = doc["vehicle"].to<JsonObject>();
    vehicle["vin"] = snap.vin;
    vehicle["profile_id"] = snap.profile_id;
    vehicle["profile_selected"] = profile != nullptr;
    if (profile) {
        vehicle["profile_brand"] = profile->brand;
        vehicle["profile_model"] = profile->model;
        vehicle["profile_year"] = profile->year;
        vehicle["profile_turbo"] = vehicleProfileIsTurbo(*profile);
    }
    vehicle["protocol"] = protoName(snap.active_protocol);
    vehicle["protocol_id"] = snap.active_protocol;
    vehicle["odometer_current_km"] = static_cast<float>(snap.odometer_total_km_x10) / 10.0f;
    vehicle["odometer_source"] = odometerSourceSlug(snap.odometer_source);
    JsonObject module_odometer = vehicle["module_odometer"].to<JsonObject>();
    const float odometer_km = static_cast<float>(snap.odometer_total_km_x10) / 10.0f;
    module_odometer["cluster_km"] = odometer_km;
    module_odometer["bcm_km"] = odometer_km;
    module_odometer["abs_km"] = odometer_km;
    module_odometer["ecm_km"] = odometer_km;

    JsonObject obd_counters = doc["obd_counters"].to<JsonObject>();
    obd_counters["distance_since_clear_km"] = snap.distance_since_clear_km;
    obd_counters["distance_mil_on_km"] = snap.distance_mil_on_km;
    obd_counters["pid_a6_supported"] = snap.pid_a6_supported != 0;

    JsonObject sensors = doc["sensors"].to<JsonObject>();
    sensors["rpm"] = snap.rpm;
    sensors["speed"] = snap.speed_kmh;
    sensors["coolant_temp"] = snap.coolant_temp_c;
    sensors["intake_temp"] = snap.intake_temp_c;
    sensors["maf"] = serialized(String(snap.maf_gs, 1));
    sensors["map"] = snap.map_kpa;
    sensors["throttle"] = snap.throttle_pct;
    sensors["engine_load"] = snap.engine_load_pct;
    sensors["battery_voltage"] = serialized(String(snap.battery_voltage, 1));
    sensors["oil_temp"] = snap.oil_temp_c;
    sensors["stft"] = serialized(String(snap.stft_pct, 1));
    sensors["ltft"] = serialized(String(snap.ltft_pct, 1));

    appendDtcArray(doc["dtcs"].to<JsonArray>(), snap.dtcs, snap.dtc_count);
    appendActiveFaults(doc["active_faults"].to<JsonArray>(), snap);
    appendAlerts(doc["alerts"].to<JsonArray>(), snap);
    appendAnomalies(doc["anomalies"].to<JsonArray>(), snap);

    if (probable_root_id != FAULT_ID_NONE) {
        JsonObject root = doc["probable_root_cause"].to<JsonObject>();
        root["id"] = probable_root->slug;
        root["label"] = probable_root->label;
        root["system"] = diagnostic_system_slug(probable_root->system);
        root["compartment"] = diagnostic_compartment_slug(probable_root->compartment);
    } else {
        doc["probable_root_cause"] = nullptr;
    }

    JsonArray derived = doc["derived_faults"].to<JsonArray>();
    for (uint8_t i = 0; i < snap.active_fault_count; i++) {
        const FaultCatalogEntry* entry = fault_catalog_get(snap.active_faults[i].fault_id);
        if (entry->fault_id != probable_root_id && entry->root_fault_id == probable_root_id) {
            derived.add(entry->slug);
        }
    }

    if (has_freeze_frame) {
        JsonObject freeze = doc["freeze_frame"].to<JsonObject>();
        freeze["fault_id"] = fault_catalog_get(freeze_frame.fault_id)->slug;
        freeze["scenario_id"] = diagnostic_scenario_slug(static_cast<DiagnosticScenarioId>(freeze_frame.scenario_id));
        freeze["source"] = freezeFrameSourceSlug(freeze_frame.source);
        freeze["sequence"] = freeze_frame.sequence;
        if (freeze_frame.dtc != 0) {
            char dtc_buf[6] = {};
            dtcValToStr(freeze_frame.dtc, dtc_buf, sizeof(dtc_buf));
            freeze["dtc"] = dtc_buf;
        }
        freeze["health_score"] = freeze_frame.health_score;
        JsonObject freeze_sensors = freeze["sensors"].to<JsonObject>();
        freeze_sensors["rpm"] = freeze_frame.rpm;
        freeze_sensors["speed"] = freeze_frame.speed_kmh;
        freeze_sensors["throttle"] = freeze_frame.throttle_pct;
        freeze_sensors["engine_load"] = freeze_frame.engine_load_pct;
        freeze_sensors["fuel_level"] = freeze_frame.fuel_level_pct;
        freeze_sensors["coolant_temp"] = freeze_frame.coolant_temp_c;
        freeze_sensors["intake_temp"] = freeze_frame.intake_temp_c;
        freeze_sensors["oil_temp"] = freeze_frame.oil_temp_c;
        freeze_sensors["maf"] = serialized(String(freeze_frame.maf_gs, 1));
        freeze_sensors["map"] = freeze_frame.map_kpa;
        freeze_sensors["ignition_advance"] = serialized(String(freeze_frame.ignition_adv, 1));
        freeze_sensors["battery_voltage"] = serialized(String(freeze_frame.battery_voltage, 1));
        freeze_sensors["stft"] = serialized(String(freeze_frame.stft_pct, 1));
        freeze_sensors["ltft"] = serialized(String(freeze_frame.ltft_pct, 1));
    } else {
        doc["freeze_frame"] = nullptr;
    }

    JsonArray freeze_history = doc["freeze_frames"].to<JsonArray>();
    for (int i = static_cast<int>(freeze_frame_count) - 1; i >= 0; i--) {
        const DiagnosticFreezeFrame& item = freeze_frames[i];
        if (!item.valid) {
            continue;
        }

        JsonObject freeze = freeze_history.add<JsonObject>();
        freeze["active"] = has_freeze_frame && item.sequence == freeze_frame.sequence;
        freeze["fault_id"] = fault_catalog_get(item.fault_id)->slug;
        freeze["scenario_id"] = diagnostic_scenario_slug(static_cast<DiagnosticScenarioId>(item.scenario_id));
        freeze["source"] = freezeFrameSourceSlug(item.source);
        freeze["sequence"] = item.sequence;
        if (item.dtc != 0) {
            char dtc_buf[6] = {};
            dtcValToStr(item.dtc, dtc_buf, sizeof(dtc_buf));
            freeze["dtc"] = dtc_buf;
        }
        freeze["health_score"] = item.health_score;
        JsonObject freeze_sensors = freeze["sensors"].to<JsonObject>();
        freeze_sensors["rpm"] = item.rpm;
        freeze_sensors["speed"] = item.speed_kmh;
        freeze_sensors["throttle"] = item.throttle_pct;
        freeze_sensors["engine_load"] = item.engine_load_pct;
        freeze_sensors["fuel_level"] = item.fuel_level_pct;
        freeze_sensors["coolant_temp"] = item.coolant_temp_c;
        freeze_sensors["intake_temp"] = item.intake_temp_c;
        freeze_sensors["oil_temp"] = item.oil_temp_c;
        freeze_sensors["maf"] = serialized(String(item.maf_gs, 1));
        freeze_sensors["map"] = item.map_kpa;
        freeze_sensors["ignition_advance"] = serialized(String(item.ignition_adv, 1));
        freeze_sensors["battery_voltage"] = serialized(String(item.battery_voltage, 1));
        freeze_sensors["stft"] = serialized(String(item.stft_pct, 1));
        freeze_sensors["ltft"] = serialized(String(item.ltft_pct, 1));
    }

    JsonObject mode06 = doc["mode06_like"].to<JsonObject>();
    JsonArray groups = mode06["failing_groups"].to<JsonArray>();
    if (probable_root_id != FAULT_ID_NONE) {
        JsonObject group = groups.add<JsonObject>();
        String group_name = String(probable_root->slug) + "_monitor";
        group["group"] = group_name;
        group["context"] = driveContextSlug(static_cast<uint8_t>(snap.sim_mode));
        JsonArray causes = group["causes"].to<JsonArray>();
        JsonObject cause = causes.add<JsonObject>();
        cause["percentage"] = 60 + (100 - snap.health_score) / 2;
        cause["cause"] = probable_root->label;
    }

    appendControlLayers(doc, snap, profile);

    String out;
    serializeJson(doc, out);
    return out;
}

static void handle_get_status(AsyncWebServerRequest* req) {
    req->send(200, "application/json", stateToJson());
}

static void handle_get_scenarios(AsyncWebServerRequest* req) {
    req->send(200, "application/json", scenariosToJson());
}

static void handle_get_diagnostics(AsyncWebServerRequest* req) {
    req->send(200, "application/json", diagnosticsToJson());
}

static void handle_post_scenario(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        req->send(400, "application/json", "{\"error\":\"json invalido\"}");
        return;
    }

    const String scenario_slug = String(doc["id"] | "");
    const DiagnosticScenarioId scenario_id = diagnostic_scenario_from_slug(scenario_slug.c_str());

    if (!scenario_slug.isEmpty() && scenario_id == DIAG_SCENARIO_NONE) {
        req->send(404, "application/json", "{\"error\":\"cenario nao encontrado\"}");
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (scenario_id == DIAG_SCENARIO_NONE) {
        diagnostic_engine_clear_scenario(*s_state, true);
        simulation_precedence_set_notice(SIM_NOTICE_SCENARIO_CLEARED);
    } else {
        diagnostic_engine_set_scenario(*s_state, scenario_id);
    }
    xSemaphoreGive(s_mutex);

    JsonDocument resp;
    resp["ok"] = true;
    resp["scenario_id"] = static_cast<uint8_t>(scenario_id);
    resp["id"] = diagnostic_scenario_slug(scenario_id);
    String out;
    serializeJson(resp, out);
    req->send(200, "application/json", out);
}

static void handle_post_params(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    clear_scenario_for_manual_edit(*s_state);
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
    bool clear_profile = false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    clear_profile = apply_protocol_selection(*s_state, pid);
    xSemaphoreGive(s_mutex);
    persist_active_protocol(pid);
    if (clear_profile) {
        clear_persisted_profile();
    }
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
    diagnostic_add_manual_dtc(*s_state, val);
    xSemaphoreGive(s_mutex);
    req->send(200, "application/json", "{\"ok\":true}");
}

static void handle_clear_dtcs(AsyncWebServerRequest* req) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    diagnostic_engine_handle_mode04_clear(*s_state);
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
    diagnostic_remove_manual_dtc(*s_state, val);
    xSemaphoreGive(s_mutex);
    req->send(200, "application/json", "{\"ok\":true}");
}

static void handle_post_mode(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    uint8_t m = doc["mode"] | 0;
    if (m >= SIM_MODE_COUNT) { req->send(400, "application/json", "{\"error\":\"invalid mode\"}"); return; }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    apply_manual_mode_selection(*s_state, static_cast<SimMode>(m));
    xSemaphoreGive(s_mutex);
    req->send(200, "application/json", "{\"ok\":true}");
}

static void handle_post_preset(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) { req->send(400); return; }
    const char* name = doc["preset"] | "idle";
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    apply_preset_selection(*s_state, name);
    xSemaphoreGive(s_mutex);
    clear_persisted_profile();
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
    dynamic_persist_odometer_now();
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
    dynamic_persist_odometer_now();
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
        obj["turbo"]    = vehicleProfileIsTurbo(VEHICLE_PROFILES[i]);
        obj["pid_a6_supported"] = vehicleProfileSupportsPidA6(VEHICLE_PROFILES[i]);
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
    dynamic_persist_odometer_now();
    apply_profile_selection(*s_state, *p);
    dynamic_reset_odometer_service_counters(*s_state);
    xSemaphoreGive(s_mutex);
    persist_active_protocol(p->protocol);
    persist_active_profile(p->id);
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
            clear_scenario_for_manual_edit(*s_state);
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
            if (pid < PROTO_COUNT) {
                const bool clear_profile = apply_protocol_selection(*s_state, pid);
                persist_active_protocol(pid);
                if (clear_profile) {
                    clear_persisted_profile();
                }
            }
        } else if (!strcmp(t, "preset")) {
            const char* name = doc["name"] | "idle";
            apply_preset_selection(*s_state, name);
            clear_persisted_profile();
        } else if (!strcmp(t, "profile")) {
            const char* pid = doc["id"] | "";
            const VehicleProfile* p = findProfile(pid);
            if (p) {
                dynamic_persist_odometer_now();
                apply_profile_selection(*s_state, *p);
                dynamic_reset_odometer_service_counters(*s_state);
                persist_active_protocol(p->protocol);
                persist_active_profile(p->id);
            }
        } else if (!strcmp(t, "mode")) {
            uint8_t m = doc["id"] | 0;
            if (m < SIM_MODE_COUNT) {
                apply_manual_mode_selection(*s_state, static_cast<SimMode>(m));
            }
        } else if (!strcmp(t, "scenario")) {
            const String scenario_slug = String(doc["id"] | "");
            const DiagnosticScenarioId scenario_id = diagnostic_scenario_from_slug(scenario_slug.c_str());
            if (scenario_slug.isEmpty()) {
                diagnostic_engine_clear_scenario(*s_state, true);
                simulation_precedence_set_notice(SIM_NOTICE_SCENARIO_CLEARED);
            } else if (scenario_id != DIAG_SCENARIO_NONE) {
                diagnostic_engine_set_scenario(*s_state, scenario_id);
            }
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
    server.on("/api/status", HTTP_GET, protect_api_get(handle_get_status));
    server.on("/api/scenarios", HTTP_GET, protect_api_get(handle_get_scenarios));
    server.on("/api/diagnostics", HTTP_GET, protect_api_get(handle_get_diagnostics));
    server.on("/api/scenario", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_post_scenario));
    server.on("/api/dtcs", HTTP_GET, protect_api_get(handle_get_dtcs));
    server.on("/api/dtcs/clear", HTTP_POST,
        protect_api_get([](AsyncWebServerRequest* r){ handle_clear_dtcs(r); }));

    server.on("/api/params", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_post_params));
    server.on("/api/protocol", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_post_protocol));
    server.on("/api/dtcs/add", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_add_dtc));
    server.on("/api/dtcs/remove", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_remove_dtc));
    server.on("/api/preset", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_post_preset));
    server.on("/api/mode", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_post_mode));
    server.on("/api/profiles", HTTP_GET, protect_api_get(handle_get_profiles));
    server.on("/api/profile", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_post_profile));
    // ⚠️ Registrar /api/wifi/scan ANTES de /api/wifi para evitar prefix-match
    server.on("/api/wifi/scan", HTTP_POST,
        protect_api_get([](AsyncWebServerRequest* r){ handle_post_wifi_scan(r); }));
    server.on("/api/wifi/scan", HTTP_GET, protect_api_get(handle_get_wifi_scan));
    server.on("/api/wifi", HTTP_GET, protect_api_get(handle_get_wifi));
    server.on("/api/wifi/remove", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_post_wifi_remove));
    server.on("/api/wifi", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_post_wifi));
    server.on("/api/reboot", HTTP_POST,
        protect_api_get([](AsyncWebServerRequest* r){ handle_post_reboot(r); }));
    server.on("/api/device/settings", HTTP_GET, protect_api_get(handle_get_device_settings));
    server.on("/api/device/settings", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_post_device_settings));
    server.on("/api/ota/info", HTTP_GET, protect_api_get(handle_get_ota_info));
    server.on("/api/ota/check", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_post_ota_check));
    server.on("/api/ota/online", HTTP_POST, protect_api_get([](AsyncWebServerRequest*){}),
        nullptr, protect_api_body(handle_post_ota_online));
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
