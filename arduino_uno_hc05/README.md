# EVA1 – Prototipo Arduino con conexión inalámbrica

**Aplicaciones Móviles para IoT (TI3V42) – INACAP**

## 1. Integrantes y caso elegido

| Integrante | Correo |
|---|---|
| Jenniffer Gonzalez | jenniffer.gonzalez05@inacapmail.cl |
| Rodrigo Cutiño | rodrigo.cutino@inacapmail.cl |

**Caso elegido:** Monitoreo de temperatura / humedad, con activación de riego.

Un **nodo sensor** (Arduino UNO 1) mide la humedad del suelo y la temperatura y humedad del aire. Cuando la humedad del suelo baja del umbral, enciende una bomba de riego (modo automático). Envía cada lectura por Bluetooth a un **nodo de control** (Arduino UNO 2). Desde ahí se ven las lecturas, se cambia el umbral y se enciende o apaga la bomba a distancia (modo manual).

> Para el riego, "superar el umbral" significa que la **sequedad** del suelo supera lo permitido: la bomba se enciende cuando `humedad_suelo < umbral` y se apaga cuando `humedad_suelo ≥ umbral + 5 %` (histéresis).

### Estructura del repositorio

```
arduino_uno_hc05/
├── nodo_sensor/            Microcontrolador 1: sensores, bomba y validación de comandos
│   ├── nodo_sensor.ino
│   └── secretos.example.h
├── nodo_control/           Microcontrolador 2: muestra lecturas y envía comandos
│   ├── nodo_control.ino
│   └── secretos.example.h
└── README.md
```
(El `.gitignore` que excluye `secretos.h` está en la raíz del repositorio.)

### Cómo cargar el proyecto

1. En Arduino IDE instalar las librerías **DHT sensor library** (Adafruit) y **Adafruit Unified Sensor**.
2. En cada carpeta, copiar `secretos.example.h` como `secretos.h` y escribir la **misma clave** en los dos.
3. Configurar los HC-05 con los comandos AT de la sección 3.1.
4. Cargar `nodo_sensor.ino` en el Arduino 1 y `nodo_control.ino` en el Arduino 2.
5. Abrir el monitor serial del nodo de control a **9600 baud**, con fin de línea **"Nueva línea"**, y escribir `ayuda`.

---

## 2. Tabla de componentes

| Componente | Cant. | Voltaje | Interfaz / protocolo | Rango / alcance | Por qué sirve para el caso |
|---|---|---|---|---|---|
| **Arduino UNO R3** (ATmega328P) | 2 | 5 V lógica; alimentación 7–12 V por jack o 5 V por USB | UART, I2C, SPI, GPIO; ADC de 10 bits | 14 pines digitales, 6 analógicos, 16 MHz, 2 KB de RAM, 1 KB de EEPROM | Lee el sensor analógico de suelo sin hardware extra. La EEPROM guarda el umbral si se corta la energía. Es la plataforma del laboratorio. |
| **Módulo Bluetooth HC-05** | 2 | Alimentación 3,6–6 V; lógica 3,3 V (RX necesita divisor desde 5 V) | Bluetooth 2.0 + EDR, perfil SPP (puerto serie), UART 9600 baud | ~10 m (clase 2) | Puede funcionar como **maestro**, así que dos Arduino se conectan sin teléfono. Se le cambia el PIN y se enlaza a una MAC fija. El mismo módulo sirve para la app Android de la EVA2. |
| **Sensor capacitivo de humedad de suelo v1.2** | 1 | 3,3–5,5 V | Salida analógica (~1,2–2,5 V) | 0–100 % de humedad relativa del sustrato (se calibra en seco y en agua) | Es el "sensor subterráneo" del caso. Al ser capacitivo, no tiene electrodos expuestos y no se corroe como el resistivo (YL-69), así que dura más enterrado. |
| **DHT22 (AM2302)** | 1 | 3,3–6 V | Digital, un solo cable (protocolo propio) | −40 a 80 °C (±0,5 °C); 0–100 % HR (±2 %); 1 lectura cada 2 s | Mide la temperatura y la humedad del aire que pide el caso. Sirve para entender la evaporación. Se lee cada 2 s, que es lo que permite el sensor. |
| **Módulo relé 1 canal 5 V** (optoacoplado) | 1 | Bobina 5 V; contacto 10 A 250 VAC / 30 VDC | GPIO digital, activo en bajo | ON/OFF | Aísla el Arduino de la corriente de la bomba. Si el pin queda en alto o desconectado, la bomba queda apagada (falla segura). |
| **Bomba sumergible DC** | 1 | 3–6 V, ~130–220 mA | ON/OFF a través del relé | ~80–120 L/h, altura ~0,4–1 m | Es el actuador de riego. Se alimenta aparte, porque el pin del Arduino no entrega esa corriente. |
| **LEDs (rojo, verde, azul) + resistencias de 220 Ω** | 3 | 2–3,2 V, ~10 mA | GPIO digital | — | En el nodo de control indican enlace, bomba y alarma sin mirar el PC. |
| **Resistencias de 1 kΩ y 2 kΩ** | 2 pares | — | Divisor de tensión | 5 V → 3,3 V | Protegen el RX de 3,3 V de cada HC-05. |
| **Resistencia de 100 kΩ** (pull-down en A0) | 1 | — | — | — | Si el sensor de suelo se desconecta, A0 lee ~0 y el código lo detecta como falla. Sin ella, el pin flotaría. |
| **Fuente de 5 V externa** (o porta pilas 4×AA) | 1 | 5–6 V | — | — | Alimenta la bomba y el relé sin provocar caídas de voltaje en el Arduino. |

