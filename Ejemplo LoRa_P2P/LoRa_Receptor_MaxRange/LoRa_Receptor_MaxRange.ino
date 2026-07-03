\
#include <SPI.h>
#include <LoRa.h>

// --- Hardware: BSFrance LoRa32u4 (SX1276 / RFM95 compatible) ---
const int PIN_SS    = 8;
const int PIN_RESET = 4;
const int PIN_DIO0  = 7;

// --- Radio params: deben coincidir con el emisor ---
const long FRECUENCIA_HZ = 915E6;  // Hz
const int  SF             = 12;     // 6..12 (12 = más alcance)
const long BW_HZ          = 125E3;  // 125 kHz
const int  CR_DENOM       = 5;      // Coding Rate 4/5
const int  TX_POWER_DBM   = 14;     // No afecta RX, lo dejamos razonable
const int  PREAMBLE_LEN   = 12;

void configurarRadio() {
  LoRa.setFrequency(FRECUENCIA_HZ);
  LoRa.setSpreadingFactor(SF);
  LoRa.setSignalBandwidth(BW_HZ);
  LoRa.setCodingRate4(CR_DENOM);
  LoRa.setPreambleLength(PREAMBLE_LEN);
  LoRa.enableCrc();
  LoRa.setTxPower(TX_POWER_DBM);
}

void setup() {
  Serial.begin(9600);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) {}

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  LoRa.setPins(PIN_SS, PIN_RESET, PIN_DIO0);
  if (!LoRa.begin(FRECUENCIA_HZ)) {
    Serial.println("Fallo al inicializar LoRa (receptor).");
    while (1) {}
  }
  configurarRadio();

  Serial.print("Receptor @ "); Serial.print(FRECUENCIA_HZ/1e6, 3); Serial.print(" MHz");
  Serial.print(", SF"); Serial.print(SF);
  Serial.print(", BW "); Serial.print(BW_HZ/1000); Serial.print(" kHz");
  Serial.print(", CR 4/"); Serial.print(CR_DENOM);
  Serial.print(", TX "); Serial.print(TX_POWER_DBM); Serial.println(" dBm");
  Serial.print("Preamble "); Serial.println(PREAMBLE_LEN);
  Serial.println("Esperando paquetes...");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    String rx;
    while (LoRa.available()) rx += (char)LoRa.read();

    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    Serial.print("RX ["); Serial.print(packetSize); Serial.print("B]: ");
    Serial.print(rx);
    Serial.print(" | RSSI="); Serial.print(rssi); Serial.print(" dBm");
    Serial.print(", SNR=");  Serial.print(snr, 1); Serial.println(" dB");
    
    // Blink LED 1 s para confirmar recepcion
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);

    // ACK: eco con prefijo "OK "
    String ack = String("OK ") + rx;
    LoRa.beginPacket();
    LoRa.write((const uint8_t*)ack.c_str(), ack.length());
    LoRa.endPacket();
    // Serial.println("ACK enviado");
  }

  delay(2);
}
