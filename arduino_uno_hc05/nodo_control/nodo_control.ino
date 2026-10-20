/*
  ============================================================================
  EVA1 - TI3V42 Aplicaciones Moviles para IoT - INACAP
  Caso: Monitoreo de temperatura / humedad con activacion de riego
  ----------------------------------------------------------------------------
  NODO DE CONTROL (microcontrolador 2)

  - Recibe por Bluetooth (HC-05 maestro) las lecturas del nodo sensor, verifica
    su firma, las interpreta y las muestra en el monitor serial y en LEDs.
  - Envia comandos para cambiar el umbral y encender / apagar la bomba de forma
    manual. Los comandos se escriben en el monitor serial (9600 baud, "Nueva
    linea"). Escribe "ayuda" para ver la lista.
  - Envia un PING cada 2 s para que el nodo sensor sepa que el enlace esta vivo.
  - Muestra la confirmacion ($ACK) o el rechazo ($NAK) de cada comando.
  - "prueba" envia comandos invalidos para demostrar que el nodo sensor los
    rechaza (firma alterada, rango fuera de limite, comando desconocido y
    repeticion de secuencia).

  LEDs: verde = enlace OK, azul = bomba encendida, rojo = sin enlace, falla,
  modo seguro o comando rechazado.
  ============================================================================
*/

#include <SoftwareSerial.h>
#include <EEPROM.h>

#if __has_include("secretos.h")
#include "secretos.h"
#else
#warning "No existe secretos.h: se usa la clave de ejemplo. Copia secretos.example.h como secretos.h y cambia la clave."
#define CLAVE_COMPARTIDA "CAMBIAR_ESTA_CLAVE"
#endif

// ---------------------------------------------------------------------------
// Pines
// ---------------------------------------------------------------------------
const uint8_t PIN_BT_RX      = 10;  // RX del Arduino  <- TX del HC-05
const uint8_t PIN_BT_TX      = 11;  // TX del Arduino  -> RX del HC-05 (con divisor 1k/2k)
const uint8_t PIN_LED_ALARMA = 4;   // Rojo
const uint8_t PIN_LED_ENLACE = 5;   // Verde
const uint8_t PIN_LED_BOMBA  = 6;   // Azul

// ---------------------------------------------------------------------------
// Tiempos (ms)
// ---------------------------------------------------------------------------
const unsigned long INTERVALO_PING_MS  = 2000;
const unsigned long TIMEOUT_DATOS_MS   = 6000;   // Sin $DAT -> enlace perdido
const unsigned long TIMEOUT_ACK_MS     = 3000;   // Comando sin confirmar
const unsigned long PULSO_ALARMA_MS    = 1500;   // LED rojo tras un $NAK
const unsigned long PASO_PRUEBA_MS     = 400;    // Separacion entre tramas de prueba

const uint8_t UMBRAL_MIN = 10;
const uint8_t UMBRAL_MAX = 90;

// Numero de secuencia persistente: se reservan bloques en EEPROM para que,
// aunque este nodo se reinicie, nunca repita una secuencia ya usada.
const int      EEPROM_DIR_MAGIA = 0;
const int      EEPROM_DIR_SEQ   = 1;
const uint8_t  EEPROM_MAGIA     = 0x5E;
const uint32_t BLOQUE_SEQ       = 1000;

const uint8_t LARGO_MAX_TRAMA   = 64;
const uint8_t LARGO_MAX_CONSOLA = 48;
const uint8_t MAX_CAMPOS        = 11;

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
SoftwareSerial bt(PIN_BT_RX, PIN_BT_TX);

uint32_t secuencia = 0;
uint32_t limiteReservado = 0;

uint32_t      pendienteSeq = 0;      // Ultimo comando que espera $ACK / $NAK
unsigned long pendienteMs  = 0;
uint32_t      ultimaSeqEnviada = 0;  // Para la prueba de repeticion

bool          enlaceOk = false;
unsigned long ultimoDatoMs = 0;
unsigned long ultimoPingMs = 0;
unsigned long alarmaHastaMs = 0;
bool          mostrarLecturas = true;
uint16_t      tramasDescartadas = 0;

