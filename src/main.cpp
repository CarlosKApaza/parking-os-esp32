#include <Arduino.h>
#include <esp_system.h>

#include "Pines.h"
#include "SensoresPosicion.h"
#include "Servomotor.h"
#include "ComunicacionServidor.h"

// ==============================================
// SENSORES Y SERVOMOTORES
// ==============================================

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
    unsigned long abiertaDesde = 0;
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

// Solo para maqueta supervisada: los sensores de garita no intervienen.
constexpr unsigned long TIEMPO_APERTURA_MAQUETA_MS = 8000;

void actualizarBarreraOCR(const char* nombre, Servomotor& servo,
                          EstadoBarrera& barrera, uint8_t pinLed,
                          unsigned long ahora,
                          const ComunicacionServidor::Respuesta& respuesta) {
    auto& estado = barrera.estado;
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
        // PRESENCIA representa un turno disponible para OCR, no presencia fisica.
        estado.fase = FaseGarita::PRESENCIA;
        Serial.printf("%s | Lista para OCR (sin IR).\n", nombre);
        return;
    }
    if (estado.fase == FaseGarita::PRESENCIA) {
        if (respuesta.accion == Accion::CANCELAR && coincide) {
            estado.fase = FaseGarita::CANCELADA;
            Serial.printf("%s | Turno OCR cancelado.\n", nombre);
        } else if (respuesta.accion == Accion::ABRIR && coincide) {
            servo.subir();
            barrera.abierta = true;
            barrera.abiertaDesde = ahora;
            estado.fase = FaseGarita::ABIERTA;
            digitalWrite(pinLed, HIGH);
            Serial.printf("%s | Autorizacion NestJS. Abierta 8 segundos.\n", nombre);
        }
        return;
    }
    if (estado.fase == FaseGarita::ABIERTA &&
        ahora - barrera.abiertaDesde >= TIEMPO_APERTURA_MAQUETA_MS) {
        servo.bajar();
        barrera.abierta = false;
        digitalWrite(pinLed, LOW);
        estado.fase = FaseGarita::PASO;
        Serial.printf("%s | Cierre temporizado; paso no verificado.\n", nombre);
    }
}

void leerGaritas() {
    const unsigned long ahora = millis();
    ComunicacionServidor::RespuestasGarita respuesta{};
    if (!ComunicacionServidor::recibirGaritas(respuesta) ||
        ahora - respuesta.tomada > 1500UL) respuesta = ComunicacionServidor::RespuestasGarita{};
    actualizarBarreraOCR("ENTRADA", servoEntrada, barreraEntrada,
                         PIN_LED_ENTRADA, ahora, respuesta.zona[0]);
    actualizarBarreraOCR("SALIDA", servoSalida, barreraSalida,
                         PIN_LED_SALIDA, ahora, respuesta.zona[1]);
    ComunicacionServidor::publicarGaritas(barreraEntrada.estado, barreraSalida.estado);
}

// ==============================================
// MONITOR SERIE DE LAS SIETE PLAZAS
// ==============================================

void imprimirPlazas() {
    Serial.printf("ENTRADA | OCR | Servo: %s\n", barreraEntrada.abierta ? "ABIERTA" : "CERRADA");
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
    Serial.println("Mantenga libres las siete plazas. Garitas OCR sin IR.");

    snprintf(arranqueGarita, sizeof(arranqueGarita), "%08lx%08lx",
             static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    servoEntrada.iniciar();
    servoSalida.iniciar();

    pinMode(PIN_LED_ENTRADA, OUTPUT);
    pinMode(PIN_LED_SALIDA, OUTPUT);
    digitalWrite(PIN_LED_ENTRADA, LOW);
    digitalWrite(PIN_LED_SALIDA, LOW);

    // Solo se calibran los sensores P1-P7. Su clase controla el emisor comun.
    Serial.println("Calibrando sensores de posicion P1 a P7...");
    plazasHabilitadas = sensoresPosicion.begin();

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
    leerGaritas();
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

