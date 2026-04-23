#include <Arduino.h>
#include "kicker.h"
#include "config/config.h"
#include "debug_utils.h"

namespace {
    uint32_t lastKickTime = 0;
    bool kicking = false;
    uint32_t kickStartTime = 0;
}

void kicker_init() {
    gpio_reset_pin(GPIO_NUM_40);
    pinMode(KICKER_PIN, OUTPUT);
    digitalWrite(KICKER_PIN, LOW);
    LOG("KICKER", "Initialized on GPIO40");
}

bool kick() {
    uint32_t now = millis();

    if (kicking) {
        if (now - kickStartTime >= KICK_DURATION_MS) {
            digitalWrite(KICKER_PIN, LOW);
            kicking = false;
            uint32_t retractTime = now - kickStartTime;
            LOG("KICKER", "Retracted in %lu ms", retractTime);
        }
        return false;
    }

    if (now - lastKickTime < KICK_COOLDOWN_MS) {
        LOG("KICKER", "Cooldown active: %lu ms remaining", KICK_COOLDOWN_MS - (now - lastKickTime));
        return false;
    }

    digitalWrite(KICKER_PIN, HIGH);
    kicking = true;
    kickStartTime = now;
    lastKickTime = now;
    LOG("KICKER", "Fired at %lu ms", now);
    return true;
}

void kicker_update() {
    if (kicking) {
        kick();
    }
}