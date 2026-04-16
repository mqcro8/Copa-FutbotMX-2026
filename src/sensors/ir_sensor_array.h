#pragma once

#include <cstdint>

struct BallVector {
    int16_t angle_deg;
    uint8_t intensity;
};

void irSensorArray_init();
BallVector irSensorArray_update();