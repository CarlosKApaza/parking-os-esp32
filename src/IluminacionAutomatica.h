#ifndef ILUMINACION_AUTOMATICA_H
#define ILUMINACION_AUTOMATICA_H

#include <Arduino.h>

class IluminacionAutomatica {
public:
    IluminacionAutomatica(
        uint8_t pinLDR,
        uint8_t pinFocos,
        float umbralNocheV
    );

    void iniciar();
    void actualizar();

    bool esNoche() const;
    bool iluminacionEncendida() const;
    float getTensionLDR() const;

private:
    uint8_t pinLDR;
    uint8_t pinFocos;
    float umbralNocheV;

    float tensionLDR;
    bool noche;
    bool focosEncendidos;
};

#endif