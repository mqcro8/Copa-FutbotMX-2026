# Copa FutBotMX 2026 — Team Lumen - Sponsored by CIATEQ

> **Autonomous robotic soccer for the Agile Category** | June 24–26, 2026 · UPIITA-IPN, Mexico City

[![Competition](https://img.shields.io/badge/Competition-Copa%20FutBotMX%202026-blue)](https://secihti.mx/futbotmx/)
[![Category](https://img.shields.io/badge/Category-Ágil%20(IR)-green)](https://secihti.mx/futbotmx/)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-red)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/Framework-PlatformIO%20%2B%20Arduino-orange)](https://platformio.org/)
[![Sponsor](https://img.shields.io/badge/Sponsor-CIATEQ-purple)](https://www.ciateq.mx/)

---

## The Team

We are a three-person student team from Mexico, sponsored and supported by **[CIATEQ — Centro de Tecnología Avanzada A.C.](https://www.ciateq.mx/)**, a leading applied research and technology center.

| Member | Role |
|--------|------|
| **Miguel** *(Team Leader)* | Software architecture, firmware, algorithms & control |
| **Enrique** | Electronics design, PCB integration & software support |
| **Norma** | Mechanical design, robot structure, chassis & hardware assembly |

We are competing in the **Categoría Ágil** — the infrared ball detection category — where two fully autonomous robots per team play 2×10-minute matches with no remote control allowed.

---

## What We're Building

Two fully autonomous soccer robots capable of detecting an IR-emitting ball, tracking it, making coordinated decisions, and scoring goals — all without any human intervention during play.

The robots communicate wirelessly with each other via **ESP-NOW**, share game state in real time, and divide responsibilities between an **attacker** and a **defender** role. Everything from ball detection to motor control runs on-board at ≥100 Hz.

---

## Technical Overview

### Hardware Platform

| Component | Specification |
|-----------|--------------|
| Microcontroller | ESP32-S3 |
| Drive system | 3-wheel omnidirectional (holonomic) |
| Ball detection | 7× VS1838B IR receiver array |
| Line detection | 4× TCS34725 RGB color sensors (via TCA9545A I²C mux) |
| Orientation | BMI160 6-axis IMU (gyro + accelerometer) |
| Actuator | Solenoid kicker |
| Communication | ESP-NOW (inter-robot), Wi-Fi AP (telemetry dashboard) |
| Max weight | 1.4 kg |
| Max diameter | 22 cm |

### Software Architecture

The firmware is written in **C++17** using PlatformIO with the Arduino core for ESP32-S3. Key design principles:

- **Non-blocking main loop** targeting ≥ 100 Hz — no `delay()` in `loop()`
- **FreeRTOS mutexes** for thread-safe sensor data sharing
- **State machine** for autonomous behavior: `IDLE → SEARCH → APPROACH → DRIBBLE → SHOOT`
- **Inverse kinematics** for 3-wheel omnidirectional drive
- **Web telemetry dashboard** served directly from the robot over Wi-Fi

---

## Project Structure

```
Copa-FutBotMX-2026/
├── src/
│   ├── main.cpp                  ← Entry point, main loop
│   ├── config/
│   │   └── config.h              ← Pin definitions, PID gains, timing constants
│   ├── core/
│   │   ├── state.h / .cpp        ← Global robot state (role, FSM, sensor data)
│   │   └── SensorManager.h / .cpp← Unified sensor access layer
│   ├── drivers/
│   │   ├── motor_control.h / .cpp← Holonomic motor driver + inverse kinematics
│   │   └── kicker.h / .cpp       ← Solenoid kicker with cooldown logic
│   ├── sensors/
│   │   ├── ir_sensor_array.h / .cpp   ← IR ball detection + circular mean angle
│   │   ├── ColorSensorArray.h / .cpp  ← Line/boundary detection (TCS34725)
│   │   └── gyro.h / .cpp              ← BMI160 IMU integration
│   ├── behaviors/
│   │   └── strategy.h / .cpp     ← Game state machine + role logic
│   ├── comms/
│   │   └── comms.h / .cpp        ← ESP-NOW inter-robot communication
│   └── net/
│       ├── wifi.h / .cpp         ← Wi-Fi AP/STA management
│       └── web_server.h / .cpp   ← Async REST API + telemetry dashboard
├── shared/
│   ├── ir_protocol.h             ← ESP-NOW message structs (roles, states)
│   ├── field_constants.h         ← Field dimensions in cm
│   └── debug_utils.h             ← LOG() macro (disabled in production builds)
├── data/
│   ├── index.html                ← Telemetry dashboard UI
│   ├── style.css
│   └── script.js
├── docs/
│   ├── BOM.md                    ← Bill of Materials (required for registration)
│   └── poster/
└── platformio.ini
```

---

## Project Highlights

### IR Ball Tracking
The robot uses 7 infrared receivers arranged around its perimeter. A **monostable retriggerable filter** removes noise, and a **circular mean algorithm** calculates the precise angle to the ball — even when multiple sensors fire simultaneously.

### Holonomic Drive
Three omni-wheels at 90°, 210°, and 330° allow the robot to move in any direction instantly — forward, sideways, diagonal — without rotating first. Full inverse kinematics implemented from scratch.

### Live Telemetry Dashboard
Each robot hosts a web dashboard over its own Wi-Fi access point. From any browser on the same network you can monitor:
- Robot role and game state
- IR sensor array (radar view)
- Gyro/IMU heading and orientation
- RGB color sensor readings
- Real-time uptime and emergency stop

### Inter-Robot Coordination
Both robots communicate at ≥10 Hz using **ESP-NOW** (no router required). They share ball angle, confidence, heading, and role — enabling cooperative strategies like one robot defending while the other attacks.

---

## Gallery

> _Photos and GIFs will be added as the build progresses._

| Robot Build | Telemetry Dashboard | Field Test |
|:-----------:|:-------------------:|:----------:|
| *(coming soon)* | *(coming soon)* | *(coming soon)* |

---

## Getting Started

### Prerequisites
- [PlatformIO](https://platformio.org/) (VS Code extension recommended)
- ESP32-S3 board

## Resources

-  [Official Competition Rules](https://secihti.mx/futbotmx/)
-  [CIATEQ — Our Sponsor](https://www.ciateq.mx/)

---

## Licencia / License

This project is developed for academic and competition purposes. See [LICENSE](LICENSE) for details.

---


---

## 🇲🇽 Versión en Español

> *For our sponsor, teammates, and the Mexican robotics community — here's a complete overview in Spanish.*

---

# Copa FutBotMX 2026 — Equipo CIATEQ

Este repositorio contiene el firmware completo y la arquitectura de software para nuestros dos robots de fútbol autónomos, con los que competimos en la **Copa FutBotMX 2026, Categoría Ágil** (24–26 de junio, UPIITA-IPN, Ciudad de México).

---

### El Equipo

Somos un equipo de tres estudiantes mexicanos, patrocinados y apoyados por **[CIATEQ — Centro de Tecnología Avanzada A.C.](https://www.ciateq.mx/)**.

| Integrante | Área |
|------------|------|
| **Miguel** *(Líder)* | Software: arquitectura, firmware, algoritmos y control |
| **Enrique** | Electrónica: diseño de circuitos, integración de PCB y apoyo en software |
| **Norma** | Hardware: diseño mecánico, estructura del chasis y ensamblaje |

---

### ¿Qué estamos construyendo?

Dos robots de fútbol completamente autónomos capaces de detectar una pelota emisora de IR, rastrearla, tomar decisiones coordinadas y marcar goles — sin ninguna intervención humana durante el juego.

Los robots se comunican entre sí de forma inalámbrica mediante **ESP-NOW**, comparten estado de juego en tiempo real y se dividen responsabilidades entre el rol de **atacante** y **defensor**. Todo el procesamiento ocurre en el robot a ≥100 Hz.

---

### Resumen Técnico

**Plataforma de hardware:**
- Microcontrolador ESP32-S3
- Sistema de tracción omnidireccional de 3 ruedas (holonómico)
- 7 receptores IR VS1838B para detección de pelota
- 4 sensores de color TCS34725 para detección de líneas blancas
- IMU BMI160 de 6 ejes para orientación
- Patada solenoide
- Comunicación ESP-NOW entre robots + dashboard de telemetría vía Wi-Fi

**Software:**
- C++17 con PlatformIO y Arduino core para ESP32-S3
- Loop principal no bloqueante a ≥100 Hz
- Máquina de estados para comportamiento autónomo
- Cinemática inversa para tracción omnidireccional
- Dashboard de telemetría en tiempo real accesible desde el navegador

---

### Estado del Proyecto

| Subsistema | Estado |
|------------|--------|
| Control de motores (holonómico) | ✅ Implementado |
| Detección IR de pelota | ✅ Implementado |
| Sensores de color / línea | ✅ Implementado |
| IMU / giroscopio | ✅ Implementado |
| Comunicación ESP-NOW | ✅ Implementado |
| Dashboard de telemetría Wi-Fi | ✅ Implementado |
| Patada (kicker solenoide) | ✅ Implementado |
| Estrategia y lógica de juego | 🔄 En desarrollo |
| Chasis + llantas + ensamble físico | 🔄 En progreso |
| Pruebas de campo completas | 🔜 Pendiente |

---

### 🤝 Agradecimientos

Agradecemos a **CIATEQ** por el patrocinio, las instalaciones y el apoyo técnico brindado a lo largo de este proyecto. Su respaldo ha sido fundamental para llevar este equipo a la competencia.

---

*Desarrollado con 💚 en México · Copa FutBotMX 2026*
