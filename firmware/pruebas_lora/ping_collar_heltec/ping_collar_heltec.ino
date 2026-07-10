/*
 * PING de prueba - lado COLLAR - Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)
 * TP Final IoT 2026 - Rastreo de ganado por LoRa P2P
 *
 * Sketch MINIMO para verificar SOLO la radio (sin GPS/MPU/DS18B20). Transmite
 * "TEST,<seq>" cada 2 s. En la otra placa se carga ping_base_lora32u4.
 * Si la base imprime los TEST, el enlace fisico entre los dos chips esta OK y
 * recien ahi tiene sentido sumar sensores.
 *
 * Radio: SX1262 via RadioLib, con los MISMOS parametros del collar real para que
 * el test valga: 915 MHz, SF12, BW 125 kHz, CR 4/5, preambulo 12, syncword 0x12,
 * CRC on, TCXO 1.8 V, DIO2 como RF switch.
 *
 * Libreria necesaria: RadioLib (nada mas).
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

const unsigned long PERIODO_MS = 2000;
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

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  int st = radio.begin(FREQ_MHZ, BW_KHZ, SF, CR_DENOM, SYNC_WORD, TX_POWER, PREAMBLE, TCXO_V);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.print("Fallo LoRa, codigo "); Serial.println(st);
    while (true) { blink(1, 100); }
  }
  radio.setDio2AsRfSwitch(true);   // Heltec V3: DIO2 controla el switch de RF
  radio.setCRC(true);

  Serial.print("Ping collar listo @ "); Serial.print(FREQ_MHZ); Serial.print(" MHz, SF");
  Serial.print(SF); Serial.print(", BW "); Serial.print(BW_KHZ); Serial.println(" kHz");

  nextSendAt = millis();
}

void loop() {
  if ((long)(millis() - nextSendAt) >= 0) {
    nextSendAt += PERIODO_MS;

    char payload[16];
    snprintf(payload, sizeof(payload), "TEST,%u", seq);

    int st = radio.transmit((uint8_t*)payload, strlen(payload));
    if (st == RADIOLIB_ERR_NONE) {
      Serial.print("TX -> "); Serial.println(payload);
      blink(5, 20);
    } else {
      Serial.print("Error TX, codigo "); Serial.println(st);
    }

    seq++;
  }
}
