# Garitas autorizadas

Requiere NestJS con GARITAS_MODO_AUTORIZADO=1. Se deriva /garitas/sincronizar desde SERVIDOR_URL existente; no cambiar su sufijo /sensores/lecturas. Mantener ConfigServidor.h local e ignorado. Restaurada la plantilla sin credenciales; las claves anteriores siguen en el historial y deben rotarse.

GPIO, drivers, calibracion y angulos de Jhonny intactos. Reiniciar con plazas y garitas vacias. IR por si solo no abre: espera permiso de NestJS para el ID actual. Si el objeto se retira antes de abrir se cancela. Si se abre, conserva cierre tras 3 segundos sin presencia; el paso se infiere del cierre, no es un sensor de direccion. Sin red, no abre una barrera cerrada; una abierta conserva la proteccion local. Una confirmacion pendiente se reintenta mientras el ESP32 siga encendido; si se apaga antes del ACK revisar visitas pendientes en NestJS.

Prueba en maqueta supervisada: tapar IR sin placa -> cerrada. Mantener IR y mostrar placa -> abrir si autorizada. Retirar -> cerrar en 3 s. Verificar BD. Salida requiere pago o modo demo explicito de backend.