---

## 3. Diagrama de conexión

> _Agregar aquí la imagen del diagrama (Fritzing, Tinkercad o foto rotulada): `docs/diagrama.png`._
> Tinkercad no trae el HC-05, así que conviene usar Fritzing o una foto rotulada.

### Nodo sensor (Arduino 1)

| Componente | Pin del componente | Pin del Arduino |
|---|---|---|
| HC-05 | VCC / GND | 5V / GND |
| HC-05 | TXD | D10 (RX por software) |
| HC-05 | RXD | D11 (TX por software) **a través del divisor 1 kΩ / 2 kΩ** |
| DHT22 | VCC / DATA / GND | 5V / D2 (pull-up de 10 kΩ si el módulo no la trae) / GND |
| Sensor de suelo | VCC / AOUT / GND | 5V / A0 (con 100 kΩ a GND) / GND |
| Relé | VCC / IN / GND | 5V / D7 / GND |
| Relé (contactos) | COM / NO | + de la fuente externa / + de la bomba (− de la bomba a − de la fuente) |
| LED integrado | — | D13 (encendido = falla o modo seguro) |

### Nodo de control (Arduino 2)

| Componente | Pin del Arduino |
|---|---|
| HC-05 VCC / GND | 5V / GND |
| HC-05 TXD | D10 |
| HC-05 RXD | D11 con divisor 1 kΩ / 2 kΩ |
| LED rojo (alarma) + 220 Ω | D4 |
| LED verde (enlace) + 220 Ω | D5 |
| LED azul (bomba) + 220 Ω | D6 |

```
Divisor para RX del HC-05:
  D11 ──[1 kΩ]──┬── RXD (HC-05)
                └──[2 kΩ]── GND
```

### 3.1 Configuración de los HC-05 (modo AT, 38400 baud, botón EN presionado al energizar)

| Nodo sensor (esclavo) | Nodo de control (maestro) |
|---|---|
| `AT+ORGL` | `AT+ORGL` |
| `AT+NAME=RIEGO_N1` | `AT+NAME=RIEGO_CTRL` |
| `AT+PSWD="4826"` (cambiar el PIN 1234 por defecto) | `AT+PSWD="4826"` (el mismo PIN) |
| `AT+ROLE=0` | `AT+ROLE=1` |
| `AT+UART=9600,0,0` | `AT+UART=9600,0,0` |
| `AT+ADDR?` → anotar la MAC, ej. `98d3:31:f5a1b2` | `AT+CMODE=0` (solo se conecta a una dirección fija) |
| | `AT+BIND=98d3,31,f5a1b2` (MAC del nodo sensor, con comas) |

> Las versiones 3.x del firmware usan `AT+PSWD="4826"`; las 2.0 usan `AT+PSWD=4826`. Pueden elegir otro PIN, pero no deben dejar 1234 ni 0000.

---

## 4. Formato del mensaje serial

Las tramas son texto ASCII de una línea, al estilo NMEA:

```
$<TIPO>,<campo1>,<campo2>,...*<FIRMA>\r\n
```

- `$` marca el inicio y `\r\n` el final. El largo máximo es de 64 caracteres.
- `FIRMA`: 4 dígitos hexadecimales. Se calcula con FNV-1a de 32 bits sobre la **clave compartida + el contenido entre `$` y `*`**, plegado a 16 bits. La clave **no viaja** en el mensaje.
- Solo se aceptan mayúsculas, dígitos, `,`, `.`, `-` y `*`.

### Lectura (nodo sensor → nodo de control, cada 2 s)

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

### Comandos (nodo de control → nodo sensor)

`$CMD,<seq>,<COMANDO>[,<VALOR>]*FIRMA`

