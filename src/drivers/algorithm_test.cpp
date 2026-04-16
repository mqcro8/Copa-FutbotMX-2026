#include "algorithm_test.h"
#include "motor_control.h"
#include "config/config.h"
#include <Arduino.h>
#include "debug_utils.h"

namespace {
constexpr int16_t TEST_FORWARD_SPEED = 150;
constexpr int16_t TEST_ROTATE_SPEED = 128;
constexpr uint32_t TEST_PHASE_DURATION_MS = 500;

enum class TestPhase : uint8_t { FORWARD, ROTATE_CW, ROTATE_CCW };
TestPhase currentPhase = TestPhase::FORWARD;
uint32_t lastPhaseChange = 0;
bool initialized = false;
}  // namespace

void algorithmTest_init()
{
    motorControl_init();
    initialized = true;
    lastPhaseChange = millis();
    LOG("TEST", "Motor test initialized");
}

void algorithmTest_loop()
{
    if (!initialized) return;

    uint32_t now = millis();
    if (now - lastPhaseChange < TEST_PHASE_DURATION_MS) return;

    lastPhaseChange = now;

    switch (currentPhase) {
    case TestPhase::FORWARD:
        LOG("TEST", "Phase: FORWARD M0=%d M1=%d M2=%d",
            TEST_FORWARD_SPEED, TEST_FORWARD_SPEED, TEST_FORWARD_SPEED);
        motorControl_setSpeed(MotorId::MOTOR_0, TEST_FORWARD_SPEED);
        motorControl_setSpeed(MotorId::MOTOR_1, TEST_FORWARD_SPEED);
        motorControl_setSpeed(MotorId::MOTOR_2, TEST_FORWARD_SPEED);
        currentPhase = TestPhase::ROTATE_CW;
        break;

    case TestPhase::ROTATE_CW:
        LOG("TEST", "Phase: ROTATE_CW M0=%d M1=%d M2=%d",
            -TEST_ROTATE_SPEED, TEST_ROTATE_SPEED, TEST_ROTATE_SPEED);
        motorControl_setSpeed(MotorId::MOTOR_0, -TEST_ROTATE_SPEED);
        motorControl_setSpeed(MotorId::MOTOR_1, TEST_ROTATE_SPEED);
        motorControl_setSpeed(MotorId::MOTOR_2, TEST_ROTATE_SPEED);
        currentPhase = TestPhase::ROTATE_CCW;
        break;

    case TestPhase::ROTATE_CCW:
        LOG("TEST", "Phase: ROTATE_CCW M0=%d M1=%d M2=%d",
            TEST_ROTATE_SPEED, -TEST_ROTATE_SPEED, -TEST_ROTATE_SPEED);
        motorControl_setSpeed(MotorId::MOTOR_0, TEST_ROTATE_SPEED);
        motorControl_setSpeed(MotorId::MOTOR_1, -TEST_ROTATE_SPEED);
        motorControl_setSpeed(MotorId::MOTOR_2, -TEST_ROTATE_SPEED);
        currentPhase = TestPhase::FORWARD;
        break;
    }
}