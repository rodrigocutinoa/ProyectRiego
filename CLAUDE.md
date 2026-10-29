# ProyectRiego – contexto para Claude Code

Proyecto del curso Aplicaciones Móviles para IoT (TI3042, INACAP). Lo hacen Rodrigo Cutiño y Jenniffer Gonzalez.

- **EVA1 (terminada):** prototipo de riego con ESP32 + MQTT en Wokwi (`esp32_wokwi/`) y una versión con Arduino UNO + HC-05 (`arduino_uno_hc05/`).
- **EVA2 (en curso):** app Android en Kotlin con Compose, con login y alertas y estado del riego, simulada con [hertzbench](https://github.com/Rixmerz/hertzbench) por MCP.

**Antes de seguir, lee [docs/EVA2_AVANCE.md](docs/EVA2_AVANCE.md).** Ahí están los requisitos, la arquitectura propuesta, las decisiones pendientes, la preparación de una máquina nueva y la bitácora.

## Convenciones

- Se responde y se documenta en español.
- El repo es **público**. No se suben la guía del profesor (`_Document/`) ni `secretos.h`.
- Al cerrar cada sesión, se agrega una fila a la bitácora de `docs/EVA2_AVANCE.md`.
- Commits por hito, con mensajes como "Hito 3: conexión MQTT entre nodo y panel".
