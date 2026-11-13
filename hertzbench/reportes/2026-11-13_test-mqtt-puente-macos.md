# Test report: MQTT over the IP bridge on macOS arm64 (host client) — PASS

Borrador de *test report* para hertzbench. Estado: **sin enviar**. Falta la parte del emulador
Android por `10.0.2.2:1883`, que es lo que pidió el profesor en el #57. Se puede enviar junto con
esa prueba o por separado.

**English**

`docs/mobile-bridge.md` says MQTT on the IP bridge had not been run. This is a run on macOS
Apple Silicon with a host MQTT client (mosquitto), before testing an app in the Android emulator.

**Environment**

- macOS 27.0.1, Apple Silicon (arm64), MacBook Air M2
- hertzbench built from source at `19847355` (`hertzbench-macos-arm64`, Deno 2.9.8)
- mosquitto clients from Homebrew

**Scenario**

Scripted boards only: a `broker` node that `serve`s `mqtt` on 1883, a `nodo-riego` node that
publishes `riego/lectura` every 5 s and `riego/estado` every 15 s and subscribes to `riego/cmd`, and
the bridge with `listen: 127.0.0.1` and `{ node: broker, port: 1883, service: mqtt, listen_port: 1883 }`.

**Steps**

```bash
hertzbench run riego-mqtt.yaml --out out &
mosquitto_sub -h 127.0.0.1 -p 1883 -t 'riego/#' -v
mosquitto_pub -h 127.0.0.1 -p 1883 -t riego/cmd -m 'CMD;bomba;ON'
# Ctrl-C after 40 s
```

**Result: PASS**

- The bridge listened on `127.0.0.1:1883` (`bridge phone: mqtt broker:1883 at 127.0.0.1:1883`).
- The host subscriber got `riego/lectura` and `riego/estado` from the simulated node.
- The node got the host's publish: `mqtt rx riego/cmd CMD;bomba;ON` in `serial-nodo-riego.log`.
- No firewall prompt on macOS with `listen: 127.0.0.1`.

**Observation (not a bug, just noting it)**

After Ctrl-C (exit 130), the report table is not printed on stdout; only the partial files are
written. With a long `duration` used for interactive testing, there is no summary at the end.

---

**Español**

Primera prueba del puente IP con MQTT en macOS M2, con mosquitto en el Mac haciendo de cliente.
Funcionó en ambos sentidos: el Mac recibió las lecturas y el estado del nodo simulado, y el nodo
recibió el comando publicado desde el Mac. Falta la prueba desde el emulador Android por
`10.0.2.2:1883`.
