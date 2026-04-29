#include "settings.h"
#include "debug_utils.h"
#include <Preferences.h>

namespace {
    Preferences prefs;
    const char* NAMESPACE = "futbotmx";
    const char* KEY_PEER_MAC = "peer_mac";
    const char* KEY_ROLE = "role";
    const char* KEY_KILLER_SPEED = "killer_speed";

    uint8_t defaultPeerMac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
}

void Settings::init() {
    prefs.begin(NAMESPACE, false);
    LOG("SETTINGS", "Initialized, namespace: %s", NAMESPACE);

    uint8_t storedMac[6];
    if (!prefs.isKey(KEY_PEER_MAC)) {
        prefs.putBytes(KEY_PEER_MAC, defaultPeerMac, 6);
        LOG("SETTINGS", "Using default peer MAC: %02X:%02X:%02X:%02X:%02X:%02X",
            defaultPeerMac[0], defaultPeerMac[1], defaultPeerMac[2],
            defaultPeerMac[3], defaultPeerMac[4], defaultPeerMac[5]);
    } else {
        prefs.getBytes(KEY_PEER_MAC, storedMac, 6);
        LOG("SETTINGS", "Loaded peer MAC: %02X:%02X:%02X:%02X:%02X:%02X",
            storedMac[0], storedMac[1], storedMac[2],
            storedMac[3], storedMac[4], storedMac[5]);
    }
}

bool Settings::getPeerMac(uint8_t mac[6]) {
    if (!prefs.isKey(KEY_PEER_MAC)) {
        return false;
    }
    prefs.getBytes(KEY_PEER_MAC, mac, 6);
    return true;
}

bool Settings::setPeerMac(const uint8_t mac[6]) {
    prefs.putBytes(KEY_PEER_MAC, mac, 6);
    LOG("SETTINGS", "Saved peer MAC: %02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return true;
}

uint8_t Settings::getRole() {
    return prefs.getUChar(KEY_ROLE, 0);
}

bool Settings::setRole(uint8_t role) {
    prefs.putUChar(KEY_ROLE, role);
    return true;
}

uint8_t Settings::getKillerSpeed() {
    return prefs.getUChar(KEY_KILLER_SPEED, 100);
}

bool Settings::setKillerSpeed(uint8_t speed) {
    prefs.putUChar(KEY_KILLER_SPEED, speed);
    return true;
}

void Settings::reset() {
    prefs.clear();
    prefs.putBytes(KEY_PEER_MAC, defaultPeerMac, 6);
    prefs.putUChar(KEY_ROLE, 0);
    prefs.putUChar(KEY_KILLER_SPEED, 100);
    LOG("SETTINGS", "Reset to defaults");
}

String Settings::getPeerMacStr() {
    uint8_t mac[6];
    if (getPeerMac(mac)) {
        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(buf);
    }
    return String("00:00:00:00:00:00");
}

String Settings::getOwnMacStr() {
    return WiFi.macAddress();
}