#!/usr/bin/env bash
# Simula al gateway ESP32: publica lecturas por MQTT con el mismo JSON que arma buildJson().
# Sirve para probar el backend, la API y el WebSocket sin hardware.
#
#   ./tools/simular-esp32.sh                 # 10 envíos, uno cada 3 s, dispositivo esp32_01
#   ./tools/simular-esp32.sh 50 1 esp32_02   # 50 envíos, uno por segundo, otro dispositivo
#
# Requiere el stack levantado (docker compose up -d). En Windows: Git Bash.
set -euo pipefail
cd "$(dirname "$0")/.."

COUNT="${1:-10}"
INTERVAL="${2:-3}"
DEVICE="${3:-esp32_01}"
TOPIC="esp/sensores/${DEVICE}"

rnd() { awk -v min="$1" -v max="$2" -v s="$RANDOM" 'BEGIN { srand(s); printf "%.1f", min + rand() * (max - min) }'; }

for i in $(seq 1 "$COUNT"); do
  ts="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  msg=$(cat <<JSON
{"metadata":{"deviceId":"${DEVICE}","timestamp":"${ts}"},"payload":[
{"sensorId":"TMP_INT","type":"temperature","value":$(rnd 24 34),"unit":"°C"},
{"sensorId":"TMP_EXT","type":"temperature","value":$(rnd 14 26),"unit":"°C"},
{"sensorId":"HUM_INT","type":"humidity","value":$(rnd 45 80),"unit":"%"},
{"sensorId":"SOIL_1","type":"soil_moisture","value":$(rnd 20 70),"unit":"%"},
{"sensorId":"LUX_1","type":"light","value":$(rnd 200 900),"unit":"lux"}]}
JSON
)
  # MSYS_NO_PATHCONV evita que Git Bash convierta el tópico en una ruta de Windows.
  MSYS_NO_PATHCONV=1 docker compose exec -T mosquitto \
    mosquitto_pub -h localhost -q 1 -t "$TOPIC" -m "$(echo "$msg" | tr -d '\n')"
  echo "[$i/$COUNT] publicado en $TOPIC ($ts)"
  [ "$i" -lt "$COUNT" ] && sleep "$INTERVAL"
done
