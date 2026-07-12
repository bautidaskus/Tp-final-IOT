/*
 * Nodo COLLAR SIMULADO - Heltec WiFi LoRa 32 V3 (ESP32-S3 + radio SX1262)
 * TP Final IoT 2026 - Rastreo y monitoreo de ganado por LoRa P2P
 *
 * Igual que collar_heltec.ino (misma radio, mismo formato de paquete, mismo
 * periodo) PERO sin sensores reales: GPS, MPU-6050 y DS18B20 se reemplazan por
 * funciones que GENERAN los datos. Sirve para probar todo el pipeline
 * (collar -> base -> PC -> Node-RED -> Grafana) sin depender de tener fix de GPS
 * ni los sensores conectados. La estacion base (base_lora32u4.ino) NO cambia:
 * recibe este paquete igual que el del collar real.
 *
 * El guion simulado es CICLICO (35 paquetes, ~3 min a 5 s por paquete) y recorre
 * el estado normal y las tres alertas, para que la demo en vivo siempre las
 * muestre sin tener que reiniciar la placa:
 *   seq  0..7  : dentro de la geocerca y con movimiento  -> tablero en verde
 *   seq  8..14 : quieto (solo gravedad)                  -> alerta de inactividad
 *   seq 15..25 : se aleja y cruza el radio de 500 m      -> alerta de fuera de zona
 *   seq 20..25 : pico de fiebre (mientras sigue afuera)  -> alerta de temperatura
 *   seq 26..34 : vuelve a la zona y se normaliza         -> el tablero vuelve a verde
 *
 * IMPORTANTE: el centro de la geocerca debe coincidir con el de Node-RED
 * (pc/nodered/logica_collar.js) y con el circulo del mapa de Grafana
 * (pc/grafana/geocerca.geojson). Si no coinciden, el animal aparece siempre
 * fuera de zona y el tablero queda en alerta permanente.
 *
 * Formato del paquete (CSV, identico al collar real):
 *   seq,lat,lon,sats,tempC,ax,ay,az
 *
 * Libreria necesaria: RadioLib (no hacen falta las de sensores).
 */

#include <RadioLib.h>

// ----------------- Radio SX1262 (pines internos del Heltec V3) -----------------
#define LORA_NSS    8
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13
#define LORA_SCK    9
#define LORA_MISO   11
#define LORA_MOSI   10

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// Parametros de radio: DEBEN coincidir con la base.
const float   FREQ_MHZ   = 915.0;
const float   BW_KHZ     = 125.0;
const uint8_t SF         = 12;
const uint8_t CR_DENOM   = 5;       // 4/5
const uint8_t SYNC_WORD  = 0x12;
const int8_t  TX_POWER   = 20;      // dBm
const uint16_t PREAMBLE  = 12;
const float   TCXO_V     = 1.8;     // Heltec V3

#define PIN_LED     35              // LED on-board del Heltec V3

// ----------------- Escenario simulado -----------------------------------------
// Centro de la geocerca: IGUAL que en pc/nodered/logica_collar.js.
const float CENTRO_LAT = -41.3292;   // Ingeniero Jacobacci, Rio Negro
const float CENTRO_LON = -69.5436;

// Tramos del ciclo (en paquetes). Ver el encabezado del archivo.
const uint16_t CICLO      = 35;
const uint16_t QUIETO_INI = 8,  QUIETO_FIN = 14;   // sin movimiento
const uint16_t FUERA_INI  = 15, FUERA_FIN  = 25;   // afuera de la geocerca
const uint16_t FIEBRE_INI = 20, FIEBRE_FIN = 25;   // pico de temperatura

const float RADIO_PASEO = 250.0;    // deambula dentro de este radio (geocerca: 500 m)
const float RADIO_FUERA = 900.0;    // hasta donde se aleja durante el evento
const float PASO_M      = 180.0;    // avance maximo por paquete
const float RUMBO_FUERA = 0.7854;   // 45 grados (noreste): rumbo del alejamiento

// Posicion actual del animal, como offset en metros respecto del centro.
float posX = 0.0, posY = 0.0;

// ----------------- Timing -----------------
const unsigned long PERIODO_MS = 5000;
unsigned long nextSendAt = 0;
uint16_t seq = 0;

void blink(int veces, int ms) {
  for (int i = 0; i < veces; i++) {
    digitalWrite(PIN_LED, HIGH); delay(ms);
    digitalWrite(PIN_LED, LOW);  delay(ms);
  }
}

// Numero aleatorio float en [a, b] (para simular ruido de los sensores).
float randf(float a, float b) {
  return a + (float)random(0, 10001) / 10000.0 * (b - a);
}

