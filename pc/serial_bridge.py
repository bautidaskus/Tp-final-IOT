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

Modo simulacion (--sim): el animal pasea DENTRO de la geocerca con temperatura
normal. Un mini-panel web (http://localhost:8000) permite, sin hardware:
  - simular por tiempo acotado que el animal sale de la zona / tiene fiebre / se
    queda quieto (asi se ven las alertas en vivo; vuelven solas a los 60 s), y
  - ajustar en vivo los umbrales de temperatura y movimiento (se publican por
    MQTT en 'ganado/config' y Node-RED los toma sin reiniciar).

Uso:
    py serial_bridge.py --port COM3                 # hardware real
    py serial_bridge.py --sim                       # datos simulados (sin base)

Requisitos:  py -m pip install pyserial paho-mqtt
"""

import argparse
import json
import math
import random
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import paho.mqtt.client as mqtt


# ---- Geocerca de la simulacion (el centro/radio coinciden con Node-RED) ------
# Ingeniero Jacobacci, Rio Negro (estepa patagonica, sin lagos).
CENTRO_LAT, CENTRO_LON = -41.3292, -69.5436
RADIO_M = 500.0            # radio de la geocerca (igual que logica_collar.js)
RADIO_NORMAL = 300.0       # el paseo normal se mantiene dentro de este radio
RADIO_FUERA = 800.0        # a donde se aleja el animal durante el evento "fuera"
BEARING_FUERA = math.radians(45.0)   # rumbo del alejamiento (noreste)
PASO_M = 120.0             # cuanto avanza por tick hacia su objetivo
EVENTO_SEG = 60            # cuanto dura un evento disparado por boton
PUERTO_PANEL = 8000        # puerto del mini-panel de control
TOPIC_CONFIG = "ganado/config"

# Momento (epoch) hasta el que cada evento simulado esta activo.
eventos = {"fuera": 0.0, "fiebre": 0.0, "inactividad": 0.0}

# Posicion actual del animal como offset en metros respecto del centro.
_pos = {"dx": 0.0, "dy": 0.0}

# Umbrales configurables desde el panel (valores por defecto). inact en segundos.
config = {"temp_max": 39.5, "mov_min": 0.5, "inact_seg": 15}

_mqtt = None   # cliente MQTT, seteado al conectar (lo usa el panel para config)


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
    global _mqtt
    client = mqtt.Client()
    client.connect(host, port, 60)
    client.loop_start()
    _mqtt = client
    print(f"[bridge] Conectado a MQTT {host}:{port}")
    return client


def publicar_config():
    """Publica los umbrales actuales (retenido) para que Node-RED los tome."""
    if _mqtt is None:
        return
    payload = {
        "temp_max": config["temp_max"],
        "mov_min": config["mov_min"],
        "inact_ms": int(config["inact_seg"] * 1000),
    }
    _mqtt.publish(TOPIC_CONFIG, json.dumps(payload), retain=True)
    print(f"[panel] config publicada: {payload}")


def _clamp(v, lo, hi):
    return max(lo, min(hi, v))


def paso_simulado(seq):
    """Genera el siguiente paquete CSV. Por defecto el animal deambula dentro de
    la geocerca; los eventos activos (por boton) lo sacan de zona, le suben la
    temperatura o lo dejan quieto."""
    ahora = time.time()
    fuera = ahora < eventos["fuera"]
    fiebre = ahora < eventos["fiebre"]
    inactivo = ahora < eventos["inactividad"]

    # --- Posicion: se mueve hacia un objetivo (fuera de zona o el centro) ---
    if fuera:
        objx = RADIO_FUERA * math.cos(BEARING_FUERA)
        objy = RADIO_FUERA * math.sin(BEARING_FUERA)
    else:
        objx, objy = 0.0, 0.0

    _pos["dx"] += _clamp(objx - _pos["dx"], -PASO_M, PASO_M)
    _pos["dy"] += _clamp(objy - _pos["dy"], -PASO_M, PASO_M)

    if not fuera:
        # Deambular: ruido aleatorio, pero sin salirse del radio normal.
        _pos["dx"] += random.uniform(-40, 40)
        _pos["dy"] += random.uniform(-40, 40)
        r = math.hypot(_pos["dx"], _pos["dy"])
        if r > RADIO_NORMAL:
            _pos["dx"] *= RADIO_NORMAL / r
            _pos["dy"] *= RADIO_NORMAL / r

    mlat = 111320.0
    mlon = 111320.0 * math.cos(math.radians(CENTRO_LAT))
    lat = CENTRO_LAT + _pos["dy"] / mlat
    lon = CENTRO_LON + _pos["dx"] / mlon

    sats = random.randint(6, 11)
    temp = 38.5 + (2.0 if fiebre else 0.0) + random.uniform(-0.2, 0.2)

    if inactivo:
        ax, ay, az = 0.05, 0.02, 9.79          # solo gravedad -> inactividad
    else:
        ax = random.uniform(-2, 2)
        ay = random.uniform(-2, 2)
        az = 9.8 + random.uniform(-1.5, 1.5)

    rssi = random.randint(-115, -70)
    snr = round(random.uniform(-8, 10), 1)
    return f"{seq},{lat:.6f},{lon:.6f},{sats},{temp:.2f},{ax:.2f},{ay:.2f},{az:.2f},{rssi},{snr}"


# ---- Mini-panel web de control de la simulacion ------------------------------

PAGINA = """<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Simulación del collar</title>
<style>
  body { font-family: Arial, Helvetica, sans-serif; background:#f4f4f3; color:#242424;
         margin:0; padding:24px; max-width:560px; line-height:1.5; }
  h1 { font-size:19px; font-weight:600; margin:0 0 4px; }
  h2 { font-size:13px; font-weight:600; color:#555; text-transform:uppercase;
       letter-spacing:.03em; margin:26px 0 10px; border-bottom:1px solid #dcdcdc;
       padding-bottom:5px; }
  p.nota { color:#777; font-size:13px; margin:0; }
  .fila { display:flex; gap:8px; flex-wrap:wrap; }
  .fila button { flex:1 1 150px; padding:11px; font-size:14px; cursor:pointer;
                 background:#fff; color:#333; border:1px solid #bcbcbc; border-radius:5px; }
  .fila button:hover { background:#ededed; }
  .estado { margin-top:12px; font-size:13px; color:#666; min-height:18px; }
  table.umbrales td { padding:5px 12px 5px 0; font-size:14px; }
  table.umbrales input { width:78px; padding:5px; border:1px solid #bcbcbc;
                         border-radius:5px; font-size:14px; }
  .guardar { margin-top:12px; padding:9px 18px; background:#3a3a3a; color:#fff;
             border:none; border-radius:5px; font-size:14px; cursor:pointer; }
  .guardar:hover { background:#555; }
  #cfgmsg { color:#2e7d46; font-size:13px; margin-left:10px; }
</style></head>
<body>
  <h1>Simulación del collar</h1>
  <p class="nota">Sin hardware conectado. Sirve para probar las alertas del tablero.</p>

  <h2>Eventos</h2>
  <div class="fila">
    <button onclick="disparar('fuera')">Salir de la zona</button>
    <button onclick="disparar('fiebre')">Fiebre</button>
    <button onclick="disparar('inactividad')">Inactividad</button>
  </div>
  <div class="estado" id="estado"></div>

  <h2>Umbrales</h2>
  <table class="umbrales">
    <tr><td>Temperatura (°C)</td><td><input id="temp_max" type="number" step="0.1"></td></tr>
    <tr><td>Movimiento (m/s²)</td><td><input id="mov_min" type="number" step="0.1"></td></tr>
    <tr><td>Inactividad (s)</td><td><input id="inact_seg" type="number" step="1"></td></tr>
  </table>
  <button class="guardar" onclick="aplicar()">Guardar</button>
  <span id="cfgmsg"></span>

<script>
const nombres = { fuera: 'fuera de zona', fiebre: 'fiebre', inactividad: 'inactividad' };
function disparar(ev){ fetch('/evento/'+ev, {method:'POST'}).then(actualizar); }
function actualizar(){
  fetch('/estado').then(r=>r.json()).then(d=>{
    const act = Object.entries(d).filter(([k,v])=>v>0);
    const el = document.getElementById('estado');
    el.textContent = act.length
      ? 'En curso: ' + act.map(([k,v])=>nombres[k]+' ('+v+' s)').join(', ')
      : 'Sin eventos activos.';
  });
}
function cargarCfg(){
  fetch('/config').then(r=>r.json()).then(c=>{
    document.getElementById('temp_max').value = c.temp_max;
    document.getElementById('mov_min').value  = c.mov_min;
    document.getElementById('inact_seg').value = c.inact_seg;
  });
}
function aplicar(){
  const body = {
    temp_max: parseFloat(document.getElementById('temp_max').value),
    mov_min:  parseFloat(document.getElementById('mov_min').value),
    inact_seg: parseFloat(document.getElementById('inact_seg').value)
  };
  fetch('/config', {method:'POST', headers:{'Content-Type':'application/json'},
                    body: JSON.stringify(body)})
    .then(()=>{ const m = document.getElementById('cfgmsg'); m.textContent = 'Guardado';
                setTimeout(()=>{ m.textContent = ''; }, 2000); });
}
setInterval(actualizar, 1000); actualizar(); cargarCfg();
</script>
</body></html>"""


class PanelHandler(BaseHTTPRequestHandler):
    def _send(self, code, body, ctype="text/html; charset=utf-8"):
        data = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/" or self.path.startswith("/index"):
            self._send(200, PAGINA)
        elif self.path == "/estado":
            ahora = time.time()
            restante = {k: max(0, round(v - ahora)) for k, v in eventos.items()}
            self._send(200, json.dumps(restante), "application/json")
        elif self.path == "/config":
            self._send(200, json.dumps(config), "application/json")
        else:
            self._send(404, "no encontrado", "text/plain")

    def do_POST(self):
        if self.path.startswith("/evento/"):
            nombre = self.path.rsplit("/", 1)[-1]
            if nombre in eventos:
                eventos[nombre] = time.time() + EVENTO_SEG
                print(f"[panel] evento '{nombre}' activado por {EVENTO_SEG}s")
                self._send(200, "ok", "text/plain")
                return
            self._send(404, "evento desconocido", "text/plain")
        elif self.path == "/config":
            largo = int(self.headers.get("Content-Length", 0))
            try:
                nuevo = json.loads(self.rfile.read(largo) or "{}")
            except json.JSONDecodeError:
                self._send(400, "json invalido", "text/plain")
                return
            for k in ("temp_max", "mov_min", "inact_seg"):
                if k in nuevo:
                    config[k] = float(nuevo[k])
            publicar_config()
            self._send(200, "ok", "text/plain")
        else:
            self._send(404, "no encontrado", "text/plain")

    def log_message(self, *args):
        pass   # silenciar el log HTTP por request


def iniciar_panel():
    servidor = ThreadingHTTPServer(("0.0.0.0", PUERTO_PANEL), PanelHandler)
    t = threading.Thread(target=servidor.serve_forever, daemon=True)
    t.start()
    print(f"[bridge] Panel de simulacion en http://localhost:{PUERTO_PANEL}")


def run_sim(client, topic):
    print("[bridge] MODO SIMULACION (sin hardware). Ctrl+C para cortar.")
    iniciar_panel()
    publicar_config()   # deja los umbrales por defecto disponibles para Node-RED
    seq = 0
    while True:
        linea = paso_simulado(seq)
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
