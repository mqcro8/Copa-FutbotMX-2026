#include "strategy.h"
#include "config/config.h"
#include "core/state.h"
#include "comms/comms.h"
#include "drivers/motor_control.h"
#include "drivers/kicker.h"
#include <Arduino.h>
#include <cmath>
#include <algorithm>

namespace {

float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

float headingError(float current, float target) {
    float err = target - current;
    while (err > 180.0f) err -= 360.0f;
    while (err < -180.0f) err += 360.0f;
    return err;
}

bool isBallDetected() {
    return Core::irData.detected && Core::ball_confidence > 0;
}

bool isPeerAttacking() {
    if (!comms_isPeerAlive()) {
        return false;
    }
    const RobotMsg peer = comms_getLastMessage();
    return peer.state == RobotState::DRIBBLE || peer.state == RobotState::SHOOT;
}

void beginGame() {
    Core::game_active = true;
    if (Core::gyroData.valid) {
        Core::home_heading_deg = Core::gyroData.yaw_deg;
    }
    if (Core::role == RobotRole::ATTACKER) {
        Core::state = RobotState::SEARCH;
    } else {
        Core::state = RobotState::REPOSITION;
    }
}

// ─── Attacker ───────────────────────────────────────────────────────────────

bool search_dir_cw = true;
uint32_t last_search_turn_ms = 0;
uint32_t search_interval_ms = 3000;
uint32_t ball_lost_since_ms = 0;
uint32_t ball_offcenter_since_ms = 0;
uint32_t reposition_start_ms = 0;
uint32_t reposition_aligned_since_ms = 0;

void updateAttackerIdle() {
    motorControl_stop();
    if (digitalRead(KILL_PIN) == HIGH) {
        beginGame();
    }
}

void updateAttackerSearch() {
    const uint32_t now = millis();
    if (now - last_search_turn_ms > search_interval_ms) {
        search_dir_cw = !search_dir_cw;
        last_search_turn_ms = now;
        search_interval_ms = random(2500, 4500);
    }

    const float omega = search_dir_cw ? -ROT_SPEED : ROT_SPEED;
    motorControl_setVelocity(0.0f, 0.0f, omega);

    if (isBallDetected()) {
        Core::state = RobotState::APPROACH;
        ball_lost_since_ms = 0;
    }
}

void updateAttackerApproach() {
    const uint32_t now = millis();

    if (!isBallDetected()) {
        if (ball_lost_since_ms == 0) {
            ball_lost_since_ms = now;
        }
        if (now - ball_lost_since_ms >= BALL_LOST_MS) {
            Core::state = RobotState::SEARCH;
            motorControl_stop();
            return;
        }
    } else {
        ball_lost_since_ms = 0;
    }

    const float angle = static_cast<float>(Core::ball_angle);
    const float omega = clampf(angle * BALL_KP, -ROT_SPEED, ROT_SPEED);
    const float vx = (std::fabs(angle) < 30.0f) ? FWD_SPEED : 0.3f;
    motorControl_setVelocity(vx, 0.0f, omega);

    if (isBallDetected()
        && std::fabs(angle) <= BALL_ANGLE_THRESHOLD
        && Core::ball_confidence >= APPROACH_MIN_INTENSITY) {
        Core::state = RobotState::DRIBBLE;
        Core::dribble_start_ms = now;
        ball_offcenter_since_ms = 0;
    }
}

void updateAttackerDribble() {
    const uint32_t now = millis();

    if (!isBallDetected()) {
        Core::state = RobotState::SEARCH;
        motorControl_stop();
        return;
    }

    const float angle = static_cast<float>(Core::ball_angle);
    if (std::fabs(angle) > BALL_OFFCENTER_ANGLE) {
        if (ball_offcenter_since_ms == 0) {
            ball_offcenter_since_ms = now;
        }
        if (now - ball_offcenter_since_ms >= BALL_OFFCENTER_MS) {
            Core::state = RobotState::APPROACH;
            return;
        }
    } else {
        ball_offcenter_since_ms = 0;
    }

    const bool shootEnabled = (Core::dribble_start_ms != 0)
        && (now - Core::dribble_start_ms > MIN_FIELD_TRAVEL_MS);
    const bool frontLine = Core::colorData.onWhiteLine[static_cast<uint8_t>(SensorPosition::FRONT)];

    if (shootEnabled && frontLine) {
        Core::state = RobotState::SHOOT;
        return;
    }

    float omega = clampf(angle * BALL_KP, -ROT_SPEED, ROT_SPEED);
    if (Core::gyroData.valid) {
        const float headingErr = headingError(Core::gyroData.yaw_deg, Core::home_heading_deg);
        omega += clampf(headingErr * HEADING_KP, -ROT_SPEED, ROT_SPEED);
    }
    motorControl_setVelocity(FWD_SPEED, 0.0f, omega);
}

void updateAttackerShoot() {
    motorControl_stop();
    kick();
    Core::state = RobotState::SEARCH;
    Core::dribble_start_ms = 0;
}

void updateAttacker() {
    switch (Core::state) {
        case RobotState::IDLE:       updateAttackerIdle(); break;
        case RobotState::SEARCH:     updateAttackerSearch(); break;
        case RobotState::APPROACH:   updateAttackerApproach(); break;
        case RobotState::DRIBBLE:    updateAttackerDribble(); break;
        case RobotState::SHOOT:      updateAttackerShoot(); break;
        default:
            Core::state = RobotState::SEARCH;
            break;
    }
}

// ─── Defender ─────────────────────────────────────────────────────────────────

void updateDefenderIdle() {
    motorControl_stop();
    if (digitalRead(KILL_PIN) == HIGH) {
        beginGame();
        reposition_start_ms = millis();
        reposition_aligned_since_ms = 0;
    }
}

void updateDefenderReposition() {
    const uint32_t now = millis();

    if (!Core::gyroData.valid) {
        motorControl_setVelocity(-FWD_SPEED * 0.5f, 0.0f, 0.0f);
        if (now - reposition_start_ms >= REPOSITION_MIN_MS) {
            Core::state = RobotState::DEFEND;
        }
        return;
    }

    const float err = headingError(Core::gyroData.yaw_deg, Core::home_heading_deg);
    if (std::fabs(err) > 10.0f) {
        reposition_aligned_since_ms = 0;
        const float omega = clampf(err * HEADING_KP, -ROT_SPEED, ROT_SPEED);
        motorControl_setVelocity(0.0f, 0.0f, omega);
        return;
    }

    if (reposition_aligned_since_ms == 0) {
        reposition_aligned_since_ms = now;
    }

    motorControl_setVelocity(-FWD_SPEED * 0.5f, 0.0f, 0.0f);

    const bool alignedLongEnough = (now - reposition_aligned_since_ms) >= REPOSITION_ALIGN_MS;
    const bool repositionedLongEnough = (now - reposition_start_ms) >= REPOSITION_MIN_MS;
    if (alignedLongEnough && repositionedLongEnough) {
        Core::state = RobotState::DEFEND;
    }
}

void updateDefenderDefend() {
    if (isPeerAttacking()) {
        motorControl_stop();
        return;
    }

    if (!isBallDetected()) {
        motorControl_stop();
        return;
    }

    const float angle = static_cast<float>(Core::ball_angle);
    const float vy = clampf(angle * 0.3f, -0.5f, 0.5f);
    motorControl_setVelocity(0.0f, vy, 0.0f);

    if (Core::ball_confidence >= DEFEND_INTERCEPT_INTENSITY
        && std::fabs(angle) <= DEFEND_INTERCEPT_ANGLE) {
        Core::state = RobotState::INTERCEPT;
    }
}

void updateDefenderIntercept() {
    if (!isBallDetected() || std::fabs(static_cast<float>(Core::ball_angle)) > 90.0f) {
        Core::state = RobotState::REPOSITION;
        reposition_start_ms = millis();
        reposition_aligned_since_ms = 0;
        motorControl_stop();
        return;
    }

    const float angle = static_cast<float>(Core::ball_angle);
    const float omega = clampf(angle * BALL_KP, -ROT_SPEED, ROT_SPEED);
    const float vx = FWD_SPEED * 0.8f;
    motorControl_setVelocity(vx, 0.0f, omega);

    if (std::fabs(angle) <= BALL_ANGLE_THRESHOLD) {
        kick();
    }
}

void updateDefender() {
    switch (Core::state) {
        case RobotState::IDLE:        updateDefenderIdle(); break;
        case RobotState::REPOSITION:  updateDefenderReposition(); break;
        case RobotState::DEFEND:      updateDefenderDefend(); break;
        case RobotState::INTERCEPT:   updateDefenderIntercept(); break;
        default:
            Core::state = RobotState::REPOSITION;
            reposition_start_ms = millis();
            break;
    }
}

}  // namespace

void strategy_init() {
    Core::state = RobotState::IDLE;
    Core::game_active = false;
    search_dir_cw = true;
    last_search_turn_ms = millis();
    search_interval_ms = 3000;
}

void strategy_update() {
    if (!Core::game_active && Core::state == RobotState::IDLE) {
        if (Core::role == RobotRole::ATTACKER) {
            updateAttackerIdle();
        } else {
            updateDefenderIdle();
        }
        return;
    }

    if (Core::role == RobotRole::ATTACKER) {
        updateAttacker();
    } else {
        updateDefender();
    }
}

RobotState strategy_getState() {
    return Core::state;
}
