# Retomar desde aquí (2026-11-16)

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

Notas de la última sesión para seguir sin perder contexto, en este Mac o en otro. El detalle completo está en [EVA2_AVANCE.md](EVA2_AVANCE.md).

## Dónde quedamos

- **2026-11-16: se sigue en la MacBook Air M2 de 24 GB.** Está todo instalado: hertzbench (`7f5bd4ae`, MCP conectado), Android Studio con el SDK y el emulador `Medium_Phone_API_37.0` (Android 17, arm64). Los dos reportes ya se enviaron al profesor: [#109](https://github.com/Rixmerz/hertzbench/issues/109) y [#110](https://github.com/Rixmerz/hertzbench/issues/110), este último con la prueba MQTT desde el emulador por `10.0.2.2:1883` (PASS).
- **App creada (Hito 0):** [RiegoIoT-App](https://github.com/rodrigocutinoa/RiegoIoT-App), en `~/Dev/Inacap/AppIoT/RiegoIoT-App`, corre en el emulador. Para trabajar en la app, abrir VS Code en esa carpeta (`code ~/Dev/Inacap/AppIoT/RiegoIoT-App`); su `CLAUDE.md` tiene el estado.
- **Hito 0 terminado.** Rodrigo no tiene teléfono Android (solo iPhone) y el docente autorizó usar el emulador.
- **Hitos 1 y 2 escritos** (2026-11-16) en [RiegoIoT-App/docs/HITOS_1_2.md](https://github.com/rodrigocutinoa/RiegoIoT-App/blob/main/docs/HITOS_1_2.md). Decisiones aprobadas: Paho, una app con roles Nodo y Panel, mensajes `ESTADO;bomba;ON` con `riego/ack`.
- **Siguiente paso:** mostrarle los Hitos 1 y 2 al docente y anotar su retroalimentación; luego el Hito 3 (permisos, conexión MQTT, datos, comandos y notificaciones). Antes, ~~preparar los Hitos 1 y 2 (problemática, medio inalámbrico justificado, mensajes y mockup), que revisa el docente antes de programar.~~

Lo que sigue es de la sesión del 2026-11-13:


- **hertzbench funciona en la MacBook Air M2 de 16 GB** (Mac14,2, SSD de 256 GB, macOS 27.0.1). No es la M2 de 24 GB de las notas de equipos. Se compiló desde el código fuente (commit `19847355`) y quedó registrado como MCP en Claude Code.
- **El puente MQTT está probado.** El escenario [hertzbench/riego-mqtt.yaml](../hertzbench/riego-mqtt.yaml) tiene un broker simulado, un nodo de riego simulado y el puente en `127.0.0.1:1883`. Se probó con mosquitto desde el Mac en los dos sentidos: PASS.
- **Hay dos reportes para el profesor, sin enviar,** en [hertzbench/reportes/](../hertzbench/reportes/):
  1. Un error de compilación con el Deno de Homebrew.
  2. La prueba del puente MQTT en macOS. Le falta la parte del emulador Android, que es lo que pidió el profesor en el #57.
- **Aún no hay código Android.** El repo de la app (`RiegoIoT-App`) no está creado.
- **Android Studio se instaló en la M2 de 16 GB, pero ese Mac ya no se usa.** En el Mac nuevo lo instala `preparar-mac.sh`; después falta abrirlo para descargar el SDK y crear el emulador.

## 0. Retomar en otro Mac

**La MacBook Air M2 de 16 GB ya no se usa** (2026-11-13). Lo que se hizo ahí quedó en git; en el Mac nuevo hay que volver a preparar lo que no vive en el repo: el binario de hertzbench, Deno, mosquitto y Android Studio con su SDK.

```bash
# 1. Repo y guía del profesor
git clone https://github.com/rodrigocutinoa/ProyectRiego.git ~/Dev/Inacap/AppIoT/ProyectRiego
cd ~/Dev/Inacap/AppIoT/ProyectRiego
# copiar la guía del profesor a _Document/ (no se sube)

# 2. Todo lo demás en un paso: Deno, mosquitto, hertzbench (clonar, compilar,
#    enlazar en ~/.local/bin), prueba del puente MQTT y Android Studio
hertzbench/preparar-mac.sh

# 3. Abrir el proyecto
code ~/Dev/Inacap/AppIoT/ProyectRiego
```

- **El MCP se carga solo.** El archivo [.mcp.json](../.mcp.json) del repo registra `hertzbench mcp --workspace .`. Al abrir Claude Code en la carpeta, hay que aprobar el servidor `hertzbench` la primera vez. Para comprobarlo se usa `/mcp`. Solo funciona si `hertzbench` está en el PATH, lo que deja listo el paso 2.
- **Android Studio:** se abre una vez (`open -a "Android Studio"`), se elige el asistente **Standard** y se aceptan las licencias, para descargar el SDK (unos 3 GB). Después se crea un solo emulador arm64.
- **Primer mensaje a Claude Code:** *"lee docs/RETOMAR.md y sigamos"*.
- **El script se puede repetir.** Sirve para Apple Silicon e Intel: actualiza hertzbench y vuelve a compilar. `SIN_ANDROID_STUDIO=1` omite Android Studio. Si `~/.local/bin` no está en el PATH, el script avisa cómo agregarlo.
- **Si algo falla,** el log queda en `hertzbench/logs/preparar_<fecha>.log` y los de la prueba del puente en `hertzbench/logs/<fecha>/`. La carpeta `logs/` no se sube porque tiene rutas locales. Con esos logs se escribe el reporte en `hertzbench/reportes/`, en el mismo formato de los que ya están.
- **Un Mac Intel** usaría `hertzbench-macos-x64`, que la comunidad todavía no ha probado. Si funciona, vale la pena reportarlo.
- **Para enviar los reportes a hertzbench** hace falta `gh` con sesión iniciada (`brew install gh && gh auth login`).
- **Para probar el puente a mano:** `hertzbench/probar-puente.sh [escenario] [segundos]`.

## 1. Decisiones

| # | Decisión | Estado |
|---|---|---|
| 1 | Descargar hertzbench y registrarlo como MCP | **Hecho** (2026-11-13), compilado desde el código fuente |
| 2 | Nodo de riego: rol de la app o ESP32 simulado | Rol de la app (el ESP32 en Mac no está probado, issue #54). Mientras no exista la app, el nodo es scripted en `riego-mqtt.yaml` |
| 3 | Dónde va el código Android | **Un repo aparte por persona**, porque la evaluación es individual; ProyectRiego queda como repo común. Nombre propuesto `RiegoIoT-App`; falta confirmarlo y crearlo en GitHub |
| 4 | Quién instala Android Studio | `preparar-mac.sh` en cualquier Mac; ustedes en la ASUS |

**Repo de la app (cuando se cree):**
- Proyecto en la raíz del repo, con minSdk 26, Kotlin DSL y Compose.
- Un `.mcp.json` con `"command": "hertzbench", "args": ["mcp", "--workspace", "."]`. Así Claude Code carga hertzbench solo en cualquier Mac que tenga `hertzbench` en el PATH.
- La app se conecta a `tcp://10.0.2.2:1883` desde el emulador.

## 2. Pendiente: OpenSSH en Windows

Al instalar el servidor OpenSSH con `Add-WindowsCapability` apareció el error **`0x800f0950`**: Windows no pudo descargar la función desde Windows Update, así que el servicio `sshd` no quedó instalado.

**Solución recomendada** (PowerShell como administrador):

```powershell
winget install --id Microsoft.OpenSSH.Preview -e
# cerrar y volver a abrir PowerShell como administrador
Start-Service sshd
Set-Service sshd -StartupType Automatic
New-NetFirewallRule -Name sshd -DisplayName "OpenSSH Server" -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 22
```

**Alternativas:**
- Configuración → Sistema → Características opcionales → "Servidor OpenSSH".
- Reiniciar el equipo y terminar las actualizaciones pendientes.
- En un equipo con WSUS, poner `RepairContentServerSource = 2` en `HKLM\SOFTWARE\Policies\Microsoft\Windows\Servicing`.

**Llave SSH:** si el usuario de Windows es administrador, la llave pública va en `C:\ProgramData\ssh\administrators_authorized_keys`, con permisos solo para Administradores y SYSTEM.

## 3. Pendiente: conectarse directo a la máquina virtual

Hoy la máquina virtual está en la red NAT de VMware del equipo anfitrión. Desde el Mac no hay ruta hacia ella, así que solo se llega pasando por ese equipo. Hay tres opciones:

1. **Instalar Tailscale dentro de la máquina virtual** con la misma cuenta. Es la opción recomendada: tendría su propia IP `100.x` desde cualquier red.
2. **Poner la red de la máquina virtual en modo puente (bridged)**, para que reciba una IP de la red local. Solo funciona desde la misma red.
3. **Que el anfitrión anuncie la subred de VMware** con `--advertise-routes`. El tráfico seguiría pasando por el anfitrión.

**Falta confirmar:**
- ¿La máquina virtual es el Windows donde falló OpenSSH?
- ¿Qué usuario tiene?
- ¿Qué IP queda una vez conectada?

Después de eso: instalar `sshd` en la máquina virtual, copiar la llave pública y agregar un alias en `~/.ssh/config`.

## 4. Notas de equipos

- **Requisitos de Android Studio y equipos:** están en EVA2_AVANCE.md, sección 5.
- **ASUS UX8402Z:**
  - Revisar si tiene RTX 3050 Ti o solo Iris Xe: Administrador de tareas → Rendimiento → GPU. El mínimo oficial para el emulador es una GPU con 4 GB de VRAM.
  - Revisar que diga "Virtualización: Habilitado" en Administrador de tareas → Rendimiento → CPU.
- **Una M2 con 16 GB de RAM y 256 GB de SSD sirve,** con un solo emulador más un teléfono físico. Dos emuladores a la vez la dejan sin RAM. Conviene:
  - Darle 2 GB de RAM al emulador.
  - Poner `org.gradle.jvmargs=-Xmx2g`.
  - Instalar una sola imagen del sistema.
  - Mantener al menos 30 GB libres.

## 5. Orden sugerido para la próxima sesión

1. En el Mac nuevo, seguir la sección 0.
2. ~~Instalar hertzbench y registrar el MCP.~~ Hecho el 2026-11-13.
3. ~~Crear un escenario con un broker MQTT simulado y el puente expuesto en `1883`.~~ Hecho el 2026-11-13: [hertzbench/riego-mqtt.yaml](../hertzbench/riego-mqtt.yaml).
4. ~~Terminar de preparar Android Studio y crear el emulador.~~ Hecho el 2026-11-16 en la M2 de 24 GB.
5. ~~Confirmar el nombre `RiegoIoT-App`, crear el repo y el proyecto con su `.mcp.json`.~~ Hecho el 2026-11-16.
6. Implementar el login y la notificación cuando el riego se enciende o se apaga. Probar en el emulador por `10.0.2.2:1883` con `riego-mqtt.yaml` corriendo.
7. ~~Completar y enviar los dos reportes a hertzbench.~~ Enviados el 2026-11-16: #109 y #110.
8. Resolver OpenSSH y el acceso directo a la máquina virtual (secciones 2 y 3), si siguen haciendo falta.
