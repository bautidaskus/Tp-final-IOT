#!/usr/bin/env python3
"""
Puente Serial -> MQTT para el TP Final IoT (rastreo de ganado por LoRa P2P).

La estacion base (LoRa32u4) esta conectada por USB y escupe una linea CSV por
cada paquete recibido del collar:

    seq,lat,lon,sats,tempC,ax,ay,az,rssi,snr

Este script lee esas lineas del puerto COM y las republica tal cual en el topic
MQTT  ganado/collar/raw . Node-RED se encarga de parsear y aplicar la logica.

Node-RED corre en Docker y en Windows no puede acceder al puerto COM; por eso
este puente corre nativo en el host y publica al broker Mosquitto expuesto en
localhost:1883.

Uso:
    py serial_bridge.py --port COM3                 # hardware real
    py serial_bridge.py --sim                       # datos simulados (sin base)

Requisitos:  py -m pip install pyserial paho-mqtt
"""

import argparse
import math
import random
import sys
import time

import paho.mqtt.client as mqtt


def parse_args():
    p = argparse.ArgumentParser(description="Puente Serial -> MQTT (TP ganado LoRa)")
    p.add_argument("--port", default="COM3", help="Puerto serie de la base (ej. COM3, /dev/ttyUSB0)")
    p.add_argument("--baud", type=int, default=115200, help="Baudrate (debe coincidir con la base)")
    p.add_argument("--mqtt-host", default="localhost")
    p.add_argument("--mqtt-port", type=int, default=1883)
    p.add_argument("--topic", default="ganado/collar/raw")
    p.add_argument("--sim", action="store_true", help="Genera datos simulados en vez de leer el COM")
    return p.parse_args()


def conectar_mqtt(host, port):
    client = mqtt.Client()
    client.connect(host, port, 60)
    client.loop_start()
    print(f"[bridge] Conectado a MQTT {host}:{port}")
    return client


def generar_simulado(seq):
    """Un recorrido que arranca dentro de la geocerca y se va alejando.
    Centro de referencia (usado tambien en Node-RED): -41.130, -71.310 (Bariloche)."""
    lat0, lon0 = -41.130, -71.310
    # El animal se desplaza ~ hacia el sureste; a partir de seq~15 sale de la zona.
    lat = lat0 - 0.0008 * seq
    lon = lon0 + 0.0008 * seq
    sats = random.randint(6, 11)
    # Temperatura corporal ~38.5C, con un pico de fiebre alrededor de seq 20-25.
    temp = 38.5 + (2.0 if 20 <= seq <= 25 else 0.0) + random.uniform(-0.2, 0.2)
    # Aceleracion: normalmente hay movimiento; entre seq 8 y 14 el animal esta quieto.
    if 8 <= seq <= 14:
        ax, ay, az = 0.05, 0.02, 9.79          # solo gravedad -> inactividad
    else:
        ax = random.uniform(-2, 2)
        ay = random.uniform(-2, 2)
        az = 9.8 + random.uniform(-1.5, 1.5)
    rssi = random.randint(-115, -70)
    snr = round(random.uniform(-8, 10), 1)
    return f"{seq},{lat:.6f},{lon:.6f},{sats},{temp:.2f},{ax:.2f},{ay:.2f},{az:.2f},{rssi},{snr}"


def run_sim(client, topic):
    print("[bridge] MODO SIMULACION (sin hardware). Ctrl+C para cortar.")
    seq = 0
    while True:
        linea = generar_simulado(seq)
        client.publish(topic, linea)
        print(f"[sim] {linea}")
        seq = (seq + 1) % 1000
        time.sleep(3)


def run_serial(client, topic, port, baud):
    try:
        import serial  # pyserial
    except ImportError:
        sys.exit("Falta pyserial. Instalar con:  py -m pip install pyserial")

    print(f"[bridge] Abriendo {port} @ {baud} ...")
    with serial.Serial(port, baud, timeout=1) as ser:
        print("[bridge] Leyendo. Ctrl+C para cortar.")
        while True:
            raw = ser.readline()
            if not raw:
                continue
            linea = raw.decode("utf-8", errors="ignore").strip()
            if not linea:
                continue
            # Solo reenviamos lineas de datos (empiezan con un numero de seq).
            if not linea[0].isdigit():
                print(f"[base] {linea}")   # mensajes de arranque de la base
                continue
            client.publish(topic, linea)
            print(f"[rx] {linea}")


def main():
    args = parse_args()
    client = conectar_mqtt(args.mqtt_host, args.mqtt_port)
    try:
        if args.sim:
            run_sim(client, args.topic)
        else:
            run_serial(client, args.topic, args.port, args.baud)
    except KeyboardInterrupt:
        print("\n[bridge] Cortado por el usuario.")
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
