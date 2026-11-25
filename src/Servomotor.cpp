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

void Servomotor::moverA(int angulo) {
    anguloActual = constrain(angulo, 0, 180);
    servo.write(anguloActual);
}

void Servomotor::subir() {
    moverA(anguloArriba);
}

void Servomotor::bajar() {
    moverA(anguloAbajo);
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