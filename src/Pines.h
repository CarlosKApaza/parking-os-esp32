#ifndef PINES_H
#define PINES_H

#include <Arduino.h>

// Indicadores LED
constexpr uint8_t PIN_LED_ENTRADA_COCHE = 14;
constexpr uint8_t PIN_LED_SALIDA_COCHE  = 27;
constexpr uint8_t PIN_LED_P1_OCUPADO    = 26;
constexpr uint8_t PIN_LED_P2_OCUPADO    = 25;

// ======================================
// ILUMINACIÓN AUTOMÁTICA
// ======================================

constexpr uint8_t PIN_LDR = 34;
constexpr uint8_t PIN_FOCOS = 23;

constexpr float UMBRAL_NOCHE_V = 0.5f;

// Sensores IR de los espacios de estacionamiento
constexpr uint8_t PIN_SENSOR_P1 = 35;
constexpr uint8_t PIN_SENSOR_P2 = 32;

// Sensores IR de las garitas
constexpr uint8_t PIN_SENSOR_ENTRADA = 36;
constexpr uint8_t PIN_SENSOR_SALIDA = 33;

// Servomotores de las barreras
constexpr uint8_t PIN_SERVO_ENTRADA = 19;
constexpr uint8_t PIN_SERVO_SALIDA = 18;

// Ángulos de la barrera de entrada
constexpr int ANGULO_ARRIBA_ENTRADA = 0;
constexpr int ANGULO_ABAJO_ENTRADA = 90;

// Ángulos de la barrera de salida
constexpr int ANGULO_ARRIBA_SALIDA = 90;
constexpr int ANGULO_ABAJO_SALIDA = 0;

// Tiempos de funcionamiento
constexpr unsigned long TIEMPO_CIERRE_MS = 2000;
constexpr unsigned long REPORTE_MS = 30000;
constexpr unsigned long REINTENTO_MS = 5000;
constexpr unsigned long INTERVALO_GARITA_MS = 100;
constexpr unsigned long INTERVALO_ESTACIONAMIENTOS_MS = 500;
constexpr unsigned long INTERVALO_IMPRESION_MS = 500;
constexpr unsigned long INTERVALO_RECONEXION_WIFI_MS = 10000;

#endif