| Comando | Valor | Efecto | Escrito en la consola |
|---|---|---|---|
| `PING` | — | Mantiene vivo el enlace (cada 2 s, automático) | — |
| `UMBRAL` | `10`–`90` | Cambia el umbral y lo guarda en EEPROM | `umbral 35` |
| `BOMBA` | `ON` / `OFF` | Control manual (pasa a modo MANUAL) | `on` / `off` |
| `MODO` | `AUTO` / `MANUAL` | Cambia el modo de operación | `auto` / `manual` |

`seq` es un número de secuencia creciente. El nodo sensor rechaza cualquier valor menor o igual al último que aceptó.

**Respuestas del nodo sensor:**
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
| 1 | **Emparejamiento por defecto** del HC-05 (nombre "HC-05", PIN 1234) | Alguien a menos de 10 m busca dispositivos con el celular, se empareja con el PIN 1234 y abre una app de terminal Bluetooth. | Puede encender la bomba (inunda el cultivo, gasta agua, pudre las raíces) o apagarla (el cultivo se seca). | Se cambian el nombre y el PIN. El maestro queda enlazado a la MAC del esclavo (`CMODE=0`, `BIND`). Cada comando debe llevar una **firma con clave compartida**: sin la clave, el comando se rechaza. | Sección 3.1 (AT); `calcularFirma()` y el paso 3 de `procesarTrama()` en `nodo_sensor.ino` |
| 2 | **Comandos aceptados sin validar** | Se envían textos mal formados, campos de más, números enormes o un umbral de 0 o 255. | Con umbral 0 nunca se riega y con 255 se riega siempre. Un texto largo podría desbordar el búfer y bloquear el Arduino. | Lista blanca de comandos, número exacto de campos, solo dígitos, **rango 10–90**, solo caracteres permitidos y **largo máximo de 64** con descarte. El nodo de control también valida antes de enviar. | `procesarTrama()`, `esNumero()`, `caracteresValidos()`, `dividirCampos()` y `leerBluetooth()` en `nodo_sensor.ino`; `cambiarUmbral()` en `nodo_control.ino` |
| 3 | **Canal sin protección contra repetición** (*replay*) | Un atacante captura una trama válida `BOMBA,ON` y la reenvía más tarde, sin necesidad de conocer la clave. | La bomba se enciende cuando nadie la pidió. | **Número de secuencia creciente**: se rechaza cualquier `seq` ya usado. El nodo de control guarda la secuencia en EEPROM por bloques, así que nunca la repite aunque se reinicie. | Paso 5 de `procesarTrama()` en `nodo_sensor.ino`; `iniciarSecuencia()` y `siguienteSecuencia()` en `nodo_control.ino` |
| 4 | **Pérdida del enlace o saturación** (interferencia, ataque de inundación de mensajes, nodo de control apagado) | Se bloquea la señal o se envían cientos de tramas por segundo justo después de dejar la bomba en manual ON. | La bomba queda encendida sin supervisión hasta vaciar el estanque o inundar. | **Falla segura**: si pasan 10 s sin un mensaje válido, entra en modo SEGURO con la bomba apagada. Hay un **límite de 8 tramas por segundo** y un **tiempo máximo de riego** de 60 s con 30 s de descanso. | `vigilarEnlace()`, `limiteDeTramasOk()` y `vigilarBomba()` en `nodo_sensor.ino` |
| 5 | **Sensor desconectado o dañado** | Se corta o desconecta el cable del sensor de suelo (por manipulación o falla). | Si se leyera 0 % de forma permanente, el riego sería continuo. | Se detectan lecturas fuera de rango (bit de falla 2) y en ese caso el modo automático no riega. Además está el enclavamiento por tiempo máximo. | `leerSensores()` y `controlAutomatico()` en `nodo_sensor.ino` |
| 6 | **Claves en el repositorio público** | Alguien lee la clave en GitHub y con ella firma comandos válidos. | Se anula la medida 1. | La clave está en `secretos.h`, que está en `.gitignore`. Solo se publica `secretos.example.h`. | `.gitignore`; `#include "secretos.h"` en los dos sketches |

**Registro de rechazos:** cada trama rechazada responde con `$NAK` y queda registrada en el monitor serial del nodo sensor con su hora (millis), el motivo y la trama completa. Además se cuenta en el campo `rechazos` de cada lectura (`rechazar()` en `nodo_sensor.ino`).

**Demostración en el video:** en el nodo de control se escribe `prueba`. Se envían 4 tramas inválidas (firma alterada, `UMBRAL,99`, comando `ABRIR` y una secuencia repetida) y el nodo sensor responde a cada una con `NAK` y el motivo (`FIRMA`, `RANGO`, `COMANDO` y `SECUENCIA`). También se puede usar `raw $CMD,1,BOMBA,ON*0000`.

