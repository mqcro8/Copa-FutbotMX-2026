#pragma once

#include <cstdint>

constexpr uint8_t MAX_IR_SENSORS = 7;

struct IRSensorReadings {
    uint8_t values[MAX_IR_SENSORS];
    uint8_t count;
};

struct BallVector {
    int16_t angle_deg;
    uint8_t intensity;
    bool detected;
};

struct IRData {
    uint8_t values[MAX_IR_SENSORS];
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
uint8_t irSensorArray_readPin(uint8_t index);
IRSensorReadings irSensorArray_read();
BallVector irSensorArray_analyze();
BallZone irSensorArray_getZone();
bool irSensorArray_isInKickerZone();
void irSensorArray_update(IRData* data);

bool irSensorArray_getStable(uint8_t index, uint32_t debounceMs);