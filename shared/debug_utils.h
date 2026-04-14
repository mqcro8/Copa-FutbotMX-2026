#pragma once

#ifdef DEBUG_ENABLED
  #define LOG(tag, fmt, ...) Serial.printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#else
  #define LOG(tag, fmt, ...)
#endif