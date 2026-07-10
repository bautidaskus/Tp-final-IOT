/*
 * PING de prueba - lado BASE - BSFrance LoRa32u4 II (ATmega32u4 + SX1276)
 * TP Final IoT 2026 - Rastreo de ganado por LoRa P2P
 *
 * Sketch MINIMO para verificar SOLO la radio. Recibe los "TEST,<seq>" que manda
 * ping_collar_heltec y los imprime por Serial con RSSI/SNR del enlace.
 * Si aparecen las lineas RX, los dos chips (SX1262 <-> SX1276) se hablan.
 *
 * Radio: SX1276 via libreria LoRa.h (sandeepmistry), con los MISMOS parametros
 * del collar: 915 MHz, SF12, BW 125 kHz, CR 4/5, preambulo 12, syncword 0x12.
 *
 * IDE: seleccionar la placa BsFrance -> LoRa32u4II (868-915MHz).
 */

#include <SPI.h>
#include <LoRa.h>

// --- Pines del modulo LoRa en la LoRa32u4 ---
const int PIN_SS    = 8;
const int PIN_RESET = 4;
const int PIN_DIO0  = 7;

// --- Parametros de radio: deben coincidir con el collar ---
const long    FRECUENCIA_HZ = 915E6;
const int     SF            = 12;
const long    BW_HZ         = 125E3;
const int     CR_DENOM      = 5;      // 4/5
const int     SYNC_WORD     = 0x12;
const int     PREAMBLE_LEN  = 12;

void configurarRadio() {
  LoRa.setSpreadingFactor(SF);
  LoRa.setSignalBandwidth(BW_HZ);
  LoRa.setCodingRate4(CR_DENOM);
  LoRa.setPreambleLength(PREAMBLE_LEN);
  LoRa.setSyncWord(SYNC_WORD);
  LoRa.enableCrc();
}

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) {}

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  LoRa.setPins(PIN_SS, PIN_RESET, PIN_DIO0);
  if (!LoRa.begin(FRECUENCIA_HZ)) {
    Serial.println(F("Fallo al inicializar LoRa (ping base)."));
    while (1) {}
  }
  configurarRadio();

  Serial.println(F("Ping base lista. Esperando TEST del collar..."));
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) return;

  String rx = "";
  while (LoRa.available()) rx += (char)LoRa.read();

  int   rssi = LoRa.packetRssi();
  float snr  = LoRa.packetSnr();

  Serial.print("RX: "); Serial.print(rx);
  Serial.print(" | RSSI="); Serial.print(rssi); Serial.print(" dBm");
  Serial.print(", SNR=");  Serial.print(snr, 1); Serial.println(" dB");

  digitalWrite(LED_BUILTIN, HIGH);
  delay(30);
  digitalWrite(LED_BUILTIN, LOW);
}
