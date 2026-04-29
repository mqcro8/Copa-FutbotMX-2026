#include "wifi.h"
#include "debug_utils.h"
#include "../settings/settings.h"
#include <esp_wifi.h>

namespace {
    WiFiConfig s_config;
    bool s_initialized = false;
    bool s_ap_started = false;
    bool s_sta_connected = false;
    WiFiMode s_current_mode = WiFiMode::AP_ONLY;
    uint32_t s_last_sta_check_ms = 0;
    uint8_t s_sta_retry_count = 0;
    uint32_t s_sta_connect_start_ms = 0;
    bool s_sta_connecting = false;
}

static void startAP() {
    if (s_config.ap_hidden) {
        WiFi.softAPdisconnect(true);
    }

    WiFi.mode(WIFI_AP_STA);
    delay(50);

    String mac = WiFi.macAddress();
    String last4 = mac.substring(mac.length() - 5);
    last4.replace(":", "");
    String ssid = "WIFI-" + last4;

    bool ok = WiFi.softAP(
        ssid.c_str(),
        s_config.ap_password,
        s_config.ap_channel,
        s_config.ap_hidden,
        s_config.ap_max_connections
    );

    if (!ok) {
        LOG("WIFI", "AP start failed");
        return;
    }

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(s_config.ap_channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    WiFi.softAPsetHostname(s_config.ap_hostname);
    s_ap_started = true;
    LOG("WIFI", "AP started: %s @ %s (ch %d)",
        ssid.c_str(),
        WiFi.softAPIP().toString().c_str(),
        WiFi.channel());
}

static void connectSTA() {
    if (strlen(s_config.sta_ssid) == 0) {
        LOG("WIFI", "No STA SSID configured");
        s_sta_retry_count = s_config.sta_retry_max; // Stop retrying immediately
        return;
    }

    LOG("WIFI", "Connecting to STA: %s", s_config.sta_ssid);
    WiFi.begin(s_config.sta_ssid, s_config.sta_password);
    s_sta_connecting = true;
    s_sta_connect_start_ms = millis();
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
    if (now - s_last_sta_check_ms < 1000) return;
    s_last_sta_check_ms = now;

    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
        if (!s_sta_connected) {
            s_sta_connected = true;
            s_sta_connecting = false;
            s_sta_retry_count = 0;
            LOG("WIFI", "STA connected: %s @ %s",
                s_config.sta_ssid,
                WiFi.localIP().toString().c_str());
        }
    } else {
        if (s_sta_connected) {
            LOG("WIFI", "STA disconnected (reason:%d)", (int)status);
            s_sta_connected = false;
        }

        if (s_sta_connecting) {
            if (now - s_sta_connect_start_ms >= s_config.connect_timeout_ms) {
                LOG("WIFI", "STA connect timeout");
                WiFi.disconnect(true);
                s_sta_connecting = false;
                s_sta_retry_count++;
            }
        } else {
            if (s_sta_retry_count < s_config.sta_retry_max) {
                connectSTA();
            } else if (s_sta_retry_count == s_config.sta_retry_max) {
                LOG("WIFI", "STA max retries reached");
                s_sta_retry_count++;
            }
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
