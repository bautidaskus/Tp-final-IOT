/*
 * Nodo COLLAR - Heltec WiFi LoRa 32 V3 (ESP32-S3 + radio SX1262)
 * TP Final IoT 2026 - Rastreo y monitoreo de ganado por LoRa P2P
 *
 * Lee GPS (NEO-6M), acelerometro (MPU-6050) y temperatura (DS18B20),
 * arma un paquete CSV y lo transmite por LoRa cada PERIODO_MS.
 *
 * Radio: SX1262 via RadioLib. Parametros elegidos para maximo alcance
 * y para ser compatibles con la base LoRa32u4 (SX1276 / libreria LoRa.h):
 *   915 MHz, SF12, BW 125 kHz, CR 4/5, preambulo 12, syncword 0x12, CRC on.
 *
 * Formato del paquete (CSV):  seq,lat,lon,sats,tempC,ax,ay,az
 *   seq  : contador de paquete (0..65535)
 *   lat  : latitud  (grados, 6 decimales; 0 si el GPS aun no fijo)
 *   lon  : longitud (grados, 6 decimales; 0 si el GPS aun no fijo)
 *   sats : satelites en uso (0 = sin fix)
 *   tempC: temperatura DS18B20 en C (-127 = sensor no detectado)
 *   ax,ay,az : aceleracion en m/s^2 (2 decimales)
 */

#include <RadioLib.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <TinyGPSPlus.h>
#include <OneWire.h>
#include <DallasTemperature.h>

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
const float   FREQ_MHZ   = 915.0;   // MHz
const float   BW_KHZ     = 125.0;   // kHz
const uint8_t SF         = 12;      // 6..12
const uint8_t CR_DENOM   = 5;       // 4/5
const uint8_t SYNC_WORD  = 0x12;    // red privada (igual que sandeepmistry LoRa)
const int8_t  TX_POWER   = 20;      // dBm (hasta 22 en SX1262)
const uint16_t PREAMBLE  = 12;
const float   TCXO_V     = 1.8;     // Heltec V3: el SX1262 usa TCXO de 1.8V

// ----------------- Sensores (pines libres del header del V3) -----------------
// Evitamos los pines del SX1262 (8-14) y del OLED (17,18,21,36).
#define PIN_SDA     41    // MPU-6050 SDA
#define PIN_SCL     42    // MPU-6050 SCL
#define PIN_GPS_RX  6     // ESP RX  <- GPS TX
#define PIN_GPS_TX  5     // ESP TX  -> GPS RX
#define PIN_DS18B20 7     // DS18B20 data (pullup 4.7k a 3V3)
#define PIN_LED     35    // LED on-board del Heltec V3

Adafruit_MPU6050 mpu;
bool mpuOk = false;

TinyGPSPlus gps;

OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);

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

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) {}

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  // --- Sensores ---
  Wire.begin(PIN_SDA, PIN_SCL);
  mpuOk = mpu.begin(0x68, &Wire);
  if (mpuOk) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("MPU-6050 OK");
  } else {
    Serial.println("MPU-6050 NO detectado (se enviara 0,0,0)");
  }

  Serial1.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  ds18b20.begin();

  // --- Radio ---
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  int st = radio.begin(FREQ_MHZ, BW_KHZ, SF, CR_DENOM, SYNC_WORD, TX_POWER, PREAMBLE, TCXO_V);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.print("Fallo LoRa, codigo "); Serial.println(st);
    while (true) { blink(1, 100); }
  }
  radio.setDio2AsRfSwitch(true);   // Heltec V3: DIO2 controla el switch de RF
  radio.setCRC(true);

  Serial.print("Collar listo @ "); Serial.print(FREQ_MHZ); Serial.print(" MHz, SF");
  Serial.print(SF); Serial.print(", BW "); Serial.print(BW_KHZ); Serial.println(" kHz");

  nextSendAt = millis();
}

void alimentarGps() {
  while (Serial1.available()) {
    gps.encode(Serial1.read());
  }
}

float leerTemp() {
  ds18b20.requestTemperatures();
  return ds18b20.getTempCByIndex(0);   // -127 si no hay sensor
}

void enviarPaquete() {
  float lat = 0, lon = 0;
  uint32_t sats = 0;
  if (gps.location.isValid()) {
    lat = gps.location.lat();
    lon = gps.location.lng();
  }
  if (gps.satellites.isValid()) sats = gps.satellites.value();

  float ax = 0, ay = 0, az = 0;
  if (mpuOk) {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);
    ax = a.acceleration.x;
    ay = a.acceleration.y;
    az = a.acceleration.z;
  }

  float tempC = leerTemp();

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
  alimentarGps();

  if ((long)(millis() - nextSendAt) >= 0) {
    nextSendAt += PERIODO_MS;
    enviarPaquete();
  }
}
