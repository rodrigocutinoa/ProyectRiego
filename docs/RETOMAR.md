# Retomar desde aquí (2026-11-03)

Notas de la última sesión para seguir sin perder contexto. El detalle completo está en [EVA2_AVANCE.md](EVA2_AVANCE.md).

## Dónde quedamos

- Revisamos la guía del profesor (Unidad 2) y hertzbench. La arquitectura propuesta es una app Android con dos roles, Nodo de riego y Panel, conectados por MQTT a través del puente IP de hertzbench.
- Aún **no se ha escrito código Android** ni se ha instalado hertzbench.
- Se abrieron los issues #54–#57 en hertzbench. El profesor cerró el #57, que agregó a su documentación el acceso del emulador Android por `10.0.2.2` y la alternativa MQTT a BLE en macOS. Pidió un reporte de ejecución real.

## 1. Decisiones que faltan (bloquean la implementación)

| # | Decisión | Recomendación |
|---|---|---|
| 1 | Descargar hertzbench y registrarlo como MCP | Sí, en la MacBook Air M2 (`hertzbench-macos-arm64`) |
| 2 | Nodo de riego: rol de la app o ESP32 simulado | Rol de la app (el ESP32 en Mac no está probado, issue #54) |
| 3 | Dónde va el código Android | Un repo por persona (la evaluación es individual). Definir el nombre, por ejemplo `RiegoIoT-App` |
| 4 | Quién instala Android Studio | Claude en la M2 (`brew install --cask android-studio`); ustedes en la ASUS y la MacBook Pro |

Para destrabar todo basta responder "sí a todo" más el nombre del repo.

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

## 4. Notas de equipos agregadas en esta sesión

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

1. Responder las 4 decisiones de la sección 1.
2. Instalar hertzbench y registrar el MCP; luego reiniciar Claude Code.
3. Crear un escenario con un broker MQTT simulado y el puente expuesto en `1883`.
4. Instalar Android Studio y crear el proyecto (minSdk 26, Kotlin DSL, Compose).
5. Implementar el login y la notificación cuando el riego se enciende o se apaga. Probar en el emulador por `10.0.2.2:1883`.
6. Enviar el reporte de prueba a hertzbench, como pidió el profesor.
7. Resolver OpenSSH y el acceso directo a la máquina virtual (secciones 2 y 3), si siguen haciendo falta.
