# ALGORITHM.md — FutBotMX 2026 · Equipo CIATEQ

> **Audiencia**: Todo el equipo (Miguel, Enrique, Norma).
> Este documento describe el comportamiento completo de los dos robots — qué
> hace cada uno, cuándo lo hace, y cómo se coordinan. Es la fuente de verdad
> para tomar decisiones de implementación.

---

## 1. Visión General

Tenemos **dos robots** en la cancha al mismo tiempo. Uno actúa como **atacante**
y el otro como **defensor**. Esta división de roles es la estrategia central
del equipo: en lugar de que ambos robots persigan la pelota (lo cual causa
colisiones entre aliados y expone nuestra portería), cada robot tiene una
responsabilidad clara.

La coordinación entre robots ocurre vía **ESP-NOW** a ≥ 10 Hz. Ningún robot
toma decisiones perfectamente aislado — cada uno sabe el estado del otro.

```
┌──────────────────────────────────────────────────────┐
│                   MAIN LOOP  (≥100 Hz)               │
│                                                      │
│  1. sensors.update()      ← IR, color, gyro          │
│  2. Core::update()        ← push data to global state│
│  3. comms.update()        ← receive peer state       │
│  4. lineGuard()           ← check white line FIRST   │
│  5. role_strategy()       ← attacker OR defender FSM │
│  6. motorControl_apply()  ← execute movement command │
│  7. comms.send()          ← broadcast our state      │
└──────────────────────────────────────────────────────┘
```

**Regla de oro**: la detección de línea blanca interrumpe cualquier otra
acción. Un robot fuera de campo es penalizado (Regla 4.4.10.4).

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

Cada robot sabe quién es desde el momento en que enciende. El otro robot conoce nuestro rol porque lo transmitimos en cada mensaje ESP-NOW.

### ¿Pueden cambiar de rol?

En la implementación inicial: no. Como mejora futura se puede agregar intercambio dinámico de rol si el defensor detecta la pelota notablemente más cerca que el atacante, pero se deja para después de consolidar el comportamiento base de forma perfecta.

---

## 3. Máquina de Estados — Atacante

El atacante tiene 5 estados. Solo puede estar en uno a la vez. Su ejecución es estrictamente asíncrona y no bloqueante.

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
         │               │ Y intensidad ≥ 2              │
         │               ▼                               │
         │    ┌──────────────────────┐                   │
         │    │       DRIBBLE        │                   │
         │    └──────────┬───────────┘                   │
         │               │ sensor FRONT detecta          │
         │               │ línea blanca del área de      │
         │               │ penalti rival → DETENER +     │
         │               │ disparar desde afuera         │
         │               ▼                               │
         │    ┌──────────────────────┐                   │
         │    │        SHOOT         │                   │
         │    └──────────┬───────────┘                   │
         │               │ temporizador de disparo fin   │
         │               └───────────────────────────────┘
         │
         │  (desde cualquier estado)
         └──── línea blanca detectada ──► AVOID_LINE (ver §5)
