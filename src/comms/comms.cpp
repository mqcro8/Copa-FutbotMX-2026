#include "comms.h"
#include "ir_protocol.h"
#include "debug_utils.h"
#include <esp_now.h>
#include <WiFi.h>

namespace {
    RobotMsg lastMessage;
    bool connected = false;
    uint8_t peerMac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
}

void OnDataSent(const uint8_t*, esp_now_send_status_t status) {
    connected = (status == ESP_NOW_SEND_SUCCESS);
}

void OnDataRecv(const uint8_t*, const uint8_t* data, int len) {
    if (len == sizeof(RobotMsg)) {
        memcpy(&lastMessage, data, len);
        connected = true;
    }
}

void comms_init() {
    WiFi.mode(WIFI_STA);
    LOG("COMMS", "MAC: %s", WiFi.macAddress().c_str());

    if (esp_now_init() != ESP_OK) {
        LOG("COMMS", "Init failed");
        return;
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, peerMac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

void comms_send() {
    RobotMsg msg;
    msg.role = RobotRole::ATTACKER;
    msg.state = RobotState::SEARCH;
    msg.ball_angle_deg = 0;
    msg.ball_confidence = 0;
    msg.heading_deg = 0;
    msg.crc = 0;

    esp_now_send(peerMac, (uint8_t*)&msg, sizeof(msg));
}

bool comms_isConnected() {
    return connected;
}

RobotMsg comms_getLastMessage() {
    return lastMessage;
}