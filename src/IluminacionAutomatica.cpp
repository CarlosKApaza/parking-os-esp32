
#include "IluminacionAutomatica.h"

namespace {
    constexpr float ALPHA_EMA = 0.35f;
}

IluminacionAutomatica::IluminacionAutomatica(
    uint8_t pinLDR,
    uint8_t pinFocos,
    float umbralNocheV
)
    : pinLDR(pinLDR),
      pinFocos(pinFocos),
      umbralNocheV(umbralNocheV),
      tensionLDR(0.0f),
      noche(false),
      focosEncendidos(false)
{
}

void IluminacionAutomatica::iniciar()
{
    pinMode(pinLDR, INPUT);
    pinMode(pinFocos, OUTPUT);

    // Primera lectura para inicializar el filtro sin un arranque
    // artificial desde cero.
    tensionLDR = analogReadMilliVolts(pinLDR) / 1000.0f;

    noche = tensionLDR > umbralNocheV;
    focosEncendidos = noche;

    digitalWrite(pinFocos, focosEncendidos ? HIGH : LOW);
}

void IluminacionAutomatica::actualizar()
{
    // Lectura instantánea del ADC en voltios.
    float tensionActual = analogRead(pinLDR) * 3.3f / 4095.0f;
    //float tensionActual = analogReadMilliVolts(pinLDR) / 1000.0f;

    // Filtro EMA.
    tensionLDR = ALPHA_EMA * tensionActual
               + (1.0f - ALPHA_EMA) * tensionLDR;

    // Clasificación usando la tensión filtrada.
    noche = tensionLDR > umbralNocheV;

    if(noche){
        digitalWrite(pinFocos, HIGH);
    }
    else{
        digitalWrite(pinFocos, LOW);
    }
}

bool IluminacionAutomatica::esNoche() const
{
    return noche;
}

bool IluminacionAutomatica::iluminacionEncendida() const
{
    return focosEncendidos;
}

float IluminacionAutomatica::getTensionLDR() const
{
    return tensionLDR;
}
