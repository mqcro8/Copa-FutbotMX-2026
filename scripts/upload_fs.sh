#!/bin/bash
# FutBotMX LittleFS Build & Upload Script
# Usage: ./scripts/upload_fs.sh [device1] [device2]

set -e

DEVICE1="${1:-/dev/ttyACM0}"
DEVICE2="${2:-/dev/ttyACM2}"
ENV="esp32s3-n16r8-usb"

echo "=== FutBotMX LittleFS Build & Upload ==="
echo "Device 1: $DEVICE1"
echo "Device 2: $DEVICE2"
echo ""

# 1. Build FS
echo "[1/3] Building LittleFS image..."
cd /home/enrique/Documentos/workspace/projects/Robotica/Copa-FutbotMX-2026
pio run --environment $ENV --target buildfs
echo ""

# 2. Upload FS to Device 1
echo "[2/3] Uploading FS to Device 1 ($DEVICE1)..."
pio run --environment $ENV --upload-port $DEVICE1 -t uploadfs
echo ""

# 3. Upload FS to Device 2
echo "[3/3] Uploading FS to Device 2 ($DEVICE2)..."
pio run --environment $ENV --upload-port $DEVICE2 -t uploadfs
echo ""

echo "=== FS UPLOAD DONE ==="
