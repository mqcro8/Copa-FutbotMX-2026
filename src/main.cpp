#include <Arduino.h>
#include "config/config.h"
#include "core/state.h"
#include "drivers/motor_control.h"
#include "drivers/algorithm_test.h"
#include "debug_utils.h"

void setup() {
    Serial.begin(115200);
    LOG("MAIN", "Starting robot in test mode...");

    Core::init();
    algorithmTest_init();

    LOG("MAIN", "Test initialization complete");
}

void safeShutdown() {
    motorControl_stopAll();
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

    algorithmTest_loop();
    Core::update();
}