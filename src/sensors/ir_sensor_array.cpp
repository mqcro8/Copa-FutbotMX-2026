#include "ir_sensor_array.h"
#include "config/config.h"
#include <Arduino.h>
#include <cmath>

namespace {
uint8_t sensorValues[IR_SENSOR_COUNT] = {0};
BallZone currentZone = BallZone::NOT_DETECTED;
BallVector currentVector = { -1, 0, false };
}  // namespace

void irSensorArray_init() {
    for (uint8_t i = 0; i < IR_SENSOR_COUNT; ++i) {
        pinMode(IR_SENSOR_PINS[i], INPUT_PULLUP);
    }
}

uint8_t irSensorArray_readPin(uint8_t index) {
    if (index >= IR_SENSOR_COUNT) return 0;
    return digitalRead(IR_SENSOR_PINS[index]);
}

static int16_t normalizeAngle(int16_t angle) {
    while (angle > 180) angle -= 360;
    while (angle < -180) angle += 360;
    return angle;
}

static int16_t circularMean(const int16_t* angles, uint8_t count) {
    float sinSum = 0.0f, cosSum = 0.0f;
    for (uint8_t i = 0; i < count; ++i) {
        float rad = angles[i] * (float)DEG_TO_RAD;
        sinSum += sinf(rad);
        cosSum += cosf(rad);
    }
    return (int16_t)(atan2f(sinSum, cosSum) * (float)RAD_TO_DEG);
}

static void classifyZone(int16_t angle) {
    int16_t absAngle = abs(angle);
    if (absAngle <= 15) {
        currentZone = BallZone::FRONT_CENTER;
    } else if (angle > -60 && angle < 0) {
        currentZone = BallZone::FRONT_LEFT;
    } else if (angle > 0 && angle < 60) {
        currentZone = BallZone::FRONT_RIGHT;
    } else if (angle <= -60) {
        currentZone = (absAngle < 120) ? BallZone::SIDE_LEFT : BallZone::REAR;
    } else {
        currentZone = (absAngle < 120) ? BallZone::SIDE_RIGHT : BallZone::REAR;
    }
}

IRSensorReadings irSensorArray_read() {
    IRSensorReadings reading;
    reading.count = IR_SENSOR_COUNT;

    for (uint8_t i = 0; i < IR_SENSOR_COUNT; ++i) {
        sensorValues[i] = digitalRead(IR_SENSOR_PINS[i]);
        reading.values[i] = sensorValues[i];
    }

    return reading;
}

BallVector irSensorArray_analyze() {
    int16_t activeAngles[IR_SENSOR_COUNT];
    uint8_t activeCount = 0;

    for (uint8_t i = 0; i < IR_SENSOR_COUNT; ++i) {
        if (sensorValues[i] == LOW) {
            activeAngles[activeCount++] = IR_SENSOR_ANGLES[i];
        }
    }

    if (activeCount == 0) {
        currentZone = BallZone::NOT_DETECTED;
        currentVector = { -1, 0, false };
        return currentVector;
    }

    currentVector.angle_deg = activeCount == 1
        ? activeAngles[0]
        : normalizeAngle(circularMean(activeAngles, activeCount));
    currentVector.intensity = activeCount;
    currentVector.detected = true;

    classifyZone(currentVector.angle_deg);

    return currentVector;
}

BallZone irSensorArray_getZone() {
    return currentZone;
}

bool irSensorArray_isInKickerZone() {
    return currentZone == BallZone::FRONT_CENTER;
}

void irSensorArray_update(IRData* data) {
    if (!data) return;

    for (uint8_t i = 0; i < IR_SENSOR_COUNT; ++i) {
        sensorValues[i] = digitalRead(IR_SENSOR_PINS[i]);
        data->values[i] = sensorValues[i];
    }

    int16_t activeAngles[IR_SENSOR_COUNT];
    uint8_t activeCount = 0;
    for (uint8_t i = 0; i < IR_SENSOR_COUNT; ++i) {
        if (sensorValues[i] == LOW) {
            activeAngles[activeCount++] = IR_SENSOR_ANGLES[i];
        }
    }

    if (activeCount == 0) {
        data->angle_deg = 0;
        data->intensity = 0;
        data->detected = false;
        return;
    }

    data->angle_deg = (activeCount == 1)
        ? activeAngles[0]
        : normalizeAngle(circularMean(activeAngles, activeCount));
    data->intensity = activeCount;
    data->detected = true;
}