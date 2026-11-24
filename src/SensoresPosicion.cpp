#include "SensoresPosicion.h"

namespace {
    constexpr float ALFA_EMA = 0.40f;
    constexpr uint8_t MUESTRAS_CALIBRACION = 10;

    struct MuestrasFase {
        uint16_t valores[MUESTRAS_CALIBRACION]{};
        uint16_t minimo = 0;
        uint16_t maximo = 0;
        uint32_t suma = 0;

        float promedio() const {
            return float(suma) / MUESTRAS_CALIBRACION;
        }

        void guardar(uint8_t indice, uint16_t valor) {
            valores[indice] = valor;
            suma += valor;

            if (indice == 0 || valor < minimo) {
                minimo = valor;
            }

            if (indice == 0 || valor > maximo) {
                maximo = valor;
            }
        }
    };

    void imprimirFase(const char* nombre, const MuestrasFase& fase) {
        Serial.printf("  %s | Muestras: ", nombre);

        for (uint8_t i = 0; i < MUESTRAS_CALIBRACION; ++i) {
            if (i > 0) {
                Serial.print(", ");
            }
            Serial.print(fase.valores[i]);
        }

        Serial.println();
        Serial.printf(
            "  %s | Min: %u | Max: %u | Variacion: %u"
            " | Suma: %lu | Promedio: %.2f\n",
            nombre,
            unsigned(fase.minimo),
            unsigned(fase.maximo),
            unsigned(fase.maximo - fase.minimo),
            static_cast<unsigned long>(fase.suma),
            fase.promedio()
        );
    }
}

bool SensoresPosicion::begin() {
    indicadores.begin();

    if (PIN_ADC_MUX_Z < 0) {
        return false;
    }

    pinMode(PIN_SELECT0, OUTPUT);
    pinMode(PIN_SELECT1, OUTPUT);
    pinMode(PIN_SELECT2, OUTPUT);

    pinMode(PIN_ADC_MUX_Z, INPUT);
    pinMode(PIN_ACTIVAR_LEDS_IR, OUTPUT);

    analogReadResolution(12);

    inicializado = true;
    calibrar();
    actualizar();

    return true;
}

uint16_t SensoresPosicion::leerCanal(uint8_t canal) const {
    // Y0 -> P1, Y1 -> P2, ..., Y6 -> P7.
    digitalWrite(PIN_SELECT0, (canal >> 0) & 1U);
    digitalWrite(PIN_SELECT1, (canal >> 1) & 1U);
    digitalWrite(PIN_SELECT2, (canal >> 2) & 1U);

    // En tu Pines.h, ESTABILIZACION_MUX_US = 2.
    delayMicroseconds(ESTABILIZACION_MUX_US);

    return analogRead(PIN_ADC_MUX_Z);
}

