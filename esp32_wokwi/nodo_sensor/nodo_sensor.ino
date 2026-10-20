/*
  ============================================================================
  EVA1 - TI3V42 Aplicaciones Moviles para IoT - INACAP
  Caso: Monitoreo de temperatura / humedad con activacion de riego
  Version ESP32 + WiFi (MQTT), pensada para simular en Wokwi
  ----------------------------------------------------------------------------
  NODO SENSOR (microcontrolador 1)

  - Lee humedad del suelo (en Wokwi: potenciometro en GPIO34) y temperatura /
    humedad del aire (DHT22) cada 2 s, sin bloquear el programa.
  - Modo AUTOMATICO: enciende la bomba cuando la humedad del suelo baja del
    umbral y la apaga al superar umbral + histeresis.
  - Modo MANUAL: la bomba se enciende / apaga por comando remoto.
  - Modo SEGURO: si se pierde la comunicacion con el nodo de control, la bomba
    se apaga hasta que vuelva un mensaje valido.
  - Publica cada lectura y el estado de la bomba por WiFi en un broker MQTT.
  - Recibe comandos por MQTT, los valida (formato, firma, secuencia, comando y
    rango), confirma cada comando aceptado con $ACK y rechaza / registra los
    invalidos con $NAK.

  Librerias: "DHT sensor library", "Adafruit Unified Sensor", "PubSubClient".
  ============================================================================
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <DHT.h>

// Clave compartida, WiFi e identificador del grupo van en secretos.h
// (no se sube a GitHub). Si falta algun valor se usa el de ejemplo.
#if __has_include("secretos.h")
#include "secretos.h"
#else
#warning "No existe secretos.h: se usan valores de ejemplo. Copia secretos.example.h como secretos.h."
#endif
#ifndef CLAVE_COMPARTIDA
#define CLAVE_COMPARTIDA "CAMBIAR_ESTA_CLAVE"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID "Wokwi-GUEST"   // Red WiFi del simulador Wokwi
#endif
#ifndef WIFI_CLAVE
#define WIFI_CLAVE ""
#endif
#ifndef ID_GRUPO
#define ID_GRUPO "grupo-cambiar"
#endif

// ---------------------------------------------------------------------------
// MQTT
// ---------------------------------------------------------------------------
#define USAR_TLS 0                 // 1 = cifrado TLS (puerto 8883)
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
const char *TOPIC_TELEMETRIA = TOPIC_BASE "/telemetria";  // $DAT, $ACK, $NAK
const char *TOPIC_COMANDOS   = TOPIC_BASE "/comandos";    // $CMD
const char *TOPIC_ESTADO     = TOPIC_BASE "/estado";      // ONLINE / OFFLINE (retenido)

// ---------------------------------------------------------------------------
// Pines (ESP32 DevKit). El sensor de suelo debe ir en un pin del ADC1
// (GPIO32-39): el ADC2 no funciona mientras el WiFi esta activo.
// ---------------------------------------------------------------------------
const uint8_t PIN_DHT     = 15;
const uint8_t PIN_SUELO   = 34;
const uint8_t PIN_RELE    = 26;
const uint8_t PIN_LED_EST = 4;   // Rojo: falla, modo seguro o sin conexion

#define DHT_TIPO DHT22
const bool RELE_ACTIVO_EN_BAJO = false;  // El rele de Wokwi se activa con HIGH

// ---------------------------------------------------------------------------
// Calibracion del sensor de suelo (ADC de 12 bits: 0-4095)
// En Wokwi: potenciometro a la izquierda = seco, a la derecha = mojado.
// Con un sensor capacitivo real los valores se invierten (seco ~2600, agua ~1100).
// ---------------------------------------------------------------------------
const int ADC_SUELO_SECO   = 200;
const int ADC_SUELO_MOJADO = 3900;
const int ADC_MIN_VALIDO   = 50;    // En el extremo = sensor desconectado
const int ADC_MAX_VALIDO   = 4050;  // En el extremo = cortocircuito / falla

// ---------------------------------------------------------------------------
// Tiempos (ms)
// ---------------------------------------------------------------------------
const unsigned long INTERVALO_LECTURA_MS = 2000;
const unsigned long TIMEOUT_ENLACE_MS    = 10000;
const unsigned long BOMBA_MAX_ON_MS      = 60000;
const unsigned long BOMBA_DESCANSO_MS    = 30000;
const unsigned long VENTANA_LIMITE_MS    = 1000;
const uint8_t       MAX_TRAMAS_VENTANA   = 8;
const unsigned long REINTENTO_MQTT_MS    = 5000;

// ---------------------------------------------------------------------------
// Umbral de activacion (% de humedad del suelo)
// ---------------------------------------------------------------------------
const uint8_t UMBRAL_MIN     = 10;
const uint8_t UMBRAL_MAX     = 90;
const uint8_t UMBRAL_DEFECTO = 40;
const uint8_t HISTERESIS     = 5;

// ---------------------------------------------------------------------------
// Protocolo
// ---------------------------------------------------------------------------
const uint8_t LARGO_MAX_TRAMA = 64;
const uint8_t MAX_CAMPOS      = 6;

const uint8_t FALLA_DHT    = 0x01;
const uint8_t FALLA_SUELO  = 0x02;
const uint8_t FALLA_TIEMPO = 0x04;

enum Modo : char { MODO_AUTO = 'A', MODO_MANUAL = 'M', MODO_SEGURO = 'S' };

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
PubSubClient mqtt(clienteRed);
Preferences  preferencias;
DHT dht(PIN_DHT, DHT_TIPO);

Modo     modo = MODO_SEGURO;
bool     bombaEncendida = false;
uint8_t  umbral = UMBRAL_DEFECTO;
uint8_t  fallas = 0;
uint16_t contadorRechazos = 0;
uint32_t numeroLectura = 0;
uint32_t ultimaSecuencia = 0;

float   temperatura = NAN;
float   humedadAire = NAN;
uint8_t humedadSuelo = 0;

unsigned long ultimaLecturaMs   = 0;
unsigned long ultimoRxValidoMs  = 0;
unsigned long inicioBombaMs     = 0;
unsigned long inicioDescansoMs  = 0;
bool          enDescanso = false;
unsigned long inicioVentanaMs   = 0;
uint8_t       tramasEnVentana   = 0;
unsigned long ultimoIntentoMqttMs = 0;
bool          wifiConectado = false;

// ===========================================================================
// Firma de mensajes: FNV-1a de 32 bits sobre (clave compartida + contenido),
// plegada a 16 bits. La clave nunca viaja por la red.
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

// Publica "$<contenido>*<FIRMA>" en el topic de telemetria
void enviarTrama(const char *contenido) {
  char trama[LARGO_MAX_TRAMA + 8];
  snprintf(trama, sizeof(trama), "$%s*%04X", contenido,
           (unsigned int)calcularFirma(contenido, strlen(contenido)));
  if (mqtt.connected()) {
    mqtt.publish(TOPIC_TELEMETRIA, trama);
  }
}

// ===========================================================================
// Actuador
// ===========================================================================
void aplicarBomba(bool encender) {
  if (encender && !bombaEncendida) {
    inicioBombaMs = millis();
  }
  bombaEncendida = encender;
  digitalWrite(PIN_RELE, (encender != RELE_ACTIVO_EN_BAJO) ? HIGH : LOW);
}

const char *nombreModo(Modo m) {
  switch (m) {
    case MODO_AUTO:   return "AUTO";
    case MODO_MANUAL: return "MANUAL";
    default:          return "SEGURO";
  }
}

void cambiarModo(Modo nuevo, const char *razon) {
  if (modo == nuevo) return;
  modo = nuevo;
  Serial.printf("[MODO] %s (%s)\n", nombreModo(modo), razon);
  if (modo == MODO_SEGURO) {
    aplicarBomba(false);
  }
}

// ===========================================================================
// Sensores
// ===========================================================================
void leerSensores() {
  long suma = 0;
  for (uint8_t i = 0; i < 8; i++) {
    suma += analogRead(PIN_SUELO);
  }
  int adc = suma / 8;

  if (adc < ADC_MIN_VALIDO || adc > ADC_MAX_VALIDO) {
    fallas |= FALLA_SUELO;
  } else {
    fallas &= ~FALLA_SUELO;
    long pct = map(adc, ADC_SUELO_SECO, ADC_SUELO_MOJADO, 0, 100);
    humedadSuelo = (uint8_t)constrain(pct, 0, 100);
  }

  temperatura = dht.readTemperature();
  humedadAire = dht.readHumidity();
  if (isnan(temperatura) || isnan(humedadAire)) {
    fallas |= FALLA_DHT;
  } else {
    fallas &= ~FALLA_DHT;
  }
}

// ===========================================================================
// Control automatico con histeresis
// ===========================================================================
void controlAutomatico() {
  if (modo != MODO_AUTO) return;

  if ((fallas & FALLA_SUELO) || enDescanso) {
    aplicarBomba(false);
    return;
  }

  if (!bombaEncendida && humedadSuelo < umbral) {
    aplicarBomba(true);
    Serial.println("[AUTO] Suelo seco -> bomba ENCENDIDA");
  } else if (bombaEncendida && humedadSuelo >= umbral + HISTERESIS) {
    aplicarBomba(false);
    Serial.println("[AUTO] Suelo humedo -> bomba APAGADA");
  }
}

// Enclavamiento: limita el tiempo continuo de riego
void vigilarBomba() {
  unsigned long ahora = millis();

  if (bombaEncendida && ahora - inicioBombaMs >= BOMBA_MAX_ON_MS) {
    aplicarBomba(false);
    enDescanso = true;
    inicioDescansoMs = ahora;
    fallas |= FALLA_TIEMPO;
    Serial.println("[ENCLAVAMIENTO] Tiempo maximo de riego alcanzado -> bomba APAGADA");
  }

  if (enDescanso && ahora - inicioDescansoMs >= BOMBA_DESCANSO_MS) {
    enDescanso = false;
    fallas &= ~FALLA_TIEMPO;
    Serial.println("[ENCLAVAMIENTO] Fin del descanso de la bomba");
  }
}

// Falla segura: sin mensajes validos del nodo de control -> bomba apagada
void vigilarEnlace() {
  if (modo != MODO_SEGURO && millis() - ultimoRxValidoMs >= TIMEOUT_ENLACE_MS) {
    cambiarModo(MODO_SEGURO, "se perdio la comunicacion");
  }
}

// ===========================================================================
// Envio de lecturas
//   $DAT,<n>,<temp C>,<HR aire %>,<suelo %>,<umbral %>,<modo>,<bomba>,<fallas>,<rechazos>*FIRMA
// ===========================================================================
void enviarLectura() {
  char txtTemp[8];
  char txtHum[6];
  if (fallas & FALLA_DHT) {
    strcpy(txtTemp, "NA");
    strcpy(txtHum, "NA");
  } else {
    snprintf(txtTemp, sizeof(txtTemp), "%.1f", temperatura);
    snprintf(txtHum, sizeof(txtHum), "%.0f", humedadAire);
  }

  char contenido[LARGO_MAX_TRAMA];
  snprintf(contenido, sizeof(contenido), "DAT,%lu,%s,%s,%u,%u,%c,%u,%u,%u",
           (unsigned long)++numeroLectura, txtTemp, txtHum,
           humedadSuelo, umbral, (char)modo,
           bombaEncendida ? 1 : 0, fallas, contadorRechazos);
  enviarTrama(contenido);

  Serial.printf("[LECTURA] T=%s C  HR=%s %%  Suelo=%u %%  Umbral=%u %%  Modo=%s  Bomba=%s%s\n",
                txtTemp, txtHum, humedadSuelo, umbral, nombreModo(modo),
                bombaEncendida ? "ON" : "OFF",
                mqtt.connected() ? "" : "  (sin MQTT, no enviada)");
}

// ===========================================================================
// Validacion y procesamiento de comandos
//   $CMD,<seq>,PING*FIRMA
//   $CMD,<seq>,UMBRAL,<10..90>*FIRMA
//   $CMD,<seq>,BOMBA,<ON|OFF>*FIRMA
//   $CMD,<seq>,MODO,<AUTO|MANUAL>*FIRMA
// ===========================================================================
void confirmar(uint32_t seq, const char *comando, const char *detalle) {
  char contenido[LARGO_MAX_TRAMA];
  snprintf(contenido, sizeof(contenido), "ACK,%lu,%s,%s", (unsigned long)seq, comando, detalle);
  enviarTrama(contenido);
}

// Rechaza, responde $NAK y deja registro en el monitor serial
void rechazar(uint32_t seq, const char *motivo, const char *tramaOriginal) {
  contadorRechazos++;
  char contenido[LARGO_MAX_TRAMA];
  snprintf(contenido, sizeof(contenido), "NAK,%lu,%s", (unsigned long)seq, motivo);
  enviarTrama(contenido);

  Serial.printf("[RECHAZO #%u] t=%lu ms motivo=%s trama=\"%s\"\n",
                contadorRechazos, millis(), motivo, tramaOriginal);
}

bool caracteresValidos(const char *s) {
  for (; *s; s++) {
    char c = *s;
    bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == ',' || c == '.' || c == '-' || c == '*';
    if (!ok) return false;
  }
  return true;
}

bool esNumero(const char *s, uint8_t maxDigitos) {
  uint8_t n = 0;
  for (; *s; s++, n++) {
    if (*s < '0' || *s > '9') return false;
  }
  return n > 0 && n <= maxDigitos;
}

bool esHex(const char *s, uint8_t largo) {
  if (strlen(s) != largo) return false;
  for (; *s; s++) {
    if (!isxdigit((unsigned char)*s)) return false;
  }
  return true;
}

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

bool limiteDeTramasOk() {
  unsigned long ahora = millis();
  if (ahora - inicioVentanaMs >= VENTANA_LIMITE_MS) {
    inicioVentanaMs = ahora;
    tramasEnVentana = 0;
  }
  return ++tramasEnVentana <= MAX_TRAMAS_VENTANA;
}

// trama: contenido sin el '$' inicial, ej. "CMD,12,PING*1A2B"
void procesarTrama(char *trama) {
  char original[LARGO_MAX_TRAMA + 1];
  strncpy(original, trama, sizeof(original));
  original[LARGO_MAX_TRAMA] = '\0';

  // 1) Limite de tramas por segundo
  if (!limiteDeTramasOk()) {
    rechazar(0, "LIMITE", original);
    return;
  }

  // 2) Formato general
  char *asterisco = strrchr(trama, '*');
  if (asterisco == NULL || !esHex(asterisco + 1, 4) || !caracteresValidos(trama)) {
    rechazar(0, "FORMATO", original);
    return;
  }

  // 3) Firma
  uint16_t firmaRecibida = (uint16_t)strtoul(asterisco + 1, NULL, 16);
  if (calcularFirma(trama, asterisco - trama) != firmaRecibida) {
    rechazar(0, "FIRMA", original);
    return;
  }
  *asterisco = '\0';

  // 4) Estructura
  char *campos[MAX_CAMPOS];
  uint8_t n = dividirCampos(trama, campos, MAX_CAMPOS);
  if (n < 3 || strcmp(campos[0], "CMD") != 0 || !esNumero(campos[1], 9)) {
    rechazar(0, "FORMATO", original);
    return;
  }
  uint32_t seq = strtoul(campos[1], NULL, 10);

  // 5) Anti-repeticion
  if (seq <= ultimaSecuencia) {
    rechazar(seq, "SECUENCIA", original);
    return;
  }
  ultimaSecuencia = seq;

  ultimoRxValidoMs = millis();
  if (modo == MODO_SEGURO) {
    cambiarModo(MODO_AUTO, "enlace restablecido");
  }

  const char *comando = campos[2];

  // 6) Lista blanca de comandos y validacion de argumentos / rango
  if (strcmp(comando, "PING") == 0 && n == 3) {
    confirmar(seq, "PING", "OK");

  } else if (strcmp(comando, "UMBRAL") == 0 && n == 4) {
    if (!esNumero(campos[3], 3)) {
      rechazar(seq, "FORMATO", original);
      return;
    }
    int valor = atoi(campos[3]);
    if (valor < UMBRAL_MIN || valor > UMBRAL_MAX) {
      rechazar(seq, "RANGO", original);
      return;
    }
    umbral = (uint8_t)valor;
    preferencias.putUChar("umbral", umbral);
    controlAutomatico();
    confirmar(seq, "UMBRAL", campos[3]);
    Serial.printf("[CMD] Umbral cambiado a %u %%\n", umbral);

  } else if (strcmp(comando, "BOMBA") == 0 && n == 4) {
    bool encender;
    if (strcmp(campos[3], "ON") == 0) {
      encender = true;
    } else if (strcmp(campos[3], "OFF") == 0) {
      encender = false;
    } else {
      rechazar(seq, "VALOR", original);
      return;
    }
    if (encender && enDescanso) {
      rechazar(seq, "BLOQUEADO", original);
      return;
    }
    cambiarModo(MODO_MANUAL, "comando BOMBA");
    aplicarBomba(encender);
    confirmar(seq, "BOMBA", campos[3]);
    Serial.printf("[CMD] Bomba manual %s\n", campos[3]);

  } else if (strcmp(comando, "MODO") == 0 && n == 4) {
    if (strcmp(campos[3], "AUTO") == 0) {
      cambiarModo(MODO_AUTO, "comando MODO");
      controlAutomatico();
    } else if (strcmp(campos[3], "MANUAL") == 0) {
      cambiarModo(MODO_MANUAL, "comando MODO");
    } else {
      rechazar(seq, "VALOR", original);
      return;
    }
    confirmar(seq, "MODO", campos[3]);

  } else {
    rechazar(seq, "COMANDO", original);
  }
}

// Llamado por PubSubClient al llegar un mensaje en un topic suscrito
void alRecibirMqtt(char *topic, byte *payload, unsigned int largo) {
  if (strcmp(topic, TOPIC_COMANDOS) != 0) return;

  // Se quitan los saltos de linea finales, si los hay
  while (largo > 0 && (payload[largo - 1] == '\n' || payload[largo - 1] == '\r')) {
    largo--;
  }

  // Copia acotada para registro (el buffer de PubSubClient se reutiliza al publicar)
  // ('$' + hasta LARGO_MAX_TRAMA caracteres + '\0')
  char trama[LARGO_MAX_TRAMA + 2];
  unsigned int aCopiar = largo > LARGO_MAX_TRAMA + 1 ? LARGO_MAX_TRAMA + 1 : largo;
  memcpy(trama, payload, aCopiar);
  trama[aCopiar] = '\0';

  if (largo == 0 || payload[0] != '$' || memchr(payload, '\0', largo) != NULL) {
    rechazar(0, "FORMATO", trama);
    return;
  }
  if (largo > LARGO_MAX_TRAMA + 1) {               // Proteccion de largo
    rechazar(0, "LARGO", trama);
    return;
  }

  memmove(trama, trama + 1, aCopiar);              // Quita el '$'
  procesarTrama(trama);
}

// ===========================================================================
// Conectividad WiFi + MQTT (sin bloquear el control local)
// ===========================================================================
void conectarMqtt() {
  char idCliente[32];
  snprintf(idCliente, sizeof(idCliente), "riego-n1-%08lX", (unsigned long)(ESP.getEfuseMac() & 0xFFFFFFFF));

  Serial.printf("[MQTT] Conectando a %s:%u ... ", MQTT_BROKER, MQTT_PUERTO);
  // Ultima voluntad: si el nodo se desconecta, el broker publica OFFLINE
  if (mqtt.connect(idCliente, TOPIC_ESTADO, 1, true, "OFFLINE")) {
    Serial.println("OK");
    mqtt.publish(TOPIC_ESTADO, "ONLINE", true);
    mqtt.subscribe(TOPIC_COMANDOS, 1);
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
    return;                      // El ESP32 se reconecta solo (setAutoReconnect)
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
void setup() {
  // Bomba apagada antes de configurar el pin como salida
  digitalWrite(PIN_RELE, RELE_ACTIVO_EN_BAJO ? HIGH : LOW);
  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_LED_EST, OUTPUT);

  Serial.begin(115200);
  dht.begin();
  analogReadResolution(12);

  // Umbral persistente en la memoria flash (Preferences)
  preferencias.begin("riego", false);
  uint8_t guardado = preferencias.getUChar("umbral", UMBRAL_DEFECTO);
  if (guardado >= UMBRAL_MIN && guardado <= UMBRAL_MAX) {
    umbral = guardado;
  }

  Serial.println("=== NODO SENSOR ESP32 - Riego IoT (EVA1 TI3V42) ===");
  Serial.printf("Topic base: %s\n", TOPIC_BASE);
  Serial.printf("Umbral inicial: %u %%  | Modo: SEGURO hasta recibir el nodo de control\n", umbral);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_CLAVE);

#if USAR_TLS
  clienteRed.setInsecure();   // Cifra el canal; no valida el certificado del broker
#endif
  mqtt.setServer(MQTT_BROKER, MQTT_PUERTO);
  mqtt.setCallback(alRecibirMqtt);
  mqtt.setSocketTimeout(3);
  mqtt.setKeepAlive(10);
}

void loop() {
  unsigned long ahora = millis();

  gestionarConexion();

  if (ahora - ultimaLecturaMs >= INTERVALO_LECTURA_MS) {
    ultimaLecturaMs = ahora;
    leerSensores();
    controlAutomatico();
    enviarLectura();
  }

  vigilarBomba();
  vigilarEnlace();

  bool alerta = fallas != 0 || modo == MODO_SEGURO || !mqtt.connected();
  digitalWrite(PIN_LED_EST, alerta ? HIGH : LOW);
}
