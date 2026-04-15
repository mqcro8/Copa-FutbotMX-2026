#pragma once
#include <cstdint>
#include <array>
#include "config/config.h"
#include "Adafruit_TCS34725.h"
#include "freertos/FreeRTOS.h"

enum class SensorPosition : uint8_t {
    FRONT = 0,
    RIGHT = 1,
    BACK = 2,
    LEFT = 3
};

struct ColorReading {
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint16_t clear;
};

struct ColorSensorData {
    std::array<ColorReading, 4> readings;
    std::array<bool, 4> valid;
};

class ColorSensorArray {
public:
    static constexpr uint8_t NUM_SENSORS = 4;

    bool init();
    ColorSensorData update();
    ColorSensorData getData() const;  // Thread-safe read

    ColorReading readSensor(SensorPosition pos);
    bool isValid(SensorPosition pos) const;

private:
    bool selectChannel(uint8_t channel);
    bool initSensor(uint8_t index);
    ColorSensorData readAllSensors();

    std::array<Adafruit_TCS34725, NUM_SENSORS> _sensors;
    std::array<bool, NUM_SENSORS> _initialized;
    ColorSensorData _lastData;
    mutable SemaphoreHandle_t _mutex;
};

void colorSensorArray_init();
ColorSensorData colorSensorArray_update();