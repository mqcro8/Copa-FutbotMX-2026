#include <Arduino.h>
#include "config/config.h"
#include "core/state.h"
#include "core/SensorManager.h"
#include "drivers/motor_control.h"
#include "debug_utils.h"

static SensorManager sensors;

void setup() {
    Serial.begin(115200);
    LOG("MAIN", "Ready for testing...");

    Core::init();
    motorControl_init();
    sensors.init();

    LOG("MAIN", "Initialization complete");
}

void safeShutdown() {
    LOG("MAIN", "Safe shutdown executed");
    motorControl_stopAll();
}

// ─── Secuencia de prueba cinemática holonómica ────────────────────────────────
enum class TestMode : uint8_t {
    FORWARD,
    BACKWARD,
    STRAFE_LEFT,
    STRAFE_RIGHT,
    ROTATE_LEFT,
    ROTATE_RIGHT,
    PAUSE,        // pausa entre ciclos completos
    COUNT         // centinela
};

struct TestStep {
    TestMode mode;
    float vx;
    float vy;
    float omega;
    uint32_t durationMs;
    const char* label;
};

// Secuencia completa de prueba — cada paso 1.5s, pausa entre pasos 400ms
static constexpr TestStep kTestSequence[] = {
    { TestMode::FORWARD,       0.7f,  0.0f,  0.0f, 1500, "FORWARD  (vx=+0.7)" },
    { TestMode::BACKWARD,     -0.7f,  0.0f,  0.0f, 1500, "BACKWARD (vx=-0.7)" },
    { TestMode::STRAFE_LEFT,   0.0f,  0.7f,  0.0f, 1500, "STRAFE-L (vy=+0.7)" },
    { TestMode::STRAFE_RIGHT,  0.0f, -0.7f,  0.0f, 1500, "STRAFE-R (vy=-0.7)" },
    { TestMode::ROTATE_LEFT,   0.0f,  0.0f,  0.6f, 1200, "ROT-CCW  (w=+0.6)"  },
    { TestMode::ROTATE_RIGHT,  0.0f,  0.0f, -0.6f, 1200, "ROT-CW   (w=-0.6)"  },
};
static constexpr uint8_t kTestStepCount = sizeof(kTestSequence) / sizeof(kTestSequence[0]);
static constexpr uint32_t kPauseBetweenSteps = 400; // ms

void loop() {
    // ─── Kill switch ──────────────────────────────────────────────────────
    if (Core::killed) {
        safeShutdown();
        while (digitalRead(KILL_PIN) == LOW) {
            delay(10);
        }
        ESP.restart();
    }

    sensors.update();
    Core::updateIRData(sensors.getIRData());

    // ─── Test state machine (non-blocking) ────────────────────────────────
    static uint8_t stepIndex = 0;
    static uint32_t stepStart = 0;
    static bool isPausing = false;

    const uint32_t now = millis();

    if (stepStart == 0) {
        stepStart = now;

        if (isPausing) {
            LOG("TEST", "--- pausa ---");
            motorControl_stop();
        } else {
            const TestStep& step = kTestSequence[stepIndex];
            LOG("TEST", "[%d/%d] %s", stepIndex + 1, kTestStepCount, step.label);
            motorControl_setVelocity(step.vx, step.vy, step.omega);
        }
    }

    const uint32_t elapsed = now - stepStart;
    const uint32_t currentDuration = isPausing
        ? kPauseBetweenSteps
        : kTestSequence[stepIndex].durationMs;

    if (elapsed >= currentDuration) {
        motorControl_stop();
        stepStart = 0;

        if (isPausing) {
            // Avanzar al siguiente paso
            isPausing = false;
            stepIndex = (stepIndex + 1) % kTestStepCount;

            if (stepIndex == 0) {
                LOG("TEST", "=== Ciclo completo, reiniciando ===");
            }
        } else {
            // Entrar en pausa antes del siguiente paso
            isPausing = true;
        }
    }
}