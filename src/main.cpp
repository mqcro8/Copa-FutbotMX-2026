#include <Arduino.h>
#include <LittleFS.h>
#include "config/config.h"
#include "core/state.h"
#include "drivers/motor_control.h"
#include "net/wifi.h"
#include "net/web_server.h"
#include "settings/settings.h"
#include "comms/comms.h"
#include "debug_utils.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    LOG("MAIN", "FutBotMX 2026 booting...");
    LOG("MAIN", "Own MAC: %s", WiFi.macAddress().c_str());

    Settings::init();

    Core::init();
    motorControl_init();

    WiFiMgr::init();
    LOG("MAIN", "WiFi initialized");

    if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
        LOG("MAIN", "LittleFS mount failed");
    } else {
        LOG("MAIN", "LittleFS mounted");
    }

    WebSrv::init();
    LOG("MAIN", "Web server initialized");

    Comms::init();
    LOG("MAIN", "ESPNOW initialized");

    LOG("MAIN", "Waiting 5 seconds before sending test...");
    delay(5000);

    String mac = WiFi.macAddress();
    String last4 = mac.substring(mac.length() - 5);
    last4.replace(":", "");
    char testMsg[32];
    snprintf(testMsg, sizeof(testMsg), "TEST robot-%s", last4.c_str());
    LOG("MAIN", "Sending: %s", testMsg);
    Comms::sendTest(testMsg);

    LOG("MAIN", "Initialization complete");
}

void loop() {
    if (Core::killed) {
        LOG("MAIN", "Emergency stop active");
        motorControl_stopAll();
        while (digitalRead(KILL_PIN) == LOW) {
            delay(10);
        }
        ESP.restart();
    }

    WiFiMgr::update();
    WebSrv::update();
    Comms::update();
}
