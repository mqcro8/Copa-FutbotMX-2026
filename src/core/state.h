#pragma once
#include "../shared/ir_protocol.h"
#include "../sensors/ir_sensor_array.h"
#include "../sensors/ColorSensorArray.h"
#include "../sensors/gyro.h"
#include <cstdint>

namespace Core {
    extern RobotRole role;
    extern RobotState state;
    extern bool killed;
    extern int16_t current_heading;
    extern int16_t ball_angle;
    extern uint8_t ball_confidence;
    extern uint32_t last_heartbeat_ms;
    extern uint32_t dribble_start_ms;
    extern float home_heading_deg;
    extern bool game_active;
    extern IRData irData;
    extern ColorSensorData colorData;
    extern GyroData gyroData;

    void init();
    void update();
    void updateIRData(const IRData& data);
    void updateColorData(const ColorSensorData& data);
    void updateGyroData(const GyroData& data);
}