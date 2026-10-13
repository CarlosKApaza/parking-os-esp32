
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include "SensorIR.h"

// ======================================
// 1. WIFI Y SERVIDOR
// ======================================

const char* WIFI_SSID = "KOKUN";
const char* WIFI_PASSWORD = "Summer04#";

const char* URL_SERVIDOR =
    "http://192.168.1.10:3000/sensores/lecturas";

// ======================================
// 2. SENSORES DE ESTACIONAMIENTO
// ======================================

SensorIR sensorP1(35);
SensorIR sensorP2(32);

// ======================================
// 3. SENSORES DE LA GARITA
// ======================================

SensorIR sensorEntrada(36);
SensorIR sensorSalida(33);

// ======================================
// 4. SERVOMOTORES
// ======================================

Servo servoEntrada;
Servo servoSalida;

const int PIN_SERVO_ENTRADA = 19;
const int PIN_SERVO_SALIDA = 18;

const int ANGULO_CERRADO = 0;
const int ANGULO_ABIERTO = 90;

const unsigned long TIEMPO_CIERRE = 2000;

// ======================================
// 5. ESTADOS DE ESTACIONAMIENTO
// ======================================

struct EstadoSensor {
    bool candidato = false;
    int muestras = 0;

    bool confirmado = false;
    bool estadoEstable = false;

    bool enviado = false;
    bool ultimoEnviado = false;

    unsigned long ultimoIntento = 0;
    unsigned long ultimoEnvio = 0;
};

EstadoSensor estadoP1;
EstadoSensor estadoP2;

const unsigned long REPORTE_MS = 30000;
const unsigned long REINTENTO_MS = 5000;

// ======================================
// 6. ESTADOS DE LAS BARRERAS
// ======================================

struct EstadoGarita {
    bool candidato = false;
    int muestras = 0;

    bool abierta = false;
    unsigned long libreDesde = 0;
};

EstadoGarita garitaEntrada;
EstadoGarita garitaSalida;

// ======================================
// 7. ENVIAR LECTURA A NESTJS
// ======================================

bool enviarLectura(const char* codigo, bool ocupado) {

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    HTTPClient http;

    http.begin(URL_SERVIDOR);
    http.setTimeout(1500);
    http.addHeader("Content-Type", "application/json");

    String json = "{\"sensorCodigo\":\"";
    json += codigo;
    json += "\",\"ocupado\":";
    json += (ocupado ? "true" : "false");
    json += "}";

    int respuesta = http.POST(json);

    Serial.printf(
        "POST %s -> HTTP %d\n",
        codigo,
        respuesta
    );

    if (respuesta > 0) {
        Serial.println(http.getString());
    }

    http.end();

    return respuesta >= 200 && respuesta < 300;
}

// ======================================
// 8. PROCESAR ESTACIONAMIENTOS
// ======================================

void procesarSensor(
    SensorIR& sensor,
    const char* codigo,
    EstadoSensor& control
) {
    sensor.actualizar();

    bool ocupado = sensor.estaOcupado();

    // Confirmar 3 lecturas consecutivas
    if (ocupado == control.candidato) {
        if (control.muestras < 3) {
            control.muestras++;
        }
    } else {
        control.candidato = ocupado;
        control.muestras = 1;
    }

    if (control.muestras >= 3) {
        control.confirmado = true;
        control.estadoEstable = ocupado;
    }

    if (!control.confirmado) return;

    // Cambio o reporte periodico
    bool cambio =
        !control.enviado ||
        control.estadoEstable != control.ultimoEnviado;

    bool reporte =
        control.enviado &&
        millis() - control.ultimoEnvio >= REPORTE_MS;

    if (!cambio && !reporte) return;

    if (WiFi.status() != WL_CONNECTED) return;

    if (
        control.ultimoIntento != 0 &&
        millis() - control.ultimoIntento < REINTENTO_MS
    ) {
        return;
    }

    control.ultimoIntento = millis();

    bool exito = enviarLectura(
        codigo,
        control.estadoEstable
    );

    if (exito) {
        control.ultimoEnviado = control.estadoEstable;
        control.ultimoEnvio = millis();
        control.enviado = true;
    }
}

// ======================================
// 9. CONTROL AUTOMATICO DE GÁRITAS
// ======================================

void procesarGarita(
    SensorIR& sensor,
    Servo& servo,
    EstadoGarita& control,
    const char* nombre
) {
    sensor.actualizar();

    bool detectado = sensor.estaOcupado();

    // Confirmar 3 muestras consecutivas
    if (detectado == control.candidato) {
        if (control.muestras < 3) {
            control.muestras++;
        }
    } else {
        control.candidato = detectado;
        control.muestras = 1;
    }

    // Cancelar cierre si vuelve a detectar
    if (detectado) {
        control.libreDesde = 0;
    }

    if (control.muestras < 3) return;

    // VEHICULO DETECTADO: ABRIR
    if (control.candidato) {

        if (!control.abierta) {
            servo.write(ANGULO_ABIERTO);
            control.abierta = true;

            Serial.printf(
                "%s: BARRERA ABIERTA\n",
                nombre
            );
        }

        control.libreDesde = 0;
    }

    // SIN VEHICULO: ESPERAR Y CERRAR
    else if (control.abierta) {

        if (control.libreDesde == 0) {
            control.libreDesde = millis();
        }

        if (
            millis() - control.libreDesde >= TIEMPO_CIERRE
        ) {
            servo.write(ANGULO_CERRADO);
            control.abierta = false;
            control.libreDesde = 0;

            Serial.printf(
                "%s: BARRERA CERRADA\n",
                nombre
            );
        }
    }
}

