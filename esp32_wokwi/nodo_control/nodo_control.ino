/*
  ============================================================================
  EVA1 - TI3V42 Aplicaciones Moviles para IoT - INACAP
  Caso: Monitoreo de temperatura / humedad con activacion de riego
  Version ESP32 + WiFi (MQTT), pensada para simular en Wokwi
  ----------------------------------------------------------------------------
  NODO DE CONTROL (microcontrolador 2)

  - Recibe por WiFi (MQTT) las lecturas del nodo sensor, verifica su firma,
    las interpreta y las muestra en el monitor serial y en LEDs.
  - Envia comandos para cambiar el umbral y encender / apagar la bomba de forma
    manual. Los comandos se escriben en el monitor serial. Escribe "ayuda".
  - Envia un PING cada 2 s para que el nodo sensor sepa que el enlace esta vivo.
  - Muestra la confirmacion ($ACK) o el rechazo ($NAK) de cada comando.
  - "prueba" envia comandos invalidos para demostrar que el nodo sensor los
    rechaza.

  LEDs: verde = enlace OK, azul = bomba encendida, rojo = sin enlace, falla,
  modo seguro o comando rechazado.

  Libreria: "PubSubClient".
  ============================================================================
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>

#if __has_include("secretos.h")
#include "secretos.h"
#else
#warning "No existe secretos.h: se usan valores de ejemplo. Copia secretos.example.h como secretos.h."
#endif
#ifndef CLAVE_COMPARTIDA
#define CLAVE_COMPARTIDA "CAMBIAR_ESTA_CLAVE"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID "Wokwi-GUEST"
#endif
#ifndef WIFI_CLAVE
#define WIFI_CLAVE ""
#endif
#ifndef ID_GRUPO
#define ID_GRUPO "grupo-cambiar"
#endif

// ---------------------------------------------------------------------------
// MQTT (debe coincidir con el nodo sensor)
// ---------------------------------------------------------------------------
#define USAR_TLS 0
#define MQTT_BROKER "broker.hivemq.com"

#if USAR_TLS
#include <WiFiClientSecure.h>
const uint16_t MQTT_PUERTO = 8883;
WiFiClientSecure clienteRed;
#else
const uint16_t MQTT_PUERTO = 1883;
WiFiClient clienteRed;
#endif

#define TOPIC_BASE "inacap/ti3v42/riego/" ID_GRUPO
const char *TOPIC_TELEMETRIA = TOPIC_BASE "/telemetria";
const char *TOPIC_COMANDOS   = TOPIC_BASE "/comandos";
const char *TOPIC_ESTADO     = TOPIC_BASE "/estado";

// ---------------------------------------------------------------------------
// Pines
// ---------------------------------------------------------------------------
const uint8_t PIN_LED_ALARMA = 25;  // Rojo
const uint8_t PIN_LED_ENLACE = 26;  // Verde
const uint8_t PIN_LED_BOMBA  = 27;  // Azul

// ---------------------------------------------------------------------------
// Tiempos (ms)
// ---------------------------------------------------------------------------
const unsigned long INTERVALO_PING_MS  = 2000;
const unsigned long TIMEOUT_DATOS_MS   = 6000;
const unsigned long TIMEOUT_ACK_MS     = 3000;
const unsigned long PULSO_ALARMA_MS    = 1500;
const unsigned long PASO_PRUEBA_MS     = 400;
const unsigned long REINTENTO_MQTT_MS  = 5000;
const unsigned long FIN_LINEA_MS       = 300;   // Consola sin "\n": se procesa igual

const uint8_t UMBRAL_MIN = 10;
const uint8_t UMBRAL_MAX = 90;

// Secuencia persistente por bloques (nunca se repite aunque se reinicie)
const uint32_t BLOQUE_SEQ = 1000;

const uint8_t LARGO_MAX_TRAMA   = 64;
const uint8_t LARGO_MAX_CONSOLA = 72;
const uint8_t MAX_CAMPOS        = 11;

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
PubSubClient mqtt(clienteRed);
Preferences  preferencias;

uint32_t secuencia = 0;
uint32_t limiteReservado = 0;

uint32_t      pendienteSeq = 0;
unsigned long pendienteMs  = 0;
uint32_t      ultimaSeqEnviada = 0;

bool          enlaceOk = false;
unsigned long ultimoDatoMs = 0;
unsigned long ultimoPingMs = 0;
unsigned long alarmaHastaMs = 0;
bool          mostrarLecturas = true;
uint16_t      tramasDescartadas = 0;
unsigned long ultimoIntentoMqttMs = 0;
bool          wifiConectado = false;

bool    bombaRemota = false;
bool    fallaRemota = false;
bool    modoSeguroRemoto = false;

uint8_t       pasoPrueba = 0;
unsigned long proximoPasoMs = 0;

char          bufferConsola[LARGO_MAX_CONSOLA + 1];
uint8_t       largoConsola = 0;
unsigned long ultimoCaracterMs = 0;

// ===========================================================================
// Firma (identica a la del nodo sensor)
// ===========================================================================
uint16_t calcularFirma(const char *datos, size_t largo) {
  uint32_t h = 2166136261UL;
  for (const char *p = CLAVE_COMPARTIDA; *p; p++) {
    h ^= (uint8_t)*p;
    h *= 16777619UL;
  }
  for (size_t i = 0; i < largo; i++) {
    h ^= (uint8_t)datos[i];
    h *= 16777619UL;
  }
  return (uint16_t)((h >> 16) ^ (h & 0xFFFF));
}

// Publica "$<contenido>*<FIRMA>" en el topic de comandos. Con alterarFirma =
// true se envia una firma incorrecta (solo para la prueba de rechazo).
bool enviarTrama(const char *contenido, bool alterarFirma = false) {
  uint16_t firma = calcularFirma(contenido, strlen(contenido));
  if (alterarFirma) firma ^= 0xFFFF;
  char trama[LARGO_MAX_TRAMA + 8];
  snprintf(trama, sizeof(trama), "$%s*%04X", contenido, (unsigned int)firma);
  return mqtt.connected() && mqtt.publish(TOPIC_COMANDOS, trama);
}

// ===========================================================================
// Secuencia
// ===========================================================================
void iniciarSecuencia() {
  uint32_t guardado = preferencias.getULong("seq", 0);
  secuencia = guardado;
  limiteReservado = guardado + BLOQUE_SEQ;
  preferencias.putULong("seq", limiteReservado);
}

uint32_t siguienteSecuencia() {
  secuencia++;
  if (secuencia >= limiteReservado) {
    limiteReservado += BLOQUE_SEQ;
    preferencias.putULong("seq", limiteReservado);
  }
  return secuencia;
}

// ===========================================================================
// Comandos hacia el nodo sensor
//   $CMD,<seq>,<COMANDO>[,<VALOR>]*FIRMA
// ===========================================================================
uint32_t enviarComando(const char *comando, const char *valor, bool esperarAck,
                       bool alterarFirma = false, uint32_t seqForzada = 0) {
  uint32_t seq = seqForzada ? seqForzada : siguienteSecuencia();
  char contenido[LARGO_MAX_TRAMA];
  if (valor != NULL) {
    snprintf(contenido, sizeof(contenido), "CMD,%lu,%s,%s", (unsigned long)seq, comando, valor);
  } else {
    snprintf(contenido, sizeof(contenido), "CMD,%lu,%s", (unsigned long)seq, comando);
  }
  bool enviado = enviarTrama(contenido, alterarFirma);

  if (esperarAck) {
    if (!enviado) {
      Serial.println("[AVISO] Sin conexion MQTT: el comando no se pudo enviar");
    } else {
      pendienteSeq = seq;
      pendienteMs = millis();
      Serial.printf(">> Enviado: $%s\n", contenido);
    }
  }
  if (!seqForzada) ultimaSeqEnviada = seq;
  return seq;
}

// ===========================================================================
// Interpretacion de tramas recibidas
// ===========================================================================
uint8_t dividirCampos(char *s, char *campos[], uint8_t max) {
  uint8_t n = 0;
  campos[n++] = s;
  for (char *p = s; *p; p++) {
    if (*p == ',') {
      if (n >= max) return 0;
      *p = '\0';
      campos[n++] = p + 1;
    }
  }
  return n;
}

String textoFallas(uint8_t fallas) {
  if (fallas == 0) return "ninguna";
  String s;
  if (fallas & 0x01) s += "SENSOR_AIRE ";
  if (fallas & 0x02) s += "SENSOR_SUELO ";
  if (fallas & 0x04) s += "TIEMPO_MAX_RIEGO ";
  return s;
}

// $DAT,<n>,<temp>,<hr>,<suelo>,<umbral>,<modo>,<bomba>,<fallas>,<rechazos>
void mostrarLectura(char *campos[], uint8_t n) {
  if (n != 10) {
    tramasDescartadas++;
    Serial.println("[DESCARTADO] Trama DAT con cantidad de campos incorrecta");
    return;
  }

  ultimoDatoMs = millis();
  if (!enlaceOk) {
    enlaceOk = true;
    Serial.println("[ENLACE] Conectado con el nodo sensor");
  }

  char modo = campos[6][0];
  uint8_t fallas = atoi(campos[8]);
  bombaRemota = strcmp(campos[7], "1") == 0;
  fallaRemota = fallas != 0;
  modoSeguroRemoto = modo == 'S';

  if (!mostrarLecturas) return;

  Serial.printf("[LECTURA #%s] Temp: %s C | HR aire: %s %% | Suelo: %s %% | Umbral: %s %% | "
                "Modo: %s | Bomba: %s | Fallas: %s | Rechazos: %s\n",
                campos[1], campos[2], campos[3], campos[4], campos[5],
                modo == 'A' ? "AUTO" : modo == 'M' ? "MANUAL" : "SEGURO",
                bombaRemota ? "ENCENDIDA" : "apagada",
                textoFallas(fallas).c_str(), campos[9]);
}

void procesarTrama(char *trama) {
  char *asterisco = strrchr(trama, '*');
  if (asterisco == NULL || strlen(asterisco + 1) != 4) {
    tramasDescartadas++;
    Serial.println("[DESCARTADO] Trama sin firma");
    return;
  }
  uint16_t firmaRecibida = (uint16_t)strtoul(asterisco + 1, NULL, 16);
  if (calcularFirma(trama, asterisco - trama) != firmaRecibida) {
    tramasDescartadas++;
    Serial.println("[DESCARTADO] Firma invalida: la trama no viene del nodo sensor o llego corrupta");
    return;
  }
  *asterisco = '\0';

  char *campos[MAX_CAMPOS];
  uint8_t n = dividirCampos(trama, campos, MAX_CAMPOS);
  if (n == 0) {
    tramasDescartadas++;
    return;
  }

  if (strcmp(campos[0], "DAT") == 0) {
    mostrarLectura(campos, n);

  } else if (strcmp(campos[0], "ACK") == 0 && n == 4) {
    uint32_t seq = strtoul(campos[1], NULL, 10);
    if (seq == pendienteSeq) pendienteSeq = 0;
    if (strcmp(campos[2], "PING") != 0) {
      Serial.printf("<< CONFIRMADO por el nodo sensor: seq=%s %s=%s\n", campos[1], campos[2], campos[3]);
    }

  } else if (strcmp(campos[0], "NAK") == 0 && n == 3) {
    uint32_t seq = strtoul(campos[1], NULL, 10);
    if (seq == pendienteSeq || seq == 0) pendienteSeq = 0;
    alarmaHastaMs = millis() + PULSO_ALARMA_MS;
    Serial.printf("<< RECHAZADO por el nodo sensor: seq=%s motivo=%s\n", campos[1], campos[2]);

  } else {
    tramasDescartadas++;
    Serial.println("[DESCARTADO] Tipo de trama desconocido");
  }
}

void alRecibirMqtt(char *topic, byte *payload, unsigned int largo) {
  if (strcmp(topic, TOPIC_ESTADO) == 0) {
    Serial.printf("[ESTADO NODO SENSOR] %.*s\n", (int)(largo > 16 ? 16 : largo), (const char *)payload);
    return;
  }
  if (strcmp(topic, TOPIC_TELEMETRIA) != 0) return;

  if (largo < 2 || largo > LARGO_MAX_TRAMA + 1 || payload[0] != '$') {
    tramasDescartadas++;
    Serial.println("[DESCARTADO] Trama con formato o largo invalido");
    return;
  }
  char trama[LARGO_MAX_TRAMA + 1];
  memcpy(trama, payload + 1, largo - 1);
  trama[largo - 1] = '\0';
  procesarTrama(trama);
}

// ===========================================================================
// Consola (monitor serial)
// ===========================================================================
void mostrarAyuda() {
  Serial.println("------------------------------------------------------------");
  Serial.println("Comandos disponibles:");
  Serial.println("  umbral <10-90>  Cambia el umbral de humedad del suelo (%)");
  Serial.println("  on              Enciende la bomba (pasa a modo MANUAL)");
  Serial.println("  off             Apaga la bomba (pasa a modo MANUAL)");
  Serial.println("  auto            Vuelve al modo AUTOMATICO");
  Serial.println("  manual          Pasa a modo MANUAL sin cambiar la bomba");
  Serial.println("  pausa           Oculta / muestra las lecturas");
  Serial.println("  estado          Estado del enlace y contadores");
  Serial.println("  prueba          Envia 4 comandos invalidos (demo de rechazo)");
  Serial.println("  raw <trama>     Publica una trama tal cual, ej: raw $CMD,1,BOMBA,ON*0000");
  Serial.println("------------------------------------------------------------");
}

void mostrarEstado() {
  Serial.printf("[ESTADO] WiFi: %s | MQTT: %s | Enlace: %s | Secuencia: %lu | Descartadas: %u\n",
                wifiConectado ? "OK" : "NO", mqtt.connected() ? "OK" : "NO",
                enlaceOk ? "OK" : "SIN CONEXION", (unsigned long)secuencia, tramasDescartadas);
  Serial.printf("[ESTADO] Topic base: %s\n", TOPIC_BASE);
}

void cambiarUmbral(const char *valor) {
  // Validacion local (defensa en profundidad); el nodo sensor vuelve a validar
  uint8_t digitos = 0;
  for (const char *p = valor; *p; p++, digitos++) {
    if (*p < '0' || *p > '9') digitos = 99;
  }
  int numero = atoi(valor);
  if (digitos == 0 || digitos > 3 || numero < UMBRAL_MIN || numero > UMBRAL_MAX) {
    Serial.println("[LOCAL] Umbral invalido. Debe ser un numero entre 10 y 90.");
    return;
  }
  char texto[4];
  snprintf(texto, sizeof(texto), "%d", numero);
  enviarComando("UMBRAL", texto, true);
}

void ejecutarConsola(char *linea) {
  while (*linea == ' ') linea++;
  for (int i = strlen(linea) - 1; i >= 0 && linea[i] == ' '; i--) linea[i] = '\0';
  if (*linea == '\0') return;

  if (strcasecmp(linea, "ayuda") == 0 || strcmp(linea, "?") == 0) {
    mostrarAyuda();
  } else if (strncasecmp(linea, "umbral ", 7) == 0) {
    cambiarUmbral(linea + 7);
  } else if (strcasecmp(linea, "on") == 0 || strcasecmp(linea, "bomba on") == 0) {
    enviarComando("BOMBA", "ON", true);
  } else if (strcasecmp(linea, "off") == 0 || strcasecmp(linea, "bomba off") == 0) {
    enviarComando("BOMBA", "OFF", true);
  } else if (strcasecmp(linea, "auto") == 0) {
    enviarComando("MODO", "AUTO", true);
  } else if (strcasecmp(linea, "manual") == 0) {
    enviarComando("MODO", "MANUAL", true);
  } else if (strcasecmp(linea, "pausa") == 0) {
    mostrarLecturas = !mostrarLecturas;
    Serial.println(mostrarLecturas ? "[LOCAL] Lecturas visibles" : "[LOCAL] Lecturas ocultas");
  } else if (strcasecmp(linea, "estado") == 0) {
    mostrarEstado();
  } else if (strcasecmp(linea, "prueba") == 0) {
    Serial.println("[PRUEBA] Enviando comandos invalidos al nodo sensor...");
    pasoPrueba = 1;
    proximoPasoMs = millis();
  } else if (strncasecmp(linea, "raw ", 4) == 0) {
    Serial.printf(">> Publicado sin procesar: %s\n", linea + 4);
    if (mqtt.connected()) mqtt.publish(TOPIC_COMANDOS, linea + 4);
  } else {
    Serial.printf("[LOCAL] Comando desconocido: %s\n", linea);
    Serial.println("Escribe 'ayuda' para ver los comandos.");
  }
}

void leerConsola() {
  while (Serial.available()) {
    char c = Serial.read();
    ultimoCaracterMs = millis();
    if (c == '\r') continue;
    if (c == '\n') {
      bufferConsola[largoConsola] = '\0';
      ejecutarConsola(bufferConsola);
      largoConsola = 0;
    } else if (largoConsola < LARGO_MAX_CONSOLA) {
      bufferConsola[largoConsola++] = c;
    }
  }
  // Algunos monitores no envian fin de linea: se procesa tras una pausa
  if (largoConsola > 0 && millis() - ultimoCaracterMs >= FIN_LINEA_MS) {
    bufferConsola[largoConsola] = '\0';
    ejecutarConsola(bufferConsola);
    largoConsola = 0;
  }
}

// Demo de rechazo, un paso cada PASO_PRUEBA_MS (sin bloquear el programa)
void ejecutarPrueba() {
  if (pasoPrueba == 0 || millis() - proximoPasoMs < PASO_PRUEBA_MS) return;
  proximoPasoMs = millis();

  switch (pasoPrueba) {
    case 1:
      Serial.println("[PRUEBA 1/4] BOMBA ON con firma alterada (como un intruso sin la clave)");
      enviarComando("BOMBA", "ON", true, true);
      break;
    case 2:
      Serial.println("[PRUEBA 2/4] UMBRAL 99, fuera del rango permitido 10-90");
      enviarComando("UMBRAL", "99", true);
      break;
    case 3:
      Serial.println("[PRUEBA 3/4] Comando desconocido ABRIR");
      enviarComando("ABRIR", "1", true);
      break;
    case 4:
      Serial.println("[PRUEBA 4/4] Repeticion de una trama ya usada (ataque de replay)");
      enviarComando("BOMBA", "ON", true, false, ultimaSeqEnviada);
      break;
  }
  pasoPrueba = (pasoPrueba >= 4) ? 0 : pasoPrueba + 1;
}

// ===========================================================================
// Conectividad
// ===========================================================================
void conectarMqtt() {
  char idCliente[32];
  snprintf(idCliente, sizeof(idCliente), "riego-n2-%08lX", (unsigned long)(ESP.getEfuseMac() & 0xFFFFFFFF));

  Serial.printf("[MQTT] Conectando a %s:%u ... ", MQTT_BROKER, MQTT_PUERTO);
  if (mqtt.connect(idCliente)) {
    Serial.println("OK");
    mqtt.subscribe(TOPIC_TELEMETRIA, 1);
    mqtt.subscribe(TOPIC_ESTADO, 1);
  } else {
    Serial.printf("fallo (estado %d), se reintenta en %lu s\n", mqtt.state(), REINTENTO_MQTT_MS / 1000);
  }
}

void gestionarConexion() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConectado) {
      wifiConectado = false;
      Serial.println("[WIFI] Desconectado, reintentando...");
    }
    return;
  }
  if (!wifiConectado) {
    wifiConectado = true;
    Serial.printf("[WIFI] Conectado, IP %s\n", WiFi.localIP().toString().c_str());
  }

  if (!mqtt.connected()) {
    unsigned long ahora = millis();
    if (ultimoIntentoMqttMs == 0 || ahora - ultimoIntentoMqttMs >= REINTENTO_MQTT_MS) {
      ultimoIntentoMqttMs = ahora;
      conectarMqtt();
    }
    return;
  }
  mqtt.loop();
}

// ===========================================================================
// Vigilancia y LEDs
// ===========================================================================
void vigilar() {
  unsigned long ahora = millis();

  if (enlaceOk && ahora - ultimoDatoMs >= TIMEOUT_DATOS_MS) {
    enlaceOk = false;
    Serial.println("[ENLACE] PERDIDO: no llegan datos del nodo sensor");
  }

  if (pendienteSeq != 0 && ahora - pendienteMs >= TIMEOUT_ACK_MS) {
    Serial.printf("[AVISO] El comando seq=%lu no fue confirmado\n", (unsigned long)pendienteSeq);
    pendienteSeq = 0;
    alarmaHastaMs = ahora + PULSO_ALARMA_MS;
  }

  bool alarma = !enlaceOk || fallaRemota || modoSeguroRemoto || (long)(alarmaHastaMs - ahora) > 0;
  digitalWrite(PIN_LED_ENLACE, enlaceOk ? HIGH : LOW);
  digitalWrite(PIN_LED_BOMBA, (enlaceOk && bombaRemota) ? HIGH : LOW);
  digitalWrite(PIN_LED_ALARMA, alarma ? HIGH : LOW);
}

// ===========================================================================
void setup() {
  pinMode(PIN_LED_ALARMA, OUTPUT);
  pinMode(PIN_LED_ENLACE, OUTPUT);
  pinMode(PIN_LED_BOMBA, OUTPUT);

  Serial.begin(115200);
  preferencias.begin("control", false);
  iniciarSecuencia();

  Serial.println("=== NODO DE CONTROL ESP32 - Riego IoT (EVA1 TI3V42) ===");
  Serial.printf("Topic base: %s\n", TOPIC_BASE);
  mostrarAyuda();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_CLAVE);

#if USAR_TLS
  clienteRed.setInsecure();
#endif
  mqtt.setServer(MQTT_BROKER, MQTT_PUERTO);
  mqtt.setCallback(alRecibirMqtt);
  mqtt.setSocketTimeout(3);
  mqtt.setKeepAlive(10);
}

void loop() {
  unsigned long ahora = millis();

  gestionarConexion();
  leerConsola();
  ejecutarPrueba();

  if (mqtt.connected() && ahora - ultimoPingMs >= INTERVALO_PING_MS) {
    ultimoPingMs = ahora;
    enviarComando("PING", NULL, false);
  }

  vigilar();
}
