/*
  ============================================================================
  EVA1 - TI3V42 Aplicaciones Moviles para IoT - INACAP
  Caso: Monitoreo de temperatura / humedad con activacion de riego
  ----------------------------------------------------------------------------
  NODO SENSOR (microcontrolador 1)

  - Lee humedad del suelo (sensor capacitivo) y temperatura / humedad del aire
    (DHT22) cada 2 s, sin bloquear el programa (temporizacion con millis()).
  - Modo AUTOMATICO: enciende la bomba cuando la humedad del suelo baja del
    umbral y la apaga al superar umbral + histeresis.
  - Modo MANUAL: la bomba se enciende / apaga por comando remoto.
  - Modo SEGURO: si se pierde la comunicacion con el nodo de control, la bomba
    se apaga (estado seguro) hasta que vuelva un mensaje valido.
  - Envia cada lectura y el estado de la bomba por Bluetooth (HC-05).
  - Recibe comandos, los valida (formato, firma, secuencia, comando y rango),
    confirma cada comando aceptado con $ACK y rechaza / registra los invalidos
    con $NAK.

  Hardware: Arduino UNO + HC-05 (esclavo) + sensor capacitivo de humedad de
  suelo v1.2 + DHT22 + modulo rele 5 V + bomba sumergible 5 V.
  Libreria requerida: "DHT sensor library" de Adafruit (y "Adafruit Unified
  Sensor").
  ============================================================================
*/

#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <DHT.h>

// La clave compartida va en secretos.h (no se sube a GitHub, ver .gitignore).
#if __has_include("secretos.h")
#include "secretos.h"
#else
#warning "No existe secretos.h: se usa la clave de ejemplo. Copia secretos.example.h como secretos.h y cambia la clave."
#define CLAVE_COMPARTIDA "CAMBIAR_ESTA_CLAVE"
#endif

// ---------------------------------------------------------------------------
// Pines
// ---------------------------------------------------------------------------
const uint8_t PIN_BT_RX   = 10;  // RX del Arduino  <- TX del HC-05
const uint8_t PIN_BT_TX   = 11;  // TX del Arduino  -> RX del HC-05 (con divisor 1k/2k)
const uint8_t PIN_DHT     = 2;
const uint8_t PIN_SUELO   = A0;
const uint8_t PIN_RELE    = 7;
const uint8_t PIN_LED_EST = 13;  // Encendido = falla o modo seguro

#define DHT_TIPO DHT22             // Cambiar a DHT11 si se usa ese sensor
const bool RELE_ACTIVO_EN_BAJO = true;  // La mayoria de los modulos rele se activan con LOW

// ---------------------------------------------------------------------------
// Calibracion del sensor capacitivo (medir con el propio sensor y ajustar)
// ---------------------------------------------------------------------------
const int ADC_SUELO_SECO   = 600;  // Lectura con el sensor al aire / suelo seco
const int ADC_SUELO_MOJADO = 280;  // Lectura con el sensor sumergido en agua
const int ADC_MIN_VALIDO   = 10;   // Bajo esto se asume sensor desconectado
const int ADC_MAX_VALIDO   = 1015; // Sobre esto se asume cortocircuito / falla

// ---------------------------------------------------------------------------
// Tiempos (ms)
// ---------------------------------------------------------------------------
const unsigned long INTERVALO_LECTURA_MS = 2000;   // Periodo de lectura y envio
const unsigned long TIMEOUT_ENLACE_MS    = 10000;  // Sin mensajes validos -> modo seguro
const unsigned long BOMBA_MAX_ON_MS      = 60000;  // Tiempo maximo continuo de riego
const unsigned long BOMBA_DESCANSO_MS    = 30000;  // Descanso obligado tras el maximo
const unsigned long VENTANA_LIMITE_MS    = 1000;   // Ventana del limitador de comandos
const uint8_t       MAX_TRAMAS_VENTANA   = 8;      // Tramas aceptadas por ventana

// ---------------------------------------------------------------------------
// Umbral de activacion (% de humedad del suelo)
// ---------------------------------------------------------------------------
const uint8_t UMBRAL_MIN     = 10;
const uint8_t UMBRAL_MAX     = 90;
const uint8_t UMBRAL_DEFECTO = 40;
const uint8_t HISTERESIS     = 5;   // Apaga al llegar a umbral + 5 %

