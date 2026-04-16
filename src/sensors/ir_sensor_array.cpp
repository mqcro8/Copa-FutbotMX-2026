#include "ir_sensor_array.h"
#include "config/config.h"
#include <Arduino.h>

namespace {
uint8_t sensorValues[IR_SENSOR_COUNT] = {0};
uint32_t lastUpdate = 0;
}  // namespace

void irSensorArray_init() {
    for (uint8_t i = 0; i < IR_SENSOR_COUNT; ++i) {
        pinMode(IR_SENSOR_PINS[i], INPUT);
    }
}

BallVector irSensorArray_update() {
    for (uint8_t i = 0; i < IR_SENSOR_COUNT; ++i) {
        sensorValues[i] = digitalRead(IR_SENSOR_PINS[i]);
    }

    int16_t bestAngle = -1;
    uint8_t maxIntensity = 0;
    int16_t weightedSum = 0;
    uint8_t activeCount = 0;

    for (uint8_t i = 0; i < IR_SENSOR_COUNT; ++i) {
        if (sensorValues[i] == HIGH) {
            int16_t angle = (i * 360) / IR_SENSOR_COUNT;
            weightedSum += angle;
            ++activeCount;
            if (maxIntensity == 0) {
                bestAngle = angle;
            }
        }
    }

    BallVector result;
    if (activeCount > 0) {
        if (activeCount == 1) {
            result.angle_deg = bestAngle;
        } else {
            result.angle_deg = weightedSum / activeCount;
        }
        result.intensity = activeCount;
    } else {
        result.angle_deg = -1;
        result.intensity = 0;
    }

    lastUpdate = millis();
    return result;
}