bool    bombaRemota = false;
bool    fallaRemota = false;
bool    modoSeguroRemoto = false;

uint8_t       pasoPrueba = 0;
unsigned long proximoPasoMs = 0;

char    bufferRx[LARGO_MAX_TRAMA + 1];
uint8_t largoRx = 0;
bool    recibiendo = false;

char    bufferConsola[LARGO_MAX_CONSOLA + 1];
uint8_t largoConsola = 0;

// ===========================================================================
// Firma (debe ser identica a la del nodo sensor)
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

// Envia "$<contenido>*<FIRMA>". Con alterarFirma = true se envia una firma
// incorrecta (solo para la prueba de rechazo).
void enviarTrama(const char *contenido, bool alterarFirma = false) {
  uint16_t firma = calcularFirma(contenido, strlen(contenido));
  if (alterarFirma) firma ^= 0xFFFF;
  char sufijo[6];
  snprintf(sufijo, sizeof(sufijo), "*%04X", (unsigned int)firma);
  bt.print('$');
  bt.print(contenido);
  bt.println(sufijo);
}

// ===========================================================================
// Secuencia
// ===========================================================================
void iniciarSecuencia() {
  uint32_t guardado = 0;
  if (EEPROM.read(EEPROM_DIR_MAGIA) == EEPROM_MAGIA) {
    EEPROM.get(EEPROM_DIR_SEQ, guardado);
  }
  secuencia = guardado;                       // Continua sobre el ultimo bloque reservado
  limiteReservado = guardado + BLOQUE_SEQ;
  EEPROM.put(EEPROM_DIR_SEQ, limiteReservado);
  EEPROM.write(EEPROM_DIR_MAGIA, EEPROM_MAGIA);
}

