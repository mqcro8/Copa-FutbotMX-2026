#pragma once

#include <cstdint>

enum class MotorId : uint8_t { MOTOR_0, MOTOR_1, MOTOR_2 };

void motorControl_init();

void motorControl_setVelocity(float vx, float vy, float omega);

void motorControl_setSpeed(MotorId motor, int16_t speed);
void motorControl_stop();
void motorControl_stopAll();
void motorControl_update();

/*
 * Motor Control API — 3-Wheel Omnidirectional (Holonomic) Drive
 * =============================================================
 *
 *          FRENTE (front)
 *            ▲
 *            │
 *        [Motor 0]          ← φ₀ = 90°
 *        /       \
 *       /    ●    \          ● = robot center
 *  [Motor 1]   [Motor 2]
 *   φ₁=210°     φ₂=330°
 *
 * Inverse kinematics:
 *   v_i = -Xm·sin(φ_i) + Ym·cos(φ_i) + ω
 *   where Xm = -vy, Ym = vx  (API → math frame mapping)
 *
 * Usage:
 *   motorControl_setVelocity(vx, vy, omega);
 *
 * Parameters (all normalized -1.0 to 1.0):
 *   - vx    : Forward velocity   (+) = forward,    (-) = backward
 *   - vy    : Strafe velocity    (+) = strafe left, (-) = strafe right
 *   - omega : Angular velocity   (+) = rotate CCW,  (-) = rotate CW
 *
 * Examples:
 *   motorControl_setVelocity( 0.5,  0.0,  0.0);  // Forward at 50%
 *   motorControl_setVelocity(-0.3,  0.0,  0.0);  // Backward at 30%
 *   motorControl_setVelocity( 0.0,  0.0,  1.0);  // Rotate left (CCW) at 100%
 *   motorControl_setVelocity( 0.0,  0.0, -1.0);  // Rotate right (CW) at 100%
 *   motorControl_setVelocity( 0.0,  0.5,  0.0);  // Strafe left at 50%
 *   motorControl_setVelocity( 0.3,  0.3,  0.0);  // Diagonal forward-left
 *   motorControl_stop();                          // Stop all motors
 */