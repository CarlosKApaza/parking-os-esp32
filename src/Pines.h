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

// Propuesta SOLO si P3-P7 estan conectados por salida digital DO (3.3 V).
// IMPORTANTE: no conocemos aun los GPIO reales de tu maqueta.
// Cambiar estos cinco pines segun el cableado fisico ANTES de habilitar.
constexpr uint8_t PIN_SENSOR_P3 = 13;
constexpr uint8_t PIN_SENSOR_P4 = 16;
constexpr uint8_t PIN_SENSOR_P5 = 17;
constexpr uint8_t PIN_SENSOR_P6 = 21;
constexpr uint8_t PIN_SENSOR_P7 = 22;

// Seguro por defecto: no reportar ocupacion falsa mientras no se confirme
// que P3-P7 usan esos GPIO y salida digital DO.
constexpr bool HABILITAR_P3_P7 = false;

// Modulos IR de obstaculos DO habituales: LOW = detecta.
// Si tus modulos funcionan al reves, cambiar a HIGH.
constexpr uint8_t NIVEL_OCUPADO_DIGITAL = LOW;

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
