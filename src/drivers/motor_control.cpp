#include "motor_control.h"
#include "config/config.h"

#include <Arduino.h>
#include <cmath>
#include <algorithm>

// ─── Internal motor descriptor ─────────────────────────────────────────────────
struct Motor {
    uint8_t pwmPin;
    uint8_t in1Pin;
    uint8_t in2Pin;
    uint8_t pwmChannel;
};

static constexpr Motor kMotors[3] = {
    { M0_PWM_PIN, M0_IN1_PIN, M0_IN2_PIN, M0_PWM_CH }, // wheel 0 – 90°
    { M1_PWM_PIN, M1_IN1_PIN, M1_IN2_PIN, M1_PWM_CH }, // wheel 1 – 210°
    { M2_PWM_PIN, M2_IN1_PIN, M2_IN2_PIN, M2_PWM_CH }, // wheel 2 – 330°
};

// ─── Wheel angles (radians) ────────────────────────────────────────────────────
static constexpr float kWheelAngle[3] = {
    static_cast<float>(M_PI / 2.0),        //  90°
    static_cast<float>(M_PI * 7.0 / 6.0),  // 210°
    static_cast<float>(M_PI * 11.0 / 6.0), // 330°
};

static void motor_drive(const Motor& motor, float speed)
{
    // Clamp speed
    speed = std::max(-1.0f, std::min(1.0f, speed));

    // Optional deadband (reduces jitter)
    if (fabs(speed) < 0.02f) speed = 0.0f;

    if (speed > 0.0f) {
        digitalWrite(motor.in1Pin, HIGH);
        digitalWrite(motor.in2Pin, LOW);
    } 
    else if (speed < 0.0f) {
        digitalWrite(motor.in1Pin, LOW);
        digitalWrite(motor.in2Pin, HIGH);
        speed = -speed;
    } 
    else {
        // Coast (or change to HIGH/HIGH for brake)
        digitalWrite(motor.in1Pin, LOW);
        digitalWrite(motor.in2Pin, LOW);
    }

    uint32_t duty = static_cast<uint32_t>(speed * MOTOR_PWM_MAX);
    ledcWrite(motor.pwmChannel, duty);
}

void motorControl_init()
{
    // Enable TB6612FNG (STBY)
    if (MOTOR_STBY_PIN != 255) {
        pinMode(MOTOR_STBY_PIN, OUTPUT);
        digitalWrite(MOTOR_STBY_PIN, HIGH);
    }

    for (const auto& m : kMotors) {
        // Direction pins
        pinMode(m.in1Pin, OUTPUT);
        pinMode(m.in2Pin, OUTPUT);

        digitalWrite(m.in1Pin, LOW);
        digitalWrite(m.in2Pin, LOW);

        // PWM setup (ESP32 LEDC)
        ledcSetup(m.pwmChannel, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
        ledcAttachPin(m.pwmPin, m.pwmChannel);
        ledcWrite(m.pwmChannel, 0);
    }
}

void motorControl_setVelocity(float vx, float vy, float omega)
{
    // API convention:   vx = forward (+), vy = strafe-left (+), omega = CCW (+)
    // Math convention:  Xm = right,       Ym = forward
    // Map:  Xm = -vy,  Ym = vx
    const float mathX = -vy;   // strafe-left in API  → negative X in math frame
    const float mathY =  vx;   // forward in API      → positive Y in math frame

    float speeds[3];
    float maxAbs = 0.0f;

    // Cinemática inversa 3WD: v_i = -Xm·sin(φ_i) + Ym·cos(φ_i) + ω
    // Con ángulos φ = {90°, 210°, 330°} los cosenos/senos se precalculan:
    //   Motor 0 (90°):  sin=1,      cos=0       → v0 = -Xm + 0 + ω
    //   Motor 1 (210°): sin=-0.5,   cos=-0.866  → v1 = 0.5·Xm - 0.866·Ym + ω
    //   Motor 2 (330°): sin=-0.5,   cos=0.866   → v2 = 0.5·Xm + 0.866·Ym + ω
    for (int i = 0; i < 3; ++i) {
        const float phi = kWheelAngle[i];
        speeds[i] = -mathX * sinf(phi)
                    + mathY * cosf(phi)
                    + omega;  // omega ya está normalizado -1..1

        const float a = fabsf(speeds[i]);
        if (a > maxAbs) maxAbs = a;
    }

    // Normalizar para mantener proporciones si alguna rueda excede ±1.0
    const float scale = (maxAbs > 1.0f) ? (1.0f / maxAbs) : 1.0f;

    for (int i = 0; i < 3; ++i) {
        motor_drive(kMotors[i], speeds[i] * scale);
    }
}

void motorControl_setSpeed(MotorId motor, int16_t speed)
{
    uint8_t idx = static_cast<uint8_t>(motor);
    if (idx >= 3) return;

    float normalized = static_cast<float>(speed) / 255.0f;
    motor_drive(kMotors[idx], normalized);
}

void motorControl_stop()
{
    for (const auto& m : kMotors) {
        ledcWrite(m.pwmChannel, 0);

        digitalWrite(m.in1Pin, LOW);
        digitalWrite(m.in2Pin, LOW);
    }
}

void motorControl_stopAll()
{
    motorControl_stop();
}

void motorControl_update()
{
    // Reserved for future features:
    // - acceleration limiting
    // - watchdog timeout
}

void test_motor(uint8_t motorIndex)
{
    if (motorIndex >= 3) return; // safety check

    const Motor& m = kMotors[motorIndex];

    // Setup pins
    pinMode(m.in1Pin, OUTPUT);
    pinMode(m.in2Pin, OUTPUT);

    ledcSetup(m.pwmChannel, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcAttachPin(m.pwmPin, m.pwmChannel);

    // Enable driver
    if (MOTOR_STBY_PIN != 255) {
        pinMode(MOTOR_STBY_PIN, OUTPUT);
        digitalWrite(MOTOR_STBY_PIN, HIGH);
    }

    // ─── Forward ─────────────────────────
    digitalWrite(m.in1Pin, HIGH);
    digitalWrite(m.in2Pin, LOW);
    ledcWrite(m.pwmChannel, MOTOR_PWM_MAX * 0.6);
    delay(3000);

    // ─── Stop ────────────────────────────
    ledcWrite(m.pwmChannel, 0);
    digitalWrite(m.in1Pin, LOW);
    digitalWrite(m.in2Pin, LOW);
    delay(1000);

    // ─── Reverse ─────────────────────────
    digitalWrite(m.in1Pin, LOW);
    digitalWrite(m.in2Pin, HIGH);
    ledcWrite(m.pwmChannel, MOTOR_PWM_MAX * 0.6);
    delay(3000);

    // ─── Final stop ──────────────────────
    ledcWrite(m.pwmChannel, 0);
    digitalWrite(m.in1Pin, LOW);
    digitalWrite(m.in2Pin, LOW);
}