#pragma once
#include "ir_protocol.h"

void comms_init();
void comms_update();
void comms_send();
bool comms_isConnected();
bool comms_isPeerAlive();
RobotMsg comms_getLastMessage();