#include <Arduino.h>
#include <esp_system.h>

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
    ANGULO_ARRIBA_ENTRADA,  // Abierta: 50 grados
    ANGULO_ABAJO_ENTRADA    // Cerrada: 140 grados
);

Servomotor servoSalida(
    PIN_SERVO_SALIDA,       // GPIO27
    ANGULO_ARRIBA_SALIDA,   // Abierta: 90 grados
    ANGULO_ABAJO_SALIDA     // Cerrada: 0 grados
);

using ComunicacionServidor::FaseGarita;
using ComunicacionServidor::Accion;

uint32_t secuenciaGarita = 0;
char arranqueGarita[17]{};

struct EstadoBarrera {
    ComunicacionServidor::Garita estado;
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
    unsigned long ahora,
    const ComunicacionServidor::Respuesta& respuesta
) {
    auto& estado = barrera.estado;
    if (estado.fase == FaseGarita::PASO || estado.fase == FaseGarita::CANCELADA) {
        if (respuesta.accion == Accion::ACK && strcmp(estado.id, respuesta.id) == 0) {
            estado = ComunicacionServidor::Garita{};
        } else {
            return; // Mantener confirmacion hasta que NestJS la reciba.
        }
    }

    if (hayPresencia) {
        if (!barrera.abierta && estado.fase == FaseGarita::LIBRE) {
            snprintf(estado.id, sizeof(estado.id), "%s-%lu", arranqueGarita,
                     static_cast<unsigned long>(++secuenciaGarita));
            estado.fase = FaseGarita::PRESENCIA;
            Serial.printf("%s | Presencia IR. Esperando OCR y autorizacion (%s).\n", nombre, estado.id);
        }
        if (barrera.contandoCierre) {
            barrera.contandoCierre = false;
            Serial.printf(
                "%s | Presencia nuevamente. Cierre cancelado.\n",
                nombre
            );
        }

        if (!barrera.abierta && estado.fase == FaseGarita::PRESENCIA &&
            respuesta.accion == Accion::ABRIR && strcmp(estado.id, respuesta.id) == 0) {
            servo.subir();
            barrera.abierta = true;
            estado.fase = FaseGarita::ABIERTA;
            Serial.printf("%s | Autorizacion NestJS recibida.\n", nombre);

            Serial.printf(
                "%s | Abrir a %d grados.\n",
                nombre, servo.getAnguloArriba()
            );
        }
        return;
    }

    if (!barrera.abierta) {
        if (estado.fase == FaseGarita::PRESENCIA) estado.fase = FaseGarita::CANCELADA;
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
        estado.fase = FaseGarita::PASO;

        Serial.printf(
            "%s | Cerrar a %d grados.\n",
            nombre, servo.getAnguloAbajo()
        );
    }
}

// Exclusivo para la maqueta supervisada: no existe confirmación física de salida.
constexpr unsigned long TIEMPO_SALIDA_MAQUETA_MS = 8000;
unsigned long salidaAbiertaDesde = 0;

void actualizarSalidaOCR(unsigned long ahora, const ComunicacionServidor::Respuesta& respuesta) {
    auto& estado = barreraSalida.estado;
    const bool coincide = strcmp(estado.id, respuesta.id) == 0;

    if (estado.fase == FaseGarita::PASO || estado.fase == FaseGarita::CANCELADA) {
        if (respuesta.accion == Accion::ACK && coincide) {
            estado = ComunicacionServidor::Garita{};
        }
        return;
    }

    if (estado.fase == FaseGarita::LIBRE) {
        snprintf(estado.id, sizeof(estado.id), "%s-%lu", arranqueGarita,
                 static_cast<unsigned long>(++secuenciaGarita));
        // PRESENCIA en este protocolo es un turno disponible para OCR, no una lectura IR.
        estado.fase = FaseGarita::PRESENCIA;
        Serial.println("SALIDA | Lista para OCR y pago (sin IR).");
        return;
    }

    if (estado.fase == FaseGarita::PRESENCIA) {
        if (respuesta.accion == Accion::CANCELAR && coincide) {
            estado.fase = FaseGarita::CANCELADA;
            Serial.println("SALIDA | Turno OCR vencido, sin apertura.");
        } else if (respuesta.accion == Accion::ABRIR && coincide) {
            servoSalida.subir();
            barreraSalida.abierta = true;
            salidaAbiertaDesde = ahora;
            estado.fase = FaseGarita::ABIERTA;
            digitalWrite(PIN_LED_SALIDA, HIGH);
            Serial.println("SALIDA | Pago autorizado. Barrera abierta por 8 segundos.");
        }
        return;
    }

    if (estado.fase == FaseGarita::ABIERTA &&
        ahora - salidaAbiertaDesde >= TIEMPO_SALIDA_MAQUETA_MS) {
        servoSalida.bajar();
        barreraSalida.abierta = false;
        digitalWrite(PIN_LED_SALIDA, LOW);
        estado.fase = FaseGarita::PASO;
        Serial.println("SALIDA | Cierre temporizado (paso no verificado por IR).");
    }
}

void leerGaritas() {
    // Entrada conserva su IR. Salida solo usa OCR y pago.
    sensoresGarita.actualizar();

    const unsigned long ahora = millis();
    if (!sensoresGarita.getCalibrado()) return;
    ComunicacionServidor::RespuestasGarita respuesta{};
    if (!ComunicacionServidor::recibirGaritas(respuesta) ||
        ahora - respuesta.tomada > 1500UL) respuesta = ComunicacionServidor::RespuestasGarita{};

    actualizarBarrera(
        "ENTRADA", sensoresGarita.getSensorEntrada(),
        servoEntrada, barreraEntrada, ahora, respuesta.zona[0]
    );

    actualizarSalidaOCR(ahora, respuesta.zona[1]);
    ComunicacionServidor::publicarGaritas(barreraEntrada.estado, barreraSalida.estado);
}

// ==============================================
// MONITOR SERIE DE LAS SIETE PLAZAS
// ==============================================

void imprimirPlazas() {
    Serial.printf("IR ENTRADA | ADC: %u | EMA: %.1f | Umbral: %u | %s\n",
        unsigned(sensoresGarita.getValorAnalogicoEntrada()), sensoresGarita.getEMAEntrada(),
        unsigned(sensoresGarita.getUmbralEntrada()), sensoresGarita.getSensorEntrada() ? "DETECTA" : "LIBRE");
    Serial.printf("SALIDA | OCR y pago | Servo: %s\n", barreraSalida.abierta ? "ABIERTA" : "CERRADA");
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

    snprintf(arranqueGarita, sizeof(arranqueGarita), "%08lx%08lx",
             static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
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

