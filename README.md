# EVA1 – Monitoreo de temperatura / humedad con activación de riego

**Aplicaciones Móviles para IoT (TI3V42) – INACAP**

El repositorio tiene dos versiones del mismo prototipo. Las dos usan el mismo protocolo de mensajes, las mismas medidas de seguridad y los mismos modos de operación (AUTO, MANUAL y SEGURO). Solo cambian la placa y la forma de comunicarse.

| Versión | Hardware | Comunicación inalámbrica | Cómo se prueba | Carpeta |
|---|---|---|---|---|
| **ESP32 + WiFi** (versión entregada) | 2 × ESP32, DHT22, potenciómetro como sensor de suelo, relé y LEDs | WiFi + MQTT (`broker.hivemq.com`) | Simulación en **Wokwi** | [esp32_wokwi/](esp32_wokwi/) |
| **Arduino UNO + Bluetooth** | 2 × Arduino UNO, 2 × HC-05, DHT22, sensor capacitivo de suelo, relé y bomba | Bluetooth SPP (HC-05 maestro/esclavo) | Placas físicas | [arduino_uno_hc05/](arduino_uno_hc05/) |

Cada carpeta tiene su propio README con todas las secciones que pide la evaluación: integrantes, componentes, diagrama, formato de mensajes, brechas de seguridad, prácticas OT y video.

La clave compartida y los datos de WiFi van en `secretos.h`, que está en [.gitignore](.gitignore). Solo se publican los archivos `secretos.example.h`.
