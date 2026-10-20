# EVA1 – Prototipo IoT con conexión inalámbrica (ESP32 + WiFi, simulado en Wokwi)

**Aplicaciones Móviles para IoT (TI3V42) – INACAP**

## 1. Integrantes y caso elegido

| Integrante | Correo |
|---|---|
| _Nombre Apellido 1_ | _correo_ |
| _Nombre Apellido 2_ | _correo_ |

**Caso elegido:** Monitoreo de temperatura / humedad, con activación de riego.

Un **nodo sensor** (ESP32 1) mide la humedad del suelo y la temperatura y humedad del aire. Cuando la humedad del suelo baja del umbral, enciende la bomba de riego (modo automático). Publica cada lectura por **WiFi** usando el protocolo **MQTT**. Un **nodo de control** (ESP32 2) recibe las lecturas, las interpreta y las muestra. Desde ahí se cambia el umbral y se enciende o apaga la bomba a distancia (modo manual).

Como no tenemos placas físicas, el prototipo se **simula en [Wokwi](https://wokwi.com)**. Cada nodo es un proyecto aparte y los dos corren a la vez en dos pestañas del navegador. El ESP32 simulado se conecta a internet por la red virtual `Wokwi-GUEST`, así que los nodos se comunican por WiFi real a través de un broker MQTT público.

> Para el riego, "superar el umbral" significa que la **sequedad** del suelo supera lo permitido: la bomba se enciende cuando `humedad_suelo < umbral` y se apaga cuando `humedad_suelo ≥ umbral + 5 %` (histéresis).

### Estructura

```
esp32_wokwi/
├── nodo_sensor/
│   ├── nodo_sensor.ino      Código del ESP32 1 (en Wokwi va en sketch.ino)
│   ├── diagram.json         Circuito de Wokwi
│   ├── libraries.txt        DHT sensor library, Adafruit Unified Sensor, PubSubClient
│   └── secretos.example.h   Plantilla de la clave, el WiFi y el id del grupo
├── nodo_control/
│   ├── nodo_control.ino     Código del ESP32 2
│   ├── diagram.json
│   ├── libraries.txt        PubSubClient
│   └── secretos.example.h
└── README.md
```

### Cómo correr la simulación en Wokwi

Repetir estos pasos para **cada nodo**:

1. Entrar a <https://wokwi.com/projects/new/esp32> (conviene iniciar sesión para guardar el proyecto).
2. En la pestaña `sketch.ino`, borrar el contenido y pegar `nodo_sensor.ino` (o `nodo_control.ino`).
3. En la pestaña `diagram.json`, borrar el contenido y pegar el `diagram.json` del nodo.
4. Con el botón **"+" → New file…** crear `libraries.txt` y pegar su contenido. También se pueden agregar las librerías desde *Library Manager*.
5. Crear también el archivo **`secretos.h`** con el contenido de `secretos.example.h`. Pongan su propia clave y un `ID_GRUPO` único (por ejemplo `grupo-perez-soto`), **iguales en los dos nodos**. Si no lo crean, los dos nodos usan el topic de ejemplo y pueden mezclarse con los de otro grupo.
6. Presionar ▶ **Start simulation**. Debería aparecer `[WIFI] Conectado` y luego `[MQTT] ... OK`.

Con los dos proyectos abiertos en pestañas o ventanas lado a lado:
- **Nodo sensor:** hacer clic en el **DHT22** para cambiar la temperatura y la humedad. Girar el **potenciómetro** para cambiar la humedad del suelo: a la izquierda es seco, a la derecha es mojado, y en un extremo simula el sensor desconectado.
- **Nodo de control:** escribir los comandos en el monitor serial (`ayuda`, `umbral 60`, `on`, `off`, `auto`, `prueba`).

> Si el navegador detiene la simulación de una pestaña en segundo plano, usen **dos ventanas** del navegador una al lado de la otra en lugar de pestañas.

---

## 2. Tabla de componentes

En Wokwi el sensor de suelo y la bomba se representan con un potenciómetro y un LED. La tabla describe los componentes reales del prototipo y entre paréntesis lo que se usa en la simulación.

| Componente | Cant. | Voltaje | Interfaz / protocolo | Rango / alcance | Por qué sirve para el caso |
|---|---|---|---|---|---|
| **ESP32 DevKitC V4** (ESP32-WROOM-32, doble núcleo de 240 MHz) | 2 | 3,3 V lógica; 5 V por USB | WiFi 802.11 b/g/n de 2,4 GHz, Bluetooth 4.2 / BLE, UART, I2C, SPI; ADC de 12 bits | WiFi: ~50 m en interior y ~100 m o más en exterior, según el router | Trae WiFi integrado, así que no necesita un módulo externo. Puede usar TLS y MQTT, que es el protocolo estándar de IoT. Guarda el umbral en la flash (`Preferences`). Su ADC de 12 bits da buena resolución para la humedad del suelo. |
| **WiFi + broker MQTT** (`broker.hivemq.com`) | — | — | MQTT 3.1.1 sobre TCP, puerto 1883 (8883 con TLS) | Internet: alcance global | Comunica los dos nodos por publicación y suscripción, sin que tengan que conocer sus direcciones IP. Es lo que usa la industria para telemetría y lo que se usará con la app Android de la EVA2. |
| **Sensor capacitivo de humedad de suelo v1.2** (potenciómetro en la simulación) | 1 | 3,3–5,5 V | Analógica (~1,2–2,5 V) al **GPIO34**, que pertenece al ADC1 | 0–100 % de humedad del sustrato (se calibra en seco y en agua) | Es el "sensor subterráneo" del caso. Al ser capacitivo, no se corroe. Se conecta a un pin del ADC1 porque el ADC2 no funciona con el WiFi activo. |
| **DHT22 (AM2302)** | 1 | 3,3–6 V | Digital, un solo cable (protocolo propio), GPIO15 | −40 a 80 °C (±0,5 °C); 0–100 % HR (±2 %); 1 lectura cada 2 s | Mide la temperatura y la humedad del aire que pide el caso. Funciona directo a 3,3 V. |
| **Módulo relé 1 canal** | 1 | Bobina 5 V; contacto 10 A 250 VAC / 30 VDC | GPIO26 digital | ON/OFF | Aísla el ESP32 de la corriente de la bomba. |
| **Bomba sumergible DC 3–6 V** (LED azul en la simulación) | 1 | 3–6 V, ~130–220 mA | ON/OFF a través del relé | ~80–120 L/h | Es el actuador de riego. Se alimenta aparte del ESP32. |
| **LEDs + resistencias de 220 Ω** | 4 | 2–3,2 V, ~10 mA | GPIO digital | — | Nodo sensor: LED de falla (GPIO4). Nodo de control: alarma, enlace y bomba (GPIO25, 26 y 27). |

---

## 3. Diagrama de conexión

> _Agregar capturas de los dos circuitos en Wokwi: `docs/wokwi_nodo_sensor.png` y `docs/wokwi_nodo_control.png`, y el link a cada proyecto._

- Proyecto Wokwi del nodo sensor: _https://wokwi.com/projects/..._
- Proyecto Wokwi del nodo de control: _https://wokwi.com/projects/..._

### Nodo sensor (ESP32 1)

| Componente | Pin del componente | Pin del ESP32 |
|---|---|---|
| DHT22 | VCC / SDA / GND | 3V3 / GPIO15 / GND |
| Potenciómetro (suelo) | VCC / SIG / GND | 3V3 / GPIO34 / GND |
| Relé | VCC / IN / GND | 5V / GPIO26 / GND |
| Relé (contactos) | COM / NO | 3V3 / resistencia 220 Ω → LED azul "BOMBA" → GND |
| LED rojo "FALLA" | A / C | GPIO4 / resistencia 220 Ω → GND |

### Nodo de control (ESP32 2)

| Componente | Pin del ESP32 |
|---|---|
| LED rojo ALARMA + 220 Ω | GPIO25 |
| LED verde ENLACE + 220 Ω | GPIO26 |
| LED azul BOMBA + 220 Ω | GPIO27 |

### Arquitectura de comunicación

```
 ESP32 NODO SENSOR                     Broker MQTT                    ESP32 NODO DE CONTROL
┌──────────────────┐ publica  .../telemetria ($DAT, $ACK, $NAK) ┌───────────────────────┐
│ DHT22, suelo,    │────────────────▶┌──────────────┐─────────────▶│ Monitor serial + LEDs │
│ relé + bomba     │                 │ broker.      │              │ El operador escribe   │
│                  │◀────────────────│ hivemq.com   │◀─────────────│ los comandos          │
└──────────────────┘ suscrito .../comandos ($CMD)  └──────────────┘ publica                └───────────────────────┘
          └── .../estado  ONLINE / OFFLINE (retenido, "última voluntad" del nodo sensor)
```

Topic base: `inacap/ti3v42/riego/<ID_GRUPO>`

---

## 4. Formato del mensaje serial

El contenido de cada mensaje MQTT es una trama de texto ASCII al estilo NMEA. Es el mismo formato que usa la versión con Bluetooth:

```
$<TIPO>,<campo1>,<campo2>,...*<FIRMA>
```

- `$` marca el inicio. El largo máximo es de 64 caracteres después del `$`.
- `FIRMA`: 4 dígitos hexadecimales. Se calcula con FNV-1a de 32 bits sobre la **clave compartida + el contenido entre `$` y `*`**, plegado a 16 bits. La clave **no viaja** en el mensaje.
- Solo se aceptan mayúsculas, dígitos, `,`, `.`, `-` y `*`.

### Lectura (nodo sensor → `…/telemetria`, cada 2 s)

`$DAT,<n>,<temp>,<hr_aire>,<suelo>,<umbral>,<modo>,<bomba>,<fallas>,<rechazos>*FIRMA`

| Campo | Unidad / valores | Descripción |
|---|---|---|
| `n` | entero | Número correlativo de la lectura |
| `temp` | °C, 1 decimal (`NA` si falla) | Temperatura del aire |
| `hr_aire` | % (`NA` si falla) | Humedad relativa del aire |
| `suelo` | % (0–100) | Humedad del suelo |
| `umbral` | % (10–90) | Umbral de activación actual |
| `modo` | `A` automático, `M` manual, `S` seguro | Modo de operación |
| `bomba` | `1` encendida, `0` apagada | Estado del actuador |
| `fallas` | máscara de bits: 1 = DHT22, 2 = sensor de suelo, 4 = tiempo máximo de riego | Diagnóstico |
| `rechazos` | entero | Comandos rechazados desde el arranque |

**Ejemplo** (con la clave de ejemplo `CAMBIAR_ESTA_CLAVE`):

```
$DAT,42,23.5,55,31,40,A,1,0,0*F7A2
```
→ 23,5 °C, 55 % HR del aire, suelo al 31 %, umbral de 40 %, modo automático, bomba encendida, sin fallas, 0 rechazos.

Así se ve en el monitor del nodo de control:
```
[LECTURA #42] Temp: 23.5 C | HR aire: 55 % | Suelo: 31 % | Umbral: 40 % | Modo: AUTO | Bomba: ENCENDIDA | Fallas: ninguna | Rechazos: 0
```

### Comandos (nodo de control → `…/comandos`)

`$CMD,<seq>,<COMANDO>[,<VALOR>]*FIRMA`

| Comando | Valor | Efecto | Escrito en la consola |
|---|---|---|---|
| `PING` | — | Mantiene vivo el enlace (cada 2 s, automático) | — |
| `UMBRAL` | `10`–`90` | Cambia el umbral y lo guarda en la flash | `umbral 35` |
| `BOMBA` | `ON` / `OFF` | Control manual (pasa a modo MANUAL) | `on` / `off` |
| `MODO` | `AUTO` / `MANUAL` | Cambia el modo de operación | `auto` / `manual` |

`seq` es un número de secuencia creciente. El nodo sensor rechaza cualquier valor menor o igual al último que aceptó.

**Respuestas del nodo sensor** (en `…/telemetria`):
- Aceptado: `$ACK,<seq>,<COMANDO>,<VALOR>*FIRMA`
- Rechazado: `$NAK,<seq>,<MOTIVO>*FIRMA`. Los motivos son `FORMATO`, `FIRMA`, `SECUENCIA`, `COMANDO`, `VALOR`, `RANGO`, `LARGO`, `LIMITE` y `BLOQUEADO`. Si el comando no se pudo leer, `seq` vale 0.

**Ejemplos:**

```
$CMD,1007,UMBRAL,35*D902     → cambiar el umbral a 35 %
$ACK,1007,UMBRAL,35*0AEB     ← confirmado

$CMD,1008,BOMBA,ON*9651      → encender la bomba en manual
$ACK,1008,BOMBA,ON*262A      ← confirmado

$CMD,1010,UMBRAL,99*AF3B     → umbral fuera de rango
$NAK,1010,RANGO*71AF         ← rechazado
```

---

## 5. Brechas de seguridad y medidas aplicadas

| # | Brecha del prototipo | Cómo se aprovecharía | Consecuencia en el riego | Medida aplicada | Dónde está en el código |
|---|---|---|---|---|---|
| 1 | **Broker MQTT público y sin autenticación**: cualquiera puede publicar en nuestro topic | El atacante se conecta a `broker.hivemq.com` con MQTTX o el cliente web de HiveMQ, se suscribe a `inacap/#` para descubrir el topic y publica `BOMBA,ON`. | Puede encender la bomba (inunda el cultivo, gasta agua, pudre las raíces) o apagarla (el cultivo se seca). | Cada comando debe llevar una **firma con clave compartida** y sin ella se rechaza (`NAK FIRMA`). El nodo de control también descarta la telemetría sin firma válida. El `ID_GRUPO` del topic no está en el repositorio. | `calcularFirma()` y el paso 3 de `procesarTrama()` en `nodo_sensor.ino`; `procesarTrama()` en `nodo_control.ino` |
| 2 | **Comandos aceptados sin validar** | Se publican textos mal formados, campos de más, mensajes enormes o un umbral de 0 o 255. | Con umbral 0 nunca se riega y con 255 se riega siempre. Un mensaje enorme podría desbordar el búfer. | Lista blanca de comandos, número exacto de campos, solo dígitos, **rango 10–90**, solo caracteres permitidos, **largo máximo de 64** y rechazo de bytes nulos. El nodo de control también valida antes de enviar. | `alRecibirMqtt()`, `procesarTrama()`, `esNumero()`, `caracteresValidos()` y `dividirCampos()` en `nodo_sensor.ino`; `cambiarUmbral()` en `nodo_control.ino` |
| 3 | **Canal sin cifrar y ataque de repetición** (*replay*) | Cualquier suscriptor ve nuestras tramas. Puede copiar un `$CMD,...,BOMBA,ON*...` válido y volver a publicarlo después, sin conocer la clave. | La bomba se enciende cuando nadie lo pidió. | **Número de secuencia creciente**: se rechaza cualquier `seq` ya usado. El nodo de control guarda la secuencia en la flash por bloques, así que nunca la repite aunque se reinicie. Opcionalmente, **TLS** (`USAR_TLS 1`, puerto 8883) cifra el canal. | Paso 5 de `procesarTrama()` en `nodo_sensor.ino`; `iniciarSecuencia()` y `siguienteSecuencia()` en `nodo_control.ino`; `#define USAR_TLS` en los dos |
| 4 | **Pérdida de WiFi o del broker, o inundación de mensajes** | Se cae el router o el broker, o un atacante publica cientos de mensajes por segundo justo después de dejar la bomba en manual ON. | La bomba queda encendida sin supervisión hasta vaciar el estanque o inundar. | **Falla segura**: si pasan 10 s sin un mensaje válido, entra en modo SEGURO con la bomba apagada. Hay un **límite de 8 tramas por segundo**, un **tiempo máximo de riego** de 60 s con 30 s de descanso y una reconexión que no bloquea el control local. | `vigilarEnlace()`, `limiteDeTramasOk()`, `vigilarBomba()` y `gestionarConexion()` en `nodo_sensor.ino` |
| 5 | **Sensor desconectado o dañado** | Se corta el cable del sensor de suelo. | Si se leyera 0 % de forma permanente, el riego sería continuo. | Se detectan lecturas fuera de rango (bit de falla 2) y en ese caso el modo automático no riega. En Wokwi se prueba girando el potenciómetro al extremo. | `leerSensores()` y `controlAutomatico()` en `nodo_sensor.ino` |
| 6 | **Claves y datos de WiFi en el repositorio público** | Alguien lee la clave y el topic en GitHub y con ellos firma comandos válidos. | Se anulan las medidas 1 y 3. | La clave, el WiFi y el `ID_GRUPO` están en `secretos.h`, que está en `.gitignore`. Solo se publica `secretos.example.h`. | `.gitignore`; `#include "secretos.h"` en los dos sketches |

**Registro de rechazos:** cada trama rechazada responde con `$NAK` y queda registrada en el monitor serial del nodo sensor con su hora (millis), el motivo y la trama completa. Además se cuenta en el campo `rechazos` de cada lectura (`rechazar()` en `nodo_sensor.ino`).

**Demostración en el video:**
- En el nodo de control se escribe `prueba`. Se envían 4 tramas inválidas (firma alterada, `UMBRAL,99`, comando `ABRIR` y una secuencia repetida) y el nodo sensor responde a cada una con `NAK` y el motivo (`FIRMA`, `RANGO`, `COMANDO` y `SECUENCIA`).
- **Ataque externo (opcional):** desde [MQTTX](https://mqttx.app) o el cliente web de HiveMQ, publicar `$CMD,999999,BOMBA,ON*0000` en `inacap/ti3v42/riego/<ID_GRUPO>/comandos` → el nodo sensor responde `NAK FIRMA`.

**Limitaciones conocidas:** la firma FNV-1a de 16 bits protege la integridad y exige conocer la clave, pero no es criptográficamente robusta. En producción se usaría HMAC-SHA256 (el ESP32 lo trae en *mbedTLS*), un broker privado con usuario y contraseña, y TLS validando el certificado del broker. Con `setInsecure()` el canal va cifrado, pero no se valida el certificado del servidor.

---

## 6. Prácticas de la industria OT aplicadas

| Práctica | Cómo se aplicó | Por qué |
|---|---|---|
| **ISA/IEC 62443-3-3, SR 3.5 – Validación de entradas** | Cada comando se revisa en formato, campos, largo, rango y lista blanca antes de ejecutarse. | Un dispositivo de campo nunca debe ejecutar una orden mal formada o fuera de límites. |
| **IEC 62443, SR 3.1 – Integridad de la comunicación** y **SR 1.2 – Autenticación de dispositivos** | Firma con clave compartida en todas las tramas, en los dos sentidos. TLS como opción. | En un broker público solo el nodo de control legítimo puede dar órdenes. |
| **IEC 62443, SR 4.1 – Confidencialidad** | `USAR_TLS 1` cifra el canal MQTT (puerto 8883). | Evita que terceros lean la telemetría y las órdenes. |
| **IEC 62443, SR 2.8 / 6.1 – Eventos auditables** | Cada rechazo se registra con hora, motivo y trama, y se cuenta en la telemetría. | Permite detectar intentos de ataque. |
| **IEC 62443, SR 7.1 – Protección contra denegación de servicio** | Límite de tramas por segundo, búfer de largo fijo y reconexión no bloqueante. | Una inundación de mensajes o una caída del broker no deben detener el control local. |
| **IEC 62443, SR 3.6 – Salidas deterministas / falla segura** (*fail-safe*) | La bomba arranca apagada. Si se pierde el enlace, pasa a modo SEGURO. Si falla el sensor, no se riega. | Ante cualquier falla, el actuador queda en el estado que no causa daño (bomba **apagada**). |
| **Zonas y conductos** (IEC 62443-3-2) | El control lo hace el nodo de campo. El conducto MQTT solo transporta consignas validadas. | Si se cae la red o el nodo remoto, el proceso no queda sin control. |
| **Modos AUTO / MANUAL** (como en un PLC con HMI) | El operador puede tomar el control manual a distancia y volver a automático. | Es la práctica estándar en control de procesos. |
| **Ciclo de barrido no bloqueante** (*scan cycle* de un PLC) | Todo se temporiza con `millis()`. No hay `delay()` y el WiFi se reconecta en segundo plano. | El sistema siempre responde a comandos y a las vigilancias de seguridad. |
| **Enclavamientos** (*interlocks*) | Tiempo máximo de riego con descanso obligatorio. Se bloquea el ON manual durante el descanso. | Protegen el proceso aunque el operador o el sensor se equivoquen. |
| **Histéresis** | Enciende bajo el umbral y apaga en umbral + 5 %. | Evita que la bomba se prenda y apague muchas veces seguidas. |
| **Confirmación de órdenes** (*handshake*) | Cada comando recibe `ACK` o `NAK`. El nodo de control avisa si no llega respuesta en 3 s. | El operador siempre sabe si su orden se ejecutó. |
| **Estado del dispositivo con "última voluntad"** (como el *birth/death certificate* de Sparkplug B, MQTT industrial) | Al conectarse, el nodo sensor publica `ONLINE` retenido. Si se cae, el broker publica `OFFLINE`. | El supervisor sabe de inmediato si el equipo de campo está conectado. |
| **Configuración persistente** | El umbral se guarda en la flash con `Preferences`. | Después de un corte de energía, el sistema vuelve con la configuración del operador. |

---

## 7. Video

> _Link al video (máximo 3 minutos):_ **https://...**

Guion sugerido (las dos simulaciones de Wokwi visibles en pantalla):
1. (0:00) Los dos circuitos y el monitor serial del nodo de control recibiendo lecturas por WiFi/MQTT.
2. (0:30) **Activación automática**: girar el potenciómetro a "seco" → la bomba (LED azul) se enciende. Girarlo a "mojado" → se apaga. Cambiar el DHT22 para mostrar que cambian las lecturas.
3. (1:00) **Cambio de umbral**: `umbral 60` → aparece `CONFIRMADO` y la bomba reacciona al nuevo valor.
4. (1:30) **Control manual**: `on` / `off` → la bomba responde con su `CONFIRMADO`. Después, `auto`.
5. (2:00) **Comando inválido**: `prueba` → los 4 `RECHAZADO` con su motivo, y el registro en el monitor del nodo sensor.
6. (2:30) **Falla segura**: detener la simulación del nodo de control con la bomba en manual ON → a los 10 s el nodo sensor pasa a modo SEGURO y apaga la bomba.
