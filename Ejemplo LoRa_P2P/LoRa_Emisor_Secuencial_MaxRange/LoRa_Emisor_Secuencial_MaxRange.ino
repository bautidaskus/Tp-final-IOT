\
#include <SPI.h>
#include <LoRa.h>

// --- Hardware: BSFrance LoRa32u4 (SX1276 / RFM95 compatible) ---
const int PIN_SS    = 8;
const int PIN_RESET = 4;
const int PIN_DIO0  = 7;

// --- Radio params: configurados para MÁXIMO ALCANCE dentro de 902–928 MHz ---

const long FRECUENCIA_HZ = 915E6;   // Hz
const int  SF             = 12;     // Spreading Factor: 6..12 (12 = mayor alcance, menor bitrate)
const long BW_HZ          = 125E3;  // Ancho de banda: 7.8k..500k (125 kHz es estándar y sensible)
const int  CR_DENOM       = 5;      // Coding rate 4/5..4/8 -> pasar denominador: 5..8
const int  TX_POWER_DBM   = 20;     // Potencia TX en dBm (hasta 20 dBm con PA_BOOST)
const int  PREAMBLE_LEN   = 12;     // Aumentar un poco ayuda a sincronizar en enlaces largos

// --- Timings ---
const unsigned long PERIODO_MS    = 5000;
const unsigned long ACK_TIMEOUTMS = 2000;

// Estado
uint16_t seq = 0;
unsigned long nextSendAt = 0;

void configurarRadio() {
  // Frecuencia/canal
  LoRa.setFrequency(FRECUENCIA_HZ);

  // Modulación para máximo alcance
  LoRa.setSpreadingFactor(SF);           
  LoRa.setSignalBandwidth(BW_HZ);        
  LoRa.setCodingRate4(CR_DENOM);         
  LoRa.setPreambleLength(PREAMBLE_LEN);  
  LoRa.enableCrc();                      

  
  LoRa.setTxPower(TX_POWER_DBM);         // 20 dBm
}

void setup() {
  Serial.begin(9600);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) {}

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  LoRa.setPins(PIN_SS, PIN_RESET, PIN_DIO0);
  if (!LoRa.begin(FRECUENCIA_HZ)) {
    Serial.println("Fallo al inicializar LoRa (emisor).");
    while (1) {}
  }
  configurarRadio();

  Serial.print("Emisor @ "); Serial.print(FRECUENCIA_HZ/1e6, 3); Serial.print(" MHz");
  Serial.print(", SF"); Serial.print(SF);
  Serial.print(", BW "); Serial.print(BW_HZ/1000); Serial.print(" kHz");
  Serial.print(", CR 4/"); Serial.print(CR_DENOM);
  Serial.print(", TX "); Serial.print(TX_POWER_DBM); Serial.println(" dBm");
  Serial.print("Preamble "); Serial.println(PREAMBLE_LEN);

  nextSendAt = millis(); // primer envío inmediato
}

void enviarPaquete(uint16_t numero) {
  char payload[4];
  snprintf(payload, sizeof(payload), "%03u", numero % 1000);

  LoRa.beginPacket();
  LoRa.write((const uint8_t*)payload, 3);
  LoRa.endPacket();

  Serial.print("TX -> "); Serial.println(payload);
   // Parpadeo LED (5 veces al enviar)
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(20);
    digitalWrite(LED_BUILTIN, LOW);
    delay(20);
  }

}

void loop() {
  unsigned long now = millis();

  // Envío periódico
  if ((long)(now - nextSendAt) >= 0) {
    enviarPaquete(seq);
    seq = (seq + 1) % 1000;
    nextSendAt += PERIODO_MS;

    // Esperar ACK con métricas
    bool gotAck = false;
    unsigned long t0 = millis();
    while (millis() - t0 < ACK_TIMEOUTMS) {
      int ps = LoRa.parsePacket();
      if (ps > 0) {
        String ack;
        while (LoRa.available()) ack += (char)LoRa.read();
        int rssi = LoRa.packetRssi();
        float snr = LoRa.packetSnr();

        Serial.print("ACK ["); Serial.print(ps); Serial.print("B]: ");
        Serial.print(ack);
        Serial.print(" | RSSI="); Serial.print(rssi); Serial.print(" dBm");
        Serial.print(", SNR=");  Serial.print(snr, 1); Serial.println(" dB");

        // Blink LED 1 s para confirmar ACK
        digitalWrite(LED_BUILTIN, HIGH);
        delay(1000);
        digitalWrite(LED_BUILTIN, LOW);

        gotAck = true;
        break;
      }
      delay(2);
    }
    if (!gotAck) {
      Serial.println("ACK timeout");
    }
  }

  // Drenar paquetes inesperados
  int ps = LoRa.parsePacket();
  if (ps > 0) { while (LoRa.available()) LoRa.read(); }
  delay(2);
}
