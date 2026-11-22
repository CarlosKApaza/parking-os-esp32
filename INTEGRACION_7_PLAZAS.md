# Parking OS — ampliacion de P1-P7 (ESP32, borrador de integracion)

## Estado

La base de datos **ya tiene** P1-P7 con `sensorCodigo` IR01-IR07. El endpoint `POST /sensores/lecturas` de NestJS actualiza cada fila con ese codigo. **No crear mas registros ni editar manualmente sus estados.**

Esta rama extiende `src/main.cpp` para leer y reportar P3-P7 sin sustituir el proyecto: preserva P1, P2, entrada, salida, dos servos, LEDs y LDR.

**IMPORTANTE: no flashear este borrador sin identificar el cableado real de los cinco IR nuevos.** Los pines a continuacion son una propuesta, no la conexion confirmada de la maqueta.

## Pines del firmware

| Elemento | GPIO | Tipo | Estado |
|---|---:|---|---|
| P1 / IR01 | 35 | AO ADC1 analogico | confirmado previamente |
| P2 / IR02 | 32 | AO ADC1 analogico | confirmado previamente |
| P3 / IR03 | 13 | DO digital | **PROPUESTO, no verificado** |
| P4 / IR04 | 16 | DO digital | **PROPUESTO, no verificado** |
| P5 / IR05 | 17 | DO digital | **PROPUESTO, no verificado** |
| P6 / IR06 | 21 | DO digital | **PROPUESTO, no verificado** |
| P7 / IR07 | 22 | DO digital | **PROPUESTO, no verificado** |
| Garita entrada | 36 | AO ADC1 | existente |
| Garita salida | 33 | AO ADC1 | existente |
| LDR | 34 | AO ADC1 | existente |
| Servo entrada/salida | 19 / 18 | PWM | existente |
| LED entrada/salida | 14 / 27 | digital salida | existente |
| LED P1/P2 | 26 / 25 | digital salida | existente |
| Focos | 23 | digital salida | existente |

**ADC2 en ESP32 clasico no funciona para lectura analogica confiable durante Wi-Fi.** Usando P1, P2, garitas y LDR ya se ocupan 5 entradas ADC1 accesibles de un ESP32-WROOM habitual. P3-P7 no pueden conectarse todos como AO analogicos a ese mismo ESP32 sin hardware auxiliar.

Esta propuesta usa la señal **DO** de los modulos IR para las cinco nuevas plazas; **DO debe estar a 0-3.3 V**, nunca a 5 V directo. Si el modulo tiene 4 pines `VCC`, `GND`, `AO` y `DO`, conectar el **DO** a GPIO correspondiente. Alimentar a 3.3 V cuando el modulo funcione correctamente a esa tension, o emplear adaptación de nivel adecuada si el modulo requiere 5 V. Los modulos de tres pines pueden tener solo DO. Si se conectaron por AO, hay que rediseñar el hardware con un multiplexor o ADC externo, o usar otra placa.

No mezclar o duplicar pines con los que ya usan LDR, LEDs y servos. En modulos ESP32 con PSRAM, GPIO16/17 pueden estar reservados; verificar el modulo exacto.

## Activacion segura

1. Abrir `src/Pines.h` en VS Code.
2. Cambiar `PIN_SENSOR_P3` ... `PIN_SENSOR_P7` por los **GPIO reales**, tras identificarlos en la protoboard.
3. Confirmar que los sensores nuevos se conectan por **DO digital**, no por AO analogica.
4. Tras verificar cables y niveles, cambiar:

```cpp
constexpr bool HABILITAR_P3_P7 = true;
```

5. Si DO entrega `LOW` cuando detecta un coche, dejar `NIVEL_OCUPADO_DIGITAL = LOW`. Si invierte, usar `HIGH`.
6. Verificar en `src/main.cpp` el **SSID y la URL del backend**: deben corresponder al Wi-Fi de la maqueta y la IPv4 actual de la computadora que corre NestJS. No confundir el puerto `8765` de la camara con `3000` del backend.
7. Compilar y flashear con PlatformIO, y abrir el monitor serie a 115200.

```powershell
pio run
pio run --target upload
pio device monitor -b 115200
```

Alternativa: botones **Build**, **Upload**, **Monitor** de PlatformIO en VS Code.

## Comprobaciones

- Con un solo coche de prueba sobre P1 a P7, comprobar `OCUPADO` en serial **solo** en esa plaza; al retirarlo deberia volver a `LIBRE`.
- Verificar `POST IR01` a `POST IR07` con HTTP 2xx. El primer mensaje y cambios se envian al confirmar 3 muestras; despues cada 30 segundos.
- En phpMyAdmin comprobar `estacionamientos` por su `codigo`: `ultimaLectura` ya no debe ser NULL para P4-P7, y `fechaCambio` se actualiza. PWA muestra siete estados actualizados al refrescar.
- Si una plaza indica lo contrario, comprobar el nivel activo DO o ajustar su potenciometro, y repetir.
- Si HTTP no conecta, verificar IP actual de la PC, Wi-Fi del ESP32 y que NestJS escuche en la LAN (no solo 127.0.0.1).

**No se modifico el control de garitas**: en este firmware las barreras se abren por sus IR. La autorizacion por OCR y pago es una fase posterior; evitar usarlo como control de acceso real hasta cambiar esa logica.
