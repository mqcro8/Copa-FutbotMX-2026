# ALGORITHM.md — FutBotMX 2026 · Equipo CIATEQ

> **Audiencia**: Todo el equipo (Miguel, Enrique, Norma).
> Este documento describe el comportamiento completo de los dos robots — qué
> hace cada uno, cuándo lo hace, y cómo se coordinan. Es la fuente de verdad
> para tomar decisiones de implementación.
>
> **Versión 2** — Revisada contra el reglamento oficial `Reglas_Copa_FutBotMX_20260105`.
> Los cambios respecto a la v1 están marcados con `[FIX vX]` con su justificación reglamentaria.

---

## 1. Visión General

Tenemos **dos robots** en la cancha al mismo tiempo. Uno actúa como **atacante**
y el otro como **defensor**. Esta división de roles es la estrategia central
del equipo: en lugar de que ambos robots persigan la pelota (lo cual causa
colisiones entre aliados y expone nuestra portería), cada robot tiene una
responsabilidad clara.

La coordinación entre robots ocurre vía **ESP-NOW** a ≥ 10 Hz. Ningún robot
toma decisiones perfectamente aislado — cada uno sabe el estado del otro.

> ⚠️ **Estado del código (Junio 2026)**: `comms_send()` actualmente envía datos
> placeholder y usa una MAC hardcodeada `{0xAA,...}`. La coordinación descrita
> en las secciones §3 y §4 **no funcionará hasta que se corrija `comms.cpp`**.
> Ver handoff.md §3 "Files in Flight". Implementar esto es el paso 8 del orden
> de implementación (§10).

```
┌──────────────────────────────────────────────────────┐
│                   MAIN LOOP  (≥100 Hz)               │
│                                                      │
│  1. killSwitch check      ← PRIMERO, siempre        │
│  2. sensors.update()      ← IR, color, gyro          │
│  3. Core::update()        ← push data to global state│
│  4. comms.update()        ← receive peer state       │
│  5. wallGuard()           ← [FIX v2-A] paredes       │
│  6. lineGuard()           ← líneas blancas           │
│  7. role_strategy()       ← attacker OR defender FSM │
│  8. motorControl_apply()  ← execute movement command │
│  9. comms.send()          ← broadcast our state      │
└──────────────────────────────────────────────────────┘
```

**Regla de oro doble** `[FIX v2-A]`: tanto la detección de línea blanca como
el contacto con una pared interrumpen cualquier otra acción. Un robot que
toca una pared es retirado 1 minuto igual que uno que entra al área de
penalti (Regla §4.4.10.4).

---

## 2. Roles y Asignación

### ¿Cómo se asigna el rol?

El rol se configura **en tiempo de compilación** mediante una constante en
`config.h`. No hay negociación de rol en tiempo de ejecución — esto simplifica
el sistema y elimina una fuente de bugs.

```cpp
// config.h
constexpr RobotRole MY_ROLE = RobotRole::ATTACKER; // o DEFENDER
```

Cada robot sabe quién es desde el momento en que enciende. El otro robot
conoce nuestro rol porque lo transmitimos en cada mensaje ESP-NOW.

### ¿Pueden cambiar de rol?

En la implementación inicial: no. Como mejora futura se puede agregar
intercambio dinámico de rol si el defensor detecta la pelota notablemente
más cerca que el atacante, pero se deja para después de consolidar el
comportamiento base.

---

## 3. Máquina de Estados — Atacante

El atacante tiene 5 estados. Solo puede estar en uno a la vez.

```
                    ┌─────────┐
         inicio ──► │  IDLE   │
                    └────┬────┘
                         │ kill switch liberado (HIGH) tras silbato
                         ▼
              ┌──────────────────────┐
         ┌───►│        SEARCH        │◄──────────────────┐
         │    └──────────┬───────────┘                   │
         │               │ IR detecta pelota             │
         │               ▼                               │
         │    ┌──────────────────────┐                   │
         │    │       APPROACH       │                   │
         │    └──────────┬───────────┘                   │
         │               │ pelota en frente (±15°)       │
         │               │ Y intensity ≥ 2               │
         │               ▼                               │
         │    ┌──────────────────────┐                   │
         │    │       DRIBBLE        │                   │
         │    └──────────┬───────────┘                   │
         │               │ sensor FRONT detecta línea    │
         │               │ blanca del área rival         │
         │               ▼                               │
         │    ┌──────────────────────┐                   │
         │    │        SHOOT         │                   │
         │    └──────────┬───────────┘                   │
         │               │ temporizador de disparo fin   │
         │               └───────────────────────────────┘
         │
         │  (desde cualquier estado, máxima prioridad)
         ├──── pared detectada ──────► wallEscape (§5)
         └──── línea blanca detectada ► lineEscape (§5)
```

