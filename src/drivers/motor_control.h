#pragma once

void motorControl_init();
void motorControl_setVelocity(float vx, float vy, float omega);
void motorControl_stop();
void motorControl_update();