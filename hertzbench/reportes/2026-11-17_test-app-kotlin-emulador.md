# Test report: app Kotlin/Compose en el emulador Android 17

Estado: **enviado** el 2026-11-17 como [#116](https://github.com/Rixmerz/hertzbench/issues/116).
Título: [test report] Kotlin/Compose app (Paho MQTT) in the Android 17 emulator through the IP bridge (#110)

---

### Checklist item

A real phone through the IP bridge. (Android **emulator** running a real Kotlin/Compose app with an MQTT client, as asked in #110.)

### Result

Passed

### System

macOS 26.7.3, Apple Silicon (MacBook Air M2, 24 GB). Android Emulator AVD: Android 17 (API 37), `google_apis_playstore` arm64-v8a.

### Hertzbench version or commit

`7f5bd4ae` (the binary the MCP server was started with), driven from Claude Code through MCP: `run_scenario`, `mqtt_publish`, `inject_fault`, `get_serial`.

### What you ran and what it printed

App: [RiegoIoT-App](https://github.com/rodrigocutinoa/RiegoIoT-App) at [`fccace5`](https://github.com/rodrigocutinoa/RiegoIoT-App/commit/fccace5), Kotlin + Jetpack Compose, Eclipse Paho `mqttv3` 1.2.5, `tcp://10.0.2.2:1883`, keep-alive 5 s, automatic reconnect. One app with two roles: **Panel** and **Nodo** (irrigation node).

**1. App as Panel against a scripted node** ([`riego-mqtt.yaml`](https://github.com/rodrigocutinoa/ProyectRiego/blob/main/hertzbench/riego-mqtt.yaml)): the panel showed soil moisture, temperature and pump state live, raised an Android notification on every pump ON/OFF change, and its command reached the scripted node:

```shell
# serial-broker.log
mqtt connect riegoiot-panel-a82ab91d from 192.168.4.250
mqtt subscribe riego/lectura from riegoiot-panel-a82ab91d
# serial-nodo-riego.log
[235027.723] mqtt rx riego/cmd CMD;bomba;OFF
```

**2. App as Nodo, with commands from MCP** ([`riego-broker.yaml`](https://github.com/rodrigocutinoa/ProyectRiego/blob/main/hertzbench/riego-broker.yaml): broker plus a scripted `monitor` subscribed to `riego/#`). `mqtt_publish` played the panel:

```shell
mqtt_publish riego/cmd CMD;bomba;ON   -> ESTADO;modo;MANUAL, ESTADO;bomba;ON, ACK;bomba;ON
mqtt_publish riego/cmd CMD;umbral;40  -> ACK;umbral;40
mqtt_publish riego/cmd CMD;modo;AUTO  -> ESTADO;modo;AUTO, ESTADO;bomba;OFF, ACK;modo;AUTO
mqtt_publish riego/cmd CMD;bomba;BORRAR_TODO -> (dropped by the app, no ACK)
```

**3. Link cut with `inject_fault`** (`phone->*` and `*->phone`, 100 % loss, 36 s): Paho detected it, the app switched the pump OFF and entered its safe mode, and after the link came back it reconnected by itself and published `ESTADO;modo;SEGURO` / `ESTADO;bomba;OFF`.

### Anything else

- **Android 17 needs `ACCESS_LOCAL_NETWORK`** for an app to reach `10.0.2.2`. Without it the connect times out (`SocketTimeoutException ... after 5000ms`), while `adb shell nc 10.0.2.2 1883` works, because the shell is not subject to it. Opened separately as a docs issue.
- Two emulators at once (node + panel) plus the IDE exhausted 24 GB on this Mac (adb hung); one emulator per role, with the other side played by `mqtt_publish`, worked well.
- Two broker gaps showed up in the cut test (no keep-alive or will, stale frames after reconnect). Opened separately as an enhancement.

**Español:** prueba con una app real en Kotlin/Compose (Paho MQTT) en el emulador Android 17 por `10.0.2.2:1883`. Funcionó como panel (datos en vivo, notificaciones, comandos al nodo scripted) y como nodo (comandos desde `mqtt_publish` con ACK, descarte de mensajes inválidos y modo seguro al cortar el enlace con `inject_fault`). Android 17 exige el permiso `ACCESS_LOCAL_NETWORK` para llegar a `10.0.2.2`.
