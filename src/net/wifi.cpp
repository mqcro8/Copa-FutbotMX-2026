#include "wifi.h"
#include "debug_utils.h"

namespace {
    WiFiConfig s_config;
    bool s_initialized = false;
    bool s_ap_started = false;
    bool s_sta_connected = false;
    WiFiMode s_current_mode = WiFiMode::AP_ONLY;
    uint32_t s_last_sta_check_ms = 0;
    uint8_t s_sta_retry_count = 0;
}

static void startAP() {
    if (s_config.ap_hidden) {
        WiFi.softAPdisconnect(true);
    }

    bool ok = WiFi.softAP(
        s_config.ap_ssid,
        s_config.ap_password,
        s_config.ap_channel,
        s_config.ap_hidden,
        s_config.ap_max_connections
    );

    if (!ok) {
        LOG("WIFI", "AP start failed");
        return;
    }

    WiFi.softAPsetHostname(s_config.ap_hostname);
    s_ap_started = true;
    LOG("WIFI", "AP started: %s @ %s",
        s_config.ap_ssid,
        WiFi.softAPIP().toString().c_str());
}

static bool connectSTA() {
    if (strlen(s_config.sta_ssid) == 0) {
        LOG("WIFI", "No STA SSID configured");
        s_sta_retry_count = s_config.sta_retry_max; // Stop retrying immediately
        return false;
    }

    WiFi.begin(s_config.sta_ssid, s_config.sta_password);

    uint32_t start = millis();
    while (millis() - start < s_config.connect_timeout_ms) {
        if (WiFi.status() == WL_CONNECTED) {
            s_sta_connected = true;
            s_sta_retry_count = 0;
            LOG("WIFI", "STA connected: %s @ %s",
                s_config.sta_ssid,
                WiFi.localIP().toString().c_str());
            return true;
        }
        delay(100);
    }

    WiFi.disconnect(true);
    s_sta_retry_count++;
    LOG("WIFI", "STA connect timeout");
    return false;
}

void WiFiMgr::init(const WiFiConfig& config) {
    s_config = config;
    s_current_mode = config.mode;
    s_initialized = false;
    s_ap_started = false;
    s_sta_connected = false;
    s_sta_retry_count = 0;

    switch (config.mode) {
        case WiFiMode::AP_ONLY:
            WiFi.mode(WIFI_AP);
            startAP();
            break;

        case WiFiMode::STA_ONLY:
            WiFi.mode(WIFI_STA);
            connectSTA();
            break;

        case WiFiMode::AP_STA:
            WiFi.mode(WIFI_AP_STA);
            startAP();
            if (strlen(config.sta_ssid) > 0) {
                connectSTA();
            } else {
                LOG("WIFI", "STA skipped (no SSID)");
            }
            break;
    }

    s_initialized = true;
    LOG("WIFI", "WiFi initialized (AP:%s STA:%s)",
        s_ap_started ? "yes" : "no",
        s_sta_connected ? "yes" : "no");
}

void WiFiMgr::update() {
    if (!s_initialized) return;
    if (s_current_mode == WiFiMode::AP_ONLY) return;

    uint32_t now = millis();
    if (now - s_last_sta_check_ms < 2000) return;
    s_last_sta_check_ms = now;

    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
        s_sta_connected = true;
        s_sta_retry_count = 0;
    } else {
        s_sta_connected = false;

        if (status != WL_IDLE_STATUS && status != WL_DISCONNECTED) {
            LOG("WIFI", "STA disconnected (reason:%d)", (int)status);
        }

        if (s_sta_retry_count < s_config.sta_retry_max) {
            connectSTA();
        } else if (s_sta_retry_count == s_config.sta_retry_max) {
            LOG("WIFI", "STA max retries reached");
            s_sta_retry_count++;
        }
    }
}

bool WiFiMgr::isConnected() {
    if (s_current_mode == WiFiMode::AP_STA) {
        return s_ap_started || s_sta_connected;
    }
    if (s_current_mode == WiFiMode::AP_ONLY) {
        return s_ap_started;
    }
    return s_sta_connected;
}

bool WiFiMgr::isSTAConnected() {
    return s_sta_connected;
}

bool WiFiMgr::isAPStarted() {
    return s_ap_started;
}

bool WiFiMgr::isDualModeActive() {
    return s_ap_started && s_sta_connected;
}

IPAddress WiFiMgr::getSTAIP() {
    return WiFi.localIP();
}

IPAddress WiFiMgr::getAPIP() {
    return WiFi.softAPIP();
}

WiFiMode WiFiMgr::getCurrentMode() {
    return s_current_mode;
}

void WiFiMgr::restart() {
    s_initialized = false;
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    delay(200);
    init(s_config);
}