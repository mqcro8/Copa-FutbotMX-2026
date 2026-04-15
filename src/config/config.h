#pragma once
#include <cstdint>

constexpr uint8_t  KILL_PIN             = 0;
constexpr uint8_t  MOTOR_PWM_PIN        = 1;
constexpr uint8_t  MOTOR_DIR_PIN        = 2;
constexpr uint8_t  KICKER_PIN           = 3;

constexpr uint8_t  IR_SENSOR_COUNT      = 8;

constexpr uint8_t  LINE_SENSOR_LEFT   = 10;
constexpr uint8_t  LINE_SENSOR_RIGHT  = 11;

constexpr uint8_t  COMPASS_SDA_PIN      = 8;
constexpr uint8_t  COMPASS_SCL_PIN      = 9;

constexpr uint8_t  COLOR_SENSOR_SDA_PIN = 8;
constexpr uint8_t  COLOR_SENSOR_SCL_PIN = 9;
constexpr uint8_t  COLOR_TCA9545A_ADDR   = 0x70;
constexpr uint8_t  COLOR_TCS34725_ADDR   = 0x29;

constexpr float HEADING_KP = 2.5f;
constexpr float HEADING_KI = 0.01f;
constexpr float HEADING_KD = 0.8f;

constexpr float BALL_KP = 1.0f;
constexpr float BALL_KI = 0.0f;
constexpr float BALL_KD = 0.0f;

constexpr uint32_t MAIN_LOOP_FREQ_HZ = 100;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 50;
constexpr uint32_t COMMS_TIMEOUT_MS = 500;