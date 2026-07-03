/*
 * Estacion BASE - BSFrance LoRa32u4 II (ATmega32u4 + radio SX1276 / HPD13A)
 * TP Final IoT 2026 - Rastreo y monitoreo de ganado por LoRa P2P
 *
 * Recibe los paquetes del collar por LoRa y los reenvia por USB (Serial)
 * a la PC, donde Node-RED los toma con el nodo "serial in".
 *
 * Radio: SX1276 via libreria LoRa.h (sandeepmistry). Parametros identicos
 * al collar: 915 MHz, SF12, BW 125 kHz, CR 4/5, preambulo 12, syncword 0x12.
 *
 * Salida por Serial (una linea por paquete, CSV):
 *   seq,lat,lon,sats,tempC,ax,ay,az,rssi,snr
 * (los 8 primeros campos son el paquete crudo del collar; se le agregan
 *  rssi y snr medidos en la recepcion.)
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
    Serial.println(F("Fallo al inicializar LoRa (base)."));
    while (1) {}
  }
  configurarRadio();

  Serial.println(F("Base lista. Esperando paquetes del collar..."));
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) return;

  String rx = "";
  while (LoRa.available()) rx += (char)LoRa.read();

  int   rssi = LoRa.packetRssi();
  float snr  = LoRa.packetSnr();

  // Reenvia paquete crudo + metricas de enlace (una linea CSV para Node-RED).
  Serial.print(rx);
  Serial.print(',');
  Serial.print(rssi);
  Serial.print(',');
  Serial.println(snr, 1);

  // Parpadeo corto para confirmar recepcion.
  digitalWrite(LED_BUILTIN, HIGH);
  delay(30);
  digitalWrite(LED_BUILTIN, LOW);
}
