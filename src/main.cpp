#include <Arduino.h>
#include "config/config.h"
#include "core/state.h"
#include "drivers/kicker.h"
#include "debug_utils.h"

void setup() {
    Serial.begin(115200);
    LOG("MAIN", "Ready for testing...");

    Core::init();
    kicker_init();

    LOG("MAIN", "Initialization complete");
}

void safeShutdown() {
    LOG("MAIN", "Safe shutdown executed");
}

void loop() {
    if (Core::killed) {
        safeShutdown();
        while (digitalRead(KILL_PIN) == LOW) {
            delay(10);
        }
        ESP.restart();
    }

    static uint32_t lastKickTest = 0;
    uint32_t now = millis();
    if (now - lastKickTest >= 3000) {
        lastKickTest = now;
        kick();
    }

    kicker_update();
    Core::update();
}