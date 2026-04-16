#pragma once

#include <cstdint>

struct BallVector {
    int16_t angle_deg;
    uint8_t intensity;
    bool detected;
};

enum class BallZone {
    NOT_DETECTED,
    FRONT_CENTER,
    FRONT_LEFT,
    FRONT_RIGHT,
    SIDE_LEFT,
    SIDE_RIGHT,
    REAR
};

void irSensorArray_init();
BallVector irSensorArray_update();
BallZone irSensorArray_getZone();
bool irSensorArray_isInKickerZone();