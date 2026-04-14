#pragma once
#include "ir_protocol.h"

void comms_init();
void comms_send();
bool comms_isConnected();
RobotMsg comms_getLastMessage();