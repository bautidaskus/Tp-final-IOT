# Monitoreo de ganado por LoRa P2P — Código fuente

**TP Final IoT 2026 · Daskus, Bautista · Muñoz, Juan Pablo · Franco, Valentín**

Esta carpeta contiene el código de la versión entregada del sistema: **enlace LoRa
real con sensores simulados por software**. El collar transmite por radio LoRa igual
que con sensores reales, pero genera los valores de GPS, temperatura y aceleración
por código; el enlace de radio y todo el procesamiento en la PC son reales.

El detalle del diseño, la justificación técnica y los resultados están en el
**informe** (`../Informe_Tecnico_Daskus_Munoz_Franco.docx`).

## Estructura

```
codigo/
├─ firmware/
│  ├─ collar_sim_heltec/   # nodo collar (Heltec V3, RadioLib) — sensores simulados
│  └─ base_lora32u4/       # estación base (LoRa32u4, LoRa.h) — recibe y reenvía por USB
├─ pc/
│  ├─ docker-compose.yml   # Mosquitto + InfluxDB + Node-RED + Grafana
│  ├─ serial_bridge.py     # puente serie → MQTT + panel de control de alertas
│  ├─ mosquitto/           # config del broker
│  ├─ nodered/             # flow a importar + lógica de referencia
│  └─ grafana/             # datasource + dashboard (provisioning) + geocerca
├─ bsfrance/               # soporte de placa de la base para la IDE de Arduino
└─ pinout.jpeg             # pinout de referencia del collar
```

```

## Requisitos

- **IDE de Arduino** con soporte para ESP32 (Espressif) y las librerías
  **RadioLib** (collar) y **LoRa** de sandeepmistry (base).
- **Docker Desktop** y **Python 3** en la PC.

## 1. Firmware

### Collar — `firmware/collar_sim_heltec/collar_sim_heltec.ino`
Placa **Heltec WiFi LoRa 32 (V3)**. Requiere la librería **RadioLib**. Compilar y
subir. No lleva sensores cableados: los datos se generan por software.

### Base — `firmware/base_lora32u4/base_lora32u4.ino`
Placa **BsFrance → LoRa32u4II (868-915 MHz)**. Requiere la librería **LoRa**
(sandeepmistry). Solo necesita antena + USB (sin cableado).

> **Soporte de placa de la base:** copiar dentro de la carpeta `Arduino`
> (normalmente `Documentos/Arduino`) una carpeta llamada `hardware` (crearla si no
> existe) y descomprimir ahí el contenido de `bsfrance/`. Al compilar, elegir
> **BsFrance → LoRa32u4II (868-915MHz)**.


Con ambos cargados, el Monitor Serie de la base a **115200 baudios** debería mostrar
líneas CSV como `5,-41.328900,-69.543100,8,38.50,0.10,0.05,9.79,-92,7.5`, lo que
confirma que el enlace LoRa funciona.

## 2. Stack de la PC

Desde la carpeta `pc/`:

```bash
docker compose up -d
```

Levanta Mosquitto (1883), InfluxDB (8086), Node-RED (1880) y Grafana (3000). Grafana
queda con el datasource y el dashboard cargados por provisioning.

**Cargar el flow de Node-RED:** abrir http://localhost:1880 → menú (☰) → *Import* →
seleccionar `pc/nodered/flow_ganado.json` → *Import* → **Deploy**.

## 3. Puente serie → MQTT

Node-RED corre en Docker y en Windows no ve el puerto COM, por eso este puente corre
nativo en el host:

```bash
py -m pip install pyserial paho-mqtt      # una sola vez
py serial_bridge.py --port COM3           # reemplazar COM3 por el puerto de la base
```

## 4. Ver el tablero

Abrir http://localhost:3000 (usuario `admin`, contraseña `admin`) → dashboard
**"Monitoreo de Ganado - LoRa P2P"**: mapa con la posición, temperatura, distancia a
la geocerca, movimiento y calidad del enlace (RSSI/SNR).

## 5. Panel de control (demostración de alertas)

El puente levanta un panel en **http://localhost:8000** para inducir las alertas a
demanda sobre los datos que igual viajaron por LoRa:

- **Botones** de *salir de la zona*, *fiebre* e *inactividad*: sobreescriben los
  campos del paquete real durante 60 s y luego vuelven solos al estado normal.
- **Umbrales** de temperatura, movimiento e inactividad ajustables en vivo (se
  publican por MQTT en `ganado/config` y Node-RED los toma sin reiniciar).

## Parámetros de radio (iguales en ambas placas)

915 MHz · SF12 · BW 125 kHz · CR 4/5 · preámbulo 12 · sync word 0x12 · CRC on.
El collar usa RadioLib (SX1262) y la base `LoRa.h` (SX1276); son chips distintos pero
interoperables porque comparten estos parámetros de capa física.

## Formato del paquete

```
seq,lat,lon,sats,tempC,ax,ay,az[,rssi,snr]
```
El collar arma los 8 primeros campos; la base agrega `rssi,snr` (calidad del enlace).

## Geocerca

Centrada en **Ingeniero Jacobacci, Río Negro** (`-41.3292, -69.5436`), radio 500 m.
Si se cambia el centro, hay que actualizarlo en tres lugares para que el tablero no
quede en alerta permanente: `pc/nodered/logica_collar.js`,
`firmware/collar_sim_heltec/collar_sim_heltec.ino` y `pc/grafana/geocerca.geojson`.
