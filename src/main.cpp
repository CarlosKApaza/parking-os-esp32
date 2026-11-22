#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "Pines.h"
#include "SensorIR.h"
#include "Servomotor.h"
#include "IluminacionAutomatica.h"
// ======================================
// 1. WIFI Y SERVIDOR
// ======================================

const char* WIFI_SSID = "Galaxy A05s 6227";
const char* WIFI_PASSWORD = "987654321";

const char* URL_SERVIDOR =
    "http://10.207.211.147:3000/sensores/lecturas";

// ======================================
// 2. SENSORES Y SERVOMOTORES
// ======================================

SensorIR sensorP1(PIN_SENSOR_P1);
SensorIR sensorP2(PIN_SENSOR_P2);

// Los cinco sensores nuevos usan DO digital (no ADC2).
// Si el cableado utiliza AO analogico, NO activar HABILITAR_P3_P7.
SensorIR sensorP3(PIN_SENSOR_P3, ModoSensorIR::DIGITAL, NIVEL_OCUPADO_DIGITAL);
SensorIR sensorP4(PIN_SENSOR_P4, ModoSensorIR::DIGITAL, NIVEL_OCUPADO_DIGITAL);
SensorIR sensorP5(PIN_SENSOR_P5, ModoSensorIR::DIGITAL, NIVEL_OCUPADO_DIGITAL);
SensorIR sensorP6(PIN_SENSOR_P6, ModoSensorIR::DIGITAL, NIVEL_OCUPADO_DIGITAL);
SensorIR sensorP7(PIN_SENSOR_P7, ModoSensorIR::DIGITAL, NIVEL_OCUPADO_DIGITAL);

SensorIR sensorEntrada(PIN_SENSOR_ENTRADA);
SensorIR sensorSalida(PIN_SENSOR_SALIDA);

// Entrada: arriba = 0°, abajo = 90°.
// Salida: arriba = 90°, abajo = 0°.
Servomotor servomotorEntrada(
    PIN_SERVO_ENTRADA,
    ANGULO_ARRIBA_ENTRADA,
    ANGULO_ABAJO_ENTRADA
);

Servomotor servomotorSalida(
    PIN_SERVO_SALIDA,
    ANGULO_ARRIBA_SALIDA,
    ANGULO_ABAJO_SALIDA
);

IluminacionAutomatica iluminacionAutomatica(
    PIN_LDR,
    PIN_FOCOS,
    UMBRAL_NOCHE_V
);

// ======================================
// 3. ESTADOS DE ESTACIONAMIENTO
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
EstadoSensor estadoP3;
EstadoSensor estadoP4;
EstadoSensor estadoP5;
EstadoSensor estadoP6;
EstadoSensor estadoP7;

// ======================================
// 4. ESTADOS DE LAS BARRERAS
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
// 5. ENVIAR LECTURA A NESTJS
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

    Serial.printf("POST %s -> HTTP %d\n", codigo, respuesta);

    if (respuesta > 0) {
        Serial.println(http.getString());
    }

    http.end();

    return respuesta >= 200 && respuesta < 300;
}

// ======================================
// 6. PROCESAR ESTACIONAMIENTOS
// ======================================

void procesarSensor(
    SensorIR& sensor,
    const char* codigo,
    EstadoSensor& control
) {
    sensor.actualizar();

    bool ocupado = sensor.estaOcupado();

    // Confirmar 3 lecturas consecutivas.
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

    if (!control.confirmado) {
        return;
    }

    // Cambio de estado o reporte periódico.
    bool cambio =
        !control.enviado ||
        control.estadoEstable != control.ultimoEnviado;

    bool reporte =
        control.enviado &&
        millis() - control.ultimoEnvio >= REPORTE_MS;

    if (!cambio && !reporte) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (
        control.ultimoIntento != 0 &&
        millis() - control.ultimoIntento < REINTENTO_MS
    ) {
        return;
    }

    control.ultimoIntento = millis();

    bool exito = enviarLectura(codigo, control.estadoEstable);

    if (exito) {
        control.ultimoEnviado = control.estadoEstable;
        control.ultimoEnvio = millis();
        control.enviado = true;
    }
}

// ======================================
// 7. CONTROL AUTOMÁTICO DE GARITAS
// ======================================

