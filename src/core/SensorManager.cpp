#include "SensorManager.h"
#include "debug_utils.h"

bool SensorManager::init() {
    LOG("SENS", "Initializing sensors...");
    
    uint8_t colorCount = _color.init();
    _colorInitialized = (colorCount > 0);

    if (_colorInitialized) {
        LOG("SENS", "Color sensors OK (%d/%d available)", colorCount, ColorSensorArray::NUM_SENSORS);
    } else {
        LOG("SENS", "Color sensors: NONE available");
    }
    
    _ir.init();
    LOG("SENS", "IR sensor array initialized");

    _gyroInitialized = _gyro.init();
    if (_gyroInitialized) {
        LOG("SENS", "Gyro BMI160 OK");
    } else {
        LOG("SENS", "Gyro: not available");
    }
    
    return _colorInitialized || _gyroInitialized;
}

void SensorManager::update() {
    if (_colorInitialized) {
        _color.update();
    }

    _ir.update();

    if (_gyroInitialized) {
        _gyro.update();
    }
}

ColorSensorData SensorManager::getColorData() const {
    if (_colorInitialized) {
        return _color.getData();
    }
    return {};
}

bool SensorManager::hasColorSensors() const {
    return _colorInitialized;
}

IRData SensorManager::getIRData() const {
    return _ir.getData();
}

GyroData SensorManager::getGyroData() const {
    if (_gyroInitialized) {
        return _gyro.getData();
    }
    return {};
}