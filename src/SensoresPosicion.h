#ifndef SENSORES_POSICION_H
#define SENSORES_POSICION_H

#include <Arduino.h>
#include "Pines.h"
#include "IndicadoresPosicion.h"

class SensoresPosicion {
public:
    bool begin();
    void calibrar();
    void actualizar();

    // Los getters reciben la plaza fisica: 1 para P1, ..., 7 para P7.
    uint16_t getLectura(uint8_t plaza) const;
    float getLecturaFiltrada(uint8_t plaza) const;

    uint16_t getUmbral(uint8_t plaza) const;
    uint16_t getReferenciaConIR(uint8_t plaza) const;
    uint16_t getReferenciaSinIR(uint8_t plaza) const;

    bool estaCalibrada(uint8_t plaza) const;
    bool estaOcupada(uint8_t plaza) const;

private:
    struct EstadoPlaza {
        uint16_t conIR = 0;
        uint16_t sinIR = 0;
        uint16_t umbral = 0;

        uint16_t lectura = 0;
        float lecturaFiltrada = 0.0f;

        bool filtroInicializado = false;
        bool calibrada = false;
        bool ocupada = false;
    };

    IndicadoresPosicion indicadores{
        PIN_74HC595_DS,
        PIN_74HC595_ST,
        PIN_74HC595_SH
    };

    EstadoPlaza plazas[CANTIDAD_INDICADORES]{};
    bool inicializado = false;

    uint16_t leerCanal(uint8_t canal) const;
    uint16_t medirReferencia(uint8_t canal) const;
    bool plazaValida(uint8_t plaza) const;
};

#endif