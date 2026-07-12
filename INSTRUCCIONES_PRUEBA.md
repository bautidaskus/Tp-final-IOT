# TP Final IoT 2026 — Guía de armado y prueba

**Sistema de geolocalización y monitoreo de ganado mediante red LoRa P2P de bajo costo.**
Grupo: Daskus · Muñoz · Franco.

Este documento explica **qué se necesita y cómo probar todo el sistema**, con o sin
el hardware. Está pensado para quien tiene las placas (Juan Pablo) pero sirve de
guía completa para la demo.

---

## 1. Qué es (en una frase)

Un **nodo collar** (Heltec WiFi LoRa 32 V3) lee GPS, acelerómetro y temperatura,
y transmite las mediciones por **LoRa punto a punto** (sin gateway ni LoRaWAN) a
una **estación base** (BSFrance LoRa32u4). La base está conectada por USB a la PC,
que corre Mosquitto + Node-RED + InfluxDB + Grafana para calcular geocerca y
alertas, guardar el histórico y mostrar el recorrido en un mapa.

> **Importante para la presentación:** esto es **LoRa P2P** (radio a radio), **no
> LoRaWAN**. LoRaWAN requeriría un gateway y la infraestructura de red (servidor de
> red, ej. The Things Network). Acá los dos módulos se hablan directo. El profe
> pidió dejar esta diferencia explícita.

```
 COLLAR (Heltec V3, SX1262)                        BASE (LoRa32u4, SX1276)
 GPS + MPU6050 + DS18B20  ──LoRa 915MHz P2P──▶  recibe ──USB──▶  PC
                                                                  │
                        Python (serial_bridge.py) ──MQTT──▶ Mosquitto
                                                                  │
                              Node-RED (geocerca + alertas) ──▶ InfluxDB ──▶ Grafana
```

---

## 2. Hardware necesario

| Componente | Detalle |
|---|---|
| Nodo collar | Heltec WiFi LoRa 32 **V3** (ESP32-S3 + radio SX1262) |
| GPS | GY-NEO6MV2 (NEO-6M) |
| Acelerómetro | MPU-6050 |
| Temperatura | DS18B20 + resistencia 4.7 kΩ (pull-up) |
| Estación base | BSFrance **LoRa32u4 II (868-915 MHz)** (SX1276) |
| Antenas | Una por cada placa (915 MHz) |
| Cables USB | Uno por placa |

> ⚠️ **Conectar SIEMPRE la antena antes de alimentar** cada placa LoRa. Transmitir
> sin antena puede dañar el amplificador de la radio.

### Conexionado del collar (Heltec V3)

Los pines del SX1262 (8–14) y del OLED (17,18,21,36) están reservados. Los sensores
van a pines libres:

| Sensor | Pin del sensor | GPIO Heltec V3 |
|---|---|---|
| MPU-6050 | VCC | 3V3 |
| MPU-6050 | GND | GND |
| MPU-6050 | SDA | **41** |
| MPU-6050 | SCL | **42** |
| GPS NEO-6M | VCC | 3V3 |
| GPS NEO-6M | GND | GND |
| GPS NEO-6M | TX  | **6** (RX del ESP) |
| GPS NEO-6M | RX  | **5** (TX del ESP) |
| DS18B20 | VCC | 3V3 |
| DS18B20 | GND | GND |
| DS18B20 | DATA | **7** (+ 4.7 kΩ entre DATA y 3V3) |

> Si algún pin no te conviene físicamente, se cambian en las constantes `#define`
> arriba de `collar_heltec.ino` (bloque "Sensores").

La base LoRa32u4 **no lleva cableado**: solo antena + USB.

---

## 3. Preparar la IDE de Arduino

### 3.1 Placas (Boards Manager)
- **Heltec V3:** instalar el paquete **esp32 by Espressif Systems** (Boards Manager).
  Al compilar elegir la placa **"Heltec WiFi LoRa 32(V3)"**.
- **Base LoRa32u4:** copiar dentro de la carpeta `Arduino` (normalmente
  `Documentos/Arduino`) una carpeta llamada `hardware` (crearla si no existe) y
  descomprimir ahí el contenido de `bsfrance.rar` (está en `TP final/bsfrance/`).
  Al compilar elegir **BsFrance → LoRa32u4II (868-915MHz)**.

