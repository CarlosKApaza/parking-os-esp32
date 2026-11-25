#ifndef SENSORES_ENTRADA_SALIDA_H
#define SENSORES_ENTRADA_SALIDA_H

#include <Arduino.h>

class SensoresEntradaSalida {
public:
    SensoresEntradaSalida() = default;

    void begin();
    void calibrar();
    void actualizar();

    bool getLecturaSensorEntrada() const;
    bool getLecturaSensorSalida() const;

    bool getSensorEntrada() const;
    bool getSensorSalida() const;

    uint16_t getValorAnalogicoEntrada() const;
    uint16_t getValorAnalogicoSalida() const;

    float getEMAEntrada() const;
    float getEMASalida() const;

    uint16_t getUmbralEntrada() const;
    uint16_t getUmbralSalida() const;

    uint16_t getMinimoEntrada() const;
    uint16_t getMaximoEntrada() const;

    uint16_t getMinimoSalida() const;
    uint16_t getMaximoSalida() const;

    bool getCalibrado() const;

private:
    uint16_t lecturaMinEntrada = 0;
    uint16_t lecturaMaxEntrada = 0;

    uint16_t lecturaMinSalida = 0;
    uint16_t lecturaMaxSalida = 0;

    uint16_t lecturaActualEntrada = 0;
    uint16_t lecturaActualSalida = 0;

    // Un filtro independiente por sensor. Alfa bajo: mayor suavizado.
    static constexpr float ALPHA_EMA = 0.10f;
    float emaEntrada = 0.0f;
    float emaSalida = 0.0f;
    bool emaInicializada = false;

    uint16_t umbralEntrada = 0;
    uint16_t umbralSalida = 0;

    bool objetoEntrada = false;
    bool objetoSalida = false;
    bool calibrado = false;
};

#endif