# Implementation Notes — Algorithm.md

> Notes from implementing `Algorithm.md` into firmware (June 2026).
> Use this alongside `Algorithm.md` and `handoff.md` for bring-up and field tuning.

---

## What Was Implemented

### Main loop (`src/main.cpp`)

Matches the flow described in `Algorithm.md` §1:

1. Kill switch (highest priority)
2. Sensors → `Core::update()`
3. Comms receive + kicker timing
4. Guards (`wallGuard` + `lineGuard`)
5. Strategy FSM (skipped while a guard escape is active)
6. `comms_send()`

Wi-Fi and the web dashboard still run non-blocking each cycle.

### Strategy FSM (`src/behaviors/strategy.cpp`)

**Attacker:** `IDLE` → `SEARCH` → `APPROACH` → `DRIBBLE` → `SHOOT` → back to `SEARCH`

| State | Behavior |
|-------|----------|
| `IDLE` | Motors stopped; waits for kill switch HIGH |
| `SEARCH` | Rotating scan with random direction changes every 2.5–4.5 s |
| `APPROACH` | Ball-angle PID + forward when aligned within 30° |
| `DRIBBLE` | Forward with heading hold; shoot only after `MIN_FIELD_TRAVEL_MS` |
| `SHOOT` | Stop, non-blocking `kick()`, return to `SEARCH` |

**Defender:** `IDLE` → `REPOSITION` → `DEFEND` → `INTERCEPT` → `REPOSITION`

| State | Behavior |
|-------|----------|
| `IDLE` | Motors stopped; waits for kill switch HIGH |
| `REPOSITION` | Align to `home_heading_deg`, then drive backward toward own goal |
| `DEFEND` | Lateral tracking; stops if teammate is in `DRIBBLE`/`SHOOT` |
| `INTERCEPT` | Advances at `FWD_SPEED * 0.8`; kicks if ball is centered (±15°) |

### Safety guards (`src/behaviors/guards.cpp`)

- **Wall guard:** accelerometer spike above `WALL_IMPACT_THRESHOLD` → 500 ms escape (`WALL_ESCAPE_MS`)
- **Line guard:** any color sensor on white line → 300 ms escape (`LINE_ESCAPE_MS`)
- **Shoot exemption:** in `DRIBBLE`, after `MIN_FIELD_TRAVEL_MS`, a front white line does **not** trigger line escape (allows transition to `SHOOT` instead of backing off on the center line)

During an escape window, escape velocity is re-applied every cycle and the strategy FSM is skipped.

### Comms (`src/comms/comms.cpp`)

- Sends real `Core` state (role, state, ball angle, confidence, heading)
- XOR checksum on `RobotMsg`
- `comms_isPeerAlive()` — peer considered lost after `COMMS_TIMEOUT_MS` (500 ms)

### Config (`src/config/config.h`)

Algorithm parameters from `Algorithm.md` §9, plus:

- `MY_ROLE` — compile-time attacker/defender assignment
- `PEER_MAC` — ESP-NOW address of the teammate

### Other changes

- `INTERCEPT` added to `RobotState` in `shared/ir_protocol.h`
- `Core::update()` derives `ball_angle`, `ball_confidence`, and `current_heading` from sensor data
- Web dashboard (`web_server.cpp`) reports `INTERCEPT` state

---

## Before Flashing

### 1. Set role per robot

In `src/config/config.h`, set a different value on each board:

```cpp
constexpr RobotRole MY_ROLE = RobotRole::ATTACKER;  // or DEFENDER
```

### 2. Set peer MAC

Read the other robot’s MAC with `utils/getMacAddress.cpp`, then update:

```cpp
constexpr uint8_t PEER_MAC[6] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};
```

Do **not** leave the placeholder `{0x00, ...}` — ESP-NOW coordination will not work.

### 3. Build and upload

```bash
pio run -e esp32s3-n16r8-usb -t upload
```

For serial logging, the `debug` environment (if configured) or `-DDEBUG_ENABLED` in `platformio.ini` enables `LOG()` macros.

---