### 3.2 Librerías (Library Manager)
Instalar todas estas:

| Librería | Para |
|---|---|
| **RadioLib** | radio SX1262 del Heltec (collar) |
| **TinyGPSPlus** | GPS NEO-6M |
| **Adafruit MPU6050** | acelerómetro (arrastra *Adafruit Unified Sensor* y *Adafruit BusIO*) |
| **OneWire** | bus 1-Wire del DS18B20 |
| **DallasTemperature** | sensor DS18B20 |
| **LoRa** (de Sandeepmistry) | radio SX1276 de la base |

### 3.3 Cargar el firmware
- **Collar:** abrir `firmware/collar_heltec/collar_heltec.ino`, placa Heltec V3,
  puerto COM correcto → Upload.
- **Base:** abrir `firmware/base_lora32u4/base_lora32u4.ino`, placa LoRa32u4II →
  Upload. *(El 32u4 usa bootloader Caterina: si falla el upload, apretar reset
  justo cuando empieza a subir, o doble reset para entrar al bootloader.)*

Con ambos cargados, abrir el **Monitor Serie de la base a 115200 baudios**:
deberían aparecer líneas como
`5,-41.329200,-69.543600,8,38.50,0.10,0.05,9.79,-92,7.5`.
Eso confirma que el enlace LoRa funciona.

---

## 4. Levantar el stack en la PC

Requiere **Docker Desktop** y **Python 3** instalados.

### 4.1 Servicios (Docker)
Desde la carpeta `pc/`:
```bash
docker compose up -d
```
Esto levanta Mosquitto (1883), InfluxDB (8086), Node-RED (1880) y Grafana (3000).
Grafana ya queda con el datasource de InfluxDB y el dashboard cargados
(provisioning automático).

### 4.2 Cargar el flow de Node-RED
1. Abrir **http://localhost:1880**.
2. Menú (☰) → **Import** → pegar/seleccionar `pc/nodered/flow_ganado.json` → Import.
3. **Deploy** (botón rojo arriba a la derecha).

### 4.3 Puente serial → MQTT (corre en el host, no en Docker)
Node-RED está en Docker y en Windows **no puede ver el puerto COM**, por eso este
puente corre nativo:
```bash
py -m pip install pyserial paho-mqtt      # una sola vez
py serial_bridge.py --port COM3           # reemplazar COM3 por el de la base
```
(Para saber el COM: Administrador de dispositivos → Puertos COM, o el menú de la IDE.)

### 4.4 Ver el tablero
Abrir **http://localhost:3000** (usuario `admin`, contraseña `admin`) → dashboard
**"Monitoreo de Ganado - LoRa P2P"**: mapa con la última posición, temperatura,
distancia a la geocerca, movimiento y RSSI del enlace.

---

## 5. Modos de prueba

El sistema se puede correr en tres configuraciones, según qué hardware haya
disponible. **Las tres usan el mismo stack de PC** (secciones 4.1–4.4).

| | Firmware del collar | Enlace LoRa | Sensores | Puente |
|---|---|---|---|---|
| **A. Todo real** | `collar_heltec` | real | reales (GPS/MPU/DS18B20) | `--port COM3` |
| **B. Sensores simulados** | `collar_sim_heltec` | **real** | simulados por código | `--port COM3` |
| **C. Sin hardware** | — (no hay placas) | — | simulados en la PC | `--sim` |

El **modo B** permite validar todo el pipeline —incluido el enlace de radio, con
RSSI y SNR reales— sin depender de que el GPS tenga fix ni de tener los sensores
cableados. La consigna de la materia lo permite explícitamente: *"En caso de no
contar con sensores específicos, se permite la simulación mediante entradas
analógicas, pulsadores o funciones en el código"*.

El **modo C** no usa las placas: el RSSI y el SNR son valores generados, **no
medidos**. Sirve para probar el stack de la PC o como respaldo si falla el hardware,
pero no es evidencia del enlace de radio.

```bash
docker compose up -d
# importar el flow en Node-RED y Deploy (paso 4.2)
py serial_bridge.py --sim
```

En modo C se levanta además un **mini-panel** en <http://localhost:8000> que permite
disparar a mano los eventos (**salir de la zona**, **fiebre**, **inactividad**; duran
60 s y vuelven solos) y **ajustar los umbrales en vivo**: se publican por MQTT en
`ganado/config` y Node-RED los toma sin reiniciar.