void procesarGarita(
    SensorIR& sensor,
    Servomotor& servomotor,
    EstadoGarita& control,
    const char* nombre
) {
    sensor.actualizar();

    bool detectado = sensor.estaOcupado();

    // Confirmar 3 muestras consecutivas.
    if (detectado == control.candidato) {
        if (control.muestras < 3) {
            control.muestras++;
        }
    } else {
        control.candidato = detectado;
        control.muestras = 1;
    }

    // Cancelar el cierre si vuelve a detectar un vehículo.
    if (detectado) {
        control.libreDesde = 0;
    }

    if (control.muestras < 3) {
        return;
    }

    // VEHÍCULO DETECTADO: SUBIR / ABRIR.
    if (control.candidato) {
        if (!control.abierta) {
            servomotor.subir();
            control.abierta = true;

            Serial.printf("%s: BARRERA ABIERTA\n", nombre);
        }

        control.libreDesde = 0;
    }
    // SIN VEHÍCULO: ESPERAR Y BAJAR / CERRAR.
    else if (control.abierta) {
        if (control.libreDesde == 0) {
            control.libreDesde = millis();
        }

        if (millis() - control.libreDesde >= TIEMPO_CIERRE_MS) {
            servomotor.bajar();
            control.abierta = false;
            control.libreDesde = 0;

            Serial.printf("%s: BARRERA CERRADA\n", nombre);
        }
    }
}

// ======================================
// 8. IMPRIMIR ESTADOS EN SERIAL
// ======================================

void imprimirEstados() {
    Serial.println();
    Serial.println("========== PARKING OS ==========");

    Serial.printf(
        "P1 | GPIO %d | RAW: %d mV | EMA: %.0f | %s\n",
        PIN_SENSOR_P1,
        sensorP1.getLectura(),
        sensorP1.getEMA(),
        sensorP1.estaOcupado() ? "OCUPADO" : "LIBRE"
    );

    Serial.printf(
        "P2 | GPIO %d | RAW: %d mV | EMA: %.0f | %s\n",
        PIN_SENSOR_P2,
        sensorP2.getLectura(),
        sensorP2.getEMA(),
        sensorP2.estaOcupado() ? "OCUPADO" : "LIBRE"
    );

    Serial.println("--------------------------------");

    if (HABILITAR_P3_P7) {
        const char* codigo[] = {"P3", "P4", "P5", "P6", "P7"};
        const uint8_t pines[] = {
            PIN_SENSOR_P3, PIN_SENSOR_P4, PIN_SENSOR_P5,
            PIN_SENSOR_P6, PIN_SENSOR_P7
        };
        const SensorIR* sensores[] = {
            &sensorP3, &sensorP4, &sensorP5, &sensorP6, &sensorP7
        };
        for (int i = 0; i < 5; i++) {
            Serial.printf(
                "%s | GPIO %d | DO: %s | %s\n",
                codigo[i], pines[i],
                sensores[i]->getNivelDigital() == LOW ? "LOW" : "HIGH",
                sensores[i]->estaOcupado() ? "OCUPADO" : "LIBRE"
            );
        }
    } else {
        Serial.println("P3-P7: pendientes de verificar GPIO y salida DO.");
    }

    Serial.println("--------------------------------");

    Serial.printf(
        "IR ENTRADA | GPIO %d | RAW: %d mV | EMA: %.0f | UMBRAL: %d | %s\n",
        PIN_SENSOR_ENTRADA,
        sensorEntrada.getLectura(),
        sensorEntrada.getEMA(),
        sensorEntrada.getUmbral(),
        sensorEntrada.estaOcupado() ? "DETECTA" : "NO DETECTA"
    );

    Serial.printf(
        "SERVO ENTRADA | GPIO %d | %s | ANGULO: %d\n",
        PIN_SERVO_ENTRADA,
        garitaEntrada.abierta ? "ABIERTO" : "CERRADO",
        servomotorEntrada.getAnguloActual()
    );

    Serial.println("--------------------------------");

    Serial.printf(
        "IR SALIDA | GPIO %d | RAW: %d mV | EMA: %.0f | UMBRAL: %d | %s\n",
        PIN_SENSOR_SALIDA,
        sensorSalida.getLectura(),
        sensorSalida.getEMA(),
        sensorSalida.getUmbral(),
        sensorSalida.estaOcupado() ? "DETECTA" : "NO DETECTA"
    );

    Serial.printf(
        "SERVO SALIDA | GPIO %d | %s | ANGULO: %d\n",
        PIN_SERVO_SALIDA,
        garitaSalida.abierta ? "ABIERTO" : "CERRADO",
        servomotorSalida.getAnguloActual()
    );

    Serial.println("================================");
}

// ======================================
// 9. SETUP
// ======================================

