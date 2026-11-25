#ifndef PINES_H
#define PINES_H

#include <Arduino.h>

// ==============================================
// 1. REGISTRO 74HC595 - INDICADORES DE POSICION
// ==============================================

constexpr uint8_t PIN_74HC595_DS = 16;
constexpr uint8_t PIN_74HC595_ST = 17;
constexpr uint8_t PIN_74HC595_SH = 5;

constexpr uint8_t CANTIDAD_INDICADORES = 7;

// ==============================================
// 2. INDICADORES LED DIRECTOS
// ==============================================

constexpr uint8_t PIN_LED_ENTRADA = 14;
constexpr uint8_t PIN_LED_SALIDA  = 13;
constexpr uint8_t PIN_LED_ALARMA  = 19;

// ==============================================
// 3. CONTROL DE ILUMINACION
// ==============================================

constexpr uint8_t PIN_ACTIVAR_LEDS_IR      = 18;
constexpr uint8_t PIN_SENAL_ENCENDER_FOCOS = 4;

// ==============================================
// 4. SENSORES DE GARITA Y LUZ
// ==============================================

constexpr uint8_t PIN_SENSOR_ENTRADA = 36;
constexpr uint8_t PIN_SENSOR_SALIDA  = 39;
constexpr uint8_t PIN_SENSOR_LUZ     = 34;

constexpr float UMBRAL_NOCHE_V = 0.5f;

// ==============================================
// 5. MULTIPLEXOR 74HC4051 - SENSORES DE POSICION
// ==============================================

constexpr uint8_t PIN_SELECT0 = 25;  // S0: bit menos significativo
constexpr uint8_t PIN_SELECT1 = 33;  // S1
constexpr uint8_t PIN_SELECT2 = 32;  // S2

// Salida comun Z del 74HC4051 conectada a GPIO35 del ESP32.
constexpr int PIN_ADC_MUX_Z = 35;

// Por plaza: 500 ms con IR encendido + 500 ms con IR apagado.
constexpr unsigned long TIEMPO_FASE_CALIBRACION_PLAZA_MS = 500;

// Espera despues de seleccionar el canal y antes de leer el ADC.
constexpr unsigned int ESTABILIZACION_MUX_US = 2;

// ==============================================
// 6. SERVOMOTORES DE BARRERAS
// ==============================================

constexpr uint8_t PIN_SERVO_ENTRADA = 26;
constexpr uint8_t PIN_SERVO_SALIDA  = 27;

constexpr int ANGULO_ARRIBA_ENTRADA = 50;   // Abierta
constexpr int ANGULO_ABAJO_ENTRADA  = 140;  // Cerrada
constexpr int ANGULO_ARRIBA_SALIDA  = 90;
constexpr int ANGULO_ABAJO_SALIDA   = 0;

// ==============================================
// 7. BUZZER
// ==============================================

constexpr uint8_t PIN_BUZZER = 23;

// ==============================================
// 8. COMUNICACION I2C
// ==============================================

constexpr uint8_t PIN_I2C_SDA = 22;
constexpr uint8_t PIN_I2C_SCL = 21;

// ==============================================
// 9. TIEMPOS DE FUNCIONAMIENTO
// ==============================================

constexpr unsigned long TIEMPO_CIERRE_MS               = 3000;
constexpr unsigned long REPORTE_MS                     = 30000;
constexpr unsigned long REINTENTO_MS                   = 5000;
constexpr unsigned long INTERVALO_GARITA_MS            = 100;
constexpr unsigned long INTERVALO_ESTACIONAMIENTOS_MS  = 500;
constexpr unsigned long INTERVALO_IMPRESION_MS         = 500;
constexpr unsigned long INTERVALO_MUESTREO_SENSORES_MS = 3000;
constexpr unsigned long INTERVALO_RECONEXION_WIFI_MS   = 10000;

#endif