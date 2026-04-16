#pragma once
#include <cstdint>

constexpr uint8_t  KILL_PIN             = 0;
//Motores
// ─── Robot geometry ────────────────────────────────────────────────────────────
constexpr float    ROBOT_WHEEL_RADIUS   = 0.03f;
constexpr float    ROBOT_BASE_RADIUS    = 0.10f;

// ─── PWM ───────────────────────────────────────────────────────────────────────
constexpr uint32_t MOTOR_PWM_FREQ       = 20000;
constexpr uint8_t  MOTOR_PWM_RESOLUTION = 8;
constexpr uint16_t MOTOR_PWM_MAX        = (1 << MOTOR_PWM_RESOLUTION) - 1;

// ─── Motor 0 (M1 – 90°) ────────────────────────────────────────────────────────
constexpr uint8_t  M0_PWM_PIN  = 10;
constexpr uint8_t  M0_IN1_PIN  = 11;
constexpr uint8_t  M0_IN2_PIN  = 12;
constexpr uint8_t  M0_PWM_CH   = 0;

// ─── Motor 1 (M2 – 210°) ───────────────────────────────────────────────────────
constexpr uint8_t  M1_PWM_PIN  = 13;
constexpr uint8_t  M1_IN1_PIN  = 14;
constexpr uint8_t  M1_IN2_PIN  = 15;
constexpr uint8_t  M1_PWM_CH   = 1;

// ─── Motor 2 (M3 – 330°) ───────────────────────────────────────────────────────
constexpr uint8_t  M2_PWM_PIN  = 16;
constexpr uint8_t  M2_IN1_PIN  = 17;
constexpr uint8_t  M2_IN2_PIN  = 18;
constexpr uint8_t  M2_PWM_CH   = 2;

// ─── TB6612FNG STBY pin ────────────────────────────────────────────────────────
constexpr uint8_t  MOTOR_STBY_PIN = 4; // or 255 if tied HIGH
//

constexpr uint8_t  KICKER_PIN           = 3;

// ─── IR Sensors (VS1838B) ──────────────────────────────────────────────────
constexpr uint8_t IR_SENSOR_COUNT    = 8;
constexpr uint8_t IR_SENSOR_PINS[IR_SENSOR_COUNT] = { 19, 20, 21, 22, 23, 24, 25, 26 };

// ─── Line Sensors ────────────────────────────────────────────────────────────

//
constexpr uint8_t  LINE_SENSOR_LEFT   = 10;
constexpr uint8_t  LINE_SENSOR_RIGHT  = 11;

// ── IR Ball Tracker Sensors (VS1838B — active LOW) ─────────────────────────────
constexpr uint8_t IR_FRONT_LEFT_PIN   = 1;   // ← adjust to your actual GPIO
constexpr uint8_t IR_FRONT_CENTER_PIN   = 2;
constexpr uint8_t IR_FRONT_RIGHT_PIN   = 3;
// Uncomment when adding peripheral sensors:
// constexpr uint8_t IR_SIDE_LEFT_PIN   = 4
// constexpr uint8_t IR_SIDE_RIGHT_PIN   = 5
// constexpr uint8_t IR_REAR_PIN   = 6

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