void setup() {
    Serial.begin(115200);
    analogReadResolution(12);

    // Indicadores LED
    pinMode(PIN_LED_ENTRADA_COCHE, OUTPUT);
    pinMode(PIN_LED_SALIDA_COCHE, OUTPUT);
    pinMode(PIN_LED_P1_OCUPADO, OUTPUT);
    pinMode(PIN_LED_P2_OCUPADO, OUTPUT);

    // Iniciar todos apagados
    digitalWrite(PIN_LED_ENTRADA_COCHE, LOW);
    digitalWrite(PIN_LED_SALIDA_COCHE, LOW);
    digitalWrite(PIN_LED_P1_OCUPADO, LOW);
    digitalWrite(PIN_LED_P2_OCUPADO, LOW);

    // Iluminacion automatica
    iluminacionAutomatica.iniciar();

    // Sensores de estacionamiento.
    sensorP1.iniciar();
    sensorP2.iniciar();
    if (HABILITAR_P3_P7) {
        sensorP3.iniciar();
        sensorP4.iniciar();
        sensorP5.iniciar();
        sensorP6.iniciar();
        sensorP7.iniciar();
    }

    // Mantener los umbrales originales.
    sensorP1.setUmbral(2750);
    sensorP2.setUmbral(1500);

    // Sensores de las garitas.
    sensorEntrada.iniciar();
    sensorSalida.iniciar();

    sensorEntrada.setUmbral(2500);
    sensorSalida.setUmbral(2000);

    // Los servomotores inician en la posición abajo/cerrada.
    servomotorEntrada.iniciar();
    servomotorSalida.iniciar();

    // WiFi.
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("Parking OS iniciado");
    Serial.println("7 plazas P1-P7, 2 sensores de garita y 2 servos");
    if (!HABILITAR_P3_P7) {
        Serial.println("[CONFIG] IR03-IR07 DESHABILITADOS: confirma GPIO y DO en Pines.h");
    }
    Serial.println("Conectando al WiFi...");
}

// ======================================
// 10. LOOP PRINCIPAL
// ======================================

void loop() {
    static unsigned long ultimoWiFi = 0;
    static bool conectadoAntes = false;

    static unsigned long ultimaLecturaP = 0;
    static unsigned long ultimaLecturaGarita = 0;
    static unsigned long ultimaImpresion = 0;

    // Verificar WiFi.
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

        if (millis() - ultimoWiFi >= INTERVALO_RECONEXION_WIFI_MS) {
            ultimoWiFi = millis();
            WiFi.reconnect();
        }
    }

    

    // Sensores de entrada y salida: cada 100 ms.
    if (millis() - ultimaLecturaGarita >= INTERVALO_GARITA_MS) {
        ultimaLecturaGarita = millis();

        iluminacionAutomatica.actualizar();
        
        procesarGarita(
            sensorEntrada,
            servomotorEntrada,
            garitaEntrada,
            "ENTRADA"
        );

        procesarGarita(
            sensorSalida,
            servomotorSalida,
            garitaSalida,
            "SALIDA"
        );
    }

    // Sensores P1-P7: cada 500 ms.
    if (millis() - ultimaLecturaP >= INTERVALO_ESTACIONAMIENTOS_MS) {
        ultimaLecturaP = millis();

        procesarSensor(sensorP1, "IR01", estadoP1);
        procesarSensor(sensorP2, "IR02", estadoP2);
        if (HABILITAR_P3_P7) {
            procesarSensor(sensorP3, "IR03", estadoP3);
            procesarSensor(sensorP4, "IR04", estadoP4);
            procesarSensor(sensorP5, "IR05", estadoP5);
            procesarSensor(sensorP6, "IR06", estadoP6);
            procesarSensor(sensorP7, "IR07", estadoP7);
        }
    }

    // Imprimir estados cada 500 ms.
    if (millis() - ultimaImpresion >= INTERVALO_IMPRESION_MS) {
        ultimaImpresion = millis();
        imprimirEstados();
    }

    // Indicador de vehículo en la entrada
    digitalWrite(
        PIN_LED_ENTRADA_COCHE,
        sensorEntrada.estaOcupado() ? HIGH : LOW
    );

    // Indicador de vehículo en la salida
    digitalWrite(
        PIN_LED_SALIDA_COCHE,
        sensorSalida.estaOcupado() ? HIGH : LOW
    );

    // Indicador de puesto 1 ocupado
    digitalWrite(
        PIN_LED_P1_OCUPADO,
        estadoP1.confirmado && estadoP1.estadoEstable ? HIGH : LOW
    );

    // Indicador de puesto 2 ocupado
    digitalWrite(
        PIN_LED_P2_OCUPADO,
        estadoP2.confirmado && estadoP2.estadoEstable ? HIGH : LOW
    );
}