const int EEPROM_DIR_MAGIA  = 0;
const int EEPROM_DIR_UMBRAL = 1;
const uint8_t EEPROM_MAGIA  = 0xA5;

// ---------------------------------------------------------------------------
// Protocolo
// ---------------------------------------------------------------------------
const uint8_t LARGO_MAX_TRAMA = 64;
const uint8_t MAX_CAMPOS      = 6;

// Bits del campo "fallas" de la trama $DAT
const uint8_t FALLA_DHT    = 0x01;
const uint8_t FALLA_SUELO  = 0x02;
const uint8_t FALLA_TIEMPO = 0x04;

enum Modo : char { MODO_AUTO = 'A', MODO_MANUAL = 'M', MODO_SEGURO = 'S' };

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
SoftwareSerial bt(PIN_BT_RX, PIN_BT_TX);
DHT dht(PIN_DHT, DHT_TIPO);

Modo     modo = MODO_SEGURO;     // Arranca en seguro hasta recibir un mensaje valido
bool     bombaEncendida = false;
uint8_t  umbral = UMBRAL_DEFECTO;
uint8_t  fallas = 0;
uint16_t contadorRechazos = 0;
uint32_t numeroLectura = 0;
uint32_t ultimaSecuencia = 0;     // Anti-repeticion: solo se aceptan secuencias mayores

float   temperatura = NAN;
float   humedadAire = NAN;
uint8_t humedadSuelo = 0;

unsigned long ultimaLecturaMs  = 0;
unsigned long ultimoRxValidoMs = 0;
unsigned long inicioBombaMs    = 0;
unsigned long inicioDescansoMs = 0;
bool          enDescanso = false;
unsigned long inicioVentanaMs  = 0;
uint8_t       tramasEnVentana  = 0;

char    bufferRx[LARGO_MAX_TRAMA + 1];
uint8_t largoRx = 0;
bool    recibiendo = false;
bool    desbordado = false;

// ===========================================================================
// Firma de mensajes: FNV-1a de 32 bits sobre (clave compartida + contenido),
// plegada a 16 bits. La clave nunca viaja por el aire.
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

