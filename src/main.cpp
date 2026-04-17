#include <Arduino.h>
#include "config/config.h"
#include "core/state.h"
#include "core/SensorManager.h"
#include "drivers/motor_control.h"
#include "debug_utils.h"

static SensorManager sensors;

void setup() {
    Serial.begin(115200);
    LOG("MAIN", "Ready for testing...");

    Core::init();
    motorControl_init();
    sensors.init();

    LOG("MAIN", "Initialization complete");
}

void safeShutdown() {
    LOG("MAIN", "Safe shutdown executed");
    motorControl_stopAll();
}

void loop() {
    if (Core::killed) {
        safeShutdown();
        while (digitalRead(KILL_PIN) == LOW) {
            delay(10);
        }
        ESP.restart();
    }

    sensors.update();
    Core::updateIRData(sensors.getIRData());

    static uint32_t lastMotorTest = 0;
    static uint8_t step = 0;
    uint32_t now = millis();

    if (now - lastMotorTest >= 5000) {
        lastMotorTest = now;
        step = (step + 1) % 4;

        switch (step) {
            case 0:
                LOG("MTR", "Forward");
                motorControl_setVelocity(0.3f, 0.0f, 0.0f);
                break;
            case 1:
                LOG("MTR", "Backward");
                motorControl_setVelocity(-0.3f, 0.0f, 0.0f);
                break;
            case 2:
                LOG("MTR", "Rotate Left");
                motorControl_setVelocity(0.0f, 0.0f, 1.0f);
                break;
            case 3:
                LOG("MTR", "Rotate Right");
                motorControl_setVelocity(0.0f, 0.0f, -1.0f);
                break;
        }
    } else if (now - lastMotorTest >= 500) {
        motorControl_stop();
    }
}