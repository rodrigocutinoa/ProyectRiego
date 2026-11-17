# Respuesta al issue #1 de RiegoIoT-App (profesor)

Borrador. Estado: **sin enviar** (2026-11-17). Se publica como comentario en https://github.com/rodrigocutinoa/RiegoIoT-App/issues/1

---

Hola profesor, gracias por la revisión. Le cuento el avance:

**Hito 3 funcionando** ([`fccace5`](https://github.com/rodrigocutinoa/RiegoIoT-App/commit/fccace5)):
- Permisos `INTERNET`, `POST_NOTIFICATIONS` y `ACCESS_LOCAL_NETWORK`. Este último es nuevo en Android 17 y sin él la app no llega a `10.0.2.2`.
- Conexión MQTT (Paho) al puente de hertzbench. Una sola app con dos roles, Panel y Nodo de riego.
- El panel muestra en vivo la humedad, la temperatura, la bomba, el modo y el umbral, y envía comandos. El nodo confirma cada comando con `ACK`.
- **Notificación cada vez que la bomba se enciende o se apaga** (requisito mínimo), además de "Enlace perdido" y "Comando recibido".
- Estado seguro: si se corta el enlace, el nodo apaga la bomba y pasa a `SEGURO`.

Lo probé contra hertzbench por MCP, con `mqtt_publish` para los comandos e `inject_fault` para cortar el enlace. Las capturas están en `docs/capturas/`. El reporte de la prueba con la app quedó en hertzbench, junto con una nota sobre Android 17 y una mejora para el broker MQTT (*keep-alive*, *Last Will* y la sesión duplicada al reconectar).

**Sobre la pareja:** la EVA2 la hacemos en pareja con Jenniffer Gonzalez, igual que la Unidad 1, y este repositorio es de los dos.

**Siguiente:** login con roles operador y observador (Hito 5), completar el modo seguro con confirmación y reenvío, historial (Hito 6), seguridad y pruebas (Hito 7) y el README con capturas.

Saludos,
Rodrigo Cutiño
