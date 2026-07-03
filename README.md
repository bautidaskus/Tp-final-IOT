# Monitoreo de ganado por LoRa P2P — TP Final IoT 2026

**Grupo:** Daskus, Bautista · Muñoz, Juan Pablo · Franco, Valentín

Resumen de **qué construimos** y **qué decisiones técnicas tomamos**. Para la guía
de armado y prueba paso a paso ver [`INSTRUCCIONES_PRUEBA.md`](INSTRUCCIONES_PRUEBA.md);
para la consigna original y la propuesta ver
[`Requisitos entrega, propuesta y comentarios.md`](Requisitos%20entrega,%20propuesta%20y%20comentarios.md).

---

## 1. El problema

En zonas ganaderas extensas (ej. la Patagonia) el ganado se desplaza por campos
amplios **sin cobertura celular**, lo que dificulta localizarlo y controlar su
bienestar. Los collares GPS comerciales dependen de redes móviles o satelitales y
son caros. Buscamos un sistema **de bajo costo y bajo consumo** para rastrear y
monitorear animales usando **LoRa** (largo alcance, hasta varios km en línea de
vista) más las herramientas de telemetría vistas en clase.

## 2. Qué construimos

Dos nodos que se comunican por **LoRa punto a punto** y una PC que procesa y
visualiza:

- **Nodo collar** (Heltec WiFi LoRa 32 V3): lee **GPS**, **acelerómetro** y
  **temperatura**, arma un paquete y lo transmite por LoRa cada 5 s.
- **Estación base** (BSFrance LoRa32u4): recibe los paquetes y los reenvía por USB
  a la PC.
- **PC**: un puente Python pasa los datos a **MQTT**; **Node-RED** calcula geocerca
  y alertas; **InfluxDB** guarda el histórico; **Grafana** muestra el recorrido en
  un mapa y los gráficos.

```
 COLLAR (Heltec V3, SX1262)                       BASE (LoRa32u4, SX1276)
 GPS + MPU6050 + DS18B20  ──LoRa 915MHz P2P──▶  recibe ──USB──▶  PC
                                                                 │
                       Python (serial_bridge.py) ──MQTT──▶ Mosquitto
                                                                 │
                             Node-RED (geocerca + alertas) ──▶ InfluxDB ──▶ Grafana
```

### Funcionalidades de aplicación
- **Geocerca (geofencing):** alerta cuando el animal sale de una zona circular
  definida (centro + radio).
- **Alerta de temperatura:** aviso por fiebre (temperatura corporal sobre umbral).
- **Alerta de inactividad:** aviso si no hay movimiento durante un tiempo
  prolongado (posible animal caído/enfermo).
- **Histórico y mapa:** registro de toda la telemetría y recorrido en Grafana.

## 3. Decisiones técnicas (qué tomamos y por qué)

| Decisión | Por qué |
|---|---|
| **LoRa P2P, no LoRaWAN** | LoRaWAN necesita un gateway y la infraestructura de red (servidor de red, ej. TTN). Para dos nodos que se hablan directo, P2P es más simple y suficiente. El profe pidió dejar esta diferencia explícita. |
| **RadioLib en el collar / `LoRa.h` en la base** | Son chips distintos: el Heltec V3 tiene un **SX1262** (lo maneja RadioLib) y la base un **SX1276** (lo maneja la librería `LoRa` de sandeepmistry). Se hablan igual porque comparten los parámetros de radio. |
| **Mismos parámetros de radio en ambos** | 915 MHz, SF12, BW 125 kHz, CR 4/5, preámbulo 12, sync 0x12, CRC on. Elegidos para **máximo alcance**. Es lo que hace interoperables al SX1262 y al SX1276. |
| **Puente Python serial→MQTT en la PC** | La base LoRa32u4 **no tiene WiFi**, así que no publica MQTT sola. Además Node-RED corre en Docker y en Windows no puede ver el puerto COM. El puente nativo resuelve las dos cosas. |
| **Lógica de alertas en la PC (Node-RED)** | Más fácil de ajustar y demostrar en vivo que reprogramar el collar. El collar solo manda datos crudos. |
| **Reuso del stack de la Práctica 2** | Mosquitto + Node-RED + InfluxDB + Grafana ya los teníamos andando en docker-compose; los reutilizamos casi sin cambios. |
| **Modo simulación (`--sim`)** | Permite validar todo el pipeline de la PC **sin hardware** y sirve de respaldo para la demo. |

## 4. Formato del paquete

CSV corto que arma el collar (la base le agrega calidad de enlace):
```
seq,lat,lon,sats,tempC,ax,ay,az[,rssi,snr]
```

## 5. Ventajas y limitaciones de la tecnología

**Ventajas**
- **Largo alcance con bajo consumo**, ideal para campo sin cobertura celular.
- **Bajo costo:** placas ESP32/LoRa económicas, sin cuotas de red móvil.
- **P2P sin infraestructura:** no requiere gateway ni servidor de red.

**Limitaciones**
- **Bajo bitrate:** en SF12 el paquete tarda ~1–2 s en aire; sirve para telemetría
  espaciada, no para datos continuos.
- **Enlace directo (1 a 1):** sin la escalabilidad ni la gestión de red de LoRaWAN
  (muchos nodos, múltiples gateways, ADR, seguridad de red).
- **Alcance sensible a la línea de vista** y a la antena; obstáculos lo degradan.
- **Sin cifrado de aplicación** en esta versión (P2P plano).

## 6. Hardware

| Rol | Placa / componente |
|---|---|
| Collar (emisor) | Heltec WiFi LoRa 32 V3 (SX1262) + GPS NEO-6M + MPU-6050 + DS18B20 |
| Base (receptor) | BSFrance LoRa32u4 II 868-915 MHz (SX1276) |

## 7. Estado y verificación

- Firmware del collar y de la base: **compilan** correctamente.
- Lógica de geocerca / temperatura / inactividad y formato de InfluxDB:
  **verificados** con datos simulados.
- Pendiente de prueba con hardware real (lo realiza el integrante que tiene las
  placas, siguiendo `INSTRUCCIONES_PRUEBA.md`).

## 8. Estructura del proyecto

```
TP final/
├─ firmware/         # sketches del collar y la base
├─ pc/               # docker-compose, puente Python, flow Node-RED, Grafana
├─ bsfrance/         # soporte de placa para la base (IDE)
├─ README.md                 # este documento (contexto y decisiones)
└─ INSTRUCCIONES_PRUEBA.md   # guía de armado y prueba
```
