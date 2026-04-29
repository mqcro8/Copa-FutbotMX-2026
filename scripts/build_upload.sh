#!/bin/bash
# FutBotMX Build & Upload Script
# Usage: ./scripts/build_upload.sh [device1] [device2]

set -e

DEVICE1="${1:-/dev/ttyACM0}"
DEVICE2="${2:-/dev/ttyACM2}"
ENV="esp32s3-n16r8-usb"

echo "=== FutBotMX Build & Upload ==="
echo "Device 1: $DEVICE1"
echo "Device 2: $DEVICE2"
echo ""

# 1. Compile
echo "[1/2] Compiling..."
cd /home/enrique/Documentos/workspace/projects/Robotica/Copa-FutbotMX-2026
pio run --environment $ENV
echo ""

# 2. Upload to both devices
echo "[2/2] Uploading to Device 1..."
pio run --environment $ENV --upload-port $DEVICE1 -t upload
echo ""

echo "[2/2] Uploading to Device 2..."
pio run --environment $ENV --upload-port $DEVICE2 -t upload
echo ""

echo "=== DONE ==="
