#pragma once
#include <stdint.h>

enum class RobotRole : uint8_t { ATTACKER = 0, DEFENDER = 1 };
enum class RobotState : uint8_t { IDLE, SEARCH, APPROACH, DRIBBLE, SHOOT, DEFEND, REPOSITION, AVOID_PENALTY };

struct __attribute__((packed)) RobotMsg {
    uint32_t   timestamp_ms;
    RobotRole role;
    RobotState state;
    int16_t    ball_angle_deg;
    uint8_t    ball_confidence;
    int16_t    heading_deg;
    uint8_t    latency_reply_ms;
    uint8_t    crc;
};