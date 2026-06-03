#pragma once
#include <cstdint>
#include "ir_protocol.h"

constexpr uint8_t KILL_PIN = 0;

// Compile-time role: set ATTACKER on one robot, DEFENDER on the other.
constexpr RobotRole MY_ROLE = RobotRole::ATTACKER;

// Peer ESP-NOW MAC — configure with utils/getMacAddress.cpp on each board.
constexpr uint8_t PEER_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// ─── Algorithm motion parameters (Algorithm.md §9) ─────────────────────────
constexpr float FWD_SPEED = 0.7f;
constexpr float ROT_SPEED = 0.5f;
constexpr float BALL_ANGLE_THRESHOLD = 15.0f;
constexpr uint8_t APPROACH_MIN_INTENSITY = 2;
constexpr uint32_t BALL_LOST_MS = 150;
constexpr uint32_t BALL_OFFCENTER_MS = 200;
constexpr float BALL_OFFCENTER_ANGLE = 30.0f;
constexpr uint32_t MIN_FIELD_TRAVEL_MS = 2000;
constexpr uint32_t LINE_ESCAPE_MS = 300;
constexpr uint32_t WALL_ESCAPE_MS = 500;
constexpr int16_t WALL_IMPACT_THRESHOLD = 8000;
constexpr float DEFENDER_BASE_CM = 40.0f;
constexpr uint32_t REPOSITION_ALIGN_MS = 500;
constexpr uint32_t REPOSITION_MIN_MS = 1500;
constexpr uint8_t DEFEND_INTERCEPT_INTENSITY = 3;
constexpr float DEFEND_INTERCEPT_ANGLE = 45.0f;
// Motores
//  ─── Robot geometry
//  ────────────────────────────────────────────────────────────
constexpr float ROBOT_WHEEL_RADIUS = 0.03f;
constexpr float ROBOT_BASE_RADIUS = 0.10f;

// ─── PWM
// ───────────────────────────────────────────────────────────────────────
constexpr uint32_t MOTOR_PWM_FREQ = 20000;
constexpr uint8_t MOTOR_PWM_RESOLUTION = 8;
constexpr uint16_t MOTOR_PWM_MAX = (1 << MOTOR_PWM_RESOLUTION) - 1;

// ─── Motor 0 (M1 – 90°)
// ────────────────────────────────────────────────────────
constexpr uint8_t M0_PWM_PIN = 10;
constexpr uint8_t M0_IN1_PIN = 12;
constexpr uint8_t M0_IN2_PIN = 11;
constexpr uint8_t M0_PWM_CH = 0;

// ─── Motor 1 (M2 – 210°)
// ───────────────────────────────────────────────────────
constexpr uint8_t M1_PWM_PIN = 13;
constexpr uint8_t M1_IN1_PIN = 14;
constexpr uint8_t M1_IN2_PIN = 15;
constexpr uint8_t M1_PWM_CH = 1;

// ─── Motor 2 (M3 – 330°)
// ───────────────────────────────────────────────────────
constexpr uint8_t M2_PWM_PIN = 16;
constexpr uint8_t M2_IN1_PIN = 17;
constexpr uint8_t M2_IN2_PIN = 18;
constexpr uint8_t M2_PWM_CH = 2;

// ─── TB6612FNG STBY pin
// ────────────────────────────────────────────────────────
constexpr uint8_t MOTOR_STBY_PIN = 9; // or 255 if tied HIGH
//

constexpr uint8_t KICKER_PIN = 40;
constexpr uint32_t KICK_DURATION_MS = 500;
constexpr uint32_t KICK_COOLDOWN_MS = 2000;

// ─── IR Sensors (VS1838B) ──────────────────────────────────────────────────
// Layout: 3 front | wheel | 2 sensors | wheel | 2 sensors = 7 total
constexpr uint8_t IR_SENSOR_COUNT = 7;
static_assert(IR_SENSOR_COUNT == 7, "IR sensor count mismatch");
constexpr uint8_t IR_SENSOR_PINS[IR_SENSOR_COUNT] = {
    1, 2, 4, 5, 6, 7, 8 // TODO: fill with actual GPIO pins
};
constexpr int16_t IR_SENSOR_ANGLES[IR_SENSOR_COUNT] = {
    -30,  // Front-left (wheel gap)
    0,    // Front-center
    30,   // Front-right (wheel gap)
    -90,  // Side-left sensor
    -120, // Side-left rear
    90,   // Side-right sensor
    120   // Side-right rear
};

// ─── Line Sensors ────────────────────────────────────────────────────────────

//
constexpr uint8_t LINE_SENSOR_LEFT = 10;
constexpr uint8_t LINE_SENSOR_RIGHT = 11;

constexpr uint8_t COMPASS_SDA_PIN = 21;
constexpr uint8_t COMPASS_SCL_PIN = 20;
constexpr uint8_t BMI160_INT1_PIN = 19;
constexpr uint8_t BMI160_I2C_ADDR = 0x68;

constexpr uint8_t COLOR_SENSOR_SDA_PIN = 42;
constexpr uint8_t COLOR_SENSOR_SCL_PIN = 41;
constexpr uint8_t COLOR_TCA9545A_ADDR = 0x70;
constexpr uint8_t COLOR_TCS34725_ADDR = 0x29;
constexpr uint16_t COLOR_WHITE_LINE_THRESHOLD = 800;

constexpr float HEADING_KP = 2.5f;
constexpr float HEADING_KI = 0.01f;
constexpr float HEADING_KD = 0.8f;

constexpr float BALL_KP = 1.0f;
constexpr float BALL_KI = 0.0f;
constexpr float BALL_KD = 0.0f;

constexpr uint32_t MAIN_LOOP_FREQ_HZ = 100;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 50;
constexpr uint32_t COMMS_TIMEOUT_MS = 500;

// ─── WiFi
// ────────────────────────────────────────────────────────────────────────────
constexpr const char *WIFI_AP_SSID = "FutBotMX-";
constexpr const char *WIFI_AP_PASSWORD = "futbot2026";
constexpr const char *WIFI_AP_HOSTNAME = "futbotmx";
constexpr uint8_t WIFI_AP_CHANNEL = 1;
constexpr int8_t WIFI_AP_MAX_CONN = 4;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint8_t WIFI_STA_RETRY_MAX = 10;

// ─── Web Server
// ────────────────────────────────────────────────────────────────────────────────
constexpr uint16_t WEB_SERVER_PORT = 80;
constexpr const char *WEB_WWW_DIR = "/www";
constexpr const char *WEB_INDEX_FILE = "index.html";
