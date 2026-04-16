#pragma once

#include <cstdint>

enum class MotorId : uint8_t { MOTOR_0, MOTOR_1, MOTOR_2 };

void motorControl_init();
void motorControl_setVelocity(float vx, float vy, float omega);
void motorControl_setSpeed(MotorId motor, int16_t speed);
void motorControl_stop();
void motorControl_stopAll();
void motorControl_update();