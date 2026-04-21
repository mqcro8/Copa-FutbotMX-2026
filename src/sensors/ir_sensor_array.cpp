#include "ir_sensor_array.h"
#include "config/config.h"
#include <Arduino.h>
#include <cmath>

// Estrategia "monoestable retrigerable":
// Cualquier pulso LOW (pelota detectada) activa el sensor por IR_HOLD_MS.
// Si llegan mas pulsos, el temporizador se reinicia.
// Tras IR_HOLD_MS sin pulsos, el sensor se apaga.
constexpr uint32_t IR_HOLD_MS = 1000;

// ─── init ────────────────────────────────────────────────────────────────────
void IRSensorArray::init() {
    _mutex = xSemaphoreCreateMutex();

    for (uint8_t i = 0; i < IR_SENSOR_COUNT; ++i) {
        pinMode(IR_SENSOR_PINS[i], INPUT_PULLUP);
        _data.values[i] = HIGH;
        _lastDetectedMs[i] = 0;
    }
    _data.angle_deg = 0;
    _data.intensity = 0;
    _data.detected  = false;
}

// ─── update ──────────────────────────────────────────────────────────────────
void IRSensorArray::update() {
    if (!_mutex) return;

    uint32_t now = millis();

    // Lectura cruda fuera del mutex (digitalRead es atomico)
    uint8_t raw[IR_SENSOR_COUNT];
    for (uint8_t i = 0; i < IR_SENSOR_COUNT; ++i) {
        raw[i] = digitalRead(IR_SENSOR_PINS[i]);
    }

    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) != pdTRUE) return;

    // Monoestable retrigerable por cada sensor
    for (uint8_t i = 0; i < IR_SENSOR_COUNT; ++i) {
        if (raw[i] == LOW) {
            _lastDetectedMs[i] = now;
        }

        bool held = (_lastDetectedMs[i] != 0)
                 && (now - _lastDetectedMs[i] < IR_HOLD_MS);
        _data.values[i] = held ? LOW : HIGH;
    }

    // Calcular angulo promedio de los sensores activos
    int16_t activeAngles[IR_SENSOR_COUNT];
    uint8_t activeCount = 0;
    for (uint8_t i = 0; i < IR_SENSOR_COUNT; ++i) {
        if (_data.values[i] == LOW) {
            activeAngles[activeCount++] = IR_SENSOR_ANGLES[i];
        }
    }

    if (activeCount == 0) {
        _data.angle_deg = 0;
        _data.intensity = 0;
        _data.detected  = false;
    } else {
        _data.angle_deg = (activeCount == 1)
            ? activeAngles[0]
            : normalizeAngle(circularMean(activeAngles, activeCount));
        _data.intensity = activeCount;
        _data.detected  = true;
    }

    xSemaphoreGive(_mutex);
}

// ─── getters (thread-safe) ───────────────────────────────────────────────────
IRData IRSensorArray::getData() const {
    IRData copy = {};
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        copy = _data;
        xSemaphoreGive(_mutex);
    }
    return copy;
}

uint8_t IRSensorArray::getFilteredValue(uint8_t index) const {
    if (index >= IR_SENSOR_COUNT) return HIGH;
    uint8_t val = HIGH;
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        val = _data.values[index];
        xSemaphoreGive(_mutex);
    }
    return val;
}

uint8_t IRSensorArray::readRawPin(uint8_t index) {
    if (index >= IR_SENSOR_COUNT) return HIGH;
    return digitalRead(IR_SENSOR_PINS[index]);
}

// ─── utilidades matematicas (privadas) ───────────────────────────────────────
int16_t IRSensorArray::normalizeAngle(int16_t angle) {
    while (angle > 180)  angle -= 360;
    while (angle < -180) angle += 360;
    return angle;
}

int16_t IRSensorArray::circularMean(const int16_t* angles, uint8_t count) {
    float sinSum = 0.0f, cosSum = 0.0f;
    for (uint8_t i = 0; i < count; ++i) {
        float rad = angles[i] * static_cast<float>(DEG_TO_RAD);
        sinSum += sinf(rad);
        cosSum += cosf(rad);
    }
    return static_cast<int16_t>(atan2f(sinSum, cosSum) * static_cast<float>(RAD_TO_DEG));
}
