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
    
    return _colorInitialized;
}

void SensorManager::update() {
    if (_colorInitialized) {
        _color.update();
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