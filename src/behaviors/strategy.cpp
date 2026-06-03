#include "strategy.h"
#include "config/config.h"
#include "core/state.h"
#include "drivers/motor_control.h"
#include "drivers/kicker.h"
#include "debug_utils.h"
#include <Arduino.h>
#include <cmath>

namespace {

RobotState currentState = RobotState::IDLE;
bool justEntered = true;
uint32_t stateEntryMs = 0;

// Search
bool searchDirCw = true;
uint32_t lastSearchTurnMs = 0;
uint32_t searchIntervalMs = 3500;

// Approach
uint32_t lastBallSeenMs = 0;

// Dribble
uint32_t dribbleStartMs = 0;
uint32_t lastBallCenteredMs = 0;

// Shoot
uint32_t shootEntryMs = 0;

// Escape
uint32_t wallEscapeUntilMs = 0;
uint32_t lineEscapeUntilMs = 0;
float escapeVx = 0.0f;
float escapeVy = 0.0f;

// Defender
int16_t initialHeading = 0;
bool headingInitialized = false;
uint32_t repositionStartMs = 0;
bool repositionReady = false;

// ── Helpers ──────────────────────────────────────────────────────────────
static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int16_t normalizeAngle(int16_t a) {
    while (a > 180)  a -= 360;
    while (a < -180) a += 360;
    return a;
}

static void setState(RobotState s) {
    if (currentState != s) {
        LOG("FSM", "State: %d -> %d", (int)currentState, (int)s);
        currentState = s;
        justEntered = true;
        stateEntryMs = millis();
    }
}

// ── Attacker FSM ─────────────────────────────────────────────────────────
static void updateAttacker() {
    const uint32_t now = millis();
    const int16_t ballAngle = Core::ball_angle;
    const uint8_t ballInt   = Core::ball_confidence;
    const bool ballDet      = Core::irData.detected;
    const float heading     = Core::gyroData.yaw_deg;
    const auto& cs          = Core::colorData;

    switch (currentState) {

    case RobotState::IDLE:
        motorControl_stop();
        if (!Core::killed) {
            setState(RobotState::SEARCH);
        }
        break;

    case RobotState::SEARCH:
        if (justEntered) {
            lastSearchTurnMs = now;
            searchIntervalMs = 3500;
            searchDirCw = (random(0, 2) == 0);
            LOG("FSM", "SEARCH: dir=%s", searchDirCw ? "CW" : "CCW");
        }

        if (now - lastSearchTurnMs > searchIntervalMs) {
            searchDirCw = !searchDirCw;
            lastSearchTurnMs = now;
            searchIntervalMs = 2500 + random(0, 2001);
            LOG("FSM", "SEARCH: flip dir=%s interval=%lu",
                searchDirCw ? "CW" : "CCW", searchIntervalMs);
        }

        motorControl_setVelocity(0.0f, 0.0f,
            searchDirCw ? ROT_SPEED : -ROT_SPEED);

        if (ballDet) {
            setState(RobotState::APPROACH);
        }
        break;

    case RobotState::APPROACH:
        if (justEntered) {
            lastBallSeenMs = now;
        }

        if (ballDet) {
            lastBallSeenMs = now;
        }

        {
            float omega = clampf(-(float)ballAngle * BALL_KP, -ROT_SPEED, ROT_SPEED);
            float vx = (fabsf((float)ballAngle) < 30.0f) ? FWD_SPEED : 0.3f;
            motorControl_setVelocity(vx, 0.0f, omega);
        }

        if (ballDet &&
            fabsf((float)ballAngle) <= BALL_ANGLE_THRESHOLD &&
            ballInt >= APPROACH_MIN_INTENSITY) {
            setState(RobotState::DRIBBLE);
        }

        if (now - lastBallSeenMs > BALL_LOST_TIMEOUT_MS) {
            setState(RobotState::SEARCH);
        }
        break;

    case RobotState::DRIBBLE:
        if (justEntered) {
            dribbleStartMs = now;
            lastBallCenteredMs = now;
        }

        if (ballDet && fabsf((float)ballAngle) <= BALL_ANGLE_THRESHOLD) {
            lastBallCenteredMs = now;
        }

        {
            float hError = normalizeAngle((int16_t)(heading - (float)initialHeading));
            float hOmega = clampf(hError * HEADING_KP, -ROT_SPEED, ROT_SPEED);
            motorControl_setVelocity(FWD_SPEED, 0.0f, hOmega);
        }

        if (!ballDet) {
            setState(RobotState::SEARCH);
        }

        if (now - lastBallCenteredMs > BALL_OFFCENTER_TIMEOUT_MS) {
            setState(RobotState::APPROACH);
        }
        break;

    case RobotState::SHOOT:
        if (justEntered) {
            motorControl_stop();
            kick();
            shootEntryMs = now;
            LOG("FSM", "SHOOT: fired at %lu", now);
        } else {
            kick();
        }

        if (now - shootEntryMs > KICK_DURATION_MS + 50) {
            setState(RobotState::SEARCH);
        }
        break;

    default:
        setState(RobotState::SEARCH);
        break;
    }
}

// ── Defender FSM ─────────────────────────────────────────────────────────
static void updateDefender() {
    const uint32_t now = millis();
    const int16_t ballAngle = Core::ball_angle;
    const uint8_t ballInt   = Core::ball_confidence;
    const bool ballDet      = Core::irData.detected;
    const float heading     = Core::gyroData.yaw_deg;

    switch (currentState) {

    case RobotState::IDLE:
        motorControl_stop();
        if (!Core::killed) {
            setState(RobotState::REPOSITION);
        }
        break;

    case RobotState::REPOSITION:
        if (justEntered) {
            repositionStartMs = now;
            repositionReady = false;
        }

        {
            float hError = normalizeAngle((int16_t)(heading - (float)initialHeading));
            float omega = clampf(hError * HEADING_KP, -ROT_SPEED, ROT_SPEED);

            if (fabsf(hError) < 10.0f) {
                if (!repositionReady) {
                    repositionReady = true;
                    repositionStartMs = now;
                }
                if (now - repositionStartMs > 500) {
                    motorControl_stop();
                    setState(RobotState::DEFEND);
                } else {
                    motorControl_setVelocity(0.3f, 0.0f, omega);
                }
            } else {
                repositionReady = false;
                motorControl_setVelocity(0.0f, 0.0f, omega);
            }
        }
        break;

    case RobotState::DEFEND:
        // If ally has ball → stay still
        if (Core::peerMsg.state == RobotState::DRIBBLE ||
            Core::peerMsg.state == RobotState::SHOOT) {
            motorControl_stop();
        } else if (ballDet) {
            float vy = clampf((float)ballAngle * 0.3f, -0.5f, 0.5f);
            motorControl_setVelocity(0.0f, vy, 0.0f);
        } else {
            motorControl_stop();
        }

        if (ballDet && ballInt >= 3 && fabsf((float)ballAngle) <= 45.0f) {
            setState(RobotState::INTERCEPT);
        }
        break;

    case RobotState::INTERCEPT:
        if (justEntered) {
            LOG("FSM", "INTERCEPT: advancing toward ball");
        }

        motorControl_setVelocity(FWD_SPEED * 0.8f, 0.0f, 0.0f);

        if (ballDet && fabsf((float)ballAngle) <= BALL_ANGLE_THRESHOLD) {
            kick();
        }

        if (!ballDet || fabsf((float)ballAngle) > 90.0f) {
            setState(RobotState::REPOSITION);
        }
        break;

    default:
        setState(RobotState::REPOSITION);
        break;
    }
}

} // namespace

