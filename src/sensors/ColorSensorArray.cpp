#include "ColorSensorArray.h"
#include <Wire.h>
#include "debug_utils.h"

namespace {
    constexpr const char* POSITION_NAMES[] = {"FRONT", "RIGHT", "BACK", "LEFT"};
    constexpr TickType_t MUTEX_TIMEOUT = pdMS_TO_TICKS(50);
}

// ─── Inicialización (solo se ejecuta una vez en setup) ──────────────────────

uint8_t ColorSensorArray::init() {
    _mutex = xSemaphoreCreateMutex();
    if (_mutex == nullptr) {
        LOG("COLOR", "Failed to create mutex");
        return 0;
    }

    // Bus I2C dedicado para los sensores de color (no interfiere con compass en Wire)
    Wire1.begin(COLOR_SENSOR_SDA_PIN, COLOR_SENSOR_SCL_PIN);

    _sensorsAvailable = 0;
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        if (initSensor(i)) {
            _sensorsAvailable++;
        } else {
            LOG("COLOR", "Sensor %s not available", POSITION_NAMES[i]);
        }
    }

    LOG("COLOR", "%d of %d color sensors initialized", _sensorsAvailable, NUM_SENSORS);
    return _sensorsAvailable;
}

bool ColorSensorArray::initSensor(uint8_t index) {
    if (!selectChannel(index)) {
        LOG("COLOR", "Failed to select channel %d", index);
        return false;
    }

    // Settle time tras cambiar canal — solo necesario durante init
    delay(50);

    // Verificar presencia física del sensor en el bus
    Wire1.beginTransmission(COLOR_TCS34725_ADDR);
    uint8_t err = Wire1.endTransmission(true);
    if (err != 0) {
        LOG("COLOR", "Sensor %d not found at 0x29, I2C error: %d", index, err);
        deselectChannel();
        return false;
    }

    if (_sensors[index].begin(COLOR_TCS34725_ADDR, &Wire1)) {
        _sensors[index].setIntegrationTime(TCS34725_INTEGRATIONTIME_50MS);
        _sensors[index].setGain(TCS34725_GAIN_4X);
        _initialized[index] = true;
        deselectChannel();
        return true;
    }

    deselectChannel();
    return false;
}

// ─── Multiplexor TCA9545A ───────────────────────────────────────────────────

bool ColorSensorArray::selectChannel(uint8_t channel) {
    if (channel >= NUM_SENSORS) {
        return false;
    }

    Wire1.beginTransmission(COLOR_TCA9545A_ADDR);
    Wire1.write(1 << channel);
    uint8_t error = Wire1.endTransmission();

    if (error != 0) {
        LOG("COLOR", "TCA9545A channel %d select error: %d", channel, error);
        return false;
    }

    return true;
}

void ColorSensorArray::deselectChannel() {
    Wire1.beginTransmission(COLOR_TCA9545A_ADDR);
    Wire1.write(0x00);
    Wire1.endTransmission();
}

// ─── Lectura de sensores ────────────────────────────────────────────────────
// Lectura directa de registros — evita el delay(51ms) interno de getRawData()

static constexpr uint8_t TCS_COMMAND_BIT = 0x80;
static constexpr uint8_t TCS_REG_CDATAL  = 0x14;
static constexpr uint8_t TCS_REG_RDATAL  = 0x16;
static constexpr uint8_t TCS_REG_GDATAL  = 0x18;
static constexpr uint8_t TCS_REG_BDATAL  = 0x1A;

static uint16_t readReg16(uint8_t reg) {
    uint8_t cmd = TCS_COMMAND_BIT | reg;
    Wire1.beginTransmission(COLOR_TCS34725_ADDR);
    Wire1.write(cmd);
    if (Wire1.endTransmission() != 0) {
        return 0;
    }
    if (Wire1.requestFrom(COLOR_TCS34725_ADDR, (uint8_t)2) != 2) {
        return 0;
    }
    uint16_t lo = Wire1.read();
    uint16_t hi = Wire1.read();
    return (hi << 8) | lo;
}

ColorReading ColorSensorArray::readSingleSensor(uint8_t index) {
    ColorReading reading;

    if (!selectChannel(index)) {
        return reading;
    }

    // Settling time mínimo tras cambio de canal del mux (~200µs)
    delayMicroseconds(200);

    reading.clear = readReg16(TCS_REG_CDATAL);
    reading.red   = readReg16(TCS_REG_RDATAL);
    reading.green = readReg16(TCS_REG_GDATAL);
    reading.blue  = readReg16(TCS_REG_BDATAL);

    deselectChannel();
    return reading;
}

// ─── Update round-robin (1 sensor por ciclo, non-blocking) ──────────────────

ColorSensorData ColorSensorArray::update() {
    if (_mutex == nullptr) {
        return _lastData;
    }

    if (xSemaphoreTake(_mutex, MUTEX_TIMEOUT) != pdTRUE) {
        LOG("COLOR", "Mutex timeout on update()");
        return _lastData;
    }

    const uint8_t idx = _currentSensorIdx;

    if (_initialized[idx]) {
        _lastData.readings[idx] = readSingleSensor(idx);
        _lastData.valid[idx] = true;
        _lastData.onWhiteLine[idx] = _lastData.readings[idx].clear > COLOR_WHITE_LINE_THRESHOLD;
    } else {
        _lastData.valid[idx] = false;
        _lastData.onWhiteLine[idx] = false;
    }

    _currentSensorIdx = (idx + 1) % NUM_SENSORS;

    xSemaphoreGive(_mutex);
    return _lastData;
}

// ─── Acceso thread-safe ─────────────────────────────────────────────────────

ColorSensorData ColorSensorArray::getData() const {
    if (_mutex == nullptr) {
        return {};
    }

    ColorSensorData copy;
    if (xSemaphoreTake(_mutex, MUTEX_TIMEOUT) == pdTRUE) {
        copy = _lastData;
        xSemaphoreGive(_mutex);
    } else {
        LOG("COLOR", "Mutex timeout on getData()");
    }

    return copy;
}

bool ColorSensorArray::isValid(SensorPosition pos) const {
    return _initialized[static_cast<uint8_t>(pos)];
}

uint8_t ColorSensorArray::sensorsAvailable() const {
    return _sensorsAvailable;
}