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
 * Sin guion fijo: el animal se mantiene SIEMPRE en valores normales (dentro de
 * la geocerca de Ingeniero Jacobacci, temperatura normal, con movimiento). Las
 * alertas (fuera de zona / fiebre / inactividad) se inducen a demanda desde el
 * panel de pc/serial_bridge.py, que sobreescribe los campos del paquete real.
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

// El animal deambula SIEMPRE dentro de la zona: jitter de +-120 m alrededor del
// centro (siempre bajo el radio de 500 m de la geocerca).
float simLat(uint16_t s) { return CENTRO_LAT + randf(-120, 120) / 111320.0; }
float simLon(uint16_t s) { return CENTRO_LON + randf(-120, 120) / (111320.0 * cos(radians(CENTRO_LAT))); }
uint32_t simSats(uint16_t s) { return random(6, 12); }   // 6..11

// Temperatura corporal normal ~38.5 C (la fiebre se induce desde el panel).
float simTemp(uint16_t s) {
  return 38.5 + randf(-0.2, 0.2);
}

// Aceleracion: siempre hay movimiento (la inactividad se induce desde el panel).
void simAccel(uint16_t s, float &ax, float &ay, float &az) {
  ax = randf(-2.0, 2.0);
  ay = randf(-2.0, 2.0);
  az = 9.8 + randf(-1.5, 1.5);
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
  float lat = simLat(seq);
  float lon = simLon(seq);
  uint32_t sats = simSats(seq);
  float tempC = simTemp(seq);
  float ax, ay, az;
  simAccel(seq, ax, ay, az);

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
