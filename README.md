# EVA1 – Monitoreo de temperatura / humedad con activación de riego

**Aplicaciones Móviles para IoT (TI3V42) – INACAP**

El repositorio tiene dos versiones del mismo prototipo. Las dos usan el mismo protocolo de mensajes, las mismas medidas de seguridad y los mismos modos de operación (AUTO, MANUAL y SEGURO). Solo cambian la placa y la forma de comunicarse.

| Versión | Hardware | Comunicación inalámbrica | Cómo se prueba | Carpeta |
|---|---|---|---|---|
| **ESP32 + WiFi** (versión entregada) | 2 × ESP32, DHT22, potenciómetro como sensor de suelo, relé y LEDs | WiFi + MQTT (`broker.hivemq.com`) | Simulación en **Wokwi** | [esp32_wokwi/](esp32_wokwi/) |
| **Arduino UNO + Bluetooth** | 2 × Arduino UNO, 2 × HC-05, DHT22, sensor capacitivo de suelo, relé y bomba | Bluetooth SPP (HC-05 maestro/esclavo) | Placas físicas | [arduino_uno_hc05/](arduino_uno_hc05/) |

Cada carpeta tiene su propio README con todas las secciones que pide la evaluación: integrantes, componentes, diagrama, formato de mensajes, brechas de seguridad, prácticas OT y video.

La clave compartida y los datos de WiFi van en `secretos.h`, que está en [.gitignore](.gitignore). Solo se publican los archivos `secretos.example.h`.

## EVA2 (en curso)

App Android para monitorear y controlar el riego, en un repositorio aparte: **[RiegoIoT-App](https://github.com/rodrigocutinoa/RiegoIoT-App)**. Este repositorio tiene la simulación del lado IoT con [hertzbench](https://github.com/Rixmerz/hertzbench) y la bitácora.

| Hito | Estado |
|---|---|
| 0 a 2 | Proyecto Android, definición y diseño ([HITOS_1_2.md](https://github.com/rodrigocutinoa/RiegoIoT-App/blob/main/docs/HITOS_1_2.md)) |
| 3 | Conexión MQTT, roles Nodo y Panel, comandos con confirmación y notificación al encender o apagar el riego |
| 4 | Mensajes cifrados con AES-GCM y código de vinculación |
| 5 y 6 | Login con SQLite, PBKDF2 y roles operador y observador; historial |
| 7 | Medidas ISO/IEC 27400 y OT, y **pruebas P1–P14: 14 de 14 pasan**, con capturas ([tabla](https://github.com/rodrigocutinoa/RiegoIoT-App#pruebas)) |
| 8 | Pendiente: guion de la demostración |

En `hertzbench/` están:

| Archivo | Para qué |
|---|---|
| `riego-mqtt.yaml` | Broker MQTT y nodo de riego simulado, que publica mensajes cifrados con el código `123456`. Sirve para probar la app como Panel |
| `riego-broker.yaml` | Broker y un monitor suscrito a `riego/#`. Sirve para probar la app como Nodo |
| `cifrar.ts` | Cifra un mensaje igual que la app, para publicarlo con `mqtt_publish`: `deno run hertzbench/cifrar.ts 123456 riego/cmd "CMD;bomba;ON"` |
| `preparar-mac.sh`, `probar-puente.sh` | Preparar un Mac (Deno, mosquitto, hertzbench, Android Studio) y probar el puente MQTT |
| `reportes/` | Reportes enviados al profesor en el repositorio de hertzbench |

El estado y los próximos pasos están en [docs/RETOMAR.md](docs/RETOMAR.md), y la bitácora en [docs/EVA2_AVANCE.md](docs/EVA2_AVANCE.md).

> **Para seguir en el Mac nuevo:**
>
> ```bash
> git clone https://github.com/rodrigocutinoa/ProyectRiego.git ~/Dev/Inacap/AppIoT/ProyectRiego
> cd ~/Dev/Inacap/AppIoT/ProyectRiego
> hertzbench/preparar-mac.sh
> code .
> ```
>
> Al abrir Claude Code, aprobar el servidor `hertzbench` y escribirle: *"lee docs/RETOMAR.md y sigamos"*.