// Envia "$<contenido>*<FIRMA>\r\n"
void enviarTrama(const char *contenido) {
  char sufijo[6];
  snprintf(sufijo, sizeof(sufijo), "*%04X", (unsigned int)calcularFirma(contenido, strlen(contenido)));
  bt.print('$');
  bt.print(contenido);
  bt.println(sufijo);
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

void cambiarModo(Modo nuevo, const __FlashStringHelper *razon) {
  if (modo == nuevo) return;
  modo = nuevo;
  Serial.print(F("[MODO] "));
  Serial.print(nombreModo(modo));
  Serial.print(F(" ("));
  Serial.print(razon);
  Serial.println(F(")"));
  if (modo == MODO_SEGURO) {
    aplicarBomba(false);
  }
}

// ===========================================================================
// Sensores
// ===========================================================================
void leerSensores() {
  // Promedio de 8 muestras para filtrar ruido del ADC (~1 ms en total)
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

  // Sin lectura confiable del suelo o en descanso obligado: no se riega
  if ((fallas & FALLA_SUELO) || enDescanso) {
    aplicarBomba(false);
    return;
  }

  if (!bombaEncendida && humedadSuelo < umbral) {
    aplicarBomba(true);
    Serial.println(F("[AUTO] Suelo seco -> bomba ENCENDIDA"));
  } else if (bombaEncendida && humedadSuelo >= umbral + HISTERESIS) {
    aplicarBomba(false);
    Serial.println(F("[AUTO] Suelo humedo -> bomba APAGADA"));
  }
}

// Enclavamiento: limita el tiempo continuo de riego (evita inundar o que la
// bomba trabaje en seco si el estanque se vacia o el sensor queda mal puesto).
void vigilarBomba() {
  unsigned long ahora = millis();

  if (bombaEncendida && ahora - inicioBombaMs >= BOMBA_MAX_ON_MS) {
    aplicarBomba(false);
    enDescanso = true;
    inicioDescansoMs = ahora;
    fallas |= FALLA_TIEMPO;
    Serial.println(F("[ENCLAVAMIENTO] Tiempo maximo de riego alcanzado -> bomba APAGADA"));
  }

  if (enDescanso && ahora - inicioDescansoMs >= BOMBA_DESCANSO_MS) {
    enDescanso = false;
    fallas &= ~FALLA_TIEMPO;
    Serial.println(F("[ENCLAVAMIENTO] Fin del descanso de la bomba"));
  }
}

// Falla segura: sin mensajes validos del nodo de control -> bomba apagada
void vigilarEnlace() {
  if (modo != MODO_SEGURO && millis() - ultimoRxValidoMs >= TIMEOUT_ENLACE_MS) {
    cambiarModo(MODO_SEGURO, F("se perdio la comunicacion"));
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
    dtostrf(temperatura, 1, 1, txtTemp);
    dtostrf(humedadAire, 1, 0, txtHum);
  }

  char contenido[LARGO_MAX_TRAMA];
  snprintf(contenido, sizeof(contenido), "DAT,%lu,%s,%s,%u,%u,%c,%u,%u,%u",
           (unsigned long)++numeroLectura, txtTemp, txtHum,
           humedadSuelo, umbral, (char)modo,
           bombaEncendida ? 1 : 0, fallas, contadorRechazos);
  enviarTrama(contenido);

  Serial.print(F("[LECTURA] T="));
  Serial.print(txtTemp);
  Serial.print(F(" C  HR="));
  Serial.print(txtHum);
  Serial.print(F(" %  Suelo="));
  Serial.print(humedadSuelo);
  Serial.print(F(" %  Umbral="));
  Serial.print(umbral);
  Serial.print(F(" %  Modo="));
  Serial.print(nombreModo(modo));
  Serial.print(F("  Bomba="));
  Serial.println(bombaEncendida ? F("ON") : F("OFF"));
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

  Serial.print(F("[RECHAZO #"));
  Serial.print(contadorRechazos);
  Serial.print(F("] t="));
  Serial.print(millis());
  Serial.print(F(" ms motivo="));
  Serial.print(motivo);
  Serial.print(F(" trama=\""));
  Serial.print(tramaOriginal);
  Serial.println('"');
}

// Solo se permiten mayusculas, digitos, coma, punto, guion y '*'
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
    if (!isxdigit(*s)) return false;
  }
  return true;
}

// Divide por comas conservando campos vacios. Devuelve 0 si hay demasiados.
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

void procesarTrama(char *trama) {
  char original[LARGO_MAX_TRAMA + 1];
  strncpy(original, trama, sizeof(original));
  original[LARGO_MAX_TRAMA] = '\0';

  // 1) Limite de tramas por segundo (proteccion ante inundacion de mensajes)
  if (!limiteDeTramasOk()) {
    rechazar(0, "LIMITE", original);
    return;
  }

  // 2) Formato general: <contenido>*<4 hex>, solo caracteres permitidos
  char *asterisco = strrchr(trama, '*');
  if (asterisco == NULL || !esHex(asterisco + 1, 4) || !caracteresValidos(trama)) {
    rechazar(0, "FORMATO", original);
    return;
  }

  // 3) Firma (integridad + conocimiento de la clave compartida)
  uint16_t firmaRecibida = (uint16_t)strtoul(asterisco + 1, NULL, 16);
  if (calcularFirma(trama, asterisco - trama) != firmaRecibida) {
    rechazar(0, "FIRMA", original);
    return;
  }
  *asterisco = '\0';

  // 4) Estructura: CMD,<seq>,<comando>[,<valor>]
  char *campos[MAX_CAMPOS];
  uint8_t n = dividirCampos(trama, campos, MAX_CAMPOS);
  if (n < 3 || strcmp(campos[0], "CMD") != 0 || !esNumero(campos[1], 9)) {
    rechazar(0, "FORMATO", original);
    return;
  }
  uint32_t seq = strtoul(campos[1], NULL, 10);

  // 5) Anti-repeticion: la secuencia debe ser mayor que la ultima aceptada
  if (seq <= ultimaSecuencia) {
    rechazar(seq, "SECUENCIA", original);
    return;
  }
  ultimaSecuencia = seq;

  // Mensaje autentico: el enlace esta vivo
  ultimoRxValidoMs = millis();
  if (modo == MODO_SEGURO) {
    cambiarModo(MODO_AUTO, F("enlace restablecido"));
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
    EEPROM.update(EEPROM_DIR_UMBRAL, umbral);
    controlAutomatico();
    confirmar(seq, "UMBRAL", campos[3]);
    Serial.print(F("[CMD] Umbral cambiado a "));
    Serial.print(umbral);
    Serial.println(F(" %"));

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
      rechazar(seq, "BLOQUEADO", original);  // Enclavamiento de tiempo maximo activo
      return;
    }
    cambiarModo(MODO_MANUAL, F("comando BOMBA"));
    aplicarBomba(encender);
    confirmar(seq, "BOMBA", campos[3]);
    Serial.print(F("[CMD] Bomba manual "));
    Serial.println(campos[3]);

  } else if (strcmp(comando, "MODO") == 0 && n == 4) {
    if (strcmp(campos[3], "AUTO") == 0) {
      cambiarModo(MODO_AUTO, F("comando MODO"));
      controlAutomatico();
    } else if (strcmp(campos[3], "MANUAL") == 0) {
      cambiarModo(MODO_MANUAL, F("comando MODO"));
    } else {
      rechazar(seq, "VALOR", original);
      return;
    }
    confirmar(seq, "MODO", campos[3]);

  } else {
    rechazar(seq, "COMANDO", original);
  }
}

