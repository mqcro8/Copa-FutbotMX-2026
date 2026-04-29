#pragma once
#include <cstdint>
#include <WiFi.h>

namespace Settings {
    void init();

    bool getPeerMac(uint8_t mac[6]);
    bool setPeerMac(const uint8_t mac[6]);

    uint8_t getRole();
    bool setRole(uint8_t role);

    uint8_t getKillerSpeed();
    bool setKillerSpeed(uint8_t speed);

    void reset();

    String getPeerMacStr();
    String getOwnMacStr();
}