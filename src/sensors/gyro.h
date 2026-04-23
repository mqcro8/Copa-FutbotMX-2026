#pragma once
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config/config.h"

struct GyroData {
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t acc_x;
    int16_t acc_y;
    int16_t acc_z;
    float temp_celsius;
    float pitch_deg;
    float roll_deg;
    float yaw_deg;
    bool valid;
};

class GyroBMI160 {
public:
    GyroBMI160();
    bool init();
    void update();
    GyroData getData() const;

    bool isInitialized() const { return _initialized; }

private:
    bool writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
    bool readRawData();

    static constexpr uint8_t I2C_ADDR = BMI160_I2C_ADDR;

    static constexpr uint8_t REG_CHIP_ID = 0x00;
    static constexpr uint8_t REG_ACC_X_L = 0x12;
    static constexpr uint8_t REG_GYRO_X_L = 0x0C;
    static constexpr uint8_t REG_CMD = 0x7E;
    static constexpr uint8_t REG_PMU_STATUS = 0x03;

    static constexpr uint8_t CHIP_ID = 0xD1;
    static constexpr uint8_t CMD_ACCEL_NORMAL = 0x11;
    static constexpr uint8_t CMD_GYRO_NORMAL = 0x15;

    bool _initialized = false;
    SemaphoreHandle_t _mutex = nullptr;
    GyroData _data = {};
};