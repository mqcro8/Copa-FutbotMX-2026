#pragma once
#include <cstdint>
#include "sensors/ColorSensorArray.h"

class SensorManager {
public:
    bool init();
    void update();
    
    ColorSensorData getColorData() const;
    bool hasColorSensors() const;

private:
    ColorSensorArray _color;
    bool _colorInitialized;
};