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

El collar simulado corre un **guion cíclico** de 35 paquetes (~3 min a 5 s por
paquete) que recorre el estado normal y las tres alertas, y vuelve a empezar:

| Paquetes | Qué pasa | Qué se ve en Grafana |
|---|---|---|
| 0–7 | dentro de la geocerca, con movimiento | tablero en verde |
| 8–14 | quieto (solo gravedad) | alerta de **inactividad** |
| 15–26 | se aleja y cruza el radio de 500 m | alerta de **fuera de zona** |
| 20–25 | pico de fiebre (mientras sigue afuera) | alerta de **temperatura** |
| 27–34 | vuelve a la zona y se normaliza | el tablero vuelve a verde |

Es un guion **acelerado a propósito**: el animal se desplaza más rápido de lo real
para que las tres alertas entren en una demo de pocos minutos.

> ⚠️ El centro de la geocerca del sketch (`CENTRO_LAT`/`CENTRO_LON`) **debe coincidir**
> con el de Node-RED (`pc/nodered/logica_collar.js`) y con el círculo del mapa
> (`pc/grafana/geocerca.geojson`). Si no coinciden, el animal aparece siempre fuera
> de zona y el tablero queda en alerta permanente.

Levantar el stack de la PC como indica `INSTRUCCIONES_PRUEBA.md` (secciones 4.1–4.4)
y correr el puente con `--port` (la base envía datos reales por USB, no hace falta
`--sim`).

## Después

Cuando ambas etapas funcionan, cargar el firmware definitivo
(`firmware/collar_heltec`) y conectar los sensores reales.
