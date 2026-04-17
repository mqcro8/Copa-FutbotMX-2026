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
 * Motor Control API
 * =================
 * 
 * The robot uses a 3-wheel omnidirectional (holonomic) drive system.
 * 
 * Usage:
 *   motorControl_setVelocity(vx, vy, omega);
 * 
 * Parameters:
 *   - vx    : Forward velocity (-1.0 to 1.0)
 *             positive = forward, negative = backward
 *   - vy    : Lateral velocity (-1.0 to 1.0)
 *             positive = left, negative = right
 *   - omega : Angular velocity (-1.0 to 1.0)
 *             positive = rotate left (CCW), negative = rotate right (CW)
 * 
 * Examples:
 *   motorControl_setVelocity(0.5, 0.0, 0.0);   // Forward at 50%
 *   motorControl_setVelocity(-0.3, 0.0, 0.0);  // Backward at 30%
 *   motorControl_setVelocity(0.0, 0.0, 1.0);    // Rotate left (CCW) at 100%
 *   motorControl_setVelocity(0.0, 0.0, -1.0);   // Rotate right (CW) at 100%
 *   motorControl_setVelocity(0.3, 0.3, 0.0);    // Diagonal forward-left
 *   motorControl_stop();                         // Stop all motors
 */