### Estado: IDLE

**¿Qué hace?** El robot espera con motores detenidos. No se mueve.

**Entrada**: al arrancar, o tras un restart por kill switch.

**Salida**: kill switch liberado (pin HIGH) → SEARCH.

**Nota de reglas (§4.3.4)**: Un robot que se mueve antes del silbato es
retirado y declarado dañado. IDLE es el único estado seguro antes del
inicio.

### Estado: SEARCH

**¿Qué hace?** El robot gira sobre sí mismo buscando la pelota.

**Entrada**: ningún sensor IR activo.

**Comportamiento**:
- Rotar en la dirección actual (`search_dir_cw`) a velocidad angular
  constante (`ROT_SPEED = 0.5`).
- Para evitar oscilaciones simétricas que dejen al robot girando en el
  mismo punto muerto, el intervalo de cambio de sentido incluye jitter:

```cpp
if (millis() - last_search_turn_ms > search_interval_ms) {
    search_dir_cw = !search_dir_cw;
    last_search_turn_ms = millis();
    search_interval_ms = random(2500, 4500);
}
```

**Salida**: cualquier sensor IR activo → APPROACH.

**Nota de reglas (§4.4.7)**: El árbitro puede declarar daño por falta de
combatividad si el robot no busca la pelota activamente. SEARCH debe ser
visible y dinámico.

### Estado: APPROACH

**¿Qué hace?** El robot se mueve hacia la pelota alineándose con ella.

**Entrada**: IR detecta pelota (al menos 1 sensor activo).

**Control**:
```cpp
omega = clamp(ball_angle * BALL_KP, -ROT_SPEED, ROT_SPEED);
vx    = (fabsf(ball_angle) < 30.0f) ? FWD_SPEED : 0.3f;
motorControl_setVelocity(vx, 0.0f, omega);
```

**Salida**:
- `|ball_angle| ≤ 15°` Y `intensity ≥ 2` → DRIBBLE.
- `intensity == 0` por más de 150 ms → SEARCH.

### Estado: DRIBBLE

**¿Qué hace?** El robot avanza hacia la portería contraria con la pelota
al frente, corrigiendo orientación con el giroscopio.

**Entrada**: pelota centrada frente al robot.

**Comportamiento**:
- Avanzar a `FWD_SPEED` mientras mantiene `ball_angle ≈ 0°`.
- Corrección de heading mediante PID sobre giroscopio (`HEADING_KP`).
- Si la pelota se descentra, tolerar máximo 200 ms antes de degradar.

**Salida**:
- Sensor FRONT detecta línea blanca del área rival → SHOOT (ver nota abajo).
- `intensity == 0` → SEARCH.
- `|ball_angle| > 30°` por más de 200 ms → APPROACH.

**`[FIX v2-B]` — Ambigüedad de líneas blancas en el camino del atacante**:
El atacante cruza **tres** líneas blancas antes de la portería rival:
la línea central, la línea frontal del área rival, y la línea de gol.
Los sensores de color no saben cuál es cuál. La distinción correcta es
**temporal**: el atacante no debe disparar en la línea central ni en la
de gol, solo en la del área. Solución implementable: el disparo solo se
habilita después de haber recorrido al menos `MIN_FIELD_TRAVEL_MS` desde
el inicio de DRIBBLE, que equivale aproximadamente a haber superado la
línea central. Valor sugerido: 2000 ms a `FWD_SPEED = 0.7`.

```cpp
// Solo habilitar SHOOT si ya se viajó suficiente tiempo en DRIBBLE
bool shootEnabled = (millis() - dribble_start_ms) > MIN_FIELD_TRAVEL_MS;
if (cs.onWhiteLine[FRONT] && shootEnabled) → SHOOT;
```