// Lectura no bloqueante del Bluetooth, byte a byte
void leerBluetooth() {
  while (bt.available()) {
    char c = bt.read();

    if (c == '$') {               // Inicio de trama: descarta lo anterior
      recibiendo = true;
      desbordado = false;
      largoRx = 0;
      continue;
    }
    if (!recibiendo || c == '\r') continue;

    if (c == '\n') {
      recibiendo = false;
      if (desbordado) {
        bufferRx[largoRx] = '\0';
        rechazar(0, "LARGO", bufferRx);
      } else if (largoRx > 0) {
        bufferRx[largoRx] = '\0';
        procesarTrama(bufferRx);
      }
      continue;
    }

    if (largoRx < LARGO_MAX_TRAMA) {
      bufferRx[largoRx++] = c;
    } else {
      desbordado = true;          // Proteccion contra desbordamiento de buffer
    }
  }
}

// ===========================================================================
void setup() {
  // La salida del rele se fija en "apagado" ANTES de configurarla como salida,
  // para que la bomba nunca parta encendida al energizar o reiniciar.
  digitalWrite(PIN_RELE, RELE_ACTIVO_EN_BAJO ? HIGH : LOW);
  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_LED_EST, OUTPUT);

  Serial.begin(9600);
  bt.begin(9600);
  dht.begin();

  // Umbral persistente: se recupera tras un corte de energia
  if (EEPROM.read(EEPROM_DIR_MAGIA) == EEPROM_MAGIA) {
    uint8_t guardado = EEPROM.read(EEPROM_DIR_UMBRAL);
    if (guardado >= UMBRAL_MIN && guardado <= UMBRAL_MAX) {
      umbral = guardado;
    }
  } else {
    EEPROM.write(EEPROM_DIR_MAGIA, EEPROM_MAGIA);
    EEPROM.write(EEPROM_DIR_UMBRAL, umbral);
  }

  Serial.println(F("=== NODO SENSOR - Riego IoT (EVA1 TI3V42) ==="));
  Serial.print(F("Umbral inicial: "));
  Serial.print(umbral);
  Serial.println(F(" %  | Modo: SEGURO hasta recibir el nodo de control"));
}

void loop() {
  unsigned long ahora = millis();

  leerBluetooth();

  if (ahora - ultimaLecturaMs >= INTERVALO_LECTURA_MS) {
    ultimaLecturaMs = ahora;
    leerSensores();
    controlAutomatico();
    enviarLectura();
  }

  vigilarBomba();
  vigilarEnlace();

  digitalWrite(PIN_LED_EST, (fallas != 0 || modo == MODO_SEGURO) ? HIGH : LOW);
}