// ── Public API ───────────────────────────────────────────────────────────
void strategy_init() {
    currentState = RobotState::IDLE;
    justEntered = true;
    stateEntryMs = 0;
    headingInitialized = false;
    lastBallSeenMs = 0;
    lastBallCenteredMs = 0;
    wallEscapeUntilMs = 0;
    lineEscapeUntilMs = 0;
    LOG("FSM", "strategy_init (role=%d)", (int)MY_ROLE);
}

void strategy_update() {
    const uint32_t now = millis();
    const auto& gyro = Core::gyroData;
    const auto& cs   = Core::colorData;

    // ── Initialize defender heading once gyro is valid ───────
    if (!headingInitialized && gyro.valid) {
        initialHeading = (int16_t)gyro.yaw_deg;
        headingInitialized = true;
        LOG("FSM", "Initial heading=%d", initialHeading);
    }

    // ── Wall guard (accelerometer impact) ─────────────────────
    if (gyro.valid) {
        if (abs(gyro.acc_x) > WALL_IMPACT_THRESHOLD ||
            abs(gyro.acc_y) > WALL_IMPACT_THRESHOLD) {
            escapeVx = (gyro.acc_x > 0) ? -ESCAPE_SPEED : ESCAPE_SPEED;
            escapeVy = (gyro.acc_y > 0) ? -ESCAPE_SPEED : ESCAPE_SPEED;
            wallEscapeUntilMs = now + WALL_ESCAPE_MS;
            LOG("FSM", "WALL: acc=(%d,%d) v=(%.1f,%.1f)",
                gyro.acc_x, gyro.acc_y, escapeVx, escapeVy);
        }
    }

    // ── Line guard ────────────────────────────────────────────
    // Exception: when in SHOOT state, commit to the shot
    if (currentState != RobotState::SHOOT) {
        const bool f = cs.onWhiteLine[(int)SensorPosition::FRONT];
        const bool r = cs.onWhiteLine[(int)SensorPosition::RIGHT];
        const bool b = cs.onWhiteLine[(int)SensorPosition::BACK];
        const bool l = cs.onWhiteLine[(int)SensorPosition::LEFT];

        if (f || r || b || l) {
            escapeVx = f ? -ESCAPE_SPEED : (b ? ESCAPE_SPEED : 0.0f);
            escapeVy = r ? -ESCAPE_SPEED : (l ? ESCAPE_SPEED : 0.0f);
            lineEscapeUntilMs = now + LINE_ESCAPE_MS;
        }
    }

    // ── Priority: DRIBBLE → SHOOT (consume before lineGuard) ─
    // If we're in DRIBBLE and front sensor fires & shootEnabled,
    // transition to SHOOT before lineGuard can take over.
    if (currentState == RobotState::DRIBBLE) {
        if (cs.onWhiteLine[(int)SensorPosition::FRONT]) {
            if ((now - dribbleStartMs) > MIN_FIELD_TRAVEL_MS) {
                setState(RobotState::SHOOT);
                wallEscapeUntilMs = 0;
                lineEscapeUntilMs = 0;
            }
        }
    }

    // ── Apply escape if active ────────────────────────────────
    if (now < wallEscapeUntilMs) {
        motorControl_setVelocity(escapeVx, escapeVy, 0.0f);
        Core::state = currentState;
        justEntered = false;
        return;
    }

    if (now < lineEscapeUntilMs) {
        motorControl_setVelocity(escapeVx, escapeVy, 0.0f);
        Core::state = currentState;
        justEntered = false;
        return;
    }

    // ── FSM ───────────────────────────────────────────────────
    if (MY_ROLE == RobotRole::ATTACKER) {
        updateAttacker();
    } else {
        updateDefender();
    }

    Core::state = currentState;
    justEntered = false;
}

RobotState strategy_getState() {
    return currentState;
}
