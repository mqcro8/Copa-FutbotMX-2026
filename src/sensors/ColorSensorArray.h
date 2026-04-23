#pragma once
#include <cstdint>
#include <array>
#include "config/config.h"
#include "Adafruit_TCS34725.h"
#include "freertos/FreeRTOS.h"

enum class SensorPosition : uint8_t {
    FRONT = 0,
    RIGHT = 1,
    BACK  = 2,
    LEFT  = 3
};

struct ColorReading {
    uint16_t red   = 0;
    uint16_t green = 0;
    uint16_t blue  = 0;
    uint16_t clear = 0;
};

struct ColorSensorData {
    std::array<ColorReading, 4> readings{};
    std::array<bool, 4> valid{};
    std::array<bool, 4> onWhiteLine{};
};

class ColorSensorArray {
public:
    static constexpr uint8_t NUM_SENSORS = 4;

    uint8_t init();                   // Retorna número de sensores inicializados
    ColorSensorData update();        // Round-robin: lee 1 sensor por ciclo
    ColorSensorData getData() const; // Snapshot thread-safe
    uint8_t sensorsAvailable() const;
    bool isValid(SensorPosition pos) const;

private:
    bool selectChannel(uint8_t channel);
    void deselectChannel();
    bool initSensor(uint8_t index);
    ColorReading readSingleSensor(uint8_t index);

    std::array<Adafruit_TCS34725, NUM_SENSORS> _sensors{};
    std::array<bool, NUM_SENSORS> _initialized{};
    ColorSensorData _lastData{};
    mutable SemaphoreHandle_t _mutex = nullptr;
    uint8_t _currentSensorIdx = 0;
    uint8_t _sensorsAvailable = 0;
};