**Limitaciones conocidas:** la firma FNV-1a de 16 bits protege la integridad y exige conocer la clave, pero no es criptográficamente robusta, y las lecturas viajan sin cifrar. En un sistema real se usaría HMAC-SHA256 o AES (por ejemplo, con un ESP32 y BLE con *Secure Connections* o TLS sobre WiFi).

---

## 6. Prácticas de la industria OT aplicadas

| Práctica | Cómo se aplicó | Por qué |
|---|---|---|
| **ISA/IEC 62443-3-3, SR 3.5 – Validación de entradas** | Cada comando se revisa en formato, campos, rango y lista blanca antes de ejecutarse. | Un dispositivo de campo nunca debe ejecutar una orden mal formada o fuera de límites. |
| **IEC 62443, SR 3.1 – Integridad de la comunicación** y **SR 1.2 – Autenticación de dispositivos** | Firma con clave compartida en todas las tramas, en los dos sentidos. Enlace Bluetooth fijado por MAC y PIN propio. | Solo el nodo de control legítimo puede dar órdenes, y se detectan las tramas corruptas. |
| **IEC 62443, SR 2.8 / 6.1 – Eventos auditables** | Cada rechazo se registra con hora, motivo y trama, y se cuenta en la telemetría. | Permite detectar intentos de ataque o fallas de comunicación. |
| **IEC 62443, SR 7.1 – Protección contra denegación de servicio** | Límite de tramas por segundo y búfer de largo fijo. | Una inundación de mensajes no debe bloquear el control local. |
| **IEC 62443, SR 3.6 – Salidas deterministas / falla segura** (*fail-safe*) | La bomba arranca apagada (el pin se fija antes de declararlo como salida). Si se pierde el enlace, pasa a modo SEGURO. Si falla el sensor, no se riega. | Ante cualquier falla, el actuador queda en el estado que no causa daño. En riego, eso es la bomba **apagada** (sin inundación ni bomba trabajando en seco). |
| **Modos AUTO / MANUAL** (como en un PLC con HMI) | El operador puede tomar el control manual a distancia y volver a automático. | Es la práctica estándar en control de procesos: la operación manual siempre está disponible y es explícita. |
| **Ciclo de barrido no bloqueante** (*scan cycle* de un PLC) | Todo se temporiza con `millis()`. No hay `delay()` ni esperas largas. | El sistema siempre responde a comandos y a las vigilancias de seguridad. |
| **Enclavamientos** (*interlocks*) | Tiempo máximo de riego con descanso obligatorio. Se bloquea el ON manual durante el descanso. | Protegen el proceso y el equipo aunque el operador o el sensor se equivoquen. |
| **Histéresis** | Enciende bajo el umbral y apaga en umbral + 5 %. | Evita que el relé y la bomba se prendan y apaguen muchas veces seguidas, lo que los desgasta. |
| **Confirmación de órdenes** (*handshake*) | Cada comando recibe `ACK` o `NAK`. El nodo de control avisa si no llega respuesta en 3 s. | El operador siempre sabe si su orden se ejecutó. |
| **Configuración persistente** | El umbral se guarda en EEPROM (con `EEPROM.update` para no gastar la memoria). | Después de un corte de energía, el sistema vuelve con la configuración del operador. |
| **Telemetría con diagnóstico** | Cada lectura informa el modo, el estado de la bomba, la máscara de fallas y el contador de rechazos. | El nodo supervisor ve el estado completo, no solo el valor medido. |
| **Segmentación / mínimo privilegio** (zonas y conductos, IEC 62443-3-2) | El control lo hace el nodo de campo. El nodo remoto solo envía consignas validadas. | Si se cae el nodo remoto, el proceso no queda sin control. |

---

## 7. Video

> _Link al video (máximo 3 minutos):_ **https://...**

Guion sugerido:
1. (0:00) Los dos equipos y el monitor serial del nodo de control recibiendo lecturas.
2. (0:30) **Activación automática**: secar el sensor (sacarlo de la tierra) → la bomba se enciende. Mojarlo → la bomba se apaga.
3. (1:00) **Cambio de umbral**: `umbral 60` → aparece `CONFIRMADO` y la bomba reacciona al nuevo valor.
4. (1:30) **Control manual**: `on` / `off` → la bomba responde con su `CONFIRMADO`. Después, `auto`.
5. (2:00) **Comando inválido**: `prueba` → los 4 `RECHAZADO` con su motivo, y el registro en el monitor del nodo sensor.
6. (2:30) **Falla segura**: desconectar el nodo de control con la bomba en manual ON → a los 10 s el nodo sensor pasa a modo SEGURO y apaga la bomba.
