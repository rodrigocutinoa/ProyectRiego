# Retomar desde aquí (2026-11-13)

Notas de la última sesión para seguir sin perder contexto, en este Mac o en otro. El detalle completo está en [EVA2_AVANCE.md](EVA2_AVANCE.md).

## Dónde quedamos

- **hertzbench funciona en la MacBook Air M2 de 16 GB** (Mac14,2, SSD de 256 GB, macOS 27.0.1). No es la M2 de 24 GB de las notas de equipos. Se compiló desde el código fuente (commit `19847355`) y quedó registrado como MCP en Claude Code.
- **El puente MQTT está probado.** El escenario [hertzbench/riego-mqtt.yaml](../hertzbench/riego-mqtt.yaml) tiene un broker simulado, un nodo de riego simulado y el puente en `127.0.0.1:1883`. Se probó con mosquitto desde el Mac en los dos sentidos: PASS.
- **Hay dos reportes para el profesor, sin enviar,** en [hertzbench/reportes/](../hertzbench/reportes/):
  1. Un error de compilación con el Deno de Homebrew.
  2. La prueba del puente MQTT en macOS. Le falta la parte del emulador Android, que es lo que pidió el profesor en el #57.
- **Aún no hay código Android.** El repo de la app (`RiegoIoT-App`) no está creado.
- **Android Studio quedó instalado en la M2 de 16 GB** (`brew install --cask android-studio`), pero todavía no se ha abierto, así que falta descargar el SDK y crear el emulador. Ver el paso 4 de la sección 5.

## 0. Retomar en otro Mac

Lo que vive en git (este repo): los escenarios, los scripts, los reportes y las notas. Lo que **no** está en git y hay que preparar en cada Mac: el binario de hertzbench, el registro del MCP, Android Studio y el SDK.

```bash
# 1. Repo y guía del profesor
git clone https://github.com/rodrigocutinoa/ProyectRiego.git ~/Dev/Inacap/AppIoT/ProyectRiego
cd ~/Dev/Inacap/AppIoT/ProyectRiego
# copiar la guía del profesor a _Document/ (no se sube)

# 2. hertzbench: instala Deno y mosquitto, clona hertzbench al lado del repo,
#    compila el binario para este Mac, lo enlaza en ~/.local/bin y prueba el puente
hertzbench/preparar-mac.sh

# 3. Registrar el MCP en Claude Code (después reiniciar Claude Code)
claude mcp add hertzbench -- hertzbench mcp --workspace "$PWD"

# 4. Android Studio
brew install --cask android-studio
```

- **El script `preparar-mac.sh` sirve para Apple Silicon e Intel,** y se puede repetir: actualiza hertzbench y vuelve a compilar. Si `~/.local/bin` no está en el PATH, el script avisa cómo agregarlo.
- **Si algo falla,** el log queda en `hertzbench/logs/preparar_<fecha>.log` y los de la prueba del puente en `hertzbench/logs/<fecha>/`. La carpeta `logs/` no se sube porque tiene rutas locales. Con esos logs se escribe el reporte en `hertzbench/reportes/`, en el mismo formato de los que ya están.
- **Un Mac Intel** usaría `hertzbench-macos-x64`, que la comunidad todavía no ha probado. Si funciona, vale la pena reportarlo.
- **Para probar el puente a mano:** `hertzbench/probar-puente.sh [escenario] [segundos]`.

## 1. Decisiones

| # | Decisión | Estado |
|---|---|---|
| 1 | Descargar hertzbench y registrarlo como MCP | **Hecho** (2026-11-13), compilado desde el código fuente |
| 2 | Nodo de riego: rol de la app o ESP32 simulado | Rol de la app (el ESP32 en Mac no está probado, issue #54). Mientras no exista la app, el nodo es scripted en `riego-mqtt.yaml` |
| 3 | Dónde va el código Android | **Un repo aparte por persona**, porque la evaluación es individual; ProyectRiego queda como repo común. Nombre propuesto `RiegoIoT-App`; falta confirmarlo y crearlo en GitHub |
| 4 | Quién instala Android Studio | Instalado por Claude en la M2 de 16 GB (2026-11-13); ustedes en la ASUS y la MacBook Pro |

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

1. En un Mac nuevo, seguir la sección 0.
2. ~~Instalar hertzbench y registrar el MCP.~~ Hecho el 2026-11-13.
3. ~~Crear un escenario con un broker MQTT simulado y el puente expuesto en `1883`.~~ Hecho el 2026-11-13: [hertzbench/riego-mqtt.yaml](../hertzbench/riego-mqtt.yaml).
4. Terminar de preparar Android Studio: abrirlo una vez para que descargue el SDK, y crear un emulador (una sola imagen arm64, 2 GB de RAM).
5. Confirmar el nombre `RiegoIoT-App`, crear el repo y el proyecto con su `.mcp.json`.
6. Implementar el login y la notificación cuando el riego se enciende o se apaga. Probar en el emulador por `10.0.2.2:1883` con `riego-mqtt.yaml` corriendo.
7. Completar el reporte del puente con la prueba del emulador y enviar los dos reportes a hertzbench. Antes hay que instalar `gh` (`brew install gh`) y aprobar el envío, porque se publican con la cuenta de Rodrigo.
8. Resolver OpenSSH y el acceso directo a la máquina virtual (secciones 2 y 3), si siguen haciendo falta.
