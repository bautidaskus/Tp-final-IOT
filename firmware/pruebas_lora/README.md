# Sketches de prueba — LoRa y collar simulado

Material **de prueba**, separado del firmware final. No reemplaza nada: el
firmware definitivo sigue siendo `firmware/collar_heltec` + `firmware/base_lora32u4`.
Sirve para validar el sistema **por etapas** antes de conectar los sensores reales.

Todos usan los mismos parámetros de radio del proyecto (915 MHz, SF12, BW 125 kHz,
CR 4/5, preámbulo 12, sync 0x12, CRC on), así que las pruebas son representativas.

## Etapa 1 — ¿Se hablan las dos radios? (ping crudo)

Prueba **solo el enlace LoRa** entre los dos micros distintos (Heltec SX1262 ↔
LoRa32u4 SX1276), sin sensores.

| Placa | Sketch | Qué hace |
|---|---|---|
| Heltec V3 | `pruebas_lora/ping_collar_heltec/` | Transmite `TEST,<seq>` cada 2 s. |
| LoRa32u4 | `pruebas_lora/ping_base_lora32u4/` | Recibe e imprime `RX: TEST,n \| RSSI=.. SNR=..`. |

1. Cargar cada sketch en su placa (Heltec V3 / BsFrance → LoRa32u4II).
2. Abrir el Monitor Serie de la **base a 115200**.
3. Si aparecen las líneas `RX: TEST,...`, el enlace físico funciona. ✅

## Etapa 2 — ¿Anda todo el pipeline? (collar con sensores simulados)

Prueba **collar → base → PC → Node-RED → Grafana** sin depender de GPS/MPU/DS18B20.

| Placa | Sketch | Qué hace |
|---|---|---|
| Heltec V3 | `../collar_sim_heltec/` | Mismo CSV que el collar real, pero con datos **simulados**. |
| LoRa32u4 | `../base_lora32u4/` (el de siempre) | Sin cambios: recibe y reenvía por USB. |

El collar simulado reproduce el mismo escenario que `pc/serial_bridge.py --sim`
(recorrido que sale de la geocerca, pico de fiebre en seq 20–25, inactividad en
seq 8–14), así que en Grafana se ven las **tres alertas**. Levantar el stack de la
PC como indica `INSTRUCCIONES_PRUEBA.md` (secciones 4.1–4.4) y correr el puente con
`--port` (la base envía datos reales por USB, no hace falta `--sim`).

## Después

Cuando ambas etapas funcionan, cargar el firmware definitivo
(`firmware/collar_heltec`) y conectar los sensores reales.
