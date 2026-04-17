#include <Arduino.h>
#include "config/config.h"
#include "core/state.h"
#include "sensors/ir_sensor_array.h"
#include "debug_utils.h"

void setup() {
    Serial.begin(115200);
    LOG("MAIN", "Ready for testing...");

    Core::init();
    irSensorArray_init();

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

    static uint8_t lastValue = 2;
    uint8_t currentValue = irSensorArray_readPin(1);
    if (currentValue != lastValue) {
        lastValue = currentValue;
        LOG("IR", "Pin1=%d", currentValue);
    }

    Core::update();
}