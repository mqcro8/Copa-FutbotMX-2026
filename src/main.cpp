#include <Arduino.h>
#include <LittleFS.h>
#include "config/config.h"
#include "core/state.h"
#include "core/SensorManager.h"
#include "drivers/motor_control.h"
#include "drivers/kicker.h"
#include "behaviors/strategy.h"
#include "behaviors/guards.h"
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
    comms_init();
    guards_init();
    strategy_init();

    WiFiMgr::init();
    LOG("MAIN", "WiFi initialized");

    if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
        LOG("MAIN", "LittleFS mount failed");
    } else {
        LOG("MAIN", "LittleFS mounted");
    }

    WebSrv::init();
    LOG("MAIN", "Role=%d init complete", static_cast<int>(Core::role));
}

void loop() {
    // 1. Kill switch — highest priority
    if (digitalRead(KILL_PIN) == LOW) {
        Core::killed = true;
        motorControl_stopAll();
        return;
    }
    if (Core::killed) {
        ESP.restart();
    }

    // Network / dashboard (non-blocking)
    WiFiMgr::update();
    WebSrv::update();

    // 2. Sensors
    sensors.update();
    Core::updateIRData(sensors.getIRData());
    if (sensors.hasColorSensors()) {
        Core::updateColorData(sensors.getColorData());
    }
    if (sensors.hasGyro()) {
        Core::updateGyroData(sensors.getGyroData());
    }
    Core::update();

    // 3. Comms + kicker timing
    comms_update();
    kicker_update();

    // 4–6. Guards then strategy (shoot transition handled in strategy before line exempt)
    guards_update();
    if (!guards_isEscaping()) {
        strategy_update();
    }

    // 7. Broadcast state to teammate
    comms_send();
}
