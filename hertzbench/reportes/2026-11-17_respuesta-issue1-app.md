# Respuesta al issue #1 de RiegoIoT-App (profesor)

Estado: **enviado** el 2026-11-17: https://github.com/rodrigocutinoa/RiegoIoT-App/issues/1#issuecomment-6949720193

---

Hola profesor, gracias por la revisión. Le cuento el avance, en el orden de su lista:

- [x] **Hito 3** ([`fccace5`](https://github.com/rodrigocutinoa/RiegoIoT-App/commit/fccace5)):
  - Permisos `INTERNET`, `POST_NOTIFICATIONS` y `ACCESS_LOCAL_NETWORK`. Este último es nuevo en Android 17 y sin él la app no llega a `10.0.2.2`.
  - Conexión MQTT (Paho) al puente de hertzbench. Una app con dos roles, Panel y Nodo de riego.
  - Humedad, temperatura, bomba, modo y umbral en vivo, y comandos con confirmación `ACK`.
- [x] **Notificación** cada vez que la bomba se enciende o se apaga, además de "Enlace perdido" y "Comando recibido".
- [x] **Login** (Hito 5, [`a1b79c6`](https://github.com/rodrigocutinoa/RiegoIoT-App/commit/a1b79c6)):
  - Usuarios en SQLite, con la contraseña guardada como PBKDF2-SHA256 (120 000 iteraciones, sal de 16 bytes).
  - El primer usuario es operador y los siguientes, observadores. El observador no puede enviar comandos.
  - 5 intentos fallidos bloquean el ingreso 30 s.
- [x] **Modo seguro** (F5, [`4c5b063`](https://github.com/rodrigocutinoa/RiegoIoT-App/commit/4c5b063)):
  - Si se corta el enlace, el nodo apaga la bomba y pasa a `SEGURO`.
  - El panel reenvía un comando sin confirmar hasta 3 veces y después avisa.
  - El estado queda retenido en el broker, así el panel lo ve apenas se conecta.
- [x] **Historial** (Hito 6, [`5fb2e16`](https://github.com/rodrigocutinoa/RiegoIoT-App/commit/5fb2e16)): lecturas y encendidos o apagados de la bomba en SQLite, con filtro "Solo bomba".
- [ ] **Seguridad y pruebas** (Hito 7) y **demostración** (Hito 8): en curso. Sigo con el cifrado AES-GCM de los mensajes y la tabla de pruebas.
- [x] **README** con cómo correr la app contra hertzbench y capturas.

Todo lo probé en el emulador contra hertzbench por MCP, con `mqtt_publish` para los comandos e `inject_fault` para cortar el enlace. Reportes en hertzbench: prueba con la app ([#116](https://github.com/Rixmerz/hertzbench/issues/116)), nota sobre Android 17 ([#114](https://github.com/Rixmerz/hertzbench/issues/114)) y una mejora para el broker MQTT ([#115](https://github.com/Rixmerz/hertzbench/issues/115)): *keep-alive*, *Last Will* y la sesión duplicada al reconectar.

**Sobre la pareja:** la EVA2 la hacemos en pareja con Jenniffer Gonzalez, igual que la Unidad 1, y este repositorio es de los dos.

Saludos,
Rodrigo Cutiño