Esto no es perfecto (depende de velocidad real), pero es robusto contra el
falso positivo de la línea central. La línea de gol (40 cm) solo existe si
el robot ya está dentro de la portería — a esa distancia la pelota ya entró
y no tiene sentido disparar.

### Estado: SHOOT

**¿Qué hace?** Detiene el avance y dispara el solenoide de forma asíncrona.

**Entrada**: sensor frontal detectó línea blanca del área rival (con
`shootEnabled == true`).

**`[FIX v2-C]` — La penalización es "completamente dentro", no parcialmente**:
El reglamento §4.4.10.1 dice: "No se permite que los robots estén
**completamente** dentro del área de penalti." Estar **parcialmente**
dentro es legal. Esto significa que el robot puede detenerse justo sobre
la línea blanca del área (con el cuerpo parcialmente dentro) sin ser
penalizado. Sin embargo, para máxima seguridad seguimos disparando desde
afuera — un disparo desde la línea es igual de efectivo y no arriesga
penalización.

```cpp
motorControl_stop();  // detenerse antes de disparar
kick();               // activa solenoide (non-blocking)
// kicker_update() gestiona la desactivación cada ciclo
→ SEARCH             // buscar de nuevo inmediatamente
```

**Prohibido usar `delay()`**: durante el ciclo de kick el robot debe seguir
leyendo sensores de línea. A `FWD_SPEED = 0.7` el robot recorre el área de
30 cm en ~200 ms — si hay un `delay(KICK_DURATION_MS=500)` el robot no
reacciona a la línea de gol.

**`KICK_COOLDOWN_MS = 2000`** controlado por software previene disparos
consecutivos involuntarios (requerido por §4.4.4 prueba de potencia).

---

## 4. Máquina de Estados — Defensor

El defensor tiene **3 estados.** Su misión es resguardar nuestra portería
sin cruzarse en la trayectoria del atacante aliado.

```
                    ┌─────────┐
         inicio ──► │  IDLE   │
                    └────┬────┘
                         │
                         ▼
              ┌──────────────────────┐
         ┌───►│      REPOSITION      │◄──────────────┐
         │    └──────────┬───────────┘               │
         │               │ robot en posición base    │
         │               ▼                           │
         │    ┌──────────────────────┐               │
         │    │        DEFEND        │◄──────────────┤
         │    └──────────┬───────────┘               │
         │               │ pelota se acerca          │
         │               ▼                           │
         │    ┌──────────────────────┐               │
         │    │      INTERCEPT       │               │
         │    └──────────┬───────────┘               │
         │               │ pelota alejada            │
         │               └───────────────────────────┘
         │
         ├──── pared detectada ──► wallEscape (§5)
         └──── línea blanca ──────► lineEscape (§5)
```

### Posición Base del Defensor

El defensor se posiciona **a ~40 cm frente a nuestra portería**, en el centro.
Posición fija, calculada relativa al heading inicial.

```cpp
constexpr float DEFENDER_BASE_CM = 40.0f;
```

El defensor opera exclusivamente en su mitad del campo. Su límite de avance
es la línea central — que sus sensores de color detectarán como línea blanca.

### Estado: REPOSITION

**¿Qué hace?** Volver a la posición base frente a la portería propia.

**Cuándo ocurre**: al inicio, después de INTERCEPT, después de cualquier
escape de línea o pared.

**Comportamiento**: usar heading del giroscopio para orientarse hacia la
portería propia y avanzar hacia la posición base.

**Salida**: `heading_error < 10°` Y `en_posición_base` → DEFEND.

### Estado: DEFEND

**¿Qué hace?** El defensor se mueve lateralmente (strafe) para seguir el
ángulo de la pelota, manteniéndose a ~40 cm de la portería.

**Comportamiento**:
```cpp
// Si el atacante aliado tiene la pelota → quieto (no interferir)
if (peer.state == RobotState::DRIBBLE || peer.state == RobotState::SHOOT) {
    motorControl_stop();
    return;
}

// Seguir ángulo de la pelota lateralmente
float vy = clamp(ball_angle * 0.3f, -0.5f, 0.5f);
motorControl_setVelocity(0.0f, vy, 0.0f);
```

**Salida**: `intensity ≥ 3` Y `|ball_angle| ≤ 45°` → INTERCEPT.

### Estado: INTERCEPT

