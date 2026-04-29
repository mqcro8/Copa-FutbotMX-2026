#pragma once
#include "ir_protocol.h"
#include <cstdint>
#include <esp_now.h>

struct CommsStats {
    uint32_t sent;
    uint32_t recv;
    uint32_t failed;
    uint32_t last_tx_ms;
    int16_t  last_latency_ms;
};

struct CommsMsgEntry {
    uint32_t timestamp_ms;
    bool     is_tx;
    RobotRole role;
    RobotState state;
    int16_t  ball_angle_deg;
    int16_t  heading_deg;
    char     text[32];
};

class ESPNowClient {
public:
    static constexpr uint32_t HEARTBEAT_MS = 100;
    static constexpr uint32_t TIMEOUT_MS = 1000;
    static constexpr uint8_t MAX_HISTORY = 10;

    ESPNowClient();
    ~ESPNowClient();

    void begin();
    void update();

    bool sendMessage(const RobotMsg& msg);
    bool isConnected() const;
    RobotMsg getLastMessage() const;
    CommsStats getStats() const;
    uint32_t getLastRxTimeMs() const { return lastRxTimeMs; }

    void setPeer(const uint8_t mac[6]);
    void sendTest();
    void sendTest(const char* text);

    void getMsgHistory(CommsMsgEntry* entries, uint8_t& count) const;

    static ESPNowClient& instance();

private:
    RobotMsg lastRx;
    CommsStats stats = {0, 0, 0, 0, 0};
    uint8_t peerMac[6] = {0, 0, 0, 0, 0, 0};
    bool peerAdded = false;
    uint32_t lastHeartbeat = 0;
    CommsMsgEntry msgHistory[MAX_HISTORY];
    uint8_t msgHistoryCount = 0;
    uint8_t msgHistoryIndex = 0;
    uint32_t lastRxTimeMs = 0;
    static ESPNowClient* s_instance;

    void addPeer();
    void addToHistory(const RobotMsg& msg, bool is_tx, const char* text);
    void handleSent(const uint8_t*, esp_now_send_status_t status);
    void handleRecv(const uint8_t*, const uint8_t* data, int len);
    bool connected() const;
    bool isValidPeerMac(const uint8_t mac[6]) const;

    friend void onSentCallback(const uint8_t*, esp_now_send_status_t);
    friend void onRecvCallback(const uint8_t*, const uint8_t*, int);
};

namespace Comms {
    inline void init() { ESPNowClient::instance().begin(); }
    inline void update() { ESPNowClient::instance().update(); }
    inline bool send(const RobotMsg& msg) { return ESPNowClient::instance().sendMessage(msg); }
    inline bool isConnected() { return ESPNowClient::instance().isConnected(); }
    inline RobotMsg getLastMessage() { return ESPNowClient::instance().getLastMessage(); }
    inline CommsStats getStats() { return ESPNowClient::instance().getStats(); }
    inline uint32_t getLastRxTimeMs() { return ESPNowClient::instance().getLastRxTimeMs(); }
    inline void setPeer(const uint8_t mac[6]) { ESPNowClient::instance().setPeer(mac); }
    inline void sendTest() { ESPNowClient::instance().sendTest(); }
    inline void sendTest(const char* text) { ESPNowClient::instance().sendTest(text); }
    inline void getMsgHistory(CommsMsgEntry* entries, uint8_t& count) { ESPNowClient::instance().getMsgHistory(entries, count); }
}