#include "guards.h"
#include "config/config.h"
#include "core/state.h"
#include "sensors/ColorSensorArray.h"
#include "drivers/motor_control.h"
#include <Arduino.h>
#include <cmath>

namespace {
    uint32_t lineEscapeUntil = 0;
    uint32_t wallEscapeUntil = 0;
    float wallEscapeVx = 0.0f;
    float wallEscapeVy = 0.0f;
    float lineEscapeVx = 0.0f;
    float lineEscapeVy = 0.0f;

    bool shootLineExempt() {
        if (Core::state != RobotState::DRIBBLE) {
            return false;
        }
        if (Core::dribble_start_ms == 0) {
            return false;
        }
        if (millis() - Core::dribble_start_ms <= MIN_FIELD_TRAVEL_MS) {
            return false;
        }
        return Core::colorData.onWhiteLine[static_cast<uint8_t>(SensorPosition::FRONT)];
    }
}

void guards_init() {
    lineEscapeUntil = 0;
    wallEscapeUntil = 0;
}

bool guards_isEscaping() {
    const uint32_t now = millis();
    return (lineEscapeUntil > now) || (wallEscapeUntil > now);
}

void guards_update() {
    const uint32_t now = millis();

    if (wallEscapeUntil > now) {
        motorControl_setVelocity(wallEscapeVx, wallEscapeVy, 0.0f);
        return;
    }

    if (lineEscapeUntil > now) {
        motorControl_setVelocity(lineEscapeVx, lineEscapeVy, 0.0f);
        return;
    }

    if (Core::gyroData.valid) {
        const int16_t ax = Core::gyroData.acc_x;
        const int16_t ay = Core::gyroData.acc_y;
        if (std::abs(ax) > WALL_IMPACT_THRESHOLD || std::abs(ay) > WALL_IMPACT_THRESHOLD) {
            wallEscapeVx = (ax > 0) ? -0.8f : 0.8f;
            wallEscapeVy = (ay > 0) ? -0.8f : 0.8f;
            motorControl_setVelocity(wallEscapeVx, wallEscapeVy, 0.0f);
            wallEscapeUntil = now + WALL_ESCAPE_MS;
            if (Core::role == RobotRole::DEFENDER) {
                Core::state = RobotState::REPOSITION;
            }
            return;
        }
    }

    if (shootLineExempt()) {
        return;
    }

    const bool front = Core::colorData.onWhiteLine[static_cast<uint8_t>(SensorPosition::FRONT)];
    const bool right = Core::colorData.onWhiteLine[static_cast<uint8_t>(SensorPosition::RIGHT)];
    const bool back  = Core::colorData.onWhiteLine[static_cast<uint8_t>(SensorPosition::BACK)];
    const bool left  = Core::colorData.onWhiteLine[static_cast<uint8_t>(SensorPosition::LEFT)];

    if (front || right || back || left) {
        lineEscapeVx = front ? -0.8f : (back ? 0.8f : 0.0f);
        lineEscapeVy = right ? -0.8f : (left ? 0.8f : 0.0f);
        motorControl_setVelocity(lineEscapeVx, lineEscapeVy, 0.0f);
        lineEscapeUntil = now + LINE_ESCAPE_MS;
        if (Core::role == RobotRole::DEFENDER) {
            Core::state = RobotState::REPOSITION;
        }
    }
}