**¿Qué hace?** La pelota viene hacia nuestra portería — el defensor avanza
para interponerse.

**Comportamiento**:
- Avanzar a `FWD_SPEED * 0.8` hacia la pelota.
- Si la pelota está al frente (±15°) y kicker disponible: `kick()` para despejar.

**`[FIX v2-D]` — Límite de avance del defensor**:
El defensor no debe cruzar la línea central (su sensor FRONT la detectará
como línea blanca). Cuando eso ocurra, el guard genérico `lineGuard()`
activa el escape y fuerza REPOSITION. No se usa "tiempo de avance" porque
es frágil ante variaciones de velocidad.

**Salida**: `intensity == 0` O `|ball_angle| > 90°` → REPOSITION.

---

## 5. Guards de Seguridad (Máxima Prioridad)

Estas dos comprobaciones se ejecutan **cada ciclo del loop, antes de la
máquina de estados**. Si se activan, reemplazan completamente la acción
del estado actual durante su duración.

### `[FIX v2-A]` wallGuard — Contacto con pared

**Origen reglamentario**: §4.4.10.4 — "Un robot que toca una **pared** o
entra por completo al área de penalti se considera fuera de límites y es
removido por 1 minuto."

La v1 del algoritmo solo manejaba líneas blancas. El contacto con la pared
negra tiene la misma penalización pero es un evento diferente: los sensores
de color no detectan la pared.

**Detección sugerida**: dado que no hay sensor de distancia dedicado
(los ToF están prohibidos en categoría Ágil), la pared se detecta de forma
indirecta. Dos opciones:

1. **Acelerómetro (BMI160)**: un impacto brusco produce un spike en `acc_x`
   o `acc_y`. Umbral sugerido: `|acc_x| > WALL_IMPACT_THRESHOLD`.
2. **Stall de motores**: si `motorControl_setVelocity(FWD_SPEED,...)` está
   activo pero el gyro no reporta cambio de posición angular y la intensidad
   IR tampoco aumenta, es probable que el robot esté contra una pared.

La opción 1 es más simple de implementar. Requiere calibración.

```cpp
// En el main loop, antes de lineGuard():
void wallGuard() {
    GyroData g = sensors.getGyroData();
    bool wallHit = (abs(g.acc_x) > WALL_IMPACT_THRESHOLD ||
                    abs(g.acc_y) > WALL_IMPACT_THRESHOLD);
    if (wallHit) {
        // Escapar en dirección opuesta al impacto
        float escape_vx = (g.acc_x > 0) ? -0.8f : 0.8f;
        float escape_vy = (g.acc_y > 0) ? -0.8f : 0.8f;
        motorControl_setVelocity(escape_vx, escape_vy, 0.0f);
        wallEscapeUntil = millis() + WALL_ESCAPE_MS;
    }
}
```

**Nota de implementación**: `WALL_ESCAPE_MS` debe ser mayor que
`LINE_ESCAPE_MS` (sugerido: 500 ms) porque la pared requiere alejarse más.

### lineGuard — Línea Blanca

```cpp
void lineGuard() {
    ColorSensorData cs = sensors.getColorData();

    bool onLine[4] = {
        cs.onWhiteLine[FRONT],
        cs.onWhiteLine[RIGHT],
        cs.onWhiteLine[BACK],
        cs.onWhiteLine[LEFT]
    };

    if (any(onLine)) {
        float escape_vx = onLine[FRONT] ? -0.8f : (onLine[BACK]  ?  0.8f : 0.0f);
        float escape_vy = onLine[RIGHT] ? -0.8f : (onLine[LEFT]  ?  0.8f : 0.0f);
        motorControl_setVelocity(escape_vx, escape_vy, 0.0f);
        lineEscapeUntil = millis() + LINE_ESCAPE_MS;
    }
}
```

Si `lineEscapeUntil > millis()`, saltar la máquina de estados y ejecutar
solo el escape. Después de `LINE_ESCAPE_MS`, retomar el estado anterior.

**Excepción controlada — DRIBBLE → SHOOT**: Si el estado es DRIBBLE y el
sensor FRONT detecta la línea blanca del área rival (con `shootEnabled ==
true`), esta transición se consume **antes** de llegar a `lineGuard()`.
`lineGuard()` solo actúa si la transición a SHOOT no fue válida (por
ejemplo, `shootEnabled == false` → el robot se aleja en lugar de disparar).

