# Integracion PCB - etapa 1

Base: firmware funcional af9d22e. No se cambian Pines.h, calibracion,
drivers, indicadores ni control local de barreras. Iluminacion inactiva.

1. Actualizar backend feat/gestion-estacionamiento a 25b19a9 o posterior.
2. Generar clave aleatoria (32 bytes), configurarla en .env del backend
   como ESP32_API_KEY y dejar SENSORES_PERMITIR_SIMULACION=0.
3. Copiar include/ConfigServidor.example.h a include/ConfigServidor.h.
   Configurar WiFi 2.4 GHz, URL LAN del PC y la misma clave.
   No publicar ConfigServidor.h.
4. Iniciar MySQL y NestJS; permitir TCP 3000 en red privada si hace falta.
5. Compilar y cargar con PlatformIO; abrir monitor serie a 115200.
   Mantener todas las plazas y garitas libres al arrancar/calibrar.
6. Verificar HTTP 201 para IR01 a IR07 y GET /estacionamientos.
   Colocar y retirar un auto en cada plaza: PWA y MySQL deben actualizarse.
   Comprobar eventos_estacionamiento.origen = SENSOR_IR cuando cambie estado.

WiFi y HTTP se ejecutan en una tarea FreeRTOS aparte. Se envia el ultimo
estado calibrado, al cambiar, tras reconectar y cada REPORTE_MS (30 s).
No se guardan transiciones intermedias durante desconexion: se reconcilia
el ultimo estado. No se reportan plazas sin calibracion valida.
Las barreras conservan su apertura local por presencia: NO autorizan
acceso por servidor ni validan OCR o pagos en esta etapa.
HTTP sin TLS es solo para la red privada de demostracion.
