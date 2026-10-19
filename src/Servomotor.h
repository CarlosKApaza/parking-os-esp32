#ifndef SERVOMOTOR_H
#define SERVOMOTOR_H

#include <Arduino.h>
#include <ESP32Servo.h>

class Servomotor {
public:
    Servomotor(uint8_t pin, int anguloArriba, int anguloAbajo);

    void iniciar();
    void subir();
    void bajar();

    int getAnguloArriba() const;
    int getAnguloAbajo() const;
    int getAnguloActual() const;

private:
    uint8_t pin;
    int anguloArriba;
    int anguloAbajo;
    int anguloActual;
    Servo servo;
};

#endif