---

## 6. Ajustar la geocerca y los umbrales

La geocerca está al principio del nodo `function` de Node-RED (archivo de
referencia `pc/nodered/logica_collar.js`):

```js
const CENTRO_LAT = -41.3292;  // Ingeniero Jacobacci, Rio Negro
const CENTRO_LON = -69.5436;
const RADIO_M    = 500;       // radio en metros
```
Los umbrales (`TEMP_MAX` 39,5 °C · `MOV_MIN` 0,5 · `INACT_MS` 15000 ms) tienen esos
valores por defecto, pero se pisan en vivo desde el panel del modo C.

> ⚠️ Si cambiás el centro, hay que cambiarlo en **tres lugares** o el tablero queda
> en alerta permanente (el animal aparece fuera de zona siempre):
> 1. `pc/nodered/logica_collar.js` (el nodo function) → **Deploy** de nuevo;
> 2. `firmware/collar_sim_heltec/collar_sim_heltec.ino` (`CENTRO_LAT`/`CENTRO_LON`);
> 3. `pc/grafana/geocerca.geojson` (el círculo que dibuja el mapa).

---

## 7. Formato del paquete LoRa

CSV corto (el collar arma esto; la base le agrega `rssi,snr`):
```
seq,lat,lon,sats,tempC,ax,ay,az[,rssi,snr]
```
| campo | significado |
|---|---|
| seq | contador de paquete |
| lat, lon | posición GPS (0 si aún no hay fix) |
| sats | satélites en uso (0 = sin fix) |
| tempC | temperatura DS18B20 (−127 = sensor no detectado) |
| ax, ay, az | aceleración en m/s² |
| rssi, snr | calidad del enlace (los agrega la base) |

---

## 8. Parámetros de radio (deben coincidir en ambas placas)

915 MHz · SF12 · BW 125 kHz · CR 4/5 · preámbulo 12 · sync word 0x12 · CRC on.
Elegidos para **máximo alcance**. El collar usa RadioLib (SX1262) y la base
`LoRa.h` (SX1276); son chips distintos pero **interoperables** porque comparten
estos parámetros de PHY. El Heltec V3 además necesita **TCXO 1.8 V** y **DIO2 como
RF switch** (ya está en el código).

---

## 9. Problemas comunes

| Síntoma | Causa / solución |
|---|---|
| La base no recibe nada | Antenas conectadas; mismos parámetros de radio; acercar las placas para la primera prueba. |
| Heltec: "Fallo LoRa, código..." | Radio no inicializa; verificar que es un **V3** (SX1262) y que el TCXO está en 1.8 V (ya en el código). |
| El GPS manda lat/lon = 0 | Sin fix todavía; el NEO-6M necesita **cielo abierto** y hasta varios minutos la primera vez. |
| tempC = −127 | DS18B20 mal conectado o sin la resistencia de 4.7 kΩ. |
| Node-RED no ve el COM | Es esperado en Docker; el puente Python corre en el host (paso 4.3). |
| Grafana sin datos | Verificar que el puente publica (`[rx]`/`[sim]` en consola), el debug de Node-RED, y `docker logs ganado-influxdb`. |
| Upload falla en la base 32u4 | Bootloader Caterina: reset al iniciar la subida o doble reset. |

---

## 10. Estructura del entregable

```
TP final/
├─ firmware/
│  ├─ collar_heltec/collar_heltec.ino     # nodo collar (Heltec V3, RadioLib)
│  └─ base_lora32u4/base_lora32u4.ino      # estación base (LoRa32u4, LoRa.h)
├─ pc/
│  ├─ docker-compose.yml                   # Mosquitto + InfluxDB + Node-RED + Grafana
│  ├─ mosquitto/config/mosquitto.conf
│  ├─ serial_bridge.py                     # puente serial->MQTT (con modo --sim)
│  ├─ nodered/
│  │  ├─ flow_ganado.json                  # flow a importar en Node-RED
│  │  └─ logica_collar.js                  # (referencia) la lógica del nodo function
│  └─ grafana/provisioning/                # datasource + dashboard automáticos
├─ bsfrance/                               # soporte de placa para la base (IDE)
└─ INSTRUCCIONES_PRUEBA.md                 # este archivo
```
