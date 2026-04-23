#pragma once

#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ─── Constantes ──────────────────────────────────────────────────────────────
constexpr uint8_t MAX_IR_SENSORS = 7;

// ─── Datos de salida (solo lectura para consumidores) ────────────────────────
struct IRData {
    uint8_t values[MAX_IR_SENSORS]; // Estado filtrado por sensor (LOW = detectado)
    int16_t angle_deg;              // Angulo promedio hacia la pelota (-180..180)
    uint8_t intensity;              // Cantidad de sensores activos
    bool detected;                  // true si al menos un sensor ve la pelota
};

// ─── Clase IRSensorArray ─────────────────────────────────────────────────────
// Responsabilidad unica: lectura de hardware + filtro de ruido (monoestable)
// + calculo de angulo/intensidad.  NO contiene logica de juego.
class IRSensorArray {
public:
    IRSensorArray() = default;

    // Configura pines como INPUT_PULLUP y crea el mutex.
    // Llamar una sola vez en setup().
    void init();

    // Lee los pines, aplica el filtro monoestable retrigerable
    // y recalcula angulo + intensidad.
    // Llamar una vez por ciclo del loop principal.
    void update();

    // Devuelve una copia thread-safe de los datos filtrados.
    IRData getData() const;

    // Devuelve el valor filtrado de un sensor especifico (LOW = pelota).
    uint8_t getFilteredValue(uint8_t index) const;

    // Devuelve la lectura cruda inmediata de un pin (sin filtro, sin mutex).
    // Util solo para diagnostico / debug.
    static uint8_t readRawPin(uint8_t index);

private:
    // Calculo trigonometrico del angulo promedio de los sensores activos.
    static int16_t circularMean(const int16_t* angles, uint8_t count);
    static int16_t normalizeAngle(int16_t angle);

    mutable SemaphoreHandle_t _mutex = nullptr;
    IRData _data = {};
    uint32_t _lastDetectedMs[MAX_IR_SENSORS] = {0};
};