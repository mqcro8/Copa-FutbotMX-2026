#include "web_server.h"
#include "wifi.h"
#include "debug_utils.h"
#include "../core/state.h"
#include "../config/config.h"
#include "../sensors/gyro.h"
#include <LittleFS.h>

namespace {
    WebServerConfig s_config;
    AsyncWebServer* s_server = nullptr;
    WebServerState s_state = WebServerState::STOPPED;
    uint32_t s_request_count = 0;
    uint32_t s_start_ms = 0;
}

static void setCORS(AsyncWebServerResponse* response) {
    if (!s_config.enable_cors) return;
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void handleStatus(AsyncWebServerRequest* request) {
    s_request_count++;
    char json[512];
    int len = snprintf_P(json, sizeof(json),
        PSTR("{\"server\":\"ok\",\"uptime_ms\":%lu,\"requests\":%lu,"
             "\"wifi\":{\"ap\":%s,\"sta\":%s,\"sta_ip\":\"%s\",\"ap_ip\":\"%s\"},"
             "\"memory\":{\"free\":%lu}}"),
        WebSrv::uptime(),
        s_request_count,
        WiFiMgr::isAPStarted() ? "true" : "false",
        WiFiMgr::isSTAConnected() ? "true" : "false",
        WiFiMgr::getSTAIP().toString().c_str(),
        WiFiMgr::getAPIP().toString().c_str(),
        (unsigned long)ESP.getFreeHeap()
    );

    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", json);
    setCORS(resp);
    resp->setCode(200);
    request->send(resp);
}

static void handleState(AsyncWebServerRequest* request) {
    s_request_count++;
    const char* roleStr = "UNKNOWN";
    switch (Core::role) {
        case RobotRole::ATTACKER: roleStr = "ATTACKER"; break;
        case RobotRole::DEFENDER: roleStr = "DEFENDER"; break;
    }

    const char* stateStr = "UNKNOWN";
    switch (Core::state) {
        case RobotState::IDLE:          stateStr = "IDLE";         break;
        case RobotState::SEARCH:           stateStr = "SEARCH";        break;
        case RobotState::APPROACH:        stateStr = "APPROACH";      break;
        case RobotState::DRIBBLE:         stateStr = "DRIBBLE";       break;
        case RobotState::SHOOT:         stateStr = "SHOOT";       break;
        case RobotState::DEFEND:           stateStr = "DEFEND";        break;
        case RobotState::REPOSITION:        stateStr = "REPOSITION";   break;
        case RobotState::INTERCEPT:         stateStr = "INTERCEPT";    break;
        case RobotState::AVOID_PENALTY:    stateStr = "AVOID_PENALTY"; break;
    }

    char json[256];
    snprintf_P(json, sizeof(json),
        PSTR("{\"role\":\"%s\",\"state\":\"%s\",\"killed\":%s,"
             "\"heading_deg\":%d,\"ball_angle_deg\":%d,\"ball_confidence\":%u}"),
        roleStr, stateStr,
        Core::killed ? "true" : "false",
        (int16_t)Core::gyroData.yaw_deg,
        Core::ball_angle,
        Core::ball_confidence
    );

    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", json);
    setCORS(resp);
    request->send(resp);
}

static void handleTelemetry(AsyncWebServerRequest* request) {
    s_request_count++;

    int16_t heading = (int16_t)Core::gyroData.yaw_deg;

    char json[2048];
    int len = snprintf_P(json, sizeof(json),
        PSTR("{\"core\":{\"role\":%d,\"state\":%d,\"killed\":%s,"
             "\"heading\":%d,\"ball_angle\":%d,\"ball_conf\":%u},\"ir\":{"),
        (uint8_t)Core::role, (uint8_t)Core::state,
        Core::killed ? "true" : "false",
        heading,
        Core::ball_angle,
        Core::ball_confidence
    );

    for (uint8_t i = 0; i < IR_SENSOR_COUNT; i++) {
        len += snprintf(json + len, sizeof(json) - len,
            "%s\"s%d\":%d",
            i > 0 ? "," : "",
            i, (int)Core::irData.values[i]
        );
    }

    len += snprintf(json + len, sizeof(json) - len,
        PSTR("},\"color\":{"));

    for (uint8_t i = 0; i < 4; i++) {
        len += snprintf(json + len, sizeof(json) - len,
            "%s\"s%d\":{\"c\":%u,\"r\":%u,\"g\":%u,\"b\":%u,\"white\":%s}",
            i > 0 ? "," : "",
            i,
            Core::colorData.readings[i].clear,
            Core::colorData.readings[i].red,
            Core::colorData.readings[i].green,
            Core::colorData.readings[i].blue,
            Core::colorData.onWhiteLine[i] ? "true" : "false"
        );
    }

    static const GyroData& gyroData = Core::gyroData;
    len += snprintf(json + len, sizeof(json) - len,
        PSTR("},\"gyro\":{\"gx\":%d,\"gy\":%d,\"gz\":%d,\"ax\":%d,\"ay\":%d,\"az\":%d,\"pitch\":%.1f,\"roll\":%.1f,\"yaw\":%.1f,\"temp\":%.1f,\"valid\":%s}"),
        gyroData.gyro_x, gyroData.gyro_y, gyroData.gyro_z,
        gyroData.acc_x, gyroData.acc_y, gyroData.acc_z,
        gyroData.pitch_deg, gyroData.roll_deg, gyroData.yaw_deg,
        gyroData.temp_celsius,
        gyroData.valid ? "true" : "false"
    );

    len += snprintf(json + len, sizeof(json) - len,
        PSTR(",\"uptime_ms\":%lu}"),
        millis() - s_start_ms
    );

    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", json);
    setCORS(resp);
    request->send(resp);
}

static void handleCommand(AsyncWebServerRequest* request) {
    s_request_count++;

    if (request->method() == HTTP_OPTIONS) {
        AsyncWebServerResponse* resp = request->beginResponse(204);
        setCORS(resp);
        request->send(resp);
        return;
    }

    if (request->method() != HTTP_POST) {
        request->send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
        return;
    }

    if (!request->hasParam("cmd", true)) {
        request->send(400, "application/json", "{\"error\":\"missing_cmd_param\"}");
        return;
    }

    const String& cmd = request->getParam("cmd", true)->value();
    char response[128];

    if (cmd == "ping") {
        snprintf(response, sizeof(response),
            PSTR("{\"cmd\":\"pong\",\"ms\":%lu}"), millis());

    } else if (cmd == "emergency_stop") {
        Core::killed = true;
        snprintf(response, sizeof(response),
            PSTR("{\"cmd\":\"emergency_stop\",\"ok\":true}"));

    } else if (cmd == "reset") {
        Core::killed = false;
        Core::state = RobotState::IDLE;
        snprintf(response, sizeof(response),
            PSTR("{\"cmd\":\"reset\",\"ok\":true}"));

    } else if (cmd == "restart") {
        snprintf(response, sizeof(response),
            PSTR("{\"cmd\":\"restart\",\"ok\":true}"));
        delay(100);
        ESP.restart();

    } else {
        snprintf(response, sizeof(response),
            PSTR("{\"error\":\"unknown_cmd\"}"));
    }

    request->send(200, "application/json", response);
}

static void handleRoot(AsyncWebServerRequest* request) {
    s_request_count++;
    if (LittleFS.exists("/index.html")) {
        request->send(LittleFS, "/index.html", "text/html");
    } else if (LittleFS.exists("/www/index.html")) {
        request->send(LittleFS, "/www/index.html", "text/html");
    } else {
        request->send(200, "text/plain",
            "FutBotMX 2026\nNo web UI. Upload files to /data folder and reflash LittleFS.");
    }
}

static void handleNotFound(AsyncWebServerRequest* request) {
    s_request_count++;
    request->send(404, "application/json", "{\"error\":\"not_found\"}");
}

void WebSrv::init(const WebServerConfig& config) {
    s_config = config;
    s_state = WebServerState::STARTING;
    s_request_count = 0;
    s_start_ms = millis();

    if (s_server != nullptr) {
        delete s_server;
        s_server = nullptr;
    }

    s_server = new AsyncWebServer(s_config.port);
    if (s_server == nullptr) {
        s_state = WebServerState::ERROR;
        LOG("WEBSRV", "Failed to allocate server");
        return;
    }

    s_server->on("/", HTTP_GET, handleRoot);
    s_server->on("/api/status", HTTP_GET, handleStatus);
    s_server->on("/api/state", HTTP_GET, handleState);
    s_server->on("/api/telemetry", HTTP_GET, handleTelemetry);
    s_server->on("/api/command", HTTP_POST, handleCommand);
    
    // Catch-all for static files in the /data folder
    s_server->serveStatic("/", LittleFS, "/");
    
    s_server->onNotFound(handleNotFound);

    s_server->begin();
    s_state = WebServerState::RUNNING;

    LOG("WEBSRV", "Server running on port %d", s_config.port);
}

void WebSrv::update() {}

void WebSrv::stop() {
    if (s_server != nullptr) {
        s_server->end();
        delete s_server;
        s_server = nullptr;
    }
    s_state = WebServerState::STOPPED;
    LOG("WEBSRV", "Server stopped");
}

WebServerState WebSrv::state() {
    return s_state;
}

uint32_t WebSrv::requestCount() {
    return s_request_count;
}

uint32_t WebSrv::uptime() {
    return millis() - s_start_ms;
}