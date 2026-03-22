#!/bin/bash
# Check Arduino Cloud device and Thing status
# Usage: ./scripts/cloud-status.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "${SCRIPT_DIR}/cloud-config.sh"

TOKEN=$(curl -s -X POST "https://api2.arduino.cc/iot/v1/clients/token" \
  -H "Content-Type: application/x-www-form-urlencoded" \
  --data-urlencode "grant_type=client_credentials" \
  --data-urlencode "client_id=$API_CLIENT_ID" \
  --data-urlencode "client_secret=$API_CLIENT_SECRET" \
  --data-urlencode "audience=https://api2.arduino.cc/iot" | python -c "import sys,json; print(json.load(sys.stdin)['access_token'])")

echo "=== Device: Marigold ==="
curl -s "https://api2.arduino.cc/iot/v2/devices/$DEVICE_ID" \
  -H "Authorization: Bearer $TOKEN" | python -c "
import sys,json
d = json.load(sys.stdin)
print(f\"  Status: {d.get('device_status','unknown')}\"  )
print(f\"  Name:   {d.get('name','unknown')}\")
print(f\"  ID:     {d.get('id','unknown')}\")
" 2>&1

echo ""
echo "=== Thing: Yun Sensor Hub ==="
curl -s "https://api2.arduino.cc/iot/v2/things/$THING_ID/properties" \
  -H "Authorization: Bearer $TOKEN" | python -c "
import sys,json
for p in json.load(sys.stdin):
    val = p.get('last_value', 'N/A')
    updated = p.get('value_updated_at', 'never')
    print(f\"  {p['name']:15s} = {val:<12} (updated: {updated})  [{p['id']}]\")
" 2>&1
