#pragma once
// Copiar este archivo como ConfigServidor.h y completar en la PC local.
// No usar 127.0.0.1: el ESP32 necesita la IP LAN del PC con NestJS.
constexpr char WIFI_SSID[] = "NOMBRE_WIFI";
constexpr char WIFI_PASSWORD[] = "CONTRASENA_WIFI";
constexpr char SERVIDOR_URL[] = "http://192.168.1.11:3000/sensores/lecturas";
constexpr char ESP32_API_KEY[] = "REEMPLAZAR_CON_LA_CLAVE_DEL_BACKEND";
