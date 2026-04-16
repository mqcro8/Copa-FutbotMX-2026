#pragma once

#include <stdint.h>

// ─── Ball position result ──────────────────────────────────────────────────────
// angle_deg: estimated ball angle relative to robot front
//            0° = directly in front, positive = right, negative = left
//            NAN = ball not detected
// confidence: 0.0 (not detected) to 1.0 (centered in front sensors)
struct BallVector {
    float angle_deg;
    float confidence;
};

// Sensor zone classification
enum class BallZone {
    NOT_DETECTED,
    FRONT_CENTER,   // Ball is in kicker zone → ready to kick/advance
    FRONT_LEFT,     // Ball slightly left of front
    FRONT_RIGHT,    // Ball slightly right of front
    SIDE_LEFT,      // Ball to the left
    SIDE_RIGHT,     // Ball to the right
    REAR,           // Ball behind robot
};

void        ballTracker_init();
BallVector  ballTracker_update();
BallZone    ballTracker_getZone();
bool        ballTracker_isInKickerZone();