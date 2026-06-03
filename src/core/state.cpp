#include "state.h"
#include "config/config.h"

namespace Core {
    RobotRole role = MY_ROLE;
    RobotState state = RobotState::IDLE;
    bool killed = false;
    int16_t current_heading = 0;
    int16_t ball_angle = 0;
    uint8_t ball_confidence = 0;
    uint32_t last_heartbeat_ms = 0;
    IRData irData = {};
    ColorSensorData colorData = {};
    GyroData gyroData = {};
    RobotMsg peerMsg = {};
    uint32_t lastPeerMsgMs = 0;

    void init() {
        role = MY_ROLE;
        state = RobotState::IDLE;
        killed = false;
    }

    void update() {
        ball_angle = irData.angle_deg;
        ball_confidence = irData.intensity;
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