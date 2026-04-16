#include <Arduino.h>
#include "config/config.h"
#include "core/state.h"
#include "drivers/motor_control.h"
#include "sensors/ir_sensor_array.h"
#include "debug_utils.h"

void setup() {
    Serial.begin(115200);
    LOG("MAIN", "Starting robot in test mode...");

    Core::init();
    motorControl_init();
    irSensorArray_init();

    LOG("MAIN", "Initialization complete");
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

    auto ball = irSensorArray_update();

    LOG("IR", "Zone=%d Angle=%d Intensity=%d Detected=%d",
        (int)irSensorArray_getZone(), ball.angle_deg, ball.intensity, ball.detected);

    Core::update();
}