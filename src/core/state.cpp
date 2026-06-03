#include "state.h"
#include "../config/config.h"
#include <Arduino.h>

namespace Core {
    RobotRole role = MY_ROLE;
    RobotState state = RobotState::IDLE;
    bool killed = false;
    int16_t current_heading = 0;
    int16_t ball_angle = 0;
    uint8_t ball_confidence = 0;
    uint32_t last_heartbeat_ms = 0;
    uint32_t dribble_start_ms = 0;
    float home_heading_deg = 0.0f;
    bool game_active = false;
    IRData irData = {};
    ColorSensorData colorData = {};
    GyroData gyroData = {};

    void init() {
        role = MY_ROLE;
        state = RobotState::IDLE;
        killed = false;
        game_active = false;
        dribble_start_ms = 0;
        pinMode(KILL_PIN, INPUT_PULLUP);
    }

    void update() {
        ball_angle = irData.angle_deg;
        ball_confidence = irData.intensity;
        if (gyroData.valid) {
            current_heading = static_cast<int16_t>(gyroData.yaw_deg);
        }
        last_heartbeat_ms = millis();
    }

    void updateIRData(const IRData& data) {
        irData = data;
    }

    void updateColorData(const ColorSensorData& data) {
        colorData = data;
    }

    void updateGyroData(const GyroData& data) {
        gyroData = data;
    }
}