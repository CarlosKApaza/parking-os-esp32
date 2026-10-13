
#pragma once
#include <Arduino.h>

class SensorIR {
private:
    uint8_t pin;
    int umbralMv = 3000;

    int lecturaMv = 0;
    float emaMv = 0;

    bool inicializado = false;

    static constexpr float ALPHA = 0.2f;

public:
    // Constructor
    SensorIR(uint8_t pinSensor) : pin(pinSensor) {}

    // Inicializar sensor
    void iniciar() {
        analogSetPinAttenuation(pin, ADC_11db);
    }

    // Setter: establecer umbral individual
    void setUmbral(int nuevoUmbral) {
        umbralMv = nuevoUmbral;
    }

    // Leer y filtrar señal analogica
    void actualizar() {
        lecturaMv = analogReadMilliVolts(pin);

        if (!inicializado) {
            emaMv = lecturaMv;
            inicializado = true;
        } else {
            emaMv = ALPHA * lecturaMv
                  + (1.0f - ALPHA) * emaMv;
        }
    }

    // Comprobar ocupacion
    bool estaOcupado() const {
        return inicializado && emaMv > umbralMv;
    }

    int getLectura() const {
        return lecturaMv;
    }

    float getEMA() const {
        return emaMv;
    }

    int getUmbral() const {
        return umbralMv;
    }
};