## Suggested Validation Order

Follow `Algorithm.md` §10 — do not skip steps:

| Step | What to validate |
|------|------------------|
| 1 | Kill switch stops motors; HIGH after LOW triggers clean restart |
| 2 | `sensors.update()` — IR, color, gyro logs look correct |
| 3 | `motorControl_setVelocity()` — forward, strafe, rotate |
| 4 | `lineGuard` — each color sensor triggers escape |
| 5 | `wallGuard` — controlled wall bump triggers escape |
| 6 | Attacker `SEARCH` + `APPROACH` (IR only) |
| 7 | Attacker `DRIBBLE` + gyro heading correction |
| 8 | Attacker `SHOOT` — only after ~2 s in `DRIBBLE`, not on center line |
| 9 | ESP-NOW — both robots exchange state; defender idles when attacker dribbles |
| 10 | Defender `REPOSITION` / `DEFEND` / `INTERCEPT` |
| 11 | Full match simulation — 3 minutes unsupervised |

---

## Known Limitations and Tuning Notes

### Defender reposition (no odometry)

`REPOSITION` uses gyro heading plus a **timed** backward drive. There are no wheel encoders yet, so “at base position” is approximated by:

- Heading error &lt; 10° for `REPOSITION_ALIGN_MS` (500 ms)
- Total reposition duration ≥ `REPOSITION_MIN_MS` (1500 ms)

Tune `REPOSITION_MIN_MS` and `REPOSITION_ALIGN_MS` on the real field after measuring distance to the goal.

### `DEFENDER_BASE_CM`

Defined in `config.h` (40 cm per `Algorithm.md`) but **not used in code** until odometry or distance sensing exists. It is a reference for future implementation.

### Attacker does not use peer state yet

`Algorithm.md` §6 lists future use of `peer.state == DEFEND/INTERCEPT` for the attacker. Only the **defender** reads peer state today (to stay quiet during ally `DRIBBLE`/`SHOOT`).

### Kill switch pin

`KILL_PIN = 0` in `config.h` — verify this does not conflict with ESP32-S3 strapping or boot behavior on your PCB.

### Comms on first boot

`comms_isPeerAlive()` returns false until the first valid message is received. Robots operate independently until ESP-NOW is confirmed (by design).

### Center line vs penalty area line

The shoot exemption + `MIN_FIELD_TRAVEL_MS` reduce false shoots on the center line. Timing-based logic depends on real `FWD_SPEED` and field friction — adjust `MIN_FIELD_TRAVEL_MS` if shoots fire too early or too late.

### Partial penalty area entry

Per `Algorithm.md` `[FIX v2-C]`, being **completely** inside the penalty area is illegal; touching the line is allowed. `lineGuard` still escapes on first white-line contact (conservative). `SHOOT` stops the robot before kicking to avoid driving deep into the area during kick duration.

---

## File Map (implementation touchpoints)

| File | Role |
|------|------|
| `src/main.cpp` | Loop orchestration |
| `src/behaviors/strategy.cpp` | Attacker/defender FSM |
| `src/behaviors/guards.cpp` | Wall + line guards |
| `src/comms/comms.cpp` | ESP-NOW send/receive |
| `src/core/state.cpp` | Global state + `Core::update()` |
| `src/config/config.h` | Tunables, role, peer MAC |
| `shared/ir_protocol.h` | `RobotState`, `RobotMsg` |
| `Algorithm.md` | Behavioral source of truth |

---

## Optional Next Steps

- Add a `debug` build flag or serial logging for FSM state transitions
- Field-tune `WALL_IMPACT_THRESHOLD`, `MIN_FIELD_TRAVEL_MS`, and reposition timers
- Implement attacker peer-awareness (`DEFEND`/`INTERCEPT` on teammate)
- Use `DEFENDER_BASE_CM` once distance-to-goal sensing exists

---

*Last updated: June 2026 · Copa FutBotMX 2026 — Equipo CIATEQ*
Cursor last session resume -> agent --resume=431716d1-e98c-4354-a2a7-21a79559c980