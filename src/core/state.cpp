#include "state.h"

namespace Core {
    RobotRole role = RobotRole::ATTACKER;
    RobotState state = RobotState::IDLE;
    bool killed = false;
    int16_t current_heading = 0;
    int16_t ball_angle = 0;
    uint8_t ball_confidence = 0;
    uint32_t last_heartbeat_ms = 0;
    IRData irData = {};
    ColorSensorData colorData = {};
    GyroData gyroData = {};

    void init() {
        state = RobotState::IDLE;
        killed = false;
    }

    void update() {
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