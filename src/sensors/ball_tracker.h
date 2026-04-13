#pragma once
#include <cstdint>

struct BallVector {
    int16_t angle_deg;
    uint8_t intensity;
};

void ballTracker_init();
BallVector ballTracker_update();