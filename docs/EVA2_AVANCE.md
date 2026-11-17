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
- La guía dice que la evaluación es individual, pero **la EVA2 se hace en pareja**, Jenniffer Gonzalez y Rodrigo Cutiño (confirmado el 2026-11-17; Rodrigo lo aclara con el profesor).

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

Issues en el repo del profesor (en inglés y español). #54, #55 y #56 siguen abiertos:

| # | Tema |
|---|---|
| [#54](https://github.com/Rixmerz/hertzbench/issues/54) | ESP32 en macOS arm64 (`libqemu-xtensa`) |
| [#55](https://github.com/Rixmerz/hertzbench/issues/55) | Placas scripted como clientes MQTT |
| [#56](https://github.com/Rixmerz/hertzbench/issues/56) | Herramienta MCP `mqtt_publish` |
| [#57](https://github.com/Rixmerz/hertzbench/issues/57) | Docs: emulador Android y alternativa a BLE en macOS. **Cerrado:** documentado en el PR [#58](https://github.com/Rixmerz/hertzbench/pull/58) (2026-10-29). El profesor pide un reporte de prueba real del emulador por `10.0.2.2` |

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

- [x] Reporte de prueba del puente por `10.0.2.2:1883` desde el emulador, que pidió el profesor en el #57: enviado el 2026-11-16 como [#110](https://github.com/Rixmerz/hertzbench/issues/110). El error de compilación con el Deno de Homebrew, como [#109](https://github.com/Rixmerz/hertzbench/issues/109).

- [x] Compilar hertzbench y registrarlo como MCP (2026-11-13, M2 de 16 GB, ver la sección 5).
- [ ] Confirmar que el nodo de riego sea un rol de la misma app, y no un ESP32 simulado. Mientras tanto, el nodo es scripted en `hertzbench/riego-mqtt.yaml`.
- [x] Dónde va el código Android: repo aparte, [RiegoIoT-App](https://github.com/rodrigocutinoa/RiegoIoT-App), que es de la pareja (al principio se pensó uno por persona porque la guía dice individual).
- [x] Instalar Android Studio en la M2 de 16 GB (2026-11-13). Falta abrirlo para descargar el SDK y crear el emulador.

## 5. Preparar una máquina nueva

```bash
git clone https://github.com/rodrigocutinoa/ProyectRiego.git
cd ProyectRiego
# copiar la guía del profesor a _Document/ (no se sube)
```

**hertzbench, la forma recomendada (macOS).** `hertzbench/preparar-mac.sh` instala Deno y mosquitto, clona hertzbench al lado del repo, compila el binario para ese Mac, lo enlaza en `~/.local/bin` y prueba el puente. Los pasos completos están en [RETOMAR.md](RETOMAR.md), sección 0.

**hertzbench desde un release** (Windows, o si falla la compilación). Se descarga desde los [releases](https://github.com/Rixmerz/hertzbench/releases), junto con `SHA256SUMS`. Hay que clonar también el repo de hertzbench para tener los escenarios de ejemplo.

| Sistema | Binario | Notas |
|---|---|---|
| macOS Apple Silicon | `hertzbench-macos-arm64` | Probado en el CI del proyecto. `xattr -d com.apple.quarantine <binario>` |
| macOS Intel | `hertzbench-macos-x64` | Existe, pero la comunidad aún no lo ha probado. Mismo comando `xattr` |
| Windows x64 | `hertzbench-windows-x64.exe` | Probado por la comunidad. El firewall pide permiso la primera vez. El puerto 8080 puede estar ocupado por `http.sys`: usar otro `listen_port` |

**Registrar el MCP en Claude Code**, desde la carpeta del proyecto:

```bash
# macOS
# no hace falta: el .mcp.json del repo lo registra al abrir Claude Code
claude mcp add hertzbench -- hertzbench mcp --workspace "$(pwd)"   # con preparar-mac.sh
# Windows (PowerShell)
claude mcp add hertzbench -- C:\ruta\hertzbench-windows-x64.exe mcp --workspace "$PWD\simulacion"
```

Después hay que reiniciar Claude Code.

**Android Studio:** descargarlo desde <https://developer.android.com/studio>. En macOS también se puede con `brew install --cask android-studio`.

### Requisitos oficiales de Android Studio

Fuente: [developer.android.com/studio/install](https://developer.android.com/studio/install), revisada el 2026-10-29. La versión actual es Rabbit 2, 2026.2.2.

| | Mínimo (solo IDE / IDE + emulador) | Recomendado |
|---|---|---|
| **Windows** | Windows 10 de 64 bits. No funciona en CPU ARM | Windows de 64 bits más reciente |
| **macOS** | macOS 12. Apple M1, o Intel Core de 6.ª generación o más nuevo (Google está dejando de soportar los Mac Intel) | macOS más reciente, Apple Silicon |
| **CPU (Windows/Linux)** | Virtualización VT-x o AMD-V activa en la BIOS. Intel de 8.ª generación (i5-8xxx) o AMD Ryzen 1xxx | Intel i5/i7/i9 serie H/HK/HX o Ryzen 5–9. No se recomiendan las series U ni N |
| **RAM** | 8 GB / 16 GB | 32 GB |
| **Disco** | 8 GB / 16 GB libres | SSD con 32 GB libres o más |
| **GPU (Windows, emulador)** | 4 GB de VRAM (GeForce 10 o más nueva, Radeon RX 5000) | 8 GB de VRAM (GeForce 20, Radeon RX 6600) |
| **Pantalla** | 1280 × 800 | 1920 × 1080 |

Cada emulador adicional (AVD) ocupa hasta 6 GB de disco. Correr varios a la vez requiere unos 4 GB de RAM por emulador.

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

**1b. MacBook Air M2 (Mac14,2) de 16 GB**: Apple M2, 16 GB de RAM, SSD de 256 GB (99 GB libres), macOS 27.0.1. Revisada el 2026-11-13. **Ya no se usa desde el 2026-11-13**; el trabajo sigue en otro Mac.

| Pieza | Estado |
|---|---|
| Homebrew, git | Instalados |
| gh | **No instalado** (`brew install gh`; hace falta para enviar los reportes a hertzbench) |
| Deno 2.9.8, mosquitto | Instalados con Homebrew (2026-11-13) |
| hertzbench | Compilado desde el código fuente (`19847355`), en `~/.local/bin`, y registrado como MCP. Puente MQTT probado: PASS |
| Android Studio | Instalado (2026-11-13); falta el SDK y el emulador |

Aplican las recomendaciones para una M2 de 16 GB de [RETOMAR.md](RETOMAR.md), sección 4: un solo emulador con 2 GB de RAM y `org.gradle.jvmargs=-Xmx2g`.

**2. MacBook Pro 2016** (Intel Core de 6.ª generación, 16 GB de RAM, 256 GB de SSD, **macOS 14 Sonoma**). Apple no da Sonoma a este modelo, así que lo más probable es que esté instalado con OpenCore Legacy Patcher, que no es oficial.

| Pieza | ¿Funciona? | Motivo |
|---|---|---|
| Android Studio y emulador | Sí | Google pide macOS 12 o superior y un Intel de 6.ª generación o más nuevo. 16 GB de RAM alcanzan para el IDE y el emulador. Google está dejando de dar soporte a los Mac Intel, así que una versión futura podría excluirlo. |
| Claude Code (CLI y extensión) | Sí | Requiere macOS 13.0 o superior y procesador x64 o ARM64. |
| hertzbench `macos-x64` | Probablemente sí, sin verificar | El binario existe, pero la comunidad no lo ha probado. Si falla, se puede correr desde el código fuente con Deno 2. |
| Disco de 256 GB | Ajustado | El IDE, el SDK y una imagen del emulador ocupan unos 20 a 30 GB. Conviene usar una sola imagen del emulador o un teléfono físico. |

Si algo falla solo en este equipo (por ejemplo, la aceleración del emulador), puede deberse al parche de OpenCore y no al proyecto.

**3. ASUS Zenbook Pro 14 Duo OLED (UX8402Z) con Windows**: Intel Core i7-12700H (12.ª generación, 14 núcleos), 32 GB de RAM, SSD NVMe de 1 TB con DRAM, **Windows 11 Pro**. La edición Pro incluye Hyper-V y WSL2, que sirven para el emulador acelerado y, si hiciera falta, para el puente BLE de hertzbench.

Es el equipo más holgado para Android Studio con dos emuladores a la vez (nodo y panel). El i7-12700H tiene VT-x; solo hay que comprobar que la virtualización esté activa en la BIOS o en el Administrador de tareas → Rendimiento → CPU → "Virtualización: Habilitado".

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
| 2026-10-29 | El profesor cerró el #57: el emulador Android (`10.0.2.2`) y la alternativa a BLE en macOS quedaron documentados en `docs/mobile-bridge.md` (PR #58). Pidió un reporte de ejecución real. Notas de las tres máquinas de trabajo agregadas. |
| 2026-11-03 | Requisitos de Android Studio y escenario de una M2 con 16 GB. Error `0x800f0950` al instalar OpenSSH en Windows (solución con winget). Opciones para conectarse directo a la máquina virtual. Todo resumido en [RETOMAR.md](RETOMAR.md) para la próxima sesión. |
| 2026-11-13 | hertzbench compilado desde el código fuente en la MacBook Air M2 de 16 GB (Deno 2.9.8 por Homebrew, binario `hertzbench-macos-arm64`). El ejemplo `udp-3-scripted.yaml` pasó (5/5 aserciones). `deno task compile` falla con el Deno de Homebrew porque el permiso `--allow-run=deno` no cubre la ruta real del binario; se resolvió corriendo `scripts/compile.ts` con `--allow-run`. Binario enlazado en `~/.local/bin` (`hertzbench`, `hzb`) y MCP registrado en Claude Code con el workspace en este repo. Escenario `hertzbench/riego-mqtt.yaml` (broker MQTT, nodo de riego scripted y puente en `127.0.0.1:1883`), probado con mosquitto desde el Mac mediante `hertzbench/probar-puente.sh`: PASS en ambos sentidos. Borradores de reporte para el profesor en `hertzbench/reportes/`. |
| 2026-11-13 (2) | Script `hertzbench/preparar-mac.sh` para dejar listo cualquier Mac en un paso, probado en la M2 de 16 GB: PASS. Android Studio instalado con Homebrew; todavía no se abre. Decisión: un repo aparte por persona para la app (`RiegoIoT-App`, falta confirmar el nombre). Notas para retomar en otro Mac en [RETOMAR.md](RETOMAR.md), sección 0. |
| 2026-11-13 (3) | Se deja de usar la M2 de 16 GB. `.mcp.json` en el repo para que Claude Code cargue hertzbench solo; `preparar-mac.sh` también instala Android Studio. Instrucciones para el Mac nuevo en [RETOMAR.md](RETOMAR.md), sección 0. |
| 2026-11-16 | Se sigue en la MacBook Air M2 de 24 GB (macOS 26.7.3). `preparar-mac.sh` sin errores: Deno, mosquitto, hertzbench `7f5bd4ae` compilado, puente MQTT PASS y Android Studio instalado. `~/.local/bin` agregado al PATH en `~/.zshrc`, y el MCP de hertzbench conectó en Claude Code. SDK descargado con el asistente Standard; emulador `Medium_Phone_API_37.0` (Android 17, arm64, 2 GB) arranca. Prueba MQTT desde el emulador por `10.0.2.2:1883` con `nc`: PASS en ambos sentidos. Enviados a hertzbench los reportes [#109](https://github.com/Rixmerz/hertzbench/issues/109) (compilación con Deno de Homebrew, sigue en `7f5bd4ae`) y [#110](https://github.com/Rixmerz/hertzbench/issues/110) (prueba del emulador). Confirmado el repo público `RiegoIoT-App`; falta crear el proyecto con el asistente de Android Studio. |
| 2026-11-16 (2) | Hito 0: proyecto `RiegoIoT-App` creado con el asistente de Android Studio (Empty Activity con Compose, paquete `cl.inacap.riegoiot`, minSdk 26, targetSdk 37, Kotlin DSL, AGP 9.5.1). Compila e instala en el emulador. `.mcp.json`, `CLAUDE.md` y README agregados; repo público [rodrigocutinoa/RiegoIoT-App](https://github.com/rodrigocutinoa/RiegoIoT-App). Hito 0 terminado con el emulador: Rodrigo solo tiene iPhone y el docente autorizó emular. |
| 2026-11-16 (3) | El profesor cerró los dos reportes: #109 corregido en el PR [#111](https://github.com/Rixmerz/hertzbench/pull/111) (confirmado en este Mac con `c565ba41`: `deno task compile` funciona con el Deno de Homebrew) y #110 citado en el README de hertzbench (PR [#112](https://github.com/Rixmerz/hertzbench/pull/112)). Pide un reporte con la app Kotlin/Compose cuando se conecte por MQTT. Borrador de los Hitos 1 y 2 en `RiegoIoT-App/docs/HITOS_1_2.md`, por revisar. |
| 2026-11-16 (4) | Simulación del riego por MCP (`run_scenario`, `mqtt_publish`, `inject_fault`): un `CMD;bomba;OFF` enviado durante un corte de 8 s se pierde (QoS 0), así que se agregan al diseño la confirmación con reenvío, el estado retenido y el estado seguro en el nodo. Hitos 1 y 2 en [RiegoIoT-App/docs/HITOS_1_2.md](https://github.com/rodrigocutinoa/RiegoIoT-App/blob/main/docs/HITOS_1_2.md), con las decisiones aprobadas: Paho, una app con roles Nodo y Panel (dos emuladores), y el formato `ESTADO;bomba;ON` con `riego/ack`. `riego-mqtt.yaml` ajustado al formato nuevo (probar-puente: PASS). Falta la revisión del docente. |
| 2026-11-17 | Revisión del profesor (issue #1 de RiegoIoT-App): Hitos 1 y 2 bien, falta la funcionalidad; la entrega del AAI quedó en borrador. La EVA2 es en pareja con Jenniffer. **Hito 3 funcionando** ([fccace5](https://github.com/rodrigocutinoa/RiegoIoT-App/commit/fccace5)): Paho MQTT, roles Nodo y Panel, comandos con ACK, notificaciones de encendido y apagado, estado SEGURO. Android 17 exige el permiso `ACCESS_LOCAL_NETWORK` para llegar a `10.0.2.2` (sin él, timeout). Nuevo escenario `hertzbench/riego-broker.yaml` (broker + monitor). Probado por MCP: el Panel recibe al nodo simulado y su comando llega; la app como Nodo responde a `mqtt_publish` (MANUAL, umbral, AUTO con histéresis, descarta `CMD;bomba;BORRAR_TODO`) y pasa a SEGURO con `inject_fault`. Hallazgos del broker de hertzbench: no aplica keep alive ni *Last Will*, y acepta mensajes atrasados de la conexión anterior tras reconectar con el mismo ID. Dos emuladores a la vez dejan al Mac sin memoria. |
| 2026-11-17 (2) | Reportes para el profesor redactados (prueba con la app, nota de Android 17, mejora del broker y respuesta al issue #1), en `hertzbench/reportes/`; la publicación quedó pendiente de la aprobación de Rodrigo. RiegoIoT-App: **Hito 5** (login SQLite + PBKDF2, roles, bloqueo; [a1b79c6](https://github.com/rodrigocutinoa/RiegoIoT-App/commit/a1b79c6)), **punto 4** (confirmación con reenvío y estado retenido en `riego/estado/<clave>`; [4c5b063](https://github.com/rodrigocutinoa/RiegoIoT-App/commit/4c5b063)), **Hito 6** (historial; [5fb2e16](https://github.com/rodrigocutinoa/RiegoIoT-App/commit/5fb2e16)) y README con capturas. Todo probado en el emulador contra hertzbench por MCP. Falta: Hitos 4 y 7 (cifrado, ISO/IEC 27400 y OT, pruebas P1–P14) y la demostración. |
| 2026-11-17 (3) | Reportes enviados: hertzbench [#114](https://github.com/Rixmerz/hertzbench/issues/114) (Android 17 y `ACCESS_LOCAL_NETWORK`), [#115](https://github.com/Rixmerz/hertzbench/issues/115) (mejora del broker MQTT), [#116](https://github.com/Rixmerz/hertzbench/issues/116) (prueba con la app Kotlin) y avance en el issue #1 de RiegoIoT-App. **Hito 4** ([16cb050](https://github.com/rodrigocutinoa/RiegoIoT-App/commit/16cb050)): mensajes cifrados con AES-GCM y código de vinculación (PBKDF2), tópico como AAD, comandos de más de 30 s rechazados. `hertzbench/cifrar.ts` (Deno) cifra igual que la app; `riego-mqtt.yaml` ahora publica cifrado con el código 123456. Probado: el Panel descifra al nodo simulado y rechaza 2 mensajes en texto plano enviados por MCP. `probar-puente.sh`: PASS. Hitos 7 y 8 (ISO/IEC 27400 y OT, pruebas P1–P14, demostración): se consultan hoy con el profesor. |
