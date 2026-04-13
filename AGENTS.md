# AGENTS.md — CopaFutBotMX 2026 (Categoría Ágil)

> This file provides context for AI coding assistants (GitHub Copilot, Cursor,
> Windsurf, Aider, etc.) about this embedded robotics project.
> **Code is written in English. Comments may be in Spanish.**

---

## Project Overview

Autonomous robotic soccer team for **Copa FutBotMX 2026 — Categoría Ágil** (June 24–26, Mexico City, UPIITA-IPN). Two fully autonomous robots per team compete in 2×10-minute matches on a 243 cm × 182 cm green-carpet field. Ball detection is based on **infrared (IR) emission** from a 42 mm diameter ball.

- **Official rules:** https://secihti.mx/futbotmx/
- **IR ball reference design:** https://github.com/robocup-junior/ir-golf-ball
- **Registration deadline:** April 17, 2026

---

## Repository Structure

```
futbotmx/
├── AGENTS.md                  ← This file
├── platformio.ini             ← PlatformIO project config
├── partitions-16mb.csv        ← Partition table for 16MB flash
├── README.md
├── src/                       ← Firmware source code
│   ├── main.cpp              ← Entry point
│   ├── config/
│   │   └── config.h          ← Pin definitions, PID gains, timing constants
│   ├── core/
│   │   ├── state.h           ← Global state (role, state, sensors)
│   │   └── state.cpp
│   ├── drivers/
│   │   ├── motor_control.h   ← Motor driver + PID
│   │   └── motor_control.cpp
│   ├── sensors/
│   │   ├── ball_tracker.h    ← IR sensor array logic
│   │   ├── ball_tracker.cpp
│   │   ├── line_detector.h   ← Field boundary sensors
│   │   ├── line_detector.cpp
│   │   ├── compass.h         ← IMU / magnetometer heading
│   │   └── compass.cpp
│   ├── comms/
│   │   ├── comms.h           ← ESP-NOW inter-robot communication
│   │   └── comms.cpp
│   └── behaviors/
│       ├── strategy.h        ← State machine + game logic
│       └── strategy.cpp
├── shared/                    ← Common headers shared across robots
│   ├── ir_protocol.h          ← ESP-NOW message structs
│   ├── field_constants.h      ← Field dimensions in cm
│   └── debug_utils.h          ← Serial log macros
└── docs/
    ├── BOM.md                 ← Bill of Materials (required for registration)
    ├── poster/
    └── video/
```

---

## Hardware Platform

| Component | Value |
|---|---|
| MCU | ESP32-S3 |
| Framework | PlatformIO + Arduino core (`espressif32`) |
| Operating voltage | TBD (max 48 V DC per rules) |
| Max weight per robot | **1.4 kg** |
| Max size per robot | **22.0 cm** diameter and height |
| Ball capture zone depth | **Max 1.5 cm** |
| IR ball diameter | 42 mm |

> **Note:** Hardware configuration fields still pending (battery, motors, driver, sensors).
> Update `config.h` and `platformio.ini` as components are selected.

---

## Coding Conventions

### Language & Style

- **All identifiers, function names, and file names in English.**
- Comments explaining *why* (not *what*) can be in Spanish if the team prefers.
- C++17, PlatformIO Arduino core for ESP32-S3.
- Use `constexpr` and `const` over `#define` for constants.
- Avoid dynamic memory allocation (`new`, `malloc`) in the main loop — prefer stack or static allocation.
- Use `enum class` for states and roles, never raw integers.

### File Structure

Each `.h` file must have include guards:
```cpp
#pragma once
```

Every module exposes an `init()` function and an `update()` function:
```cpp
void ballTracker_init();
BallVector ballTracker_update();  // Returns angle + intensity
```

### Serial Debugging

Use macros from `shared/debug_utils.h`:
```cpp
#ifdef DEBUG_ENABLED
  #define LOG(tag, fmt, ...) Serial.printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#else
  #define LOG(tag, fmt, ...)
#endif
```

Enable with `-DDEBUG_ENABLED` in `platformio.ini` for the `debug` environment.
**Never leave blocking Serial prints in production builds.**

### No Blocking Code in `loop()`

- No `delay()` in `loop()` — use non-blocking timing with `millis()`.
- Sensor reads, PID updates, and ESP-NOW callbacks must all be non-blocking.
- Target main loop frequency: **≥ 100 Hz**.


---

## Testing & Validation

### Unit Tests (native environment)

Use PlatformIO's `test/` folder with `Unity` framework:
```
platformio test -e native
```
Write unit tests for:
- PID output given known error values.
- State machine transitions given mock sensor inputs.
- ESP-NOW message serialization / CRC validation.

### On-Robot Integration Tests

Before competition, validate each subsystem in order:
1. **Emergency stop** — must halt all motors immediately.
2. **IR ball detection** — robot rotates until ball is at 0°.
3. **Line detection** — robot stops at field boundary.
4. **Compass** — heading reads stable 0–359° while rotating.
5. **Motor PID** — robot drives straight 1 m and returns.
6. **Kick power test** — ball kicked from own goal must not return after rebound (Rule 4.4.4).
7. **ESP-NOW** — both robots exchange state at ≥10 Hz with <50 ms latency.
8. **Full match simulation** — 3-minute unsupervised run.

---

## What AI Assistants Should NOT Generate

- Code that uses `analogRead()` on IR emitter pins to detect the ball — use a **dedicated IR receiver array**, not IR emitters.
- `delay()` calls inside `loop()` or ISR callbacks.
- Remote control or serial command handlers for match play.
- Any reference to orange, yellow, or blue color constants for robot body coloring.
- Dynamic heap allocation in time-critical paths.
- Hard-coded MAC addresses for ESP-NOW without a comment explaining how to reconfigure them.

---

## Contact & Resources

- Official site: https://secihti.mx/futbotmx/
- Email: futbotmx@secihti.mx
- IR ball GitHub: https://github.com/robocup-junior/ir-golf-ball
- RoboCup Junior forum: https://junior.forum.robocup.org/
