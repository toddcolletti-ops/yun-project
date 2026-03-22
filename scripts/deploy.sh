#!/bin/bash
# Deploy a compiled sketch to the Arduino Yun via network (SCP + SSH)
# Usage: ./scripts/deploy.sh <sketch_name>
#
# This uploads the compiled hex file and uses avrdude on the Yun
# to flash the ATmega32U4 over the Bridge.

set -e

YUN_HOST="yun"
SKETCH_NAME="${1:?Usage: deploy.sh <sketch_name>}"
FQBN="arduino:avr:yun"
BUILD_DIR="build"
HEX_FILE="${BUILD_DIR}/${SKETCH_NAME}.ino.hex"

echo "=== Compiling ${SKETCH_NAME} ==="
arduino-cli compile --fqbn "$FQBN" --output-dir "$BUILD_DIR" "sketches/${SKETCH_NAME}"

if [ ! -f "$HEX_FILE" ]; then
  # arduino-cli may use a different output naming convention
  HEX_FILE=$(find "$BUILD_DIR" -name "*.hex" | head -1)
  if [ -z "$HEX_FILE" ]; then
    echo "ERROR: No .hex file found in ${BUILD_DIR}/"
    exit 1
  fi
fi

echo "=== Uploading to Yun at ${YUN_HOST} ==="
scp "$HEX_FILE" "${YUN_HOST}:/tmp/sketch.hex"

echo "=== Flashing ATmega32U4 ==="
ssh "$YUN_HOST" "merge-sketch-with-bootloader.lua /tmp/sketch.hex && run-avrdude /tmp/sketch.hex"

echo "=== Deploy complete ==="
