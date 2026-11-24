#ifndef INDICADORES_POSICION_H
#define INDICADORES_POSICION_H

#include <Arduino.h>

class IndicadoresPosicion {
public:
    IndicadoresPosicion(uint8_t ds, uint8_t st, uint8_t sh);

    void begin();
    void apagarTodos();

    // Enciende o apaga un LED conservando el estado de los demas.
    // Indice 0 = P1 (QB), ..., indice 6 = P7 (QH).
    void setPos(uint8_t posicion, bool estado);

    // Para el barrido: deja encendida solo la plaza indicada.
    // Recibe el numero fisico de plaza: 1 a 7.
    void encenderSolo(uint8_t plaza);

    void setPos0(bool estado) { setPos(0, estado); }
    void setPos1(bool estado) { setPos(1, estado); }
    void setPos2(bool estado) { setPos(2, estado); }
    void setPos3(bool estado) { setPos(3, estado); }
    void setPos4(bool estado) { setPos(4, estado); }
    void setPos5(bool estado) { setPos(5, estado); }
    void setPos6(bool estado) { setPos(6, estado); }

    void setIndicadorPos0(bool estado) { setPos0(estado); }
    void setIndicadorPos1(bool estado) { setPos1(estado); }
    void setIndicadorPos2(bool estado) { setPos2(estado); }
    void setIndicadorPos3(bool estado) { setPos3(estado); }
    void setIndicadorPos4(bool estado) { setPos4(estado); }
    void setIndicadorPos5(bool estado) { setPos5(estado); }
    void setIndicadorPos6(bool estado) { setPos6(estado); }

private:
    uint8_t pinDS;
    uint8_t pinST;
    uint8_t pinSH;

    // Bit 0 = QA, sin utilizar. Bits 1 a 7 = QB a QH.
    uint8_t registro;

    void actualizarRegistro();
};

#endif