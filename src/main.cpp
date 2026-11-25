#include <Arduino.h>

#include "Pines.h"
#include "SensoresEntradaSalida.h"
#include "SensoresPosicion.h"
#include "Servomotor.h"
#include "ComunicacionServidor.h"

// ==============================================
// SENSORES Y SERVOMOTORES
// ==============================================

SensoresEntradaSalida sensoresGarita;
SensoresPosicion sensoresPosicion;

Servomotor servoEntrada(
    PIN_SERVO_ENTRADA,      // GPIO26
    ANGULO_ARRIBA_ENTRADA,  // Abierta: 40 grados
    ANGULO_ABAJO_ENTRADA    // Cerrada: 130 grados
);

Servomotor servoSalida(
    PIN_SERVO_SALIDA,       // GPIO27
    ANGULO_ARRIBA_SALIDA,   // Abierta: 90 grados
    ANGULO_ABAJO_SALIDA     // Cerrada: 0 grados
);

struct EstadoBarrera {
    bool abierta = false;
    bool contandoCierre = false;
    unsigned long libreDesde = 0;
};

EstadoBarrera barreraEntrada;
EstadoBarrera barreraSalida;

bool plazasHabilitadas = false;

unsigned long ultimaLecturaGarita = 0;
unsigned long ultimaLecturaPlazas = 0;
unsigned long ultimaImpresion = 0;

// ==============================================
// CONTROL DE BARRERAS
// ==============================================

void actualizarBarrera(
    const char* nombre,
    bool hayPresencia,
    Servomotor& servo,
    EstadoBarrera& barrera,
    unsigned long ahora
) {
    if (hayPresencia) {
        if (barrera.contandoCierre) {
            barrera.contandoCierre = false;
            Serial.printf(
                "%s | Presencia nuevamente. Cierre cancelado.\n",
                nombre
            );
        }

        if (!barrera.abierta) {
            servo.subir();
            barrera.abierta = true;

            Serial.printf(
                "%s | Abrir a %d grados.\n",
                nombre, servo.getAnguloArriba()
            );
        }
        return;
    }

    if (!barrera.abierta) {
        return;
    }

    // Primera lectura libre: iniciar el temporizador de esta barrera.
    if (!barrera.contandoCierre) {
        barrera.libreDesde = ahora;
        barrera.contandoCierre = true;

        Serial.printf(
            "%s | Libre. Cierre en %lu ms.\n",
            nombre, TIEMPO_CIERRE_MS
        );
        return;
    }

    if (ahora - barrera.libreDesde >= TIEMPO_CIERRE_MS) {
        servo.bajar();
        barrera.abierta = false;
        barrera.contandoCierre = false;

        Serial.printf(
            "%s | Cerrar a %d grados.\n",
            nombre, servo.getAnguloAbajo()
        );
    }
}

void leerGaritas() {
    // Lee GPIO36 y GPIO39 y actualiza los LEDs de GPIO14 y GPIO13.
    sensoresGarita.actualizar();

    const unsigned long ahora = millis();

    actualizarBarrera(
        "ENTRADA", sensoresGarita.getSensorEntrada(),
        servoEntrada, barreraEntrada, ahora
    );

    actualizarBarrera(
        "SALIDA", sensoresGarita.getSensorSalida(),
        servoSalida, barreraSalida, ahora
    );
}

// ==============================================
// MONITOR SERIE DE LAS SIETE PLAZAS
// ==============================================

void imprimirPlazas() {
    Serial.println("---------- PLAZAS ----------");

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
    Serial.println("Mantenga libres ambas garitas y las siete plazas.");

    servoEntrada.iniciar();
    servoSalida.iniciar();

    // Los dos drivers usan GPIO18. Completar una calibracion por vez.
    Serial.println("Calibrando sensores de entrada y salida...");
    sensoresGarita.begin();

    Serial.printf(
        "Umbrales de garita | Entrada: %u | Salida: %u\n",
        unsigned(sensoresGarita.getUmbralEntrada()),
        unsigned(sensoresGarita.getUmbralSalida())
    );

    Serial.println("Calibrando sensores de posicion P1 a P7...");
    plazasHabilitadas = sensoresPosicion.begin();

    // Leer de nuevo las garitas ahora que los emisores IR estan encendidos.
    leerGaritas();

    if (plazasHabilitadas) {
        imprimirPlazas();
    } else {
        Serial.println(
            "Plazas deshabilitadas. Revise PIN_ADC_MUX_Z. "
            "Las garitas siguen activas."
        );
    }

    const unsigned long ahora = millis();
    ultimaLecturaGarita = ahora;
    ultimaLecturaPlazas = ahora;
    ultimaImpresion = ahora;

    // Iniciar red solo despues de terminar la calibracion fisica.
    ComunicacionServidor::iniciar();
    if (plazasHabilitadas) ComunicacionServidor::publicar(sensoresPosicion);

    Serial.println("Monitoreo iniciado.");
}

// ==============================================
// MONITOREO PERIODICO
// ==============================================

void loop() {
    const unsigned long ahora = millis();

    if (ahora - ultimaLecturaGarita >= INTERVALO_GARITA_MS) {
        ultimaLecturaGarita = ahora;
        leerGaritas();
    }

    if (plazasHabilitadas &&
        ahora - ultimaLecturaPlazas >= INTERVALO_ESTACIONAMIENTOS_MS) {
        ultimaLecturaPlazas = ahora;

        // Lee P1-P7 por el 74HC4051 y actualiza QB-QH en el 74HC595.
        sensoresPosicion.actualizar();
        ComunicacionServidor::publicar(sensoresPosicion);
    }

    if (plazasHabilitadas &&
        ahora - ultimaImpresion >= INTERVALO_IMPRESION_MS) {
        ultimaImpresion = ahora;
        imprimirPlazas();
    }
}

