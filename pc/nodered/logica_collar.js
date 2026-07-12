// Nodo "function" de Node-RED: parsea el CSV del collar, calcula geocerca y
// alertas, y arma la linea de InfluxDB (salida 1) + un mensaje de alerta MQTT
// opcional (salida 2).
//
// CSV de entrada:  seq,lat,lon,sats,tempC,ax,ay,az,rssi,snr

// ---- Geocerca (constante; para cambiarla se edita aca y se regenera el
//      circulo del mapa con pc/grafana/geocerca.geojson). ----
const CENTRO_LAT = -41.3292;  // Ingeniero Jacobacci, Rio Negro
const CENTRO_LON = -69.5436;
const RADIO_M    = 500;       // radio de la geocerca en metros

// ---- Umbrales configurables en vivo desde el panel (topic ganado/config).
//      Si todavia no llego config, se usan estos valores por defecto. ----
const TEMP_MAX = flow.get("TEMP_MAX") ?? 39.5;      // umbral de fiebre (C)
const MOV_MIN  = flow.get("MOV_MIN")  ?? 0.5;        // m/s^2 sobre gravedad = "movimiento"
const INACT_MS = flow.get("INACT_MS") ?? 15000;     // sin movimiento por mas de esto => inactividad

function haversine(lat1, lon1, lat2, lon2) {
  const R = 6371000;
  const toRad = (d) => d * Math.PI / 180;
  const dLat = toRad(lat2 - lat1);
  const dLon = toRad(lon2 - lon1);
  const a = Math.sin(dLat / 2) ** 2 +
            Math.cos(toRad(lat1)) * Math.cos(toRad(lat2)) * Math.sin(dLon / 2) ** 2;
  return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

const linea = String(msg.payload).trim();
const p = linea.split(",");
if (p.length < 10) {
  node.warn("Paquete incompleto: " + linea);
  return null;
}

const seq  = parseInt(p[0], 10);
const lat  = parseFloat(p[1]);
const lon  = parseFloat(p[2]);
const sats = parseInt(p[3], 10);
const temp = parseFloat(p[4]);
const ax   = parseFloat(p[5]);
const ay   = parseFloat(p[6]);
const az   = parseFloat(p[7]);
const rssi = parseInt(p[8], 10);
const snr  = parseFloat(p[9]);

const tieneFix = sats > 0 && (lat !== 0 || lon !== 0);

// --- Geocerca ---
let dist = 0;
let fueraZona = 0;
if (tieneFix) {
  dist = haversine(CENTRO_LAT, CENTRO_LON, lat, lon);
  fueraZona = dist > RADIO_M ? 1 : 0;
}

// --- Temperatura ---
const alertaTemp = (temp > TEMP_MAX && temp > -100) ? 1 : 0;

// --- Inactividad (usa contexto de flujo para medir tiempo sin movimiento) ---
const movimiento = Math.abs(Math.sqrt(ax * ax + ay * ay + az * az) - 9.8);
const ahora = Date.now();
let ultimoMov = flow.get("ultimoMov") || ahora;
if (movimiento >= MOV_MIN) {
  ultimoMov = ahora;
  flow.set("ultimoMov", ultimoMov);
}
const inactivo = (ahora - ultimoMov) > INACT_MS ? 1 : 0;

// --- Salida 1: linea de InfluxDB (line protocol) ---
const campos = [
  `lat=${lat}`,
  `lon=${lon}`,
  `sats=${sats}i`,
  `temp=${temp}`,
  `ax=${ax}`, `ay=${ay}`, `az=${az}`,
  `rssi=${rssi}i`,
  `snr=${snr}`,
  `movimiento=${movimiento.toFixed(3)}`,
  `dist_m=${dist.toFixed(1)}`,
  `fuera_zona=${fueraZona}i`,
  `alerta_temp=${alertaTemp}i`,
  `inactivo=${inactivo}i`,
  `temp_max=${TEMP_MAX}`,       // umbral vigente, para graficarlo como linea dinamica
].join(",");

const msgInflux = {
  headers: { "Content-Type": "text/plain" },
  payload: `ganado,device=collar01 ${campos}`,
};

// --- Salida 2: alerta MQTT (solo si hay alguna condicion activa) ---
let msgAlerta = null;
const motivos = [];
if (fueraZona)  motivos.push(`fuera de zona (${dist.toFixed(0)} m)`);
if (alertaTemp) motivos.push(`temperatura alta (${temp} C)`);
if (inactivo)   motivos.push("inactividad prolongada");
if (motivos.length > 0) {
  msgAlerta = {
    topic: "ganado/collar/alertas",
    payload: JSON.stringify({ seq, lat, lon, temp, motivos }),
  };
  node.status({ fill: "red", shape: "dot", text: motivos.join(" | ") });
} else {
  node.status({ fill: "green", shape: "dot", text: `ok seq ${seq}` });
}

return [msgInflux, msgAlerta];
