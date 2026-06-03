#include <Arduino.h>
#include <LittleFS.h>
#include "config/config.h"
#include "core/state.h"
#include "core/SensorManager.h"
#include "drivers/motor_control.h"
#include "drivers/kicker.h"
#include "behaviors/strategy.h"
#include "comms/comms.h"
#include "net/wifi.h"
#include "net/web_server.h"
#include "debug_utils.h"

static SensorManager sensors;

void setup() {
    Serial.begin(115200);
    delay(500);
    LOG("MAIN", "FutBotMX 2026 booting...");

    Core::init();
    motorControl_init();
    kicker_init();
    sensors.init();
    strategy_init();
    comms_init();

    WiFiMgr::init();
    LOG("MAIN", "WiFi initialized");

    if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
        LOG("MAIN", "LittleFS mount failed");
    } else {
        LOG("MAIN", "LittleFS mounted");
    }

    WebSrv::init();
    LOG("MAIN", "Initialization complete");
}

void loop() {
    // ─── 1. Kill switch (always first) ──────────────────────────
    if (digitalRead(KILL_PIN) == LOW) {
        if (!Core::killed) {
            Core::killed = true;
            motorControl_stopAll();
            LOG("MAIN", "KILL: emergency stop");
        }
        WiFiMgr::update();
        WebSrv::update();
        return;
    }

    if (Core::killed) {
        LOG("MAIN", "KILL: released, restarting...");
        delay(100);
        ESP.restart();
    }

    // ─── 2. Sensors ─────────────────────────────────────────────
    sensors.update();
    Core::updateIRData(sensors.getIRData());
    if (sensors.hasColorSensors()) {
        Core::updateColorData(sensors.getColorData());
    }
    if (sensors.hasGyro()) {
        Core::updateGyroData(sensors.getGyroData());
    }
    Core::update();

    // ─── 3. Comms receive (poll peer state) ─────────────────────
    Core::peerMsg = comms_getLastMessage();
    if (comms_isConnected()) {
        Core::lastPeerMsgMs = millis();
    }

    // ─── 4. Strategy (wallGuard + lineGuard + FSM + motor) ──────
    strategy_update();

    // ─── 5. Comms send ──────────────────────────────────────────
    comms_send();

    // ─── 6. Network (non-blocking) ──────────────────────────────
    WiFiMgr::update();
    WebSrv::update();
}
