#pragma once
#include "../shared/ir_protocol.h"
#include <cstdint>

namespace Core {
    extern RobotRole role;
    extern RobotState state;
    extern bool killed;
    extern int16_t current_heading;
    extern int16_t ball_angle;
    extern uint8_t ball_confidence;
    extern uint32_t last_heartbeat_ms;

    void init();
    void update();
}