uint32_t siguienteSecuencia() {
  secuencia++;
  if (secuencia >= limiteReservado) {
    limiteReservado += BLOQUE_SEQ;
    EEPROM.put(EEPROM_DIR_SEQ, limiteReservado);
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
  enviarTrama(contenido, alterarFirma);

  if (esperarAck) {
    pendienteSeq = seq;
    pendienteMs = millis();
    Serial.print(F(">> Enviado: $"));
    Serial.println(contenido);
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

void imprimirFallas(uint8_t fallas) {
  if (fallas == 0) {
    Serial.print(F("ninguna"));
    return;
  }
  if (fallas & 0x01) Serial.print(F("SENSOR_AIRE "));
  if (fallas & 0x02) Serial.print(F("SENSOR_SUELO "));
  if (fallas & 0x04) Serial.print(F("TIEMPO_MAX_RIEGO "));
}

// $DAT,<n>,<temp>,<hr>,<suelo>,<umbral>,<modo>,<bomba>,<fallas>,<rechazos>
void mostrarLectura(char *campos[], uint8_t n) {
  if (n != 10) {
    tramasDescartadas++;
    Serial.println(F("[DESCARTADO] Trama DAT con cantidad de campos incorrecta"));
    return;
  }

  ultimoDatoMs = millis();
  if (!enlaceOk) {
    enlaceOk = true;
    Serial.println(F("[ENLACE] Conectado con el nodo sensor"));
  }

  char modo = campos[6][0];
  uint8_t fallas = atoi(campos[8]);
  bombaRemota = strcmp(campos[7], "1") == 0;
  fallaRemota = fallas != 0;
  modoSeguroRemoto = modo == 'S';

  if (!mostrarLecturas) return;

  Serial.print(F("[LECTURA #"));
  Serial.print(campos[1]);
  Serial.print(F("] Temp: "));
  Serial.print(campos[2]);
  Serial.print(F(" C | HR aire: "));
  Serial.print(campos[3]);
  Serial.print(F(" % | Suelo: "));
  Serial.print(campos[4]);
  Serial.print(F(" % | Umbral: "));
  Serial.print(campos[5]);
  Serial.print(F(" % | Modo: "));
  Serial.print(modo == 'A' ? F("AUTO") : modo == 'M' ? F("MANUAL") : F("SEGURO"));
  Serial.print(F(" | Bomba: "));
  Serial.print(bombaRemota ? F("ENCENDIDA") : F("apagada"));
  Serial.print(F(" | Fallas: "));
  imprimirFallas(fallas);
  Serial.print(F(" | Rechazos: "));
  Serial.println(campos[9]);
}

void procesarTrama(char *trama) {
  char *asterisco = strrchr(trama, '*');
  if (asterisco == NULL || strlen(asterisco + 1) != 4) {
    tramasDescartadas++;
    Serial.println(F("[DESCARTADO] Trama sin firma"));
    return;
  }
  uint16_t firmaRecibida = (uint16_t)strtoul(asterisco + 1, NULL, 16);
  if (calcularFirma(trama, asterisco - trama) != firmaRecibida) {
    tramasDescartadas++;
    Serial.println(F("[DESCARTADO] Firma invalida: la trama no viene del nodo sensor o llego corrupta"));
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
    if (strcmp(campos[2], "PING") != 0) {   // Los PING se confirman en silencio
      Serial.print(F("<< CONFIRMADO por el nodo sensor: seq="));
      Serial.print(campos[1]);
      Serial.print(F(" "));
      Serial.print(campos[2]);
      Serial.print(F("="));
      Serial.println(campos[3]);
    }

  } else if (strcmp(campos[0], "NAK") == 0 && n == 3) {
    uint32_t seq = strtoul(campos[1], NULL, 10);
    if (seq == pendienteSeq || seq == 0) pendienteSeq = 0;
    alarmaHastaMs = millis() + PULSO_ALARMA_MS;
    Serial.print(F("<< RECHAZADO por el nodo sensor: seq="));
    Serial.print(campos[1]);
    Serial.print(F(" motivo="));
    Serial.println(campos[2]);

  } else {
    tramasDescartadas++;
    Serial.println(F("[DESCARTADO] Tipo de trama desconocido"));
  }
}

void leerBluetooth() {
  while (bt.available()) {
    char c = bt.read();
    if (c == '$') {
      recibiendo = true;
      largoRx = 0;
      continue;
    }
    if (!recibiendo || c == '\r') continue;
    if (c == '\n') {
      recibiendo = false;
      bufferRx[largoRx] = '\0';
      if (largoRx > 0) procesarTrama(bufferRx);
      continue;
    }
    if (largoRx < LARGO_MAX_TRAMA) {
      bufferRx[largoRx++] = c;
    } else {
      recibiendo = false;           // Trama demasiado larga: se descarta
      tramasDescartadas++;
    }
  }
}

// ===========================================================================
// Consola (monitor serial)
// ===========================================================================
void mostrarAyuda() {
  Serial.println(F("------------------------------------------------------------"));
  Serial.println(F("Comandos disponibles:"));
  Serial.println(F("  umbral <10-90>  Cambia el umbral de humedad del suelo (%)"));
  Serial.println(F("  on              Enciende la bomba (pasa a modo MANUAL)"));
  Serial.println(F("  off             Apaga la bomba (pasa a modo MANUAL)"));
  Serial.println(F("  auto            Vuelve al modo AUTOMATICO"));
  Serial.println(F("  manual          Pasa a modo MANUAL sin cambiar la bomba"));
  Serial.println(F("  pausa           Oculta / muestra las lecturas"));
  Serial.println(F("  estado          Estado del enlace y contadores"));
  Serial.println(F("  prueba          Envia 4 comandos invalidos (demo de rechazo)"));
  Serial.println(F("  raw <trama>     Envia una trama tal cual, ej: raw $CMD,1,BOMBA,ON*0000"));
  Serial.println(F("------------------------------------------------------------"));
}

void mostrarEstado() {
  Serial.print(F("[ESTADO] Enlace: "));
  Serial.print(enlaceOk ? F("OK") : F("SIN CONEXION"));
  Serial.print(F(" | Secuencia actual: "));
  Serial.print(secuencia);
  Serial.print(F(" | Tramas descartadas: "));
  Serial.println(tramasDescartadas);
}

void cambiarUmbral(const char *valor) {
  // Validacion local (defensa en profundidad); el nodo sensor vuelve a validar
  uint8_t digitos = 0;
  for (const char *p = valor; *p; p++, digitos++) {
    if (*p < '0' || *p > '9') digitos = 99;
  }
  int numero = atoi(valor);
  if (digitos == 0 || digitos > 3 || numero < UMBRAL_MIN || numero > UMBRAL_MAX) {
    Serial.println(F("[LOCAL] Umbral invalido. Debe ser un numero entre 10 y 90."));
    return;
  }
  char texto[4];
  snprintf(texto, sizeof(texto), "%d", numero);
  enviarComando("UMBRAL", texto, true);
}

void ejecutarConsola(char *linea) {
  // Recorta espacios al inicio y al final
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
    Serial.println(mostrarLecturas ? F("[LOCAL] Lecturas visibles") : F("[LOCAL] Lecturas ocultas"));
  } else if (strcasecmp(linea, "estado") == 0) {
    mostrarEstado();
  } else if (strcasecmp(linea, "prueba") == 0) {
    Serial.println(F("[PRUEBA] Enviando comandos invalidos al nodo sensor..."));
    pasoPrueba = 1;
    proximoPasoMs = millis();
  } else if (strncasecmp(linea, "raw ", 4) == 0) {
    Serial.print(F(">> Enviado sin procesar: "));
    Serial.println(linea + 4);
    bt.println(linea + 4);
  } else {
    Serial.print(F("[LOCAL] Comando desconocido: "));
    Serial.println(linea);
    Serial.println(F("Escribe 'ayuda' para ver los comandos."));
  }
}

void leerConsola() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      bufferConsola[largoConsola] = '\0';
      ejecutarConsola(bufferConsola);
      largoConsola = 0;
    } else if (largoConsola < LARGO_MAX_CONSOLA) {
      bufferConsola[largoConsola++] = c;
    }
  }
}