**`[FIX v2-C]` — Precisión de la regla de penalti**: El reglamento §4.4.10.1
penaliza estar **completamente** dentro del área. Estar parcialmente dentro
(el robot toca la línea) no es ilegal. Sin embargo, el `lineGuard()` actúa
al primer contacto con la línea — esto es conservador y correcto: si
escapamos inmediatamente al tocar la línea, nunca llegaremos a estar
completamente dentro.

---

## 6. Coordinación Entre Robots (ESP-NOW)

### ¿Qué se transmite?

Cada robot envía su `RobotMsg` a ≥ 10 Hz:

```cpp
struct RobotMsg {
    RobotRole  role;             // ATTACKER / DEFENDER
    RobotState state;            // estado actual
    int16_t    ball_angle_deg;   // ángulo a la pelota
    uint8_t    ball_confidence;  // intensidad IR (0–7)
    int16_t    heading_deg;      // orientación actual
    uint8_t    crc;              // checksum básico
};
```

### ¿Quién usa qué?

| Robot    | Dato del peer                     | Para qué                                              |
|----------|-----------------------------------|-------------------------------------------------------|
| Defensor | `peer.state == DRIBBLE/SHOOT`     | Quedarse quieto mientras el atacante tiene la pelota  |
| Defensor | `peer.ball_angle_deg`             | Verificar si el atacante ya ve la pelota              |
| Atacante | `peer.state == DEFEND/INTERCEPT`  | (futuro) Evitar ir a la portería propia si el defensor está ocupado |

### Pérdida de comunicación

Si no se recibe mensaje del peer por más de `COMMS_TIMEOUT_MS = 500 ms`,
cada robot opera **completamente independiente**. No se paraliza.

### ⚠️ Estado actual del código

`comms_send()` en `src/comms/comms.cpp` envía datos hardcodeados
(`RobotRole::ATTACKER`, `RobotState::SEARCH`, todo a cero) y usa una MAC
placeholder. **La coordinación descrita arriba no funciona hasta corregir
esto.** La corrección consiste en:

```cpp
// comms.cpp — comms_send() corregido
void comms_send() {
    RobotMsg msg;
    msg.role             = Core::role;
    msg.state            = Core::state;
    msg.ball_angle_deg   = Core::ball_angle;
    msg.ball_confidence  = Core::ball_confidence;
    msg.heading_deg      = (int16_t)Core::gyroData.yaw_deg;
    msg.crc              = 0; // TODO: calcular CRC real
    esp_now_send(peerMac, (uint8_t*)&msg, sizeof(msg));
}
```

La MAC del peer debe configurarse en `config.h` usando la utilidad
`utils/getMacAddress.cpp` y **no** dejarse como placeholder.

---

## 7. Secuencia de Arranque (Setup)

```
 1. Serial.begin(115200)
 2. Core::init()            → estado = IDLE, killed = false
 3. motorControl_init()     → todos los motores a 0
 4. kicker_init()           → pin LOW
 5. sensors.init()          → IR, color, gyro
 6. comms_init()            → ESP-NOW con MAC real del peer
 7. WiFiMgr::init()         → AP para telemetría
 8. LittleFS.begin()        → web dashboard
 9. WebSrv::init()          → servidor HTTP
10. delay(500)              → tiempo para que sensores se estabilicen
11. → IDLE (esperando kill switch HIGH)
```

El robot permanece en IDLE con motores detenidos hasta que el kill switch
sea liberado. No transiciona a SEARCH/REPOSITION hasta que el pin sea HIGH.

### Protocolo operativo el día del partido (§4.3.4)

> Un robot que se mueve antes del silbato es retirado y declarado dañado.

```
1. Encender el robot con el kill switch en LOW (bloqueado).
2. Colocar el robot en la posición reglamentaria:
   - Saque inicial: en la mitad propia, orientado hacia la portería rival.
   - Sin saque inicial: en el extremo defensivo, a ≥ 30 cm del balón
     (fuera del círculo central — §4.3.2).
3. Esperar el silbato del árbitro.
4. Liberar el kill switch (HIGH) → el robot inicia SEARCH / REPOSITION.
```