```

### Estado: SEARCH

**¿Qué hace?** El robot gira sobre sí mismo buscando la pelota.

**Entrada**: ningún sensor IR activo.

**Comportamiento**:
- Rotar en la dirección actual (`search_dir_cw`) a velocidad angular constante (`ROT_SPEED = 0.5`).
- Aleatorización de Bucle: Para evitar que el robot quede atrapado en una oscilación infinita (girando de izquierda a derecha repetidamente en el mismo punto muerto sin encontrar nada), el intervalo de cambio de sentido añade un componente aleatorio (jitter).

```
if (millis() - last_search_turn_ms > search_interval_ms) {
    search_dir_cw = !search_dir_cw;
    last_search_turn_ms = millis();
    search_interval_ms = random(2500, 4500); // Rompe el ciclo simétrico de oscilación
}
```

**Salida**: cualquier sensor IR se activa → APPROACH.

**Nota de reglas**: El árbitro puede declarar daño por falta de combatividad si el robot no busca la pelota activamente hacia el punto neutral más cercano (Regla 4.4.7). SEARCH debe ser altamente dinámico.

### Estado: APPROACH

**¿Qué hace?** El robot se mueve hacia la pelota alineándose con ella.

**Entrada**: IR detecta pelota (al menos 1 sensor activo).

**Comportamiento**:
- Calcular `ball_angle` desde `IRData.angle_deg`.
- Si `|ball_angle| > 15°`: girar hacia la pelota (omega proporcional al ángulo usando `BALL_KP`).
- Si `|ball_angle| ≤ 15°`: avanzar en línea recta hacia la pelota.
- Velocidad de avance: `FWD_SPEED = 0.7` (puede reducirse conforme se acerca).

**Control**:
```
omega = clamp(ball_angle * BALL_KP, -ROT_SPEED, ROT_SPEED)
vx    = (|ball_angle| < 30°) ? FWD_SPEED : 0.3
motorControl_setVelocity(vx, 0, omega)
```

**Salida**:
- `ball_angle ∈ [-15°, +15°]` Y `intensity ≥ 2` → DRIBBLE.
- `intensity == 0` → SEARCH (pelota perdida).

### Estado: DRIBBLE

**¿Qué hace?** El robot tiene la pelota controlada al frente y avanza hacia la portería contraria, corrigiendo su orientación (heading) con el giroscopio.

**Entrada**: pelota centrada frente al robot.

**Comportamiento**:
- Avanzar a `FWD_SPEED` mientras mantiene `ball_angle ≈ 0°`.
- Usar el giroscopio para mantener la orientación recta hacia el arco rival mediante un bucle PID (`HEADING_KP`).
- Si la pelota se descentra levemente, se tolera un margen de ruido por un máximo de 200 ms antes de degradar el estado.

**Heading target**: La orientación inicial calibrada al arranque (el atacante inicia apuntando de forma recta a la portería rival).

**Salida**:
- [FIX #3/5] Sensor FRONT detecta línea blanca del área de penalti rival → **STOP + SHOOT desde afuera**.
  El robot detiene el avance en cuanto el sensor frontal toca la línea. El disparo ocurre con el robot
  parado justo fuera del área. Esto evita entrar al área de penalti (Regla 4.4.10.1).
- `intensity == 0` → SEARCH.
- `|ball_angle| > 30°` por más de 200ms → APPROACH.

### Estado: SHOOT

**¿Qué hace?** Detiene el avance, dispara el solenoide del kicker de forma asíncrona y regresa de inmediato al flujo de control.

**Entrada**: sensor frontal de color detectó línea blanca del área de penalti rival. El robot está parado justo afuera del área.

**Comportamiento**:
- `motorControl_stop()` — el robot no debe avanzar mientras dispara (evita entrar al área).
- Prohibido usar `delay()`: el uso de retrasos síncronos deja al robot ciego (sin leer sensores de líneas blancas), lo que causaría una entrada inmediata al área penalizada.
- La activación física y desactivación del solenoide se maneja mediante variables de tiempo de software (`millis()`).

```
motorControl_stop()  // detener antes de disparar
kick()               // activa el solenoide (non-blocking)
// kicker_update() en cada ciclo gestiona la desactivación
→ SEARCH             // inmediatamente buscar de nuevo
```

**Nota de reglas**: La prueba de potencia de tiro (Regla 4.4.4) exige que el balón cruce al lado opuesto y no rebote de vuelta. Para evitar disparos accidentales consecutivos, se impone un `KICK_COOLDOWN_MS = 2000` controlado por software.

---

## 4. Máquina de Estados — Defensor

El defensor tiene **3 estados.** Su misión principal es resguardar nuestra portería, evitando a toda costa cruzarse en la trayectoria del atacante aliado.

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
         │    │     INTERCEPT        │               │
         │    └──────────┬───────────┘               │
         │               │ pelota alejada            │
         │               └───────────────────────────┘
         │
         └──── línea blanca ──► AVOID_LINE
```

### Posición Base del Defensor

El defensor se posiciona **a ~40 cm frente a nuestra portería**, en el centro.
Esta posición es fija y calculada relativa al heading inicial.

```
DEFENDER_BASE_CM = 40   // distancia a la portería propia
```

El defensor opera exclusivamente en su mitad del campo. Su límite de avance
es la **línea blanca del borde frontal de su propio área de penalti** — que
sus sensores de color sí detectan. El defensor nunca cruza hacia la mitad
del atacante.

### Estado: REPOSITION

**¿Qué hace?** Volver a la posición base frente a la portería propia.

