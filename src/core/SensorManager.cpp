#include "SensorManager.h"
#include "debug_utils.h"

bool SensorManager::init() {
    LOG("SENS", "Initializing sensors...");
    
    _colorInitialized = _color.init();
    
    if (_colorInitialized) {
        LOG("SENS", "Color sensors initialized OK");
    } else {
        LOG("SENS", "Color sensors FAILED");
    }
    
    _irMutex = xSemaphoreCreateMutex();
    
    irSensorArray_init(); // Initialize IR sensor pins
    
    return _colorInitialized;
}

void SensorManager::update() {
    if (_colorInitialized) {
        _color.update();
    }

    if (_irMutex && xSemaphoreTake(_irMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        irSensorArray_update(&_irData);
        xSemaphoreGive(_irMutex);
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
    IRData data = {};
    if (_irMutex && xSemaphoreTake(_irMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        data = _irData;
        xSemaphoreGive(_irMutex);
    }
    return data;
}