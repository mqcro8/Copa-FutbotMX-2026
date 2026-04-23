#pragma once
#include <cstdint>
#include "sensors/ColorSensorArray.h"
#include "sensors/ir_sensor_array.h"
#include "sensors/gyro.h"

class SensorManager {
public:
    bool init();
    void update();
    
    ColorSensorData getColorData() const;
    bool hasColorSensors() const;

    IRData getIRData() const;

    GyroData getGyroData() const;
    bool hasGyro() const { return _gyroInitialized; }

private:
    ColorSensorArray _color;
    bool _colorInitialized = false;

    IRSensorArray _ir;

    GyroBMI160 _gyro;
    bool _gyroInitialized = false;
};