**Cuándo ocurre**: al inicio, después de interceptar, después de AVOID_LINE.

**Comportamiento**: usar el heading del giroscopio para orientarse hacia la
portería propia y avanzar hacia la posición base.

**Salida**: `heading_error < 10°` Y `en_posición_base` → DEFEND.

### Estado: DEFEND

**¿Qué hace?** El defensor se mueve lateralmente (strafe) para seguir el
ángulo de la pelota, manteniéndose a ~40 cm de la portería.

**Comportamiento**:
```
// Si el atacante aliado tiene la pelota → quieto
if (peer.state == DRIBBLE || peer.state == SHOOT) → stay still

// Si no: seguir ángulo de la pelota lateralmente
vy = clamp(ball_angle * 0.3, -0.5, 0.5)
motorControl_setVelocity(0, vy, 0)
```

**Uso de datos del peer**: el defensor usa `comms_getLastMessage()` para saber
si el atacante está en DRIBBLE o SHOOT. En ese caso se queda quieto — el
atacante tiene el control.

**Salida**: `intensity ≥ 3` Y `ball_angle ∈ [-45°, 45°]` → INTERCEPT.

### Estado: INTERCEPT

**¿Qué hace?** La pelota viene hacia nuestra portería — el defensor avanza
para interponerse y despejarla.

