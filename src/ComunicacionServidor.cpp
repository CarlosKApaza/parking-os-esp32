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
    struct SnapshotGarita {
        ComunicacionServidor::Garita zona[2]{};
        unsigned long tomada = 0;
    };
    QueueHandle_t colaGarita = nullptr;
    QueueHandle_t colaRespuesta = nullptr;

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

    const char* faseTexto(ComunicacionServidor::FaseGarita fase) {
        using F = ComunicacionServidor::FaseGarita;
        switch (fase) {
            case F::PRESENCIA: return "PRESENCIA";
            case F::ABIERTA: return "ABIERTA";
            case F::PASO: return "PASO";
            case F::CANCELADA: return "CANCELADA";
            default: return "LIBRE";
        }
    }

    void sincronizarGaritas(const SnapshotGarita& s) {
        String url(SERVIDOR_URL);
        const int indice = url.lastIndexOf("/sensores/lecturas");
        if (indice < 0) return;
        url = url.substring(0, indice) + "/garitas/sincronizar";
        WiFiClient cliente;
        HTTPClient http;
        http.setConnectTimeout(900);
        http.setTimeout(900);
        if (!http.begin(cliente, url)) return;
        http.addHeader("Content-Type", "application/json");
        http.addHeader("x-esp32-key", ESP32_API_KEY);
        char cuerpo[240];
        snprintf(cuerpo, sizeof(cuerpo),
            "{\"ENTRADA\":{\"id\":\"%s\",\"fase\":\"%s\"},\"SALIDA\":{\"id\":\"%s\",\"fase\":\"%s\"},\"salidaSinIR\":true}",
            s.zona[0].id, faseTexto(s.zona[0].fase), s.zona[1].id, faseTexto(s.zona[1].fase));
        const unsigned long inicio = millis();
        const int codigo = http.POST(String(cuerpo));
        String texto;
        if (codigo >= 200 && codigo < 300) texto = http.getString();
        http.end();
        if (codigo < 200 || codigo >= 300) {
            static unsigned long ultimoError = 0;
            if (millis() - ultimoError >= 3000UL) {
                ultimoError = millis();
                Serial.printf("[GARITA] HTTP %d (sin autorizacion)\n", codigo);
            }
            return;
        }
        if (millis() - inicio > 1500UL || texto.length() > 160) return;
        ComunicacionServidor::RespuestasGarita r{};
        r.tomada = inicio;
        bool alguna = false;
        for (uint8_t z=0; z<2; ++z) {
            if (s.zona[z].id[0] == '\0') continue;
            const String prefijo(z == 0 ? "ENTRADA:" : "SALIDA:");
            for (int pos=0; pos < int(texto.length());) {
                const int fin = texto.indexOf('\n', pos);
                String linea = fin < 0 ? texto.substring(pos) : texto.substring(pos,fin);
                linea.trim();
                using F = ComunicacionServidor::FaseGarita;
                using A = ComunicacionServidor::Accion;
                if (s.zona[z].fase == F::PRESENCIA && linea == prefijo+"ABRIR:"+s.zona[z].id) {
                    r.zona[z].accion = A::ABRIR;
                } else if (z == 1 && s.zona[z].fase == F::PRESENCIA && linea == prefijo+"CANCELAR:"+s.zona[z].id) {
                    r.zona[z].accion = A::CANCELAR;
                } else if ((s.zona[z].fase == F::PASO || s.zona[z].fase == F::CANCELADA)
                        && linea == prefijo+"ACK:"+s.zona[z].id) {
                    r.zona[z].accion = A::ACK;
                }
                if (r.zona[z].accion != A::NINGUNA) {
                    snprintf(r.zona[z].id, sizeof(r.zona[z].id), "%s", s.zona[z].id);
                    alguna = true;
                    break;
                }
                if (fin < 0) break;
                pos = fin+1;
            }
        }
        if (alguna) xQueueOverwrite(colaRespuesta, &r);
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
        unsigned long ultimaGarita = 0;

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

            // Intercalar una consulta de garitas con el reporte de plazas.
            // Nunca ejecutar HTTP desde loop() ni enviar un estado local antiguo.
            if (ahora - ultimaGarita >= 250UL) {
                SnapshotGarita g{};
                if (xQueuePeek(colaGarita, &g, 0) == pdTRUE && ahora - g.tomada <= 500UL) {
                    sincronizarGaritas(g);
                }
                ultimaGarita = millis();
                ahora = millis();
                Snapshot reciente{};
                if (xQueueReceive(cola, &reciente, 0) == pdTRUE) {
                    actual = reciente;
                    haySnapshot = true;
                }
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
    colaGarita = xQueueCreate(1, sizeof(SnapshotGarita));
    colaRespuesta = xQueueCreate(1, sizeof(RespuestasGarita));
    if (!cola || !colaGarita || !colaRespuesta) {
        if (cola) vQueueDelete(cola);
        if (colaGarita) vQueueDelete(colaGarita);
        if (colaRespuesta) vQueueDelete(colaRespuesta);
        cola = colaGarita = colaRespuesta = nullptr;
        Serial.println("[RED] No se pudo crear la cola; barreras sin autorizacion.");
        return;
    }
    if (xTaskCreate(tareaRed, "parking-red", 8192, nullptr, 1, nullptr) != pdPASS) {
        vQueueDelete(cola);
        vQueueDelete(colaGarita);
        vQueueDelete(colaRespuesta);
        cola = colaGarita = colaRespuesta = nullptr;
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

void ComunicacionServidor::publicarGaritas(const Garita& entrada, const Garita& salida) {
    if (!colaGarita) return;
    SnapshotGarita s{};
    s.zona[0] = entrada;
    s.zona[1] = salida;
    s.tomada = millis();
    xQueueOverwrite(colaGarita, &s);
}

bool ComunicacionServidor::recibirGaritas(RespuestasGarita& respuesta) {
    return colaRespuesta && xQueueReceive(colaRespuesta, &respuesta, 0) == pdTRUE;
}
