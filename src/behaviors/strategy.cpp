#include "strategy.h"
#include "ir_protocol.h"

namespace {
    RobotState currentState = RobotState::IDLE;
}

void strategy_init() {
    currentState = RobotState::SEARCH;
}

void strategy_update() {
}

RobotState strategy_getState() {
    return currentState;
}