# Diseño — Sketches de prueba LoRa y collar con sensores simulados

**Fecha:** 2026-07-10
**Contexto:** TP Final IoT 2026 — monitoreo de ganado por LoRa P2P.
**Objetivo:** agregar código de prueba, **sin tocar nada de lo existente**, para
validar el sistema por etapas antes de conectar los sensores reales.

## Motivación

Hoy el par que hay que verificar es **heterogéneo**: collar = Heltec V3 (SX1262,
RadioLib) y base = LoRa32u4 (SX1276, `LoRa.h`). Los ejemplos en `Ejemplo LoRa_P2P/`
son **ambos para el LoRa32u4**, así que no prueban ese enlace real. Además, el
collar real depende de GPS con fix + MPU + DS18B20, lo que complica probar el
pipeline de la PC. Se agregan dos etapas de prueba aisladas.

## Alcance (qué se agrega)

Tres sketches nuevos. **No se modifica** `firmware/collar_heltec`,
`firmware/base_lora32u4`, `pc/`, `bsfrance/` ni la documentación existente.

### A) Test de comunicación LoRa (ping crudo, una vía + RSSI)

```
firmware/pruebas_lora/
├─ ping_collar_heltec/ping_collar_heltec.ino     # Heltec V3 (SX1262, RadioLib) → TX
└─ ping_base_lora32u4/ping_base_lora32u4.ino      # LoRa32u4 (SX1276, LoRa.h)  → RX
```

- Mismos parámetros de radio del proyecto: 915 MHz, SF12, BW 125 kHz, CR 4/5,
  preámbulo 12, sync 0x12, CRC on. Heltec: TCXO 1.8 V + DIO2 como RF switch.
- **ping_collar:** cada 2 s transmite `TEST,<seq>`, imprime `TX -> TEST,n`, parpadea
  LED. Sin librerías de sensores (solo RadioLib) → aísla la radio.
- **ping_base:** recibe, imprime `RX: TEST,n | RSSI=.. SNR=..`, parpadea LED.
- **Propósito:** confirmar que los dos chips distintos se hablan. Si esto anda, el
  enlace físico está OK.

### B) Collar con sensores simulados

```
firmware/collar_sim_heltec/collar_sim_heltec.ino   # Heltec V3, mismo CSV, datos simulados
```

- Idéntico al `collar_heltec.ino` real (misma radio, mismo formato
  `seq,lat,lon,sats,tempC,ax,ay,az`, período 5 s) pero **sin GPS/MPU/DS18B20**.
- Funciones `simLat/simLon/simSats/simTemp/simAccel(seq)` que reproducen el mismo
  guion que `serial_bridge.py --sim`:
  - recorrido que arranca en la geocerca (−41.130, −71.310) y sale ~seq 15,
  - pico de fiebre en seq 20–25,
  - inactividad (solo gravedad) en seq 8–14.
  Así Grafana muestra las 3 alertas (geocerca, temperatura, inactividad).
- **La base no cambia:** se reutiliza `firmware/base_lora32u4/base_lora32u4.ino`
  (es agnóstica a si el dato es real o simulado; le agrega rssi/snr reales).

## Orden de prueba que habilita

1. A — ¿se hablan las radios de los dos micros? (solo enlace físico)
2. B — ¿anda el pipeline completo Heltec→base→PC→Node-RED→Grafana con datos realistas?
3. Sensores reales (firmware existente sin cambios).

## No-objetivos

- No modificar el firmware ni el stack existentes.
- No agregar ACK/bidireccional al ping (una vía + RSSI alcanza para verificar).
- No simular en la base (no lee sensores).
