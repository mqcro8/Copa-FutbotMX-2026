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
uint8_t irSensorArray_readPin(uint8_t index);  // Read single pin value
IRSensorReadings irSensorArray_read();       // Clase 1: lectura-cruda
BallVector irSensorArray_analyze();            // Clase 2: analisis
BallZone irSensorArray_getZone();
bool irSensorArray_isInKickerZone();