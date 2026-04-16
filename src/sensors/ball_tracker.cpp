#include "ball_tracker.h"
#include "config/config.h"

#include <Arduino.h>
#include <cmath>

// ─── Sensor pin configuration ──────────────────────────────────────────────────
// VS1838B: output is LOW when IR signal at 38kHz is detected (active LOW)
// Define these in config/config.h:
//   IR_FRONT_LEFT_PIN, IR_FRONT_CENTER_PIN, IR_FRONT_RIGHT_PIN
//   IR_SIDE_LEFT_PIN, IR_SIDE_RIGHT_PIN, IR_REAR_PIN
//
// Adjust kSensorCount and kSensorAngles to match your physical layout.

struct IRSensor {
    uint8_t pin;
    float   angle_deg; // Angle relative to robot front (0° = front, CW positive)
    bool    active;    // true = currently detecting ball
};

// ─── Sensor table ─────────────────────────────────────────────────────────────
// Populate with your actual pin assignments from config.h
// Angles represent sensor mounting position around the robot.
static IRSensor kSensors[] = {
    { IR_FRONT_LEFT_PIN,   -30.0f, false }, // Front-left
    { IR_FRONT_CENTER_PIN,   0.0f, false }, // Front-center (kicker zone)
    { IR_FRONT_RIGHT_PIN,   30.0f, false }, // Front-right
    // ── Add peripheral sensors below when ready ──
    // { IR_SIDE_LEFT_PIN,   -90.0f, false },
    // { IR_SIDE_RIGHT_PIN,   90.0f, false },
    // { IR_REAR_PIN,        180.0f, false },
};

static constexpr uint8_t kSensorCount =
    static_cast<uint8_t>(sizeof(kSensors) / sizeof(kSensors[0]));

// ─── Kicker zone definition ────────────────────────────────────────────────────
// A sensor is considered "in kicker zone" if its angle is within this range.
static constexpr float kKickerZoneHalfAngle = 15.0f; // ±15° from center

// ─── Internal state ────────────────────────────────────────────────────────────
static BallZone  sCurrentZone       = BallZone::NOT_DETECTED;
static BallVector sCurrentVector    = { NAN, 0.0f };

// ─── Helpers ───────────────────────────────────────────────────────────────────

// VS1838B is active LOW: LOW = ball detected, HIGH = no signal
static inline bool sensorIsDetecting(uint8_t pin) {
    return digitalRead(pin) == LOW;
}

// Circular mean for angles — avoids 180°/-180° wraparound artifacts
static float circularMeanAngle(const float* angles, const float* weights, uint8_t count) {
    float sinSum = 0.0f;
    float cosSum = 0.0f;

    for (uint8_t i = 0; i < count; ++i) {
        float rad = angles[i] * (float)DEG_TO_RAD;
        sinSum += weights[i] * sinf(rad);
        cosSum += weights[i] * cosf(rad);
    }

    return atan2f(sinSum, cosSum) * (float)RAD_TO_DEG;
}

// ─── Public API ────────────────────────────────────────────────────────────────

void ballTracker_init() {
    for (uint8_t i = 0; i < kSensorCount; ++i) {
        pinMode(kSensors[i].pin, INPUT);
        kSensors[i].active = false;
    }

    Serial.println("[BallTracker] Initialized with " + String(kSensorCount) + " IR sensors.");
}

BallVector ballTracker_update() {
    float   activeAngles[kSensorCount];
    float   activeWeights[kSensorCount];
    uint8_t activeCount = 0;

    // ── Read all sensors ────────────────────────────────────────────────────────
    for (uint8_t i = 0; i < kSensorCount; ++i) {
        kSensors[i].active = sensorIsDetecting(kSensors[i].pin);

        if (kSensors[i].active) {
            activeAngles[activeCount]  = kSensors[i].angle_deg;
            activeWeights[activeCount] = 1.0f; // Equal weight; tune per sensor if needed
            activeCount++;
        }
    }

    // ── No detection ────────────────────────────────────────────────────────────
    if (activeCount == 0) {
        sCurrentZone   = BallZone::NOT_DETECTED;
        sCurrentVector = { NAN, 0.0f };
        return sCurrentVector;
    }

    // ── Compute estimated ball angle ────────────────────────────────────────────
    float estimatedAngle = circularMeanAngle(activeAngles, activeWeights, activeCount);

    // Confidence: ratio of active sensors, boosted if center sensor is active
    float confidence = (float)activeCount / (float)kSensorCount;
    if (kSensors[1].active) { // Front-center sensor bonus
        confidence = fminf(1.0f, confidence + 0.3f);
    }

    sCurrentVector = { estimatedAngle, confidence };

    // ── Classify zone ───────────────────────────────────────────────────────────
    if (fabsf(estimatedAngle) <= kKickerZoneHalfAngle && kSensors[1].active) {
        sCurrentZone = BallZone::FRONT_CENTER;
    } else if (estimatedAngle > -60.0f && estimatedAngle < 0.0f) {
        sCurrentZone = BallZone::FRONT_LEFT;
    } else if (estimatedAngle > 0.0f && estimatedAngle < 60.0f) {
        sCurrentZone = BallZone::FRONT_RIGHT;
    } else if (estimatedAngle <= -60.0f) {
        sCurrentZone = BallZone::SIDE_LEFT;
    } else if (estimatedAngle >= 60.0f) {
        sCurrentZone = BallZone::SIDE_RIGHT;
    } else {
        sCurrentZone = BallZone::REAR;
    }

    return sCurrentVector;
}

BallZone ballTracker_getZone() {
    return sCurrentZone;
}

bool ballTracker_isInKickerZone() {
    return sCurrentZone == BallZone::FRONT_CENTER;
}