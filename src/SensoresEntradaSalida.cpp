#include "SensoresEntradaSalida.h"
#include "Pines.h"

void SensoresEntradaSalida::begin() {
    pinMode(PIN_SENSOR_ENTRADA, INPUT);
    pinMode(PIN_SENSOR_SALIDA, INPUT);

    pinMode(PIN_ACTIVAR_LEDS_IR, OUTPUT);
    pinMode(PIN_LED_ENTRADA, OUTPUT);
    pinMode(PIN_LED_SALIDA, OUTPUT);

    analogReadResolution(12);

    digitalWrite(PIN_LED_ENTRADA, LOW);
    digitalWrite(PIN_LED_SALIDA, LOW);
    digitalWrite(PIN_ACTIVAR_LEDS_IR, LOW);

    calibrar();
    actualizar();
}

void SensoresEntradaSalida::calibrar() {
    constexpr unsigned long tiempoPorFaseMs = 3000;

    calibrado = false;
    emaInicializada = false;
    objetoEntrada = false;
    objetoSalida = false;

    // Fase 1. Encender los emisores y medir despues de la espera.
    digitalWrite(PIN_ACTIVAR_LEDS_IR, HIGH);

    Serial.printf(
        "IR encendidos. Mantenga entrada y salida libres durante %lu ms.\n",
        tiempoPorFaseMs
    );

    delay(tiempoPorFaseMs);

    lecturaMinEntrada = analogRead(PIN_SENSOR_ENTRADA);
    lecturaMinSalida  = analogRead(PIN_SENSOR_SALIDA);

    Serial.printf(
        "IR encendidos | Entrada: %u | Salida: %u\n",
        unsigned(lecturaMinEntrada),
        unsigned(lecturaMinSalida)
    );

    // Fase 2. Apagar los emisores y medir despues de la espera.
    digitalWrite(PIN_ACTIVAR_LEDS_IR, LOW);

    Serial.printf(
        "IR apagados. Midiendo en %lu ms...\n",
        tiempoPorFaseMs
    );

    delay(tiempoPorFaseMs);

    lecturaMaxEntrada = analogRead(PIN_SENSOR_ENTRADA);
    lecturaMaxSalida  = analogRead(PIN_SENSOR_SALIDA);

    Serial.printf(
        "IR apagados   | Entrada: %u | Salida: %u\n",
        unsigned(lecturaMaxEntrada),
        unsigned(lecturaMaxSalida)
    );

    // Cada sensor usa exclusivamente sus propias dos lecturas.
    umbralEntrada = (lecturaMinEntrada + lecturaMaxEntrada) / 2;
    umbralSalida  = (lecturaMinSalida + lecturaMaxSalida) / 2;

    Serial.printf(
        "Cambio al apagar IR | Entrada: %+d | Salida: %+d\n",
        int(lecturaMaxEntrada) - int(lecturaMinEntrada),
        int(lecturaMaxSalida) - int(lecturaMinSalida)
    );

    // Dejar los emisores encendidos para la deteccion normal.
    digitalWrite(PIN_ACTIVAR_LEDS_IR, HIGH);
    delay(5);

    calibrado = true;
}

void SensoresEntradaSalida::actualizar() {
    if (!calibrado) {
        return;
    }

    lecturaActualEntrada = analogRead(PIN_SENSOR_ENTRADA);
    lecturaActualSalida  = analogRead(PIN_SENSOR_SALIDA);

    // Inicializar con la primera muestra real, no con cero.
    if (!emaInicializada) {
        emaEntrada = lecturaActualEntrada;
        emaSalida = lecturaActualSalida;
        emaInicializada = true;
    } else {
        emaEntrada += ALPHA_EMA * (float(lecturaActualEntrada) - emaEntrada);
        emaSalida  += ALPHA_EMA * (float(lecturaActualSalida) - emaSalida);
    }

    // Tanto los LEDs como las garitas consumen el mismo estado filtrado.
    objetoEntrada = emaEntrada > umbralEntrada;
    objetoSalida  = emaSalida > umbralSalida;

    digitalWrite(PIN_LED_ENTRADA, objetoEntrada ? HIGH : LOW);
    // El LED de salida lo controla main.cpp durante la apertura autorizada.
}

bool SensoresEntradaSalida::getLecturaSensorEntrada() const {
    return objetoEntrada;
}

bool SensoresEntradaSalida::getLecturaSensorSalida() const {
    return objetoSalida;
}

bool SensoresEntradaSalida::getSensorEntrada() const {
    return objetoEntrada;
}

bool SensoresEntradaSalida::getSensorSalida() const {
    return objetoSalida;
}

uint16_t SensoresEntradaSalida::getValorAnalogicoEntrada() const {
    return lecturaActualEntrada;
}

uint16_t SensoresEntradaSalida::getValorAnalogicoSalida() const {
    return lecturaActualSalida;
}

float SensoresEntradaSalida::getEMAEntrada() const {
    return emaEntrada;
}

float SensoresEntradaSalida::getEMASalida() const {
    return emaSalida;
}

uint16_t SensoresEntradaSalida::getUmbralEntrada() const {
    return umbralEntrada;
}

uint16_t SensoresEntradaSalida::getUmbralSalida() const {
    return umbralSalida;
}

uint16_t SensoresEntradaSalida::getMinimoEntrada() const {
    return lecturaMinEntrada;
}

uint16_t SensoresEntradaSalida::getMaximoEntrada() const {
    return lecturaMaxEntrada;
}

uint16_t SensoresEntradaSalida::getMinimoSalida() const {
    return lecturaMinSalida;
}

uint16_t SensoresEntradaSalida::getMaximoSalida() const {
    return lecturaMaxSalida;
}

bool SensoresEntradaSalida::getCalibrado() const {
    return calibrado;
}