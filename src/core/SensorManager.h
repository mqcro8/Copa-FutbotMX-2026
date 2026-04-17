#pragma once
#include <cstdint>
#include "sensors/ColorSensorArray.h"
#include "sensors/ir_sensor_array.h"
#include "freertos/FreeRTOS.h"
#include "semphr.h"

class SensorManager {
public:
    bool init();
    void update();
    
    ColorSensorData getColorData() const;
    bool hasColorSensors() const;

    IRData getIRData() const;

private:
    ColorSensorArray _color;
    bool _colorInitialized;

    SemaphoreHandle_t _irMutex;
    IRData _irData;
};