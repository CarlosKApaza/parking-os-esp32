#include <Arduino.h>

#include "Pines.h"
#include "SensoresEntradaSalida.h"
#include "SensoresPosicion.h"

// Cada clase controla los LEDs correspondientes a sus sensores.
SensoresEntradaSalida sensoresGarita;
SensoresPosicion sensoresPosicion;

bool plazasHabilitadas = false;

unsigned long ultimaLecturaGarita = 0;
unsigned long ultimaLecturaPlazas = 0;
unsigned long ultimaImpresion = 0;

// ==============================================
// RESULTADOS DE CALIBRACION
// ==============================================

void imprimirCalibracion() {
    Serial.println();
    Serial.println("========== CALIBRACION DE GARITAS ==========");

    Serial.printf(
        "ENTRADA | Con IR: %u | Sin IR: %u | Umbral: %u\n",
        unsigned(sensoresGarita.getMinimoEntrada()),
        unsigned(sensoresGarita.getMaximoEntrada()),
        unsigned(sensoresGarita.getUmbralEntrada())
    );

    Serial.printf(
        "SALIDA  | Con IR: %u | Sin IR: %u | Umbral: %u\n",
        unsigned(sensoresGarita.getMinimoSalida()),
        unsigned(sensoresGarita.getMaximoSalida()),
        unsigned(sensoresGarita.getUmbralSalida())
    );

    if (!plazasHabilitadas) {
        return;
    }

    Serial.println();
    Serial.println("========== CALIBRACION DE PLAZAS ==========");

    for (uint8_t plaza = 1; plaza <= CANTIDAD_INDICADORES; ++plaza) {
        const uint16_t conIR = sensoresPosicion.getReferenciaConIR(plaza);
        const uint16_t sinIR = sensoresPosicion.getReferenciaSinIR(plaza);

        Serial.printf(
            "P%u | Con IR: %u | Sin IR: %u | Diferencia: %+d"
            " | Umbral: %u | %s\n",
            unsigned(plaza),
            unsigned(conIR),
            unsigned(sinIR),
            int(sinIR) - int(conIR),
            unsigned(sensoresPosicion.getUmbral(plaza)),
            sensoresPosicion.estaCalibrada(plaza)
                ? "CALIBRADA"
                : "REVISAR CALIBRACION"
        );
    }
}

// ==============================================
// ESTADOS DE ENTRADA, SALIDA Y PLAZAS
// ==============================================

void imprimirEstados() {
    Serial.println();
    Serial.println("========== ESTADO DEL ESTACIONAMIENTO ==========");

    Serial.printf(
        "ENTRADA | ADC: %u | Umbral: %u | %s\n",
        unsigned(sensoresGarita.getValorAnalogicoEntrada()),
        unsigned(sensoresGarita.getUmbralEntrada()),
        sensoresGarita.getSensorEntrada() ? "COCHE PRESENTE" : "LIBRE"
    );

    Serial.printf(
        "SALIDA  | ADC: %u | Umbral: %u | %s\n",
        unsigned(sensoresGarita.getValorAnalogicoSalida()),
        unsigned(sensoresGarita.getUmbralSalida()),
        sensoresGarita.getSensorSalida() ? "COCHE PRESENTE" : "LIBRE"
    );

    if (!plazasHabilitadas) {
        return;
    }

    for (uint8_t plaza = 1; plaza <= CANTIDAD_INDICADORES; ++plaza) {
        const char* estado = "REVISAR CALIBRACION";

        if (sensoresPosicion.estaCalibrada(plaza)) {
            estado = sensoresPosicion.estaOcupada(plaza)
                ? "OCUPADA"
                : "LIBRE";
        }

        Serial.printf(
            "P%u | Y%u | Q%c | ADC: %u | Umbral: %u | %s\n",
            unsigned(plaza),
            unsigned(plaza - 1),
            'A' + plaza,
            unsigned(sensoresPosicion.getLectura(plaza)),
            unsigned(sensoresPosicion.getUmbral(plaza)),
            estado
        );
    }
}

// ==============================================
// INICIALIZACION
// ==============================================

void setup() {
    Serial.begin(115200);

    Serial.println();
    Serial.println("SISTEMA DE ESTACIONAMIENTO");
    Serial.println("Mantenga libres la entrada, la salida y las siete plazas.");

    // Ambas clases usan GPIO18. Completar una calibracion antes de iniciar la otra.
    Serial.println("Calibrando entrada y salida...");
    sensoresGarita.begin();

    Serial.println("Calibrando posiciones P1 a P7...");
    plazasHabilitadas = sensoresPosicion.begin();

    if (!plazasHabilitadas) {
        Serial.println("Plazas deshabilitadas. Revise PIN_ADC_MUX_Z en Pines.h.");
    }

    // Los emisores ya estan encendidos para el funcionamiento normal.
    // Refrescar las garitas tras el tiempo dedicado a calibrar las plazas.
    sensoresGarita.actualizar();

    imprimirCalibracion();
    imprimirEstados();

    const unsigned long ahora = millis();
    ultimaLecturaGarita = ahora;
    ultimaLecturaPlazas = ahora;
    ultimaImpresion = ahora;
}

// ==============================================
// MONITOREO PERIODICO
// ==============================================

void loop() {
    const unsigned long ahora = millis();

    // Cada 100 ms: leer entrada/salida y actualizar sus dos LEDs.
    if (ahora - ultimaLecturaGarita >= INTERVALO_GARITA_MS) {
        ultimaLecturaGarita = ahora;
        sensoresGarita.actualizar();
    }

    // Cada 500 ms: leer P1 a P7 y actualizar sus LEDs en el 74HC595.
    if (plazasHabilitadas &&
        ahora - ultimaLecturaPlazas >= INTERVALO_ESTACIONAMIENTOS_MS) {
        ultimaLecturaPlazas = ahora;
        sensoresPosicion.actualizar();
    }

    // Cada 500 ms: mostrar la ultima lectura de los nueve sensores.
    if (ahora - ultimaImpresion >= INTERVALO_IMPRESION_MS) {
        ultimaImpresion = ahora;
        imprimirEstados();
    }
}