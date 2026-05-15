# Handoff — FutBotMX 2026

## 1. Goal

Build two fully autonomous IR-soccer robots for **Copa FutBotMX 2026** (Ágil category, June 24–26, UPIITA-IPN, Mexico City). Each robot detects an IR-emitting ball via 7× VS1838B receivers, drives holonomically (3 omni-wheels), communicates with its teammate via ESP-NOW (≥10 Hz), and executes cooperative attacker/defender behaviors — all on an ESP32-S3 at ≥100 Hz with no blocking calls.

## 2. Current State

| Subsystem | Status | Notes |
|-----------|--------|-------|
| Motor control (holonomic IK) | ✅ Done | 3-wheel inverse kinematics, normalized -1..1, TB6612FNG driver |
| IR sensor array | ✅ Done | 7 VS1838B, monostable retriggerable filter, circular mean angle |
| Color sensor array | ✅ Done | 4× TCS34725 via TCA9545A I²C mux, round-robin reads, white-line threshold |
| BMI160 gyro/IMU | ✅ Done | Gyro + accel, dead-reckoning yaw integration, pitch/roll from accel |
| Kicker solenoid | ✅ Done | Non-blocking, configurable duration + cooldown |
| ESP-NOW comms | 🟡 Skeleton | `comms_init/send` exist but hardcoded peer MAC, msg has placeholder data |
| Wi-Fi AP + web server | ✅ Done | AP_STA mode, AsyncWebServer, REST API (status/state/telemetry/command) |
| Web dashboard UI | ✅ Done | HTML/CSS/JS served from LittleFS |
| SensorManager | ✅ Done | Unified init/update for all sensors, FreeRTOS mutex thread-safety |
| Core state module | ✅ Done | Global role, state, heading, ball angle, IR/color/gyro data |
| TaskFactory | ✅ Done | Generic FreeRTOS pinned-task creator |
| **Behavioral FSM** | **🟡 Skeleton** | `strategy.h/.cpp` — both `init()` and `update()` are empty shells |
| **Main loop logic** | **🟡 Stub** | Currently direct reactive control (forward on center sensor, turn otherwise) — does NOT call strategy module or comms |
| Physical chassis + wheels | 🔄 In progress | Not yet assembled |

### Architecture summary

- `main.cpp:setup()` → inits Core, motor, SensorManager, WiFi, LittleFS, WebSrv
- `main.cpp:loop()` → polls sensors via `SensorManager::update()`, copies data into `Core`, then runs a **direct reactive stub** (group-based forward/turn) — no state machine called
- `strategy_update()` is never invoked
- `comms_send()` is never invoked
- `kick()` is never invoked

## 3. Files in Flight

| File | Status | What's missing |
|------|--------|----------------|
| `src/behaviors/strategy.cpp` | Empty body | The entire FSM — `strategy_update()` has no code |
| `src/behaviors/strategy.h` | Declared only | Public API exists (3 functions), nothing behind it |
| `src/main.cpp` | Working stub | Loop needs to call `strategy_update()`, `comms_send()`, kicker; replace direct reactive with FSM-driven output |
| `src/comms/comms.cpp` | Hardcoded stub | Peer MAC is placeholder `{0xAA,...}`; `comms_send()` sends dummy data instead of real `Core` state |
| `src/core/TaskFactory.h` | Present | Utility for FreeRTOS tasks — not used anywhere yet |

## 4. Changes (This Session)

No changes — this is a read-only analysis. The commit history shows:

- **2efbf67** — Revise README with full project and team details
- **dc312ee / a35cb2a** — Gyro fixes: BMI160 scale factor, integration scaling, yaw wrap (0–360)
- **2954e24** — Refactor color sensors and IR array handling
- **abc6599** — Add BMI160 gyro driver with pitch/roll/yaw
- **693f69a** — IR sensor array with retriggerable monostable filter
- **ee52a65** — WiFi AP + web server + IR ball tracking
- **Earlier** — Motor control, kicker, initial sensor work

## 5. Failed / Abandoned Approaches

| Attempt | What happened | Evidence |
|---------|---------------|----------|
| `ball_tracker` module | Deleted, consolidated into `ir_sensor_array` | Commit `d81e93: cleanup: remove unused ball_tracker module` |
| `compass.h/.cpp` | Written but never included; `gyro.cpp` is the active IMU driver | File still at `src/sensors/compass.*` but not referenced anywhere in the build |
| `line_detector.h/.cpp` | Written but never used; `ColorSensorArray` handles line detection | File still exists but not included |
| Duplicate `kicker.cpp` | `src/others/kicker.cpp` has `kicker_kick()` while `src/drivers/kicker.cpp` has `kick()` — the `others/` copy is a stale leftover from an earlier API version | Not included from `main.cpp` |
| `algorithm_test` | `src/drivers/algorithm_test.*` exists but `algorithmTest_loop()` is never called | Dead code from an earlier offline-validation attempt |
| Large IR sensor count | Originally had more than 7 IR sensors, reduced after testing | Commit `6486529: fix: reduce IR sensors to 7 (removed rear)` |

## 6. Next Step

**Implement the state machine in `strategy.cpp`.**

Specifically:

1. `strategy_init()` already sets state to `SEARCH` — OK.
2. `strategy_update()` needs to read `Core::irData` / `Core::ball_angle` and run transitions like:

```
SEARCH:
  if ball detected → go to APPROACH
  else → rotate in place (search sweep)

APPROACH:
  if ball not centered → adjust rotation (use ball_angle PID from config.h)
  if ball centered + close (high intensity) → go to DRIBBLE
  if ball lost for N ms → back to SEARCH

DRIBBLE:
  drive forward slowly toward opponent goal
  if aligned and ball centered → go to SHOOT

SHOOT:
  call kick()
  after kick cooldown done → back to SEARCH
```

3. Wire `strategy_update()` into `main.cpp:loop()` — call it every cycle.
4. Use the output from strategy to drive `motorControl_setVelocity()` instead of the current group-based logic.

After FSM is working, the follow-on steps are:
- Pull real data into `comms_send()` and use the teammate's message for role negotiation (attacker vs defender)
- Add line detection to trigger state changes (stop/reverse when approaching white line)
- Handle emergency stop properly in the strategy loop
