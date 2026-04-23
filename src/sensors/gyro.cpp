#include "gyro.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "debug_utils.h"

namespace {
    float _yaw = 0.0f;
    float _pitch = 0.0f;
    float _roll = 0.0f;
    uint32_t _lastUpdate = 0;
    bool _firstReading = true;
}

GyroBMI160::GyroBMI160() {}

bool GyroBMI160::init() {
    LOG("GYRO", "Initializing BMI160...");

    Wire.begin(COMPASS_SDA_PIN, COMPASS_SCL_PIN);
    Wire.setClock(400000);

    uint8_t chipId = readRegister(REG_CHIP_ID);
    LOG("GYRO", "Chip ID: 0x%02X", chipId);

    if (chipId != CHIP_ID) {
        LOG("GYRO", "Invalid chip ID: expected 0x%02X", CHIP_ID);
        return false;
    }

    writeRegister(0x40, 0x00);
    writeRegister(0x40, 0x00);

    delay(1);

    writeRegister(REG_CMD, CMD_ACCEL_NORMAL);
    delay(20);
    writeRegister(REG_CMD, CMD_GYRO_NORMAL);
    delay(20);

    _mutex = xSemaphoreCreateMutex();
    if (_mutex == nullptr) {
        LOG("GYRO", "Failed to create mutex");
        return false;
    }

    _initialized = true;
    LOG("GYRO", "BMI160 initialized successfully");
    return true;
}

void GyroBMI160::update() {
    if (!_initialized) return;

    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        readRawData();
        xSemaphoreGive(_mutex);
    }
}

GyroData GyroBMI160::getData() const {
    GyroData result;
    if (_mutex != nullptr && xSemaphoreTake(const_cast<SemaphoreHandle_t>(_mutex), pdMS_TO_TICKS(10)) == pdTRUE) {
        result = _data;
        xSemaphoreGive(const_cast<SemaphoreHandle_t>(_mutex));
    } else {
        result = _data;
    }
    return result;
}

uint8_t GyroBMI160::readRegister(uint8_t reg) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return 0xFF;
    Wire.requestFrom(I2C_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

bool GyroBMI160::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool GyroBMI160::readRawData() {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(REG_GYRO_X_L);
    if (Wire.endTransmission() != 0) return false;

    uint8_t bytes[12];
    Wire.requestFrom(I2C_ADDR, (uint8_t)12);
    if (Wire.available() < 12) return false;

    for (uint8_t i = 0; i < 12; i++) {
        bytes[i] = Wire.read();
    }

    _data.gyro_x = (int16_t)(bytes[0] | (bytes[1] << 8));
    _data.gyro_y = (int16_t)(bytes[2] | (bytes[3] << 8));
    _data.gyro_z = (int16_t)(bytes[4] | (bytes[5] << 8));
    _data.acc_x = (int16_t)(bytes[6] | (bytes[7] << 8));
    _data.acc_y = (int16_t)(bytes[8] | (bytes[9] << 8));
    _data.acc_z = (int16_t)(bytes[10] | (bytes[11] << 8));

    float gx = (float)_data.gyro_x / 131.0f;
    float gy = (float)_data.gyro_y / 131.0f;
    float gz = (float)_data.gyro_z / 131.0f;

    uint32_t now = micros();
    if (_lastUpdate > 0 && !_firstReading) {
        float dt = (float)(now - _lastUpdate) / 1000000.0f;

        _pitch += gx * dt;
        _roll += gy * dt;
        _yaw += gz * dt;
    } else {
        _firstReading = false;

        float ax = (float)_data.acc_x / 16384.0f;
        float ay = (float)_data.acc_y / 16384.0f;
        float az = (float)_data.acc_z / 16384.0f;
        _pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / M_PI;
        _roll = atan2(ay, az) * 180.0f / M_PI;
    }
    _lastUpdate = now;

    // Wrap yaw to -180 to 180 for heading display
    while (_yaw > 180.0f) _yaw -= 360.0f;
    while (_yaw < -180.0f) _yaw += 360.0f;

    _data.pitch_deg = _pitch;
    _data.roll_deg = _roll;
    _data.yaw_deg = _yaw;

    uint8_t tempReg = 0x22;
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(tempReg);
    if (Wire.endTransmission() == 0) {
        Wire.requestFrom(I2C_ADDR, (uint8_t)1);
        if (Wire.available()) {
            uint8_t rawTemp = Wire.read();
            _data.temp_celsius = (float)rawTemp / 512.0f + 23.0f;
        }
    }

    _data.valid = true;
    return true;
}