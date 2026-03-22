#!/bin/bash
# Upload sketch to Arduino Yun using arduino-cli network upload
# Usage: ./scripts/upload-network.sh <sketch_name>

set -e

SKETCH_NAME="${1:?Usage: upload-network.sh <sketch_name>}"
FQBN="arduino:avr:yun"
YUN_IP="192.168.11.36"

echo "=== Compiling ${SKETCH_NAME} ==="
arduino-cli compile --fqbn "$FQBN" "sketches/${SKETCH_NAME}"

echo "=== Uploading to Yun at ${YUN_IP} ==="
arduino-cli upload --fqbn "$FQBN" --port "net:${YUN_IP}" "sketches/${SKETCH_NAME}"

echo "=== Upload complete ==="