// Tramo del ciclo al que corresponde este paquete.
uint16_t fase(uint16_t s)   { return s % CICLO; }
bool quieto(uint16_t c)     { return c >= QUIETO_INI && c <= QUIETO_FIN; }
bool fueraDeZona(uint16_t c){ return c >= FUERA_INI  && c <= FUERA_FIN; }
bool fiebre(uint16_t c)     { return c >= FIEBRE_INI && c <= FIEBRE_FIN; }

// Posicion: el animal camina hacia un objetivo (lejos de la zona durante el
// evento, el centro el resto del tiempo). Cuando esta dentro, deambula.
void simPos(uint16_t c, float &lat, float &lon) {
  float objX = 0.0, objY = 0.0;
  if (fueraDeZona(c)) {
    objX = RADIO_FUERA * cos(RUMBO_FUERA);
    objY = RADIO_FUERA * sin(RUMBO_FUERA);
  }

  posX += constrain(objX - posX, -PASO_M, PASO_M);
  posY += constrain(objY - posY, -PASO_M, PASO_M);

  // Solo deambula una vez que volvio al radio de paseo (si viene de afuera,
  // primero tiene que terminar de acercarse al centro).
  if (!fueraDeZona(c) && sqrt(posX * posX + posY * posY) <= RADIO_PASEO) {
    posX += randf(-40.0, 40.0);
    posY += randf(-40.0, 40.0);
    float r = sqrt(posX * posX + posY * posY);
    if (r > RADIO_PASEO) {
      posX *= RADIO_PASEO / r;
      posY *= RADIO_PASEO / r;
    }
  }

  const float M_POR_GRADO_LAT = 111320.0;
  const float M_POR_GRADO_LON = 111320.0 * cos(radians(CENTRO_LAT));
  lat = CENTRO_LAT + posY / M_POR_GRADO_LAT;
  lon = CENTRO_LON + posX / M_POR_GRADO_LON;
}

uint32_t simSats(uint16_t c) { return random(6, 12); }   // 6..11

// Temperatura corporal ~38.5 C, con pico de fiebre durante su tramo.
float simTemp(uint16_t c) {
  return 38.5 + (fiebre(c) ? 2.0 : 0.0) + randf(-0.2, 0.2);
}

// Aceleracion: normalmente hay movimiento; en el tramo de inactividad solo se
// mide la gravedad -> Node-RED dispara la alerta.
void simAccel(uint16_t c, float &ax, float &ay, float &az) {
  if (quieto(c)) {
    ax = 0.05; ay = 0.02; az = 9.79;
  } else {
    ax = randf(-2.0, 2.0);
    ay = randf(-2.0, 2.0);
    az = 9.8 + randf(-1.5, 1.5);
  }
}

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) {}

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  randomSeed(esp_random());

  // --- Radio (identico al collar real) ---
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  int st = radio.begin(FREQ_MHZ, BW_KHZ, SF, CR_DENOM, SYNC_WORD, TX_POWER, PREAMBLE, TCXO_V);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.print("Fallo LoRa, codigo "); Serial.println(st);
    while (true) { blink(1, 100); }
  }
  radio.setDio2AsRfSwitch(true);   // Heltec V3: DIO2 controla el switch de RF
  radio.setCRC(true);

  Serial.print("Collar SIMULADO listo @ "); Serial.print(FREQ_MHZ); Serial.print(" MHz, SF");
  Serial.print(SF); Serial.print(", BW "); Serial.print(BW_KHZ); Serial.println(" kHz");

  nextSendAt = millis();
}

void enviarPaquete() {
  uint16_t c = fase(seq);

  float lat, lon;
  simPos(c, lat, lon);
  uint32_t sats = simSats(c);
  float tempC = simTemp(c);
  float ax, ay, az;
  simAccel(c, ax, ay, az);

  char payload[96];
  snprintf(payload, sizeof(payload), "%u,%.6f,%.6f,%lu,%.2f,%.2f,%.2f,%.2f",
           seq, lat, lon, (unsigned long)sats, tempC, ax, ay, az);

  int st = radio.transmit((uint8_t*)payload, strlen(payload));
  if (st == RADIOLIB_ERR_NONE) {
    Serial.print("TX -> "); Serial.println(payload);
    blink(5, 20);
  } else {
    Serial.print("Error TX, codigo "); Serial.println(st);
  }

  seq++;
}

void loop() {
  if ((long)(millis() - nextSendAt) >= 0) {
    nextSendAt += PERIODO_MS;
    enviarPaquete();
  }
}
