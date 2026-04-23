#pragma once
#include <cstdint>
#include <WiFi.h>

enum class WiFiMode : uint8_t {
    AP_ONLY,
    STA_ONLY,
    AP_STA
};

struct WiFiConfig {
    const char* ap_ssid;
    const char* ap_password;
    const char* ap_hostname;
    const char* sta_ssid;
    const char* sta_password;
    WiFiMode mode;
    uint8_t ap_channel;
    int8_t ap_max_connections;
    int8_t ap_hidden;
    uint32_t connect_timeout_ms;
    uint8_t sta_retry_max;
};

constexpr WiFiConfig DEFAULT_WIFI_CONFIG = {
    .ap_ssid = "FutBotMX-Robot",
    .ap_password = "futbot2026",
    .ap_hostname = "futbotmx",
    .sta_ssid = "",
    .sta_password = "",
    .mode = WiFiMode::AP_STA,
    .ap_channel = 1,
    .ap_max_connections = 4,
    .ap_hidden = 0,
    .connect_timeout_ms = 15000,
    .sta_retry_max = 10
};

namespace WiFiMgr {
    void init(const WiFiConfig& config = DEFAULT_WIFI_CONFIG);
    void update();
    bool isConnected();
    bool isSTAConnected();
    bool isAPStarted();
    bool isDualModeActive();
    IPAddress getSTAIP();
    IPAddress getAPIP();
    WiFiMode getCurrentMode();
    void restart();
}