**`[FIX v2-E]` — Posición del robot sin saque inicial**: La v1 solo describía
la posición del atacante con saque inicial. §4.3.2 especifica que el equipo
sin saque inicial debe colocar sus robots en el **extremo defensivo** a
≥ 30 cm del balón. Ambos robots del equipo deben cumplir esto.

**Nunca colocar el robot en la cancha con el kill switch ya liberado.**

---

## 8. Kill Switch

```cpp
// En cada ciclo del loop — PRIMERO que todo
if (digitalRead(KILL_PIN) == LOW) {
    Core::killed = true;
    motorControl_stopAll();
    return; // no ejecutar nada más este ciclo
}

// Si estaba killed y ahora el pin está HIGH: reiniciar limpio
if (Core::killed && digitalRead(KILL_PIN) == HIGH) {
    ESP.restart();
}
```

El kill switch debe ser físicamente accesible. §3.3.3: función de parada
de emergencia obligatoria.

---

## 9. Parámetros Ajustables

Todos en `config.h`. Ningún magic number en `strategy.cpp`.

| Parámetro                | Valor sugerido | Descripción                                              |
|--------------------------|----------------|----------------------------------------------------------|
| `FWD_SPEED`              | 0.7            | Velocidad de avance (0–1)                                |
| `ROT_SPEED`              | 0.5            | Velocidad de giro en SEARCH/APPROACH                     |
| `BALL_KP`                | 1.0            | Proporcional del PID de ángulo de pelota                 |
| `HEADING_KP`             | 2.5            | Proporcional del PID de heading (gyro)                   |
| `BALL_ANGLE_THRESHOLD`   | 15°            | Ángulo máximo para considerar pelota "centrada"          |
| `APPROACH_MIN_INTENSITY` | 2              | Sensores IR mínimos para entrar a DRIBBLE                |
| `MIN_FIELD_TRAVEL_MS`    | 2000           | `[FIX v2-B]` Tiempo mínimo en DRIBBLE para habilitar SHOOT |
| `LINE_ESCAPE_MS`         | 300            | Duración del escape de línea en ms                       |
| `WALL_ESCAPE_MS`         | 500            | `[FIX v2-A]` Duración del escape de pared en ms          |
| `WALL_IMPACT_THRESHOLD`  | 8000           | `[FIX v2-A]` Umbral acelerómetro para detectar pared (raw BMI160) |
| `COMMS_TIMEOUT_MS`       | 500            | Tiempo sin mensaje del peer → modo independiente         |
| `DEFENDER_BASE_CM`       | 40             | Distancia posición base del defensor a la portería propia|
| `KICK_COOLDOWN_MS`       | 2000           | Cooldown entre disparos del kicker (§4.4.4)              |

### Dimensiones de la cancha (referencia rápida — Figura 1, §7)

| Elemento                          | Medida           | Fuente      |
|-----------------------------------|------------------|-------------|
| Campo total                       | 243 × 182 cm     | §7.1        |
| Altura de paredes                 | ≥ 22 cm          | §7.1.4      |
| Apertura de portería              | 60 cm ancho      | §7.4.1      |
| Altura de portería                | 10 cm            | §7.4.1      |
| Profundidad de portería           | 10 cm            | §7.4.1      |
| Línea de gol (blanca en el suelo) | 40 cm ancho      | §7.3.5      |
| Área de penalti                   | 30 cm prof × 60 cm ancho | §7.3.4 |
| Círculo central (negro)           | 60 cm diámetro   | §7.3.2      |
| Línea central (blanca)            | Divide campo en 121.5 cm c/lado | §7.3.1 |
| Puntos neutrales                  | 4 puntos, 45 cm de esquinas, sobre línea central | §7.3.3 |
| Grosor de líneas                  | 2 cm (±0.5 cm)   | §7.2.3      |
| Tolerancia dimensional            | ±5%              | §7.5.2      |

---

## 10. Orden de Implementación Recomendado

Implementar y validar en este orden. No avanzar al siguiente paso sin que
el anterior funcione en la cancha real.

