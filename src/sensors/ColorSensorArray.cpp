#include "ColorSensorArray.h"
#include <Wire.h>
#include "debug_utils.h"

namespace {
    constexpr const char* POSITION_NAMES[] = {"FRONT", "RIGHT", "BACK", "LEFT"};
}

bool ColorSensorArray::init() {
    _mutex = xSemaphoreCreateMutex();
    if (_mutex == nullptr) {
        LOG("COLOR", "Failed to create mutex");
        return false;
    }

    Wire.begin(COLOR_SENSOR_SDA_PIN, COLOR_SENSOR_SCL_PIN);
    
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        _initialized[i] = false;
    }

    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        if (!initSensor(i)) {
            LOG("COLOR", "Failed to init sensor %s", POSITION_NAMES[i]);
        }
    }

    bool all_ok = true;
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        if (!_initialized[i]) {
            all_ok = false;
            LOG("COLOR", "Sensor %s not available", POSITION_NAMES[i]);
        }
    }

    if (all_ok) {
        LOG("COLOR", "All 4 color sensors initialized");
    }

    return all_ok;
}

bool ColorSensorArray::selectChannel(uint8_t channel) {
    if (channel >= NUM_SENSORS) {
        return false;
    }

    Wire.beginTransmission(COLOR_TCA9545A_ADDR);
    Wire.write(1 << channel);
    return Wire.endTransmission() == 0;
}

bool ColorSensorArray::initSensor(uint8_t index) {
    if (!selectChannel(index)) {
        return false;
    }

    delay(10);

    Adafruit_TCS34725* sensor = &_sensors[index];
    if (sensor->begin(COLOR_TCS34725_ADDR, &Wire)) {
        sensor->setIntegrationTime(TCS34725_INTEGRATIONTIME_154MS);
        sensor->setGain(TCS34725_GAIN_1X);
        _initialized[index] = true;
        return true;
    }

    return false;
}

ColorReading ColorSensorArray::readSensor(SensorPosition pos) {
    uint8_t idx = static_cast<uint8_t>(pos);
    ColorReading reading = {0, 0, 0, 0};

    if (idx >= NUM_SENSORS) {
        return reading;
    }

    if (!_initialized[idx]) {
        return reading;
    }

    if (!selectChannel(idx)) {
        return reading;
    }

    uint16_t r, g, b, c;
    _sensors[idx].getRawData(&r, &g, &b, &c);

    reading.red = r;
    reading.green = g;
    reading.blue = b;
    reading.clear = c;

    return reading;
}

ColorSensorData ColorSensorArray::readAllSensors() {
    ColorSensorData data;
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        if (_initialized[i]) {
            data.readings[i] = readSensor(static_cast<SensorPosition>(i));
            data.valid[i] = true;
        } else {
            data.valid[i] = false;
        }
    }
    return data;
}

ColorSensorData ColorSensorArray::update() {
    if (_mutex == nullptr) {
        return _lastData;
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    _lastData = readAllSensors();
    xSemaphoreGive(_mutex);
    return _lastData;
}

ColorSensorData ColorSensorArray::getData() const {
    ColorSensorData copy = {};
    if (_mutex == nullptr) {
        return copy;
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    copy = _lastData;
    xSemaphoreGive(_mutex);
    return copy;
}

bool ColorSensorArray::isValid(SensorPosition pos) const {
    return _initialized[static_cast<uint8_t>(pos)];
}

static ColorSensorArray g_colorSensors;

void colorSensorArray_init() {
    g_colorSensors.init();
}

ColorSensorData colorSensorArray_update() {
    return g_colorSensors.update();
}