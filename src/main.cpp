#include <Arduino.h>
#include <LittleFS.h>
#include "config/config.h"
#include "core/state.h"
#include "core/SensorManager.h"
#include "drivers/motor_control.h"
#include "net/wifi.h"
#include "net/web_server.h"
#include "sensors/ColorSensorArray.h"
#include "sensors/gyro.h"
#include "debug_utils.h"

static SensorManager sensors;

void setup() {
    Serial.begin(115200);
    delay(500);
    LOG("MAIN", "FutBotMX 2026 booting...");

    Core::init();
    motorControl_init();
    sensors.init();

    WiFiMgr::init();
    LOG("MAIN", "WiFi initialized");

    // begin(formatOnFail, basePath, maxOpenFiles, partitionLabel)
    if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
        LOG("MAIN", "LittleFS mount failed");
    } else {
        LOG("MAIN", "LittleFS mounted");
    }

    WebSrv::init();
    LOG("MAIN", "Initialization complete");
}

void safeShutdown() {
    LOG("MAIN", "Safe shutdown executed");
    motorControl_stopAll();
}

static void handleEmergency() {
    Core::killed = true;
    motorControl_stopAll();
    WebSrv::stop();
    LOG("MAIN", "EMERGENCY STOP activated");
}

// ─── Velocidades de movimiento ────────────────────────────────────────────────
constexpr float FWD_SPEED = 0.7f;    // velocidad lineal hacia adelante
constexpr float ROT_SPEED = 0.6f;    // velocidad de giro

// ─── Agrupación de sensores IR (índices 0-based) ──────────────────────────────
//   Sensor 1 (idx 0)       → frente/centro  → ir derecho
//   Sensores 2,3,4 (idx 1,2,3) → lado derecho → girar derecha
//   Sensores 5,6,7 (idx 4,5,6) → lado izquierdo → girar izquierda

// Retorna true si al menos uno de los sensores del grupo está activo (LOW)
static bool groupActive(const IRData& ir, const uint8_t* indices, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) {
        if (indices[i] < IR_SENSOR_COUNT && ir.values[indices[i]] == LOW) {
            return true;
        }
    }
    return false;
}

enum class BallAction : uint8_t {
    NONE,
    FORWARD,
    TURN_RIGHT,
    TURN_LEFT
};

void loop() {
    // ─── Kill switch ──────────────────────────────────────────────────────
    if (Core::killed) {
        safeShutdown();
        while (digitalRead(KILL_PIN) == LOW) {
            delay(10);
        }
        ESP.restart();
    }

    // ─── Network (non-blocking) ──────────────────────────────────────────
    WiFiMgr::update();
    WebSrv::update();

    // ─── Sensors ────────────────────────────────────────────────────────
    sensors.update();
    const IRData ir = sensors.getIRData();
    Core::updateIRData(ir);

    if (sensors.hasColorSensors()) {
        const ColorSensorData cs = sensors.getColorData();
        Core::updateColorData(cs);
    }

    if (sensors.hasGyro()) {
        const GyroData gyro = sensors.getGyroData();
        Core::updateGyroData(gyro);
    }

    // ─── Evaluar qué grupo de sensores detecta la pelota ──────────────────
    static constexpr uint8_t kCenterIdx[]  = { 0 };         // Sensor 1
    static constexpr uint8_t kRightIdx[]   = { 1, 2, 3 };   // Sensores 2, 3, 4
    static constexpr uint8_t kLeftIdx[]    = { 4, 5, 6 };   // Sensores 5, 6, 7

    const bool center = groupActive(ir, kCenterIdx, 1);
    const bool right  = groupActive(ir, kRightIdx,  3);
    const bool left   = groupActive(ir, kLeftIdx,   3);

    // Prioridad: centro > derecha > izquierda
    BallAction action = BallAction::NONE;
    if (center) {
        action = BallAction::FORWARD;
    } else if (right) {
        action = BallAction::TURN_RIGHT;
    } else if (left) {
        action = BallAction::TURN_LEFT;
    }

    // ─── Aplicar movimiento ───────────────────────────────────────────────
    static BallAction lastAction = BallAction::NONE;

    switch (action) {
        case BallAction::FORWARD:
            motorControl_setVelocity(FWD_SPEED, 0.0f, 0.0f);
            break;
        case BallAction::TURN_RIGHT:
            motorControl_setVelocity(0.0f, 0.0f, -ROT_SPEED);
            break;
        case BallAction::TURN_LEFT:
            motorControl_setVelocity(0.0f, 0.0f, ROT_SPEED);
            break;
        case BallAction::NONE:
            motorControl_stop();
            break;
    }

    // Log solo cuando cambia la acción (evitar spam serial)
    if (action != lastAction) {
        switch (action) {
            case BallAction::FORWARD:
                LOG("BALL", "Sensor 1 activo -> FORWARD");
                break;
            case BallAction::TURN_RIGHT:
                LOG("BALL", "Sensores 2-4 activos -> TURN RIGHT (CW)");
                break;
            case BallAction::TURN_LEFT:
                LOG("BALL", "Sensores 5-7 activos -> TURN LEFT (CCW)");
                break;
            case BallAction::NONE:
                LOG("BALL", "Sin deteccion -> STOP");
                break;
        }
        lastAction = action;
    }

    // ─── Debug 5 segundos ──────────────────────────────────────────────────
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime > 5000) {
        lastDebugTime = millis();
        if (sensors.hasGyro()) {
            const GyroData gyro = sensors.getGyroData();
            LOG("GYRO_DEBUG", "G: x:%d y:%d z:%d | A: x:%d y:%d z:%d | T:%.1fC valid:%d",
                gyro.gyro_x, gyro.gyro_y, gyro.gyro_z,
                gyro.acc_x, gyro.acc_y, gyro.acc_z,
                gyro.temp_celsius, gyro.valid);
        } else {
            LOG("GYRO_DEBUG", "No gyro detected.");
        }
    }
}