```
Paso 1  ── Kill switch físico funcional + protocolo de arranque documentado
Paso 2  ── sensors.update() → logs seriales correctos (IR, color, gyro)
Paso 3  ── motorControl_setVelocity() validado (forward, strafe, rotate)
Paso 4  ── lineGuard(): escape de línea blanca (validar con cada sensor)
Paso 5  ── [FIX v2-A] wallGuard(): detección de pared con acelerómetro
Paso 6  ── SEARCH + APPROACH del atacante (solo IR, sin gyro)
Paso 7  ── DRIBBLE + corrección de heading con gyro
Paso 8  ── SHOOT: [FIX v2-B] con MIN_FIELD_TRAVEL_MS + validación en campo
Paso 9  ── comms.cpp: corregir MAC + enviar Core::state real
Paso 10 ── Defensor: REPOSITION + DEFEND (lateral tracking)
Paso 11 ── Defensor: INTERCEPT con límite de línea central
Paso 12 ── Prueba de partido completo: 3 minutos sin intervención
```

---

## 11. Preguntas Frecuentes del Equipo

**¿Por qué el defensor no persigue la pelota?**
Porque si ambos robots persiguen la pelota, nuestra portería queda
desprotegida. Un robot atacante + un defensor estático es más efectivo en
la práctica que dos atacantes caóticos.

**¿Qué pasa si el atacante pierde la pelota justo antes de disparar?**
Regresa a APPROACH si `intensity > 0`, o a SEARCH si `intensity == 0`.
El estado DRIBBLE tiene una tolerancia de 200 ms antes de hacer la
transición, para evitar estados rápidos por ruido del sensor.

**¿Cómo sabe el defensor que la pelota se acerca a nuestra portería?**
Por el ángulo IR: si el defensor ve la pelota en `|angle| ≤ 45°` con
`intensity ≥ 3`, la pelota está viniendo hacia nuestra portería.

**¿El atacante sabe dónde está la portería rival?**
No tenemos localización. El atacante asume que la portería rival está en
la dirección de su heading inicial (calibrado al colocarlo en la cancha).
El gyro mantiene ese heading durante DRIBBLE. Funciona bien en una cancha
de 243 × 182 cm.

**¿Qué pasa si los dos robots aliados se acercan mutuamente?**
El defensor se queda quieto cuando el atacante está en DRIBBLE/SHOOT.
Esto reduce la probabilidad de colisión entre aliados.

**¿Por qué el atacante se detiene antes de disparar en lugar de disparar en movimiento?**
Porque si dispara avanzando, el robot puede entrar al área de penalti.
El área mide solo 30 cm de profundidad — a FWD_SPEED = 0.7 el robot la
cruza en ~200 ms, que es menos que `KICK_DURATION_MS = 500 ms`.
Detenerse garantiza el disparo desde afuera. `[FIX v2-C]`: técnicamente
estar "parcialmente" dentro es legal (§4.4.10.1), pero detenerse en la
línea es la posición más segura y no reduce la potencia del disparo.

**`[FIX v2-B]` ¿Por qué el robot disparó en la línea central?**
Porque `shootEnabled` no estaba implementado en v1. El sensor FRONT detecta
cualquier línea blanca, incluyendo la línea central. `MIN_FIELD_TRAVEL_MS`
resuelve esto asegurando que el disparo solo ocurre en la segunda mitad
del campo.

**¿Qué pasa si la pared nos empuja?**
§4.4.10.6: el árbitro puede omitir la penalización si el robot fue empujado
accidentalmente por un oponente. Sin embargo, `wallGuard()` intenta
escapar de la pared en cualquier caso — es mejor salir solo que esperar
una decisión del árbitro.

---

## 12. Resumen de Cambios Respecto a v1

| ID       | Descripción                                    | Regla          |
|----------|------------------------------------------------|----------------|
| FIX v2-A | Añadido `wallGuard()` — contacto con pared     | §4.4.10.4      |
| FIX v2-B | `MIN_FIELD_TRAVEL_MS` para evitar disparo en línea central | Lógica de campo |
| FIX v2-C | Penalti es "completamente dentro", no parcialmente | §4.4.10.1   |
| FIX v2-D | Defensor limitado por línea central, no área rival | §4.3.2 + campo |
| FIX v2-E | Posición de arranque sin saque inicial documentada | §4.3.2       |
| FIX v2-F | Warning explícito: `comms.cpp` aún envía datos dummy | handoff.md  |

---

*Última actualización: Junio 2026 · Copa FutBotMX 2026 — Equipo CIATEQ*
*Revisada contra `Reglas_Copa_FutBotMX_20260105` (18 páginas, Figura 1)*