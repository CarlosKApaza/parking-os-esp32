#include "ComunicacionServidor.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#if __has_include("ConfigServidor.h")
#include "ConfigServidor.h"
#define PARKING_CONFIGURADO 1
#else
#define PARKING_CONFIGURADO 0
#endif

namespace {
    struct Snapshot {
        bool calibrada[CANTIDAD_INDICADORES];
        bool ocupada[CANTIDAD_INDICADORES];
        unsigned long tomada;
    };
    QueueHandle_t cola = nullptr;

#if PARKING_CONFIGURADO
    struct EstadoEnvio {
        bool enviado = false;
        bool ocupado = false;
        bool fallo = false;
        unsigned long ultimoEnvio = 0;
        unsigned long ultimoIntento = 0;
    };

    bool enviar(uint8_t i, bool ocupado) {
        WiFiClient cliente;
        HTTPClient http;
        http.setConnectTimeout(1500);
        http.setTimeout(1500);
        if (!http.begin(cliente, SERVIDOR_URL)) return false;
        http.addHeader("Content-Type", "application/json");
        http.addHeader("x-esp32-key", ESP32_API_KEY);
        char cuerpo[80];
        snprintf(cuerpo, sizeof(cuerpo), "{\"sensorCodigo\":\"IR%02u\",\"ocupado\":%s}",
                 unsigned(i + 1), ocupado ? "true" : "false");
        int codigo = http.POST(String(cuerpo));
        http.end();
        Serial.printf("[RED] IR%02u %s HTTP %d\n", unsigned(i + 1),
                      ocupado ? "OCUPADO" : "LIBRE", codigo);
        return codigo >= 200 && codigo < 300;
    }

    void tareaRed(void*) {
        WiFi.mode(WIFI_STA);
        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        unsigned long ultimoWifi = millis();
        bool conectadoAntes = false;
        EstadoEnvio estados[CANTIDAD_INDICADORES]{};
        Snapshot actual{};
        bool haySnapshot = false;
        uint8_t siguiente = 0;

        for (;;) {
            Snapshot nuevo{};
            if (xQueueReceive(cola, &nuevo, 0) == pdTRUE) {
                actual = nuevo;
                haySnapshot = true;
            }
            unsigned long ahora = millis();
            if (WiFi.status() != WL_CONNECTED) {
                if (conectadoAntes) Serial.println("[RED] WiFi desconectado; control local activo.");
                conectadoAntes = false;
                if (ahora - ultimoWifi >= INTERVALO_RECONEXION_WIFI_MS) {
                    ultimoWifi = ahora;
                    WiFi.reconnect();
                }
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            if (!conectadoAntes) {
                conectadoAntes = true;
                Serial.printf("[RED] WiFi conectado. IP: %s\n", WiFi.localIP().toString().c_str());
                // Reenviar las siete plazas al recuperar la red.
                for (auto& e : estados) e = EstadoEnvio{};
            }

            // No presentar como reciente una lectura si el control local dejo de actualizar.
            if (haySnapshot && ahora - actual.tomada <= 2000UL) {
                for (uint8_t n = 0; n < CANTIDAD_INDICADORES; ++n) {
                    uint8_t i = (siguiente + n) % CANTIDAD_INDICADORES;
                    if (!actual.calibrada[i]) continue;
                    EstadoEnvio& e = estados[i];
                    if (e.fallo && ahora - e.ultimoIntento < REINTENTO_MS) continue;
                    if (e.enviado && e.ocupado == actual.ocupada[i] &&
                        ahora - e.ultimoEnvio < REPORTE_MS) continue;

                    bool ocupado = actual.ocupada[i];
                    bool ok = enviar(i, ocupado);
                    e.ultimoIntento = millis();
                    e.fallo = !ok;
                    if (ok) {
                        e.enviado = true;
                        e.ocupado = ocupado;
                        e.ultimoEnvio = e.ultimoIntento;
                    }
                    // Solo una peticion por vuelta; leer el snapshot mas reciente
                    // antes de enviar otra plaza. No acumular estados obsoletos.
                    siguiente = (i + 1) % CANTIDAD_INDICADORES;
                    break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
#endif
}

void ComunicacionServidor::iniciar() {
#if PARKING_CONFIGURADO
    if (cola) return;
    if (strlen(ESP32_API_KEY) < 32 || strcmp(WIFI_SSID, "NOMBRE_WIFI") == 0 ||
        strcmp(ESP32_API_KEY, "REEMPLAZAR_CON_LA_CLAVE_DEL_BACKEND") == 0) {
        Serial.println("[RED] Completar ConfigServidor.h; red desactivada.");
        return;
    }
    cola = xQueueCreate(1, sizeof(Snapshot));
    if (!cola) {
        Serial.println("[RED] No se pudo crear la cola; control local activo.");
        return;
    }
    if (xTaskCreate(tareaRed, "parking-red", 8192, nullptr, 1, nullptr) != pdPASS) {
        vQueueDelete(cola);
        cola = nullptr;
        Serial.println("[RED] No se pudo iniciar la tarea; control local activo.");
    }
#else
    Serial.println("[RED] Falta include/ConfigServidor.h; control local activo.");
#endif
}

void ComunicacionServidor::publicar(const SensoresPosicion& sensores) {
    if (!cola) return;
    Snapshot s{};
    for (uint8_t i = 0; i < CANTIDAD_INDICADORES; ++i) {
        s.calibrada[i] = sensores.estaCalibrada(i + 1);
        s.ocupada[i] = sensores.estaOcupada(i + 1);
    }
    s.tomada = millis();
    xQueueOverwrite(cola, &s);
}
