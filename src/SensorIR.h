#pragma once
#include <Arduino.h>

enum class ModoSensorIR : uint8_t {
    ANALOGICO,
    DIGITAL
};

class SensorIR {
private:
    uint8_t pin;
    ModoSensorIR modo;
    uint8_t nivelOcupado;
    int umbralMv = 3000;
    int lecturaMv = 0;
    int lecturaDigital = HIGH;
    float emaMv = 0;
    bool inicializado = false;
    static constexpr float ALPHA = 0.2f;

public:
    // Por defecto conserva el modo analogico anterior para P1, P2 y garitas.
    explicit SensorIR(
        uint8_t pinSensor,
        ModoSensorIR modoSensor = ModoSensorIR::ANALOGICO,
        uint8_t nivelActivo = LOW
    ) : pin(pinSensor), modo(modoSensor), nivelOcupado(nivelActivo) {}

    void iniciar() {
        if (modo == ModoSensorIR::DIGITAL) {
            pinMode(pin, INPUT_PULLUP);
        } else {
            analogSetPinAttenuation(pin, ADC_11db);
        }
    }

    void setUmbral(int nuevoUmbral) { umbralMv = nuevoUmbral; }

    void actualizar() {
        if (modo == ModoSensorIR::DIGITAL) {
            lecturaDigital = digitalRead(pin);
            inicializado = true;
            return;
        }

        lecturaMv = analogReadMilliVolts(pin);
        if (!inicializado) {
            emaMv = lecturaMv;
            inicializado = true;
        } else {
            emaMv = ALPHA * lecturaMv + (1.0f - ALPHA) * emaMv;
        }
    }

    bool estaOcupado() const {
        if (!inicializado) return false;
        if (modo == ModoSensorIR::DIGITAL) return lecturaDigital == nivelOcupado;
        return emaMv > umbralMv;
    }

    bool esDigital() const { return modo == ModoSensorIR::DIGITAL; }
    int getNivelDigital() const { return lecturaDigital; }
    int getLectura() const { return lecturaMv; }
    float getEMA() const { return emaMv; }
    int getUmbral() const { return umbralMv; }
};
