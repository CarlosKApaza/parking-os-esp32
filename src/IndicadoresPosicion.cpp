#include "IndicadoresPosicion.h"
#include "Pines.h"

IndicadoresPosicion::IndicadoresPosicion(
    uint8_t ds,
    uint8_t st,
    uint8_t sh
)
    : pinDS(ds), pinST(st), pinSH(sh), registro(0) {
}

void IndicadoresPosicion::begin() {
    pinMode(pinDS, OUTPUT);
    pinMode(pinST, OUTPUT);
    pinMode(pinSH, OUTPUT);

    digitalWrite(pinDS, LOW);
    digitalWrite(pinST, LOW);
    digitalWrite(pinSH, LOW);

    apagarTodos();
}

void IndicadoresPosicion::actualizarRegistro() {
    digitalWrite(pinST, LOW);
    shiftOut(pinDS, pinSH, MSBFIRST, registro);
    digitalWrite(pinST, HIGH);
}

void IndicadoresPosicion::apagarTodos() {
    registro = 0;
    actualizarRegistro();
}

void IndicadoresPosicion::setPos(uint8_t posicion, bool estado) {
    if (posicion >= CANTIDAD_INDICADORES) {
        return;
    }

    // El indice 0 corresponde al bit 1 del registro (QB).
    const uint8_t mascara = 1U << (posicion + 1);

    if (estado) {
        registro |= mascara;
    } else {
        registro &= ~mascara;
    }

    actualizarRegistro();
}

void IndicadoresPosicion::encenderSolo(uint8_t plaza) {
    if (plaza < 1 || plaza > CANTIDAD_INDICADORES) {
        return;
    }

    // El numero fisico de plaza coincide con el numero de bit:
    // P1 -> bit 1 (QB), ..., P7 -> bit 7 (QH).
    registro = 1U << plaza;
    actualizarRegistro();
}