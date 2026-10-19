#include "Servomotor.h"

Servomotor::Servomotor(uint8_t pin, int anguloArriba, int anguloAbajo)
    : pin(pin),
      anguloArriba(anguloArriba),
      anguloAbajo(anguloAbajo),
      anguloActual(anguloAbajo) {
}

void Servomotor::iniciar() {
    servo.setPeriodHertz(50);
    servo.attach(pin);
    bajar();
}

void Servomotor::subir() {
    servo.write(anguloArriba);
    anguloActual = anguloArriba;
}

void Servomotor::bajar() {
    servo.write(anguloAbajo);
    anguloActual = anguloAbajo;
}

int Servomotor::getAnguloArriba() const {
    return anguloArriba;
}

int Servomotor::getAnguloAbajo() const {
    return anguloAbajo;
}

int Servomotor::getAnguloActual() const {
    return anguloActual;
}
