#include <Arduino.h>
#include "config/config.h"
#include "core/state.h"
#include "drivers/motor_control.h"
#include "sensors/ball_tracker.h"
#include "sensors/line_detector.h"
#include "sensors/compass.h"
#include "comms/comms.h"
#include "behaviors/strategy.h"
#include "debug_utils.h"

static uint32_t lastHeartbeat = 0;

void setup() {
    Serial.begin(115200);
    LOG("MAIN", "Starting robot...");

    Core::init();
    motorControl_init();
    ballTracker_init();
    lineDetector_init();
    compass_init();
    comms_init();
    strategy_init();

    LOG("MAIN", "Initialization complete");
}

void safeShutdown() {
    motorControl_stop();
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

    uint32_t now = millis();

    auto ball = ballTracker_update();
    Core::ball_angle = ball.angle_deg;
    Core::ball_confidence = ball.intensity;

    Core::current_heading = compass_readHeading();

    strategy_update();
    Core::state = strategy_getState();

    motorControl_update();

    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        comms_send();
        lastHeartbeat = now;
    }

    Core::update();
}