// ======================================
// 10. IMPRIMIR ESTADOS EN SERIAL
// ======================================

void imprimirEstados() {

    Serial.println();
    Serial.println("========== PARKING OS ==========");

    // ESTACIONAMIENTOS
    Serial.printf(
        "P1 | GPIO 35 | RAW: %d mV | EMA: %.0f | %s\n",
        sensorP1.getLectura(),
        sensorP1.getEMA(),
        sensorP1.estaOcupado() ? "OCUPADO" : "LIBRE"
    );

    Serial.printf(
        "P2 | GPIO 32 | RAW: %d mV | EMA: %.0f | %s\n",
        sensorP2.getLectura(),
        sensorP2.getEMA(),
        sensorP2.estaOcupado() ? "OCUPADO" : "LIBRE"
    );

    Serial.println("--------------------------------");

    // SENSOR IR ENTRADA
    Serial.printf(
        "IR ENTRADA | GPIO 36 | RAW: %d mV | EMA: %.0f | UMBRAL: %d | %s\n",
        sensorEntrada.getLectura(),
        sensorEntrada.getEMA(),
        sensorEntrada.getUmbral(),
        sensorEntrada.estaOcupado() ? "DETECTA" : "NO DETECTA"
    );

    // SERVO ENTRADA
    Serial.printf(
        "SERVO ENTRADA | GPIO 19 | %s | ANGULO: %d\n",
        garitaEntrada.abierta ? "ABIERTO" : "CERRADO",
        garitaEntrada.abierta ? ANGULO_ABIERTO : ANGULO_CERRADO
    );

    Serial.println("--------------------------------");

    // SENSOR IR SALIDA
    Serial.printf(
        "IR SALIDA | GPIO 33 | RAW: %d mV | EMA: %.0f | UMBRAL: %d | %s\n",
        sensorSalida.getLectura(),
        sensorSalida.getEMA(),
        sensorSalida.getUmbral(),
        sensorSalida.estaOcupado() ? "DETECTA" : "NO DETECTA"
    );

    // SERVO SALIDA
    Serial.printf(
        "SERVO SALIDA | GPIO 18 | %s | ANGULO: %d\n",
        garitaSalida.abierta ? "ABIERTO" : "CERRADO",
        garitaSalida.abierta ? ANGULO_ABIERTO : ANGULO_CERRADO
    );

    Serial.println("================================");
}

// ======================================
// 11. SETUP
// ======================================

void setup() {

    Serial.begin(115200);

    analogReadResolution(12);

    // Sensores estacionamiento
    sensorP1.iniciar();
    sensorP2.iniciar();

    // Mantener tus umbrales
    sensorP1.setUmbral(2750);
    sensorP2.setUmbral(1500);

    // Sensores garita
    sensorEntrada.iniciar();
    sensorSalida.iniciar();

    // Umbrales actuales
    sensorEntrada.setUmbral(2500);
    sensorSalida.setUmbral(2000);

    // Configurar servos
    servoEntrada.setPeriodHertz(50);
    servoSalida.setPeriodHertz(50);

    servoEntrada.attach(PIN_SERVO_ENTRADA);
    servoSalida.attach(PIN_SERVO_SALIDA);

    // Posicion inicial
    servoEntrada.write(ANGULO_CERRADO);
    servoSalida.write(ANGULO_CERRADO);

    // WiFi
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("Parking OS iniciado");
    Serial.println("4 sensores IR y 2 servos");
    Serial.println("Conectando al WiFi...");
}

// ======================================
// 12. LOOP PRINCIPAL
// ======================================

void loop() {

    static unsigned long ultimoWiFi = 0;
    static bool conectadoAntes = false;

    static unsigned long ultimaLecturaP = 0;
    static unsigned long ultimaLecturaGarita = 0;
    static unsigned long ultimaImpresion = 0;

    // VERIFICAR WIFI
    if (WiFi.status() == WL_CONNECTED) {

        if (!conectadoAntes) {
            Serial.println("WiFi conectado");
            Serial.println(WiFi.localIP());

            conectadoAntes = true;
        }

    } else {

        if (conectadoAntes) {
            Serial.println("WiFi desconectado");
            conectadoAntes = false;
        }

        if (millis() - ultimoWiFi >= 10000) {
            ultimoWiFi = millis();
            WiFi.reconnect();
        }
    }

    // SENSORES ENTRADA Y SALIDA
    // Cada 100 ms
    if (millis() - ultimaLecturaGarita >= 100) {

        ultimaLecturaGarita = millis();

        procesarGarita(
            sensorEntrada,
            servoEntrada,
            garitaEntrada,
            "ENTRADA"
        );

        procesarGarita(
            sensorSalida,
            servoSalida,
            garitaSalida,
            "SALIDA"
        );
    }

    // SENSORES P1 Y P2
    // Cada 500 ms
    if (millis() - ultimaLecturaP >= 500) {

        ultimaLecturaP = millis();

        procesarSensor(sensorP1, "IR01", estadoP1);
        procesarSensor(sensorP2, "IR02", estadoP2);
    }

    // IMPRIMIR TODO CADA 500 ms
    if (millis() - ultimaImpresion >= 500) {

        ultimaImpresion = millis();
        imprimirEstados();
    }
}
