# Tiempo de espera entre mediciones (en microsegundos)

## SLEEP_DURATION 
La notación 10e6 que estás usando significa 10 millones de microsegundos, lo que equivale a 10 segundos (1 segundo = 1,000,000 microsegundos). Para hacer que el ESP32 entre en modo deep sleep durante 2 minutos, debes calcular la cantidad de microsegundos que hay en 2 minutos:

- 1 minuto = 60 segundos
- 2 minutos = 120 segundos
- 120 segundos = 120 * 1,000,000 microsegundos = 120,000,000 microsegundos

Cambiar este datoa en caso de querer más tiempo de espera, este calculo es para el programa SDC30-MQTT-DSM