// Demo de rechazo, un paso cada PASO_PRUEBA_MS (sin bloquear el programa)
void ejecutarPrueba() {
  if (pasoPrueba == 0 || millis() - proximoPasoMs < PASO_PRUEBA_MS) return;
  proximoPasoMs = millis();

  switch (pasoPrueba) {
    case 1:
      Serial.println(F("[PRUEBA 1/4] BOMBA ON con firma alterada (como un intruso sin la clave)"));
      enviarComando("BOMBA", "ON", true, true);
      break;
    case 2:
      Serial.println(F("[PRUEBA 2/4] UMBRAL 99, fuera del rango permitido 10-90"));
      enviarComando("UMBRAL", "99", true);
      break;
    case 3:
      Serial.println(F("[PRUEBA 3/4] Comando desconocido ABRIR"));
      enviarComando("ABRIR", "1", true);
      break;
    case 4:
      Serial.println(F("[PRUEBA 4/4] Repeticion de una trama ya usada (ataque de replay)"));
      enviarComando("BOMBA", "ON", true, false, ultimaSeqEnviada);
      break;
  }
  pasoPrueba = (pasoPrueba >= 4) ? 0 : pasoPrueba + 1;
}

// ===========================================================================
// Vigilancia y LEDs
// ===========================================================================
void vigilar() {
  unsigned long ahora = millis();

  if (enlaceOk && ahora - ultimoDatoMs >= TIMEOUT_DATOS_MS) {
    enlaceOk = false;
    Serial.println(F("[ENLACE] PERDIDO: no llegan datos del nodo sensor"));
  }

  if (pendienteSeq != 0 && ahora - pendienteMs >= TIMEOUT_ACK_MS) {
    Serial.print(F("[AVISO] El comando seq="));
    Serial.print(pendienteSeq);
    Serial.println(F(" no fue confirmado"));
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

  Serial.begin(9600);
  bt.begin(9600);
  iniciarSecuencia();

  Serial.println(F("=== NODO DE CONTROL - Riego IoT (EVA1 TI3V42) ==="));
  mostrarAyuda();
}

void loop() {
  unsigned long ahora = millis();

  leerBluetooth();
  leerConsola();
  ejecutarPrueba();

  if (ahora - ultimoPingMs >= INTERVALO_PING_MS) {
    ultimoPingMs = ahora;
    enviarComando("PING", NULL, false);
  }

  vigilar();
}
