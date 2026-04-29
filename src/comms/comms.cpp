#include "comms.h"
#include "debug_utils.h"
#include "../settings/settings.h"
#include "../core/state.h"
#include "../net/wifi.h"
#include <WiFi.h>

ESPNowClient* ESPNowClient::s_instance = nullptr;

void onSentCallback(const uint8_t* mac, esp_now_send_status_t status) {
    if (ESPNowClient::s_instance) ESPNowClient::s_instance->handleSent(mac, status);
}

void onRecvCallback(const uint8_t* mac, const uint8_t* data, int len) {
    if (ESPNowClient::s_instance) ESPNowClient::s_instance->handleRecv(mac, data, len);
}

ESPNowClient::ESPNowClient() { s_instance = this; }
ESPNowClient::~ESPNowClient() { s_instance = nullptr; }

ESPNowClient& ESPNowClient::instance() {
    static ESPNowClient inst;
    return inst;
}

void ESPNowClient::addPeer() {
    if (peerAdded) {
        esp_now_del_peer(peerMac);
        peerAdded = false;
    }
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, peerMac, 6);
    
    // If we are in AP_ONLY mode, we MUST use WIFI_IF_AP. Otherwise, use WIFI_IF_STA
    // because WiFi.macAddress() returns the STA MAC, which is what peers expect.
    wifi_interface_t iface = (WiFiMgr::getCurrentMode() == WiFiMode::AP_ONLY) ? WIFI_IF_AP : WIFI_IF_STA;
    peer.ifidx = iface; 
    peer.channel = WIFI_AP_CHANNEL;
    peer.encrypt = false;
    
    if (esp_now_add_peer(&peer) == ESP_OK) {
        peerAdded = true;
        LOG("ESPNOW", "Peer: %02X:%02X:%02X:%02X:%02X:%02X (%s ch %d)",
            peerMac[0], peerMac[1], peerMac[2], peerMac[3], peerMac[4], peerMac[5],
            iface == WIFI_IF_AP ? "AP" : "STA", WIFI_AP_CHANNEL);
    } else {
        LOG("ESPNOW", "Failed to add peer on interface %s", iface == WIFI_IF_AP ? "AP" : "STA");
    }
}

void ESPNowClient::addToHistory(const RobotMsg& msg, bool is_tx, const char* text) {
    msgHistory[msgHistoryIndex] = {
        msg.timestamp_ms, is_tx, msg.role, msg.state,
        msg.ball_angle_deg, msg.heading_deg, {0}
    };
    if (text && text[0] != '\0') {
        size_t len = strlen(text);
        if (len > 31) len = 31;
        memcpy(msgHistory[msgHistoryIndex].text, text, len);
        msgHistory[msgHistoryIndex].text[len] = '\0';
    }
    msgHistoryIndex = (msgHistoryIndex + 1) % MAX_HISTORY;
    if (msgHistoryCount < MAX_HISTORY) msgHistoryCount++;
}

void ESPNowClient::handleSent(const uint8_t*, esp_now_send_status_t status) {
    stats.last_tx_ms = millis();
    if (status == ESP_NOW_SEND_SUCCESS) stats.sent++;
    else stats.failed++;
}

void ESPNowClient::handleRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(RobotMsg)) return;

    RobotMsg tmpMsg;
    memcpy(&tmpMsg, data, len);

    if (tmpMsg.crc != 0x42) {
        stats.failed++;
        LOG("ESPNOW", "RX bad CRC");
        return;
    }

    memcpy(&lastRx, &tmpMsg, sizeof(RobotMsg));
    stats.recv++;
    lastRxTimeMs = millis();
    addToHistory(lastRx, false, nullptr);

    LOG("ESPNOW", "RX from %02X%02X: role=%d state=%d ball=%d heading=%d",
        mac[4], mac[5],
        (int)lastRx.role, (int)lastRx.state,
        lastRx.ball_angle_deg, lastRx.heading_deg);

    if (lastRx.latency_reply_ms > 0) { // Fix latency calculation
        uint32_t now = millis();
        uint32_t rtt = now - lastRx.latency_reply_ms;
        if (rtt < 1000) stats.last_latency_ms = (int16_t)(rtt / 2);
    }
}

bool ESPNowClient::isValidPeerMac(const uint8_t mac[6]) const {
    if (mac == nullptr) return false;
    bool allZero = true;
    bool allFF = true;
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0x00) allZero = false;
        if (mac[i] != 0xFF) allFF = false;
    }
    return !allZero && !allFF;
}

bool ESPNowClient::connected() const {
    return lastRxTimeMs > 0 && (millis() - lastRxTimeMs) < TIMEOUT_MS;
}

void ESPNowClient::begin() {
    LOG("ESPNOW", "Initializing...");
    if (esp_now_init() != ESP_OK) { LOG("ESPNOW", "Init failed"); return; }

    uint8_t mac[6];
    if (Settings::getPeerMac(mac) && isValidPeerMac(mac)) {
        memcpy(peerMac, mac, 6);
        addPeer();
    } else {
        LOG("ESPNOW", "No peer MAC configured");
    }

    esp_now_register_send_cb(onSentCallback);
    esp_now_register_recv_cb(onRecvCallback);
    LOG("ESPNOW", "Ready (WiFi channel: %d)", WiFi.channel());
}

void ESPNowClient::update() {
    if (!isValidPeerMac(peerMac)) return;
    if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
        lastHeartbeat = millis();

        RobotRole r = (RobotRole)Settings::getRole();
        RobotState s = Core::state;
        int16_t ball = Core::ball_angle;
        uint8_t conf = Core::ball_confidence;
        int16_t hdg = Core::current_heading;

        RobotMsg msg;
        msg.timestamp_ms = lastHeartbeat;
        msg.role = r;
        msg.state = s;
        msg.ball_angle_deg = ball;
        msg.ball_confidence = conf;
        msg.heading_deg = hdg;
        msg.latency_reply_ms = lastRx.timestamp_ms; // Echo back the sender's timestamp for RTT calculation
        msg.crc = 0x42;

        sendMessage(msg);
    }
}

bool ESPNowClient::sendMessage(const RobotMsg& msg) {
    return peerAdded && esp_now_send(peerMac, (const uint8_t*)&msg, sizeof(msg)) == ESP_OK;
}

bool ESPNowClient::isConnected() const { return connected(); }
RobotMsg ESPNowClient::getLastMessage() const { return lastRx; }
CommsStats ESPNowClient::getStats() const { return stats; }

void ESPNowClient::setPeer(const uint8_t mac[6]) {
    if (!isValidPeerMac(mac)) return;
    memcpy(peerMac, mac, 6);
    Settings::setPeerMac(mac);
    addPeer();
}

void ESPNowClient::sendTest() { sendTest(nullptr); }

void ESPNowClient::sendTest(const char* text) {
    RobotMsg msg;
    msg.timestamp_ms = millis();
    msg.role = (RobotRole)Settings::getRole();
    msg.state = RobotState::IDLE;
    msg.ball_angle_deg = 0;
    msg.ball_confidence = 0;
    msg.heading_deg = 0;
    msg.latency_reply_ms = 0;
    msg.crc = 0x42;
    addToHistory(msg, true, text);
    sendMessage(msg);
}

void ESPNowClient::getMsgHistory(CommsMsgEntry* entries, uint8_t& count) const {
    count = msgHistoryCount;
    for (uint8_t i = 0; i < msgHistoryCount; i++) {
        uint8_t idx = (msgHistoryIndex + MAX_HISTORY - msgHistoryCount + i) % MAX_HISTORY;
        entries[i] = msgHistory[idx];
    }
}