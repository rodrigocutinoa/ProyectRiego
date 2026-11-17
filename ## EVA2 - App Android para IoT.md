## EVA2 - App Android para IoT con conexión inalámbrica
# --------------------------------------------------

# Estudiantes
- Jenniffer Gonzalez
- Rodrigo Cutiño

# RiegoIoT-App: monitoreo y control del riego desde Android

App Android (Kotlin y Jetpack Compose) para el sistema de riego de la Unidad 1. Un nodo mide la humedad del suelo y la temperatura y controla la bomba; el panel lo monitorea y lo controla por Wi-Fi con MQTT. La app permite:

- Iniciar sesión con usuario y contraseña, con roles operador y observador.
- Ver en vivo la humedad, la temperatura, el estado de la bomba y el modo (AUTO, MANUAL o SEGURO).
- Recibir una notificación cada vez que el riego se enciende o se apaga.
- Encender o apagar la bomba y cambiar el modo y el umbral a distancia.
- Pasar a un estado seguro, con la bomba apagada, si se corta el enlace.

El lado IoT se simula con hertzbench, conectado por MCP: broker MQTT, canal Wi-Fi y fallas.

# Github
- App: https://github.com/rodrigocutinoa/RiegoIoT-App
- Definición y diseño (Hitos 1 y 2): https://github.com/rodrigocutinoa/RiegoIoT-App/blob/main/docs/HITOS_1_2.md
- Proyecto común (EVA1 y simulación): https://github.com/rodrigocutinoa/ProyectRiego
