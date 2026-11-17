# Mejora: broker MQTT con keep-alive, Last Will y sesión duplicada

Borrador para hertzbench. Estado: **sin enviar** (2026-11-17).
Título: MQTT broker: keep-alive expiry, will message and client-id takeover / keep-alive, Last Will y sesión duplicada

---

**English**

While testing an Android app (Paho MQTT, keep-alive 5 s, `cleanSession`, automatic reconnect) against a scripted broker through the IP bridge, an `inject_fault` cut of 36 s (`phone->*` and `*->phone`, 100 % loss) showed two gaps. The header of `orchestrator/boards/drivers/tcp-services.ts` already says "No will, no sessions", so this is an enhancement request with the evidence, not a bug.

Environment: macOS 26.7.3 arm64, hertzbench `7f5bd4ae` over MCP, scenario [`riego-broker.yaml`](https://github.com/rodrigocutinoa/ProyectRiego/blob/main/hertzbench/riego-broker.yaml), app [RiegoIoT-App `fccace5`](https://github.com/rodrigocutinoa/RiegoIoT-App/commit/fccace5).

**1. The broker never notices a silent client (keep-alive) and has no will**

The app declares keep-alive 5 s and a will (`riego/estado` → `ESTADO;enlace;CAIDO`). During the 36 s cut the broker logged nothing: no disconnect after 1.5 keep-alives (MQTT 3.1.1 §3.1.2.10) and no will publish. Other subscribers (a panel) cannot learn that the node is gone, which is the usual way to show "node offline" in an IoT app.

```shell
[56846.292] mqtt publish riego/cmd "CMD;bomba;ON" from mcp    # cut starts at 56851
[101964.667] mqtt connect riegoiot-nodo-4ea99955 from 192.168.4.250   # next broker line, after the link is back
```

**2. Same client id reconnects: the old connection stays open and its late frames are accepted**

After the link came back, the app reconnected with the same client id and published its current state (safe mode, pump OFF). About 5.7 s later, frames from the **old** TCP connection, queued during the cut, were delivered and published, so subscribers saw a stale `ESTADO;bomba;ON` after the fresh `OFF`:

```shell
[101964.667] mqtt connect riegoiot-nodo-4ea99955 from 192.168.4.250
[102187.616] mqtt publish riego/estado "ESTADO;modo;SEGURO" from riegoiot-nodo-4ea99955
[102486.850] mqtt publish riego/estado "ESTADO;bomba;OFF" from riegoiot-nodo-4ea99955
[107867.724] mqtt publish riego/estado "ESTADO;modo;MANUAL" from riegoiot-nodo-4ea99955   # stale, old connection
[107867.872] mqtt publish riego/estado "ESTADO;bomba;ON" from riegoiot-nodo-4ea99955     # stale, old connection
[107867.986] mqtt publish riego/ack "ACK;bomba;ON" from riegoiot-nodo-4ea99955          # stale, old connection
```

MQTT 3.1.1 §3.1.4 (MQTT-3.1.4-2): if the client id is already connected, the server must disconnect the existing client. A real broker would have closed the old connection at 101964, and its queued frames would never be published.

**Proposal**

- Close a session silent for 1.5× its declared keep-alive, and publish its will (if any) when the session ends without DISCONNECT (keep-alive expiry or the link closing).
- On CONNECT with a client id that already has a session, close the old connection first.
- A one-line serial for each, e.g. `mqtt keep-alive expired <id>`, `mqtt will <topic> from <id>`, `mqtt takeover <id>`, so a test can assert on them.

Both matter for fault-injection labs: without them, a student's "node offline" alarm and safe-state logic cannot be shown against the simulated broker.

---

**Español:** el broker MQTT scripted no aplica el keep-alive del cliente ni publica su *Last Will*, así que el panel no se entera de que el nodo cayó. Y cuando el cliente se reconecta con el mismo ID, la conexión antigua sigue abierta y sus mensajes atrasados se publican después del estado nuevo (MQTT 3.1.1 §3.1.4 pide cerrar la sesión antigua). Propongo implementar ambas cosas, con una línea en el serial para cada una.