**Comportamiento**:
- Avanzar a `FWD_SPEED * 0.8` hacia la pelota.
- Si tiene kicker y la pelota queda enfrente: `kick()` para despejar.
- [FIX #2] **Límite de avance: el sensor BACK del defensor** (mirando hacia su portería).
  El defensor retrocede si el sensor FRONT detecta la línea blanca del área de penalti
  del campo del atacante — esa es la única línea blanca que encontrará si avanza demasiado.
  No se usa "tiempo de avance" porque es frágil; se usa la línea blanca como tope físico
  garantizado por el reglamento (§7.3.4).

```
// Limite hardware del defensor en INTERCEPT:
if (cs.onWhiteLine[FRONT]) {
    motorControl_stop();   // llegó al límite — no cruzar
    currentState = REPOSITION;
}
```

**Salida**: `intensity == 0` O `ball_angle > 90°` → REPOSITION.

---

## 5. Detección de Línea Blanca (Todos los Robots)

Esta lógica tiene **prioridad sobre cualquier estado**. Se evalúa cada ciclo
del loop antes que la máquina de estados.

### Regla de las líneas (Reglamento §4.4.10.4)

Un robot que entra completamente al área de penalti o toca la pared es
retirado 1 minuto. Debemos evitarlo activamente.

### Mapa de líneas blancas en la cancha

Es crítico entender qué líneas puede encontrar cada robot para no confundirlas:

```
[PORTERÍA PROPIA] [área penalti propia] ... [línea central] ... [área penalti rival] [PORTERÍA RIVAL]
                   └── línea blanca ──┘                         └── línea blanca ──┘
                   ↑ el defensor NO debe                         ↑ el atacante dispara
                     cruzar esta línea                             justo antes de esta línea
```

**El círculo central es negro** (§7.3.2) — los sensores de color no lo
detectarán como línea blanca. No usarlo como referencia de posición.

### Lógica

```
ColorSensorData cs = sensors.getColorData();

bool onLine[4] = {
    cs.onWhiteLine[FRONT],
    cs.onWhiteLine[RIGHT],
    cs.onWhiteLine[BACK],
    cs.onWhiteLine[LEFT]
};

if (any(onLine)) {
    // Calcular dirección de escape: alejarse del sensor que detectó línea
    float escape_vx = onLine[FRONT] ? -0.8 : (onLine[BACK]  ? 0.8 : 0.0);
    float escape_vy = onLine[RIGHT] ? -0.8 : (onLine[LEFT]  ? 0.8 : 0.0);
    motorControl_setVelocity(escape_vx, escape_vy, 0);
    // Mantener escape por 300ms (no bloqueante, usar millis())
    lineEscapeUntil = millis() + 300;
}
```

**Importante**: si `lineEscapeUntil > millis()`, saltarse la máquina de
estados y solo ejecutar el escape. Después de 300ms, retomar el estado
anterior.

**Excepción para SHOOT**: si el estado es SHOOT y el sensor FRONT detecta
línea blanca del área de penalti rival, NO es un error — es la señal de
disparo. La transición DRIBBLE → SHOOT ya consume ese evento antes de que
llegue al guard genérico de escape.

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

| Robot    | Dato que consume del peer         | Para qué                          |
|----------|-----------------------------------|-----------------------------------|
| Defensor | `peer.state == DRIBBLE/SHOOT`     | Quedarse quieto mientras el atacante tiene la pelota |
| Defensor | `peer.ball_angle_deg`             | Verificar si el atacante ya ve la pelota antes de interceptar |
| Atacante | `peer.state == DEFEND/INTERCEPT`  | (futuro) Evitar ir a la portería propia si el defensor está ocupado |

### Manejo de pérdida de comunicación

Si no se recibe mensaje del peer por más de `COMMS_TIMEOUT_MS = 500ms`,
cada robot opera de manera **completamente independiente** como si fuera
el único en la cancha. No se paraliza ni se queda esperando.

---

## 7. Secuencia de Arranque (Setup)

```
1. Serial.begin(115200)
2. Core::init()            → estado = IDLE, killed = false
3. motorControl_init()     → todos los motores a 0
4. kicker_init()           → pin LOW
5. sensors.init()          → IR, color, gyro
6. comms_init()            → ESP-NOW con MAC del peer
7. WiFiMgr::init()         → AP para telemetría
8. LittleFS.begin()        → web dashboard
9. WebSrv::init()          → servidor HTTP
10. delay(500)             → tiempo para que sensores se estabilicen
11. → IDLE (esperando kill switch HIGH)
```

El robot permanece en IDLE con motores detenidos hasta que el kill switch
sea liberado. **No transiciona a SEARCH/REPOSITION hasta que el pin sea HIGH.**

### Protocolo operativo el día del partido (Regla 4.3.4)

> Un robot que se mueve antes del silbato es retirado y declarado dañado.

Procedimiento obligatorio antes de cada partido:

```
1. Encender el robot con el kill switch en LOW (bloqueado).
2. Colocar el robot en la posición reglamentaria dentro del campo.
   - Atacante: en su mitad, orientado hacia la portería rival.
   - Defensor: en su mitad defensiva, fuera del círculo central (≥30 cm del balón).
3. Esperar el silbato del árbitro.
4. Liberar el kill switch (HIGH) → el robot inicia SEARCH / REPOSITION.
```

**Nunca colocar el robot en la cancha con el kill switch ya liberado.**

---

## 8. Kill Switch

```cpp
// En cada ciclo del loop — PRIMERO que todo
if (digitalRead(KILL_PIN) == LOW) {
    Core::killed = true;
    motorControl_stopAll();
    // No ejecutar nada más este ciclo
    return;
}

// Si estaba killed y ahora el pin está HIGH: reiniciar
if (Core::killed && digitalRead(KILL_PIN) == HIGH) {
    ESP.restart();   // Reinicio limpio
}
```

El kill switch debe ser **físicamente accesible** y fácil de activar.
Reglamento §3.3.3: función de parada de emergencia obligatoria.

---

## 9. Parámetros Ajustables

Todos los parámetros de comportamiento deben estar en `config.h` para que
cualquier miembro del equipo pueda ajustarlos sin tocar la lógica.

| Parámetro               | Valor actual | Descripción                                           |
|-------------------------|--------------|-------------------------------------------------------|
| `FWD_SPEED`             | 0.7          | Velocidad de avance (0–1)                             |
| `ROT_SPEED`             | 0.5          | Velocidad de giro en SEARCH/APPROACH                  |
| `BALL_KP`               | 1.0          | Proporcional del PID de ángulo de pelota              |
| `HEADING_KP`            | 2.5          | Proporcional del PID de heading (gyro)                |
| `BALL_ANGLE_THRESHOLD`  | 15°          | Ángulo máximo para considerar pelota "centrada"       |
| `APPROACH_MIN_INTENSITY`| 2            | Sensores IR mínimos para entrar a DRIBBLE             |
| `LINE_ESCAPE_MS`        | 300          | Duración del escape de línea en ms                    |
| `COMMS_TIMEOUT_MS`      | 500          | Tiempo sin mensaje del peer → modo independiente      |
| `DEFENDER_BASE_CM`      | 40           | Distancia de la posición base del defensor a la portería |

### Dimensiones de la cancha (referencia rápida)

Extraídas del reglamento §7 para consulta rápida durante ajustes en campo:

| Elemento                  | Medida         |
|---------------------------|----------------|
| Campo total               | 243 × 182 cm   |
| Apertura de portería      | **60 cm** ancho, 10 cm alto |
| Línea de gol (blanca)     | 40 cm          |
| Área de penalti           | 30 cm prof. × 60 cm ancho |
| Círculo central (negro)   | 60 cm diámetro |
| Línea central (blanca)    | Divide campo en 121.5 cm c/lado |
| Punto neutral (esquinas)  | 45 cm de cada esquina, sobre línea central |

---

## 10. Orden de Implementación Recomendado

Implementar y validar en este orden. No avanzar al siguiente paso sin que el
anterior funcione en la cancha real.

```
Paso 1 ── Kill switch físico funcional + protocolo de arranque documentado
Paso 2 ── sensors.update() → logs seriales correctos (IR, color, gyro)
Paso 3 ── motorControl_setVelocity() validado (forward, strafe, rotate)
Paso 4 ── Detección de línea blanca + escape
Paso 5 ── SEARCH + APPROACH del atacante (solo IR, sin gyro)
Paso 6 ── DRIBBLE + corrección de heading con gyro
Paso 7 ── SHOOT: disparar desde afuera del área de penalti (validar con sensor de color)
Paso 8 ── ESP-NOW: envío y recepción de RobotMsg
Paso 9 ── Defensor: REPOSITION + DEFEND (lateral tracking)
Paso 10 ─ Defensor: INTERCEPT con límite de línea blanca
Paso 11 ─ Prueba de partido completo: 3 minutos sin intervención
```

---

## 11. Preguntas Frecuentes del Equipo

**¿Por qué el defensor no persigue la pelota?**
Porque si ambos robots persiguen la pelota, nuestra portería queda
desprotegida. Un robot atacante + un defensor estático es más efectivo en
la práctica que dos atacantes caóticos.

**¿Qué pasa si el atacante pierde la pelota justo antes de disparar?**
Regresa a APPROACH si `intensity > 0`, o a SEARCH si `intensity == 0`.
El estado DRIBBLE tiene una tolerancia de 200ms antes de hacer la
transición, para evitar estados rápidos por ruido del sensor.

**¿Cómo sabe el defensor que la pelota se acerca a nuestra portería?**
Por el ángulo IR: si el defensor ve la pelota en un ángulo `∈ [-45°, 45°]`
(frente a él, que está mirando al centro de la cancha) con intensidad ≥ 3,
la pelota está viniendo hacia nuestra portería.

**¿El atacante sabe dónde está la portería rival?**
No tenemos localización. El atacante asume que la portería rival está en la
dirección de su heading inicial (calibrado al colocarlo en la cancha). El
gyro mantiene ese heading durante el DRIBBLE. Es una aproximación que
funciona bien en una cancha pequeña de 243 × 182 cm.

**¿Qué pasa si los dos robots del equipo se acercan mutuamente?**
El defensor sabe cuándo el atacante está en DRIBBLE/SHOOT y se queda quieto.
Esto reduce la probabilidad de colisión entre aliados. En el futuro se puede
agregar detección de proximidad entre aliados via RSSI del ESP-NOW.

**¿Por qué el atacante se detiene antes de disparar en lugar de disparar en movimiento?**
Porque si dispara avanzando, el robot entra al área de penalti durante el
ciclo de KICK_DURATION_MS (500ms) sin leer sensores. El área de penalti mide
solo 30 cm de profundidad — a FWD_SPEED = 0.7 el robot la cruza en ~200ms.
Detenerse es la única forma de garantizar que el disparo ocurra desde afuera.

---

*Última actualización: Mayo 2026 · Copa FutBotMX 2026 — Equipo CIATEQ*
*Revisado contra Reglas_Copa_FutBotMX_20260105 — correcciones: §3.1 (Fix #1 portería 60cm), §4.2 (Fix #2 límite defensor), §4.3/3 (Fix #3/5 SHOOT desde afuera), §7 (Fix #4 protocolo silbato)*