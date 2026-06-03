#include "comms.h"
#include "ir_protocol.h"
#include "core/state.h"
#include "config/config.h"
#include "debug_utils.h"
#include <esp_now.h>
#include <WiFi.h>
#include <cstring>

namespace {
    RobotMsg lastMessage = {};
    bool sendOk = false;
    uint32_t lastRecvMs = 0;

    uint8_t calcCrc(const RobotMsg& msg) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&msg);
        uint8_t crc = 0;
        for (size_t i = 0; i < sizeof(RobotMsg) - 1; ++i) {
            crc ^= bytes[i];
        }
        return crc;
    }
}

void OnDataSent(const uint8_t*, esp_now_send_status_t status) {
    sendOk = (status == ESP_NOW_SEND_SUCCESS);
}

void OnDataRecv(const uint8_t*, const uint8_t* data, int len) {
    if (len != sizeof(RobotMsg)) {
        return;
    }
    RobotMsg incoming;
    memcpy(&incoming, data, len);
    if (incoming.crc != calcCrc(incoming)) {
        return;
    }
    lastMessage = incoming;
    lastRecvMs = millis();
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
    memcpy(peer.peer_addr, PEER_MAC, 6);
    peer.channel = 0;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        LOG("COMMS", "Failed to add peer — set PEER_MAC in config.h");
    }
}

void comms_update() {
    // Receive is callback-driven; nothing to poll.
}

void comms_send() {
    RobotMsg msg = {};
    msg.role = Core::role;
    msg.state = Core::state;
    msg.ball_angle_deg = Core::ball_angle;
    msg.ball_confidence = Core::ball_confidence;
    msg.heading_deg = Core::current_heading;
    msg.crc = calcCrc(msg);
    esp_now_send(const_cast<uint8_t*>(PEER_MAC), reinterpret_cast<uint8_t*>(&msg), sizeof(msg));
}

bool comms_isConnected() {
    return sendOk;
}

bool comms_isPeerAlive() {
    if (lastRecvMs == 0) {
        return false;
    }
    return (millis() - lastRecvMs) < COMMS_TIMEOUT_MS;
}

RobotMsg comms_getLastMessage() {
    return lastMessage;
}
