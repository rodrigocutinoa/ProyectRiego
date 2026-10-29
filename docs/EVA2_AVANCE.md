# EVA2 – Avance: app Android para el proyecto de riego

**Aplicaciones Móviles para IoT (TI3042) – INACAP · Unidad 2**

Bitácora del trabajo de la Unidad 2, para retomarlo en cualquier computador. Se actualiza al cerrar cada sesión.

## 1. Qué pide el profesor

La guía de la unidad es *"Guía de desarrollo — Unidad 2 · App Android para IoT"* (35 páginas). Es material del curso y no se publica en este repo, que es público. Cada uno la guarda en `_Document/`, una carpeta que no se sube.

- Trabajamos **a partir de la sección 2.2, Tecnologías de conexión inalámbrica**. Solo se salta la 2.1, que compara formas de hacer apps. Los hitos 0 a 8 se hacen todos.
- Requisitos mínimos que pidió el profesor para conectar el proyecto de riego a una app Android:
  1. **Autenticación con login.**
  2. **Alerta y estado del riego.** Cuando el riego se enciende o se apaga, la app muestra una notificación y el estado actual.
- La simulación del lado IoT se hace con **[hertzbench](https://github.com/Rixmerz/hertzbench)**, conectado a Claude Code por MCP.
- **La evaluación es individual.** Cada estudiante construye, sube a su propio GitHub y entrega su propia app.

### Hitos de la guía

| Hito | Contenido | Formativa | Sumativa |
|---|---|:-:|:-:|
| 0 | Android Studio, proyecto con minSdk 26 y Kotlin DSL, correrlo en el teléfono, GitHub | ✔ | ✔ |
| 1 | Problemática, funcionalidades y herramientas justificadas (revisión docente) | ✔ | ✔ |
| 2 | Medio inalámbrico justificado, mensajes y mockup (revisión docente) | ✔ | ✔ |
| 3 | Permisos, conexión, envío de datos y comandos, notificaciones | ✔ | ✔ |
| 4 | Pruebas P1–P7 y README de la preentrega | ✔ | |
| 5 | Login con SQLite, PBKDF2, roles y bloqueo tras 5 intentos | | ✔ |
| 6 | Guardar las lecturas y mostrar un historial | | ✔ |
| 7 | Seguridad ISO/IEC 27400 y OT, pruebas P8–P14 | | ✔ |
| 8 | Demostración y entrega del `.txt` con el link en el AAI | | ✔ |

## 2. Qué es hertzbench (revisado el 2026-10-28)

Es un simulador de la red inalámbrica entre placas: de 2 a 10 nodos con distancia, muros, pérdidas y latencia. Tiene tres partes útiles para nosotros:

- **Servidor MCP con 23 herramientas:** crear y correr escenarios, leer el serial, mover nodos e inyectar fallas en vivo.
- **Puente móvil IP:** el teléfono o el emulador se conecta a un nodo simulado por HTTP, WebSocket o MQTT a través de un puerto del computador.
- **Vista web en vivo** de los nodos y enlaces.

Limitaciones encontradas:

- El **puente BLE solo funciona en Linux**. En macOS y Windows hay que usar el puente IP (Wi-Fi con MQTT).
- Correr **ESP32 con firmware real** necesita `libqemu-xtensa`, que solo está probado en Linux x86_64.
- Las placas **scripted** son generadores de tráfico. Pueden ser broker MQTT, pero no publican como clientes.
- El emulador Android llega al computador en `10.0.2.2`. Un teléfono físico necesita estar en la misma red Wi-Fi y que el escenario use `listen: 0.0.0.0`.

Issues abiertos en el repo del profesor (en inglés y español):

| # | Tema |
|---|---|
| [#54](https://github.com/Rixmerz/hertzbench/issues/54) | ESP32 en macOS arm64 (`libqemu-xtensa`) |
| [#55](https://github.com/Rixmerz/hertzbench/issues/55) | Placas scripted como clientes MQTT |
| [#56](https://github.com/Rixmerz/hertzbench/issues/56) | Herramienta MCP `mqtt_publish` |
| [#57](https://github.com/Rixmerz/hertzbench/issues/57) | Docs: emulador Android y alternativa a BLE en macOS |

## 3. Arquitectura propuesta (pendiente de confirmar)

```
[App Android · rol "Nodo de riego"] ──MQTT──┐
                                            ├─► puente hertzbench (PC:1883) ─► broker MQTT simulado
[App Android · rol "Panel" + login] ──MQTT──┘        (canal Wi-Fi simulado: distancia, muros, pérdidas)
```

- **Una sola app** en Kotlin con Jetpack Compose. Login en SQLite con PBKDF2, roles operador y observador, y bloqueo tras 5 intentos fallidos.
- **Rol Nodo de riego:** simula la humedad del suelo y la bomba. Publica las lecturas y `riego/estado ON|OFF`.
- **Rol Panel:** muestra el estado y lanza una **notificación cada vez que el riego se enciende o se apaga**.
- **Estado seguro:** si se pierde el enlace, la bomba se apaga. Se demuestra con `inject_fault` por MCP.
- **Mensajes**, adaptados de la guía:

  | Mensaje | Dirección |
  |---|---|
  | `LECTURA;suelo;42` | nodo → panel |
  | `CMD;bomba;ON` | panel → nodo |
  | `ACK;bomba;ON` | nodo → panel |

- **Medio:** Wi-Fi con MQTT, porque BLE no es posible en el puente sobre macOS ni Windows. Hay que justificarlo en el README, ya que la guía recomienda Bluetooth.

## 4. Decisiones pendientes

- [ ] Descargar el binario de hertzbench y registrarlo como MCP (ver la sección 5).
- [ ] Confirmar que el nodo de riego sea un rol de la misma app, y no un ESP32 simulado.
- [ ] Decidir dónde va el código Android: `android_app/` en este repo, o un repo aparte por persona.
- [ ] Instalar Android Studio.

## 5. Preparar una máquina nueva

```bash
git clone https://github.com/rodrigocutinoa/ProyectRiego.git
cd ProyectRiego
# copiar la guía del profesor a _Document/ (no se sube)
```

**hertzbench.** Se descarga desde los [releases](https://github.com/Rixmerz/hertzbench/releases), junto con `SHA256SUMS`. Hay que clonar también el repo de hertzbench para tener los escenarios de ejemplo.

| Sistema | Binario | Notas |
|---|---|---|
| macOS Apple Silicon | `hertzbench-macos-arm64` | Probado en el CI del proyecto. `xattr -d com.apple.quarantine <binario>` |
| macOS Intel | `hertzbench-macos-x64` | Existe, pero la comunidad aún no lo ha probado. Mismo comando `xattr` |
| Windows x64 | `hertzbench-windows-x64.exe` | Probado por la comunidad. El firewall pide permiso la primera vez. El puerto 8080 puede estar ocupado por `http.sys`: usar otro `listen_port` |

**Registrar el MCP en Claude Code**, desde la carpeta del proyecto:

```bash
# macOS
claude mcp add hertzbench -- /ruta/hertzbench-macos-arm64 mcp --workspace "$(pwd)/simulacion"
# Windows (PowerShell)
claude mcp add hertzbench -- C:\ruta\hertzbench-windows-x64.exe mcp --workspace "$PWD\simulacion"
```

Después hay que reiniciar Claude Code.

**Android Studio:** descargarlo desde <https://developer.android.com/studio>. En macOS también se puede con `brew install --cask android-studio`.

### Máquinas de trabajo (revisadas el 2026-10-28)

**1. MacBook Air M2 (2022)**: Apple M2, 24 GB de RAM, SSD de 460 GB (297 GB libres), macOS 26.7.1.

| Pieza | Estado |
|---|---|
| git, gh (con sesión de `rodrigocutinoa`), Homebrew | Instalados |
| Claude Code | Se usa desde la extensión de VS Code (el comando `claude` no está en el PATH de la terminal) |
| Android Studio | **Falta** (`brew install --cask android-studio`) |
| hertzbench | **Falta**. Binario `hertzbench-macos-arm64`, el único Mac que prueba el CI del proyecto |
| Deno, Docker | No instalados. No hacen falta si se usa el binario de hertzbench |
| Puente BLE de hertzbench | No disponible (solo funciona en Linux). Se usa el puente IP con MQTT |

Es la máquina recomendada para hertzbench y la simulación por MCP.

**2. MacBook Pro 2016** (Intel Core de 6.ª generación, 16 GB de RAM, 256 GB de SSD, **macOS 14 Sonoma**). Apple no da Sonoma a este modelo, así que lo más probable es que esté instalado con OpenCore Legacy Patcher, que no es oficial.

| Pieza | ¿Funciona? | Motivo |
|---|---|---|
| Android Studio y emulador | Sí | Google pide macOS 12 o superior y un Intel de 6.ª generación o más nuevo. 16 GB de RAM alcanzan para el IDE y el emulador. Google está dejando de dar soporte a los Mac Intel, así que una versión futura podría excluirlo. |
| Claude Code (CLI y extensión) | Sí | Requiere macOS 13.0 o superior y procesador x64 o ARM64. |
| hertzbench `macos-x64` | Probablemente sí, sin verificar | El binario existe, pero la comunidad no lo ha probado. Si falla, se puede correr desde el código fuente con Deno 2. |
| Disco de 256 GB | Ajustado | El IDE, el SDK y una imagen del emulador ocupan unos 20 a 30 GB. Conviene usar una sola imagen del emulador o un teléfono físico. |

Si algo falla solo en este equipo (por ejemplo, la aceleración del emulador), puede deberse al parche de OpenCore y no al proyecto.

**3. PC con Windows** (especificaciones por completar: procesador, RAM, disco, versión de Windows).

| Pieza | ¿Funciona? | Requisito o nota |
|---|---|---|
| Android Studio y emulador | Sí | Windows 10 u 11 de 64 bits. Hay que activar la virtualización en la BIOS (VT-x o AMD-V). Se recomiendan 16 GB de RAM. |
| Claude Code | Sí | Windows 10 1809 o superior. Se recomienda Git for Windows. |
| hertzbench `windows-x64.exe` | Sí | Probado por la comunidad. El firewall pide permiso la primera vez. Si el puerto 8080 está ocupado por `http.sys`, se usa otro `listen_port`. |
| Puente BLE / ESP32 real | No de forma nativa | Solo con WSL2 (y para BLE, además un adaptador USB). |

## 6. Bitácora

| Fecha | Avance |
|---|---|
| 2026-10-28 | Repo clonado. Guía de la Unidad 2 revisada completa. Revisión de hertzbench (MCP, puente IP y BLE, limitaciones). Abiertos los issues #54–#57 en hertzbench. Arquitectura propuesta y decisiones pendientes registradas en este archivo. |
