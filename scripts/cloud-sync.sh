#!/bin/bash
# Arduino Cloud Sync - runs on the PC
# Reads sensor data from the Yun's Bridge REST API (HTTP) and pushes
# to Arduino Cloud (HTTPS). Works around the Yun's outdated TLS stack.
#
# Usage: ./scripts/cloud-sync.sh [interval_seconds]
# Default interval: 10 seconds

INTERVAL="${1:-10}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Load configuration (credentials, IDs, property UUIDs)
CONFIG_FILE="${SCRIPT_DIR}/cloud-config.sh"
if [ ! -f "$CONFIG_FILE" ]; then
  echo "ERROR: Missing ${CONFIG_FILE}"
  echo "Copy scripts/cloud-config.example.sh to scripts/cloud-config.sh and fill in your credentials."
  exit 1
fi
. "$CONFIG_FILE"

API_BASE="https://api2.arduino.cc/iot/v2"
TOKEN=""
TOKEN_EXPIRY=0

log() {
  echo "$(date '+%H:%M:%S') $1"
}

get_token() {
  log "Requesting API token..."
  RESPONSE=$(curl -s -X POST "https://api2.arduino.cc/iot/v1/clients/token" \
    -H "Content-Type: application/x-www-form-urlencoded" \
    --data-urlencode "grant_type=client_credentials" \
    --data-urlencode "client_id=${API_CLIENT_ID}" \
    --data-urlencode "client_secret=${API_CLIENT_SECRET}" \
    --data-urlencode "audience=https://api2.arduino.cc/iot")

  TOKEN=$(echo "$RESPONSE" | python -c "import sys,json; print(json.load(sys.stdin).get('access_token',''))" 2>/dev/null)

  if [ -z "$TOKEN" ]; then
    log "ERROR: Failed to get token: $RESPONSE"
    return 1
  fi

  TOKEN_EXPIRY=$(($(date +%s) + 240))
  log "Token acquired (expires in 240s)"
  return 0
}

ensure_token() {
  NOW=$(date +%s)
  if [ -z "$TOKEN" ] || [ "$NOW" -ge "$TOKEN_EXPIRY" ]; then
    get_token
    return $?
  fi
  return 0
}

read_bridge() {
  # Read a value from the Yun's Bridge datastore
  curl -s --connect-timeout 3 "http://${YUN_HOST}/data/get/${1}" 2>/dev/null | \
    python -c "import sys,json; print(json.load(sys.stdin).get('value',''))" 2>/dev/null
}

read_all_bridge() {
  # Read all values from the Yun's Bridge datastore
  curl -s --connect-timeout 3 "http://${YUN_HOST}/data/get" 2>/dev/null
}

push_property() {
  PROP_NAME="$1"
  PROP_VALUE="$2"

  [ -z "$PROP_VALUE" ] && return

  RESULT=$(curl -s -o /dev/null -w "%{http_code}" \
    -X PUT "${API_BASE}/things/${THING_ID}/properties/${PROP_NAME}/publish" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"value\": $PROP_VALUE}" 2>&1)

  if [ "$RESULT" != "200" ]; then
    log "  WARN: push $PROP_NAME=$PROP_VALUE returned HTTP $RESULT"
  fi
}

log "=== Arduino Cloud Sync ==="
log "Device: $DEVICE_ID (Marigold)"
log "Thing:  $THING_ID (Yun Sensor Hub)"
log "Yun:    http://$YUN_HOST"
log "Interval: ${INTERVAL}s"
log "Press Ctrl+C to stop"
echo ""

while true; do
  ensure_token || { log "Token error, retrying in 30s..."; sleep 30; continue; }

  # Read sensor data from Yun Bridge
  BRIDGE_DATA=$(read_all_bridge)

  # Parse available values
  TEMP=$(echo "$BRIDGE_DATA" | python -c "
import sys,json
try:
  d={i['key']:i['value'] for i in json.load(sys.stdin).get('value',[])}
  keys=['temperature','analog_voltage','ow_temp_0']
  for k in keys:
    if k in d:
      print(d[k]); break
except: pass
" 2>/dev/null)

  HUMIDITY=$(echo "$BRIDGE_DATA" | python -c "
import sys,json
try:
  d={i['key']:i['value'] for i in json.load(sys.stdin).get('value',[])}
  print(d.get('humidity',''))
except: pass
" 2>/dev/null)

  LIGHT=$(echo "$BRIDGE_DATA" | python -c "
import sys,json
try:
  d={i['key']:i['value'] for i in json.load(sys.stdin).get('value',[])}
  v=d.get('light',d.get('a0',d.get('analog_raw','')))
  print(v)
except: pass
" 2>/dev/null)

  MOTION=$(echo "$BRIDGE_DATA" | python -c "
import sys,json
try:
  d={i['key']:i['value'] for i in json.load(sys.stdin).get('value',[])}
  v=d.get('motion',d.get('digital_state',d.get('d2','')))
  if v in ('1','HIGH'): print('true')
  elif v in ('0','LOW'): print('false')
  elif v: print(v)
except: pass
" 2>/dev/null)

  ANALOG=$(echo "$BRIDGE_DATA" | python -c "
import sys,json
try:
  d={i['key']:i['value'] for i in json.load(sys.stdin).get('value',[])}
  v=d.get('analog_raw',d.get('a0',''))
  print(v)
except: pass
" 2>/dev/null)

  # Push to Arduino Cloud (using property UUIDs)
  PUSHED=""
  if [ -n "$TEMP" ]; then push_property "$PROP_TEMPERATURE" "$TEMP"; PUSHED="${PUSHED} t=$TEMP"; fi
  if [ -n "$HUMIDITY" ]; then push_property "$PROP_HUMIDITY" "$HUMIDITY"; PUSHED="${PUSHED} h=$HUMIDITY"; fi
  if [ -n "$LIGHT" ]; then push_property "$PROP_LIGHT" "$LIGHT"; PUSHED="${PUSHED} l=$LIGHT"; fi
  if [ -n "$ANALOG" ]; then push_property "$PROP_ANALOG_RAW" "$ANALOG"; PUSHED="${PUSHED} a=$ANALOG"; fi

  if [ -n "$PUSHED" ]; then
    log "Synced:$PUSHED"
  else
    log "No sensor data available (is a sketch running on the Yun?)"
  fi

  sleep "$INTERVAL"
done
