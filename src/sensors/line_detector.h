#pragma once
#include <cstdint>

struct LineData {
    bool left_detected;
    bool right_detected;
};

void lineDetector_init();
LineData lineDetector_update();