void SensoresPosicion::calibrar() {
    if (!inicializado) {
        return;
    }

    indicadores.apagarTodos();

    // Con la configuracion actual: 500 ms / 10 muestras = 50 ms.
    constexpr unsigned long intervaloMuestraMs =
        TIEMPO_FASE_CALIBRACION_PLAZA_MS / MUESTRAS_CALIBRACION;

    static_assert(intervaloMuestraMs > 0,
                  "La fase debe durar al menos 1 ms por muestra.");

    Serial.println();
    Serial.println("========== INICIO DE CALIBRACION ==========");
    Serial.println("Mantenga las siete plazas libres hasta finalizar.");
    Serial.printf(
        "ADC: GPIO%d | Control de emisores IR: GPIO%u\n",
        PIN_ADC_MUX_Z,
        unsigned(PIN_ACTIVAR_LEDS_IR)
    );
    Serial.printf(
        "Muestras por fase: %u | Intervalo: %lu ms | Espera MUX: %u us\n",
        unsigned(MUESTRAS_CALIBRACION),
        intervaloMuestraMs,
        unsigned(ESTABILIZACION_MUX_US)
    );
    Serial.println("Orden por plaza: IR apagado, luego IR encendido.");

    const unsigned long inicioCalibracion = millis();

    // Se guardan los diagnosticos para imprimir un resumen de las siete plazas.
    float diferencias[CANTIDAD_INDICADORES]{};
    int separaciones[CANTIDAD_INDICADORES]{};

    for (uint8_t canal = 0; canal < CANTIDAD_INDICADORES; ++canal) {
        EstadoPlaza& plaza = plazas[canal];
        plaza = EstadoPlaza{};

        MuestrasFase sinIR;
        MuestrasFase conIR;

        Serial.println();
        Serial.printf(
            "--- P%u | Canal Y%u | LED Q%c | S2 S1 S0 = %u %u %u ---\n",
            unsigned(canal + 1),
            unsigned(canal),
            'B' + canal,
            unsigned((canal >> 2) & 1U),
            unsigned((canal >> 1) & 1U),
            unsigned(canal & 1U)
        );

        // Fase 1: medir con los emisores IR apagados.
        digitalWrite(PIN_ACTIVAR_LEDS_IR, LOW);

        for (uint8_t i = 0; i < MUESTRAS_CALIBRACION; ++i) {
            delay(intervaloMuestraMs);
            sinIR.guardar(i, leerCanal(canal));
        }

        // Fase 2: medir la misma plaza con los emisores IR encendidos.
        digitalWrite(PIN_ACTIVAR_LEDS_IR, HIGH);

        for (uint8_t i = 0; i < MUESTRAS_CALIBRACION; ++i) {
            delay(intervaloMuestraMs);
            conIR.guardar(i, leerCanal(canal));
        }

        const float promedioSinIR = sinIR.promedio();
        const float promedioConIR = conIR.promedio();
        const float diferencia = promedioSinIR - promedioConIR;
        const float puntoMedio = (promedioSinIR + promedioConIR) / 2.0f;

        // Los getters de la clase devuelven referencias en cuentas enteras.
        plaza.sinIR = static_cast<uint16_t>(promedioSinIR + 0.5f);
        plaza.conIR = static_cast<uint16_t>(promedioConIR + 0.5f);

        // Se calcula desde los promedios originales, antes de redondearlos.
        plaza.umbral = static_cast<uint16_t>(puntoMedio + 0.5f);

        // Se conserva la polaridad de tu montaje:
        // cuando el receptor pierde el haz, se espera que el ADC suba.
        plaza.calibrada = diferencia > 0.0f;

        diferencias[canal] = diferencia;

        // Positivo: hasta la menor lectura sin IR supera a la mayor con IR.
        separaciones[canal] = int(sinIR.minimo) - int(conIR.maximo);

        // Imprimir despues de adquirir ambas fases para que la salida serie
        // no introduzca pausas entre las muestras de una misma fase.
        imprimirFase("IR APAGADO  ", sinIR);
        imprimirFase("IR ENCENDIDO", conIR);

        Serial.printf(
            "  Diferencia de promedios (apagado - encendido): %+.2f\n",
            diferencia
        );
        Serial.printf(
            "  Separacion observada (min apagado - max encendido): %+d\n",
            separaciones[canal]
        );
        Serial.printf(
            "  Punto medio: %.2f | Umbral ADC utilizado: %u\n",
            puntoMedio,
            unsigned(plaza.umbral)
        );

        if (!plaza.calibrada) {
            Serial.println(
                "  REVISAR CALIBRACION: el promedio apagado es igual "
                "o inferior al promedio encendido."
            );
        } else if (separaciones[canal] <= 0) {
            Serial.println(
                "  CALIBRADA CON ADVERTENCIA: los promedios tienen el "
                "orden esperado, pero los rangos se tocan o se solapan."
            );
        } else {
            Serial.println(
                "  CALIBRADA: las dos series de muestras quedaron separadas."
            );
        }
    }

    // Dejar los emisores encendidos para detectar ocupacion.
    digitalWrite(PIN_ACTIVAR_LEDS_IR, HIGH);
    delay(5);

    Serial.println();
    Serial.println("========== RESUMEN FINAL ==========");
    Serial.println("Plaza | Diferencia medias | Separacion | Umbral | Resultado");

    for (uint8_t canal = 0; canal < CANTIDAD_INDICADORES; ++canal) {
        const EstadoPlaza& plaza = plazas[canal];
        const char* resultado = "REVISAR CALIBRACION";

        if (plaza.calibrada) {
            resultado = separaciones[canal] > 0
                ? "CALIBRADA"
                : "CALIBRADA - RANGOS SOLAPADOS";
        }

        Serial.printf(
            "P%u    | %+.2f | %+d | %u | %s\n",
            unsigned(canal + 1),
            diferencias[canal],
            separaciones[canal],
            unsigned(plaza.umbral),
            resultado
        );
    }

    Serial.printf(
        "Tiempo transcurrido: %lu ms | Emisores IR: ENCENDIDOS\n",
        millis() - inicioCalibracion
    );
    Serial.println("===================================");
    Serial.println();
}

void SensoresPosicion::actualizar() {
    if (!inicializado) {
        return;
    }

    for (uint8_t canal = 0; canal < CANTIDAD_INDICADORES; ++canal) {
        EstadoPlaza& plaza = plazas[canal];

        plaza.lectura = leerCanal(canal);

        if (plaza.filtroInicializado) {
            plaza.lecturaFiltrada +=
                ALFA_EMA * (float(plaza.lectura) - plaza.lecturaFiltrada);
        } else {
            plaza.lecturaFiltrada = plaza.lectura;
            plaza.filtroInicializado = true;
        }

        const bool ocupadaAhora =
            plaza.calibrada && plaza.lecturaFiltrada > plaza.umbral;

        if (ocupadaAhora != plaza.ocupada) {
            plaza.ocupada = ocupadaAhora;
            indicadores.setPos(canal, ocupadaAhora);
        }
    }
}

bool SensoresPosicion::plazaValida(uint8_t plaza) const {
    return plaza >= 1 && plaza <= CANTIDAD_INDICADORES;
}

uint16_t SensoresPosicion::getLectura(uint8_t plaza) const {
    if (!plazaValida(plaza)) return 0;
    return plazas[plaza - 1].lectura;
}

float SensoresPosicion::getLecturaFiltrada(uint8_t plaza) const {
    if (!plazaValida(plaza)) return 0.0f;
    return plazas[plaza - 1].lecturaFiltrada;
}

uint16_t SensoresPosicion::getUmbral(uint8_t plaza) const {
    if (!plazaValida(plaza)) return 0;
    return plazas[plaza - 1].umbral;
}

uint16_t SensoresPosicion::getReferenciaConIR(uint8_t plaza) const {
    if (!plazaValida(plaza)) return 0;
    return plazas[plaza - 1].conIR;
}

uint16_t SensoresPosicion::getReferenciaSinIR(uint8_t plaza) const {
    if (!plazaValida(plaza)) return 0;
    return plazas[plaza - 1].sinIR;
}

bool SensoresPosicion::estaCalibrada(uint8_t plaza) const {
    return plazaValida(plaza) && plazas[plaza - 1].calibrada;
}

bool SensoresPosicion::estaOcupada(uint8_t plaza) const {
    return plazaValida(plaza) && plazas[plaza - 1].ocupada;
}