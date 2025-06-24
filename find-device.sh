#!/bin/bash

# ESP32 Device Detection Script

echo "🔍 Scanning for ESP32 devices..."
echo "================================"

# Common ESP32 device patterns
PATTERNS=(
    "/dev/tty.usbmodem*"
    "/dev/tty.usbserial-*" 
    "/dev/tty.SLAB_USBtoUART*"
    "/dev/ttyUSB*"
    "/dev/ttyACM*"
)

FOUND_DEVICES=()

for pattern in "${PATTERNS[@]}"; do
    devices=$(ls $pattern 2>/dev/null || true)
    if [ ! -z "$devices" ]; then
        for device in $devices; do
            FOUND_DEVICES+=("$device")
        done
    fi
done

if [ ${#FOUND_DEVICES[@]} -eq 0 ]; then
    echo "❌ No ESP32 devices found"
    echo ""
    echo "Troubleshooting:"
    echo "• Make sure ESP32 is connected via USB"
    echo "• Check if drivers are installed (CP210x/CH340)"
    echo "• Try a different USB cable"
    echo "• On macOS, devices appear as /dev/tty.usbmodem* or /dev/tty.usbserial-*"
    echo ""
    echo "All /dev/tty.* devices:"
    ls /dev/tty.* 2>/dev/null || echo "  None found"
else
    echo "✅ Found ESP32 device(s):"
    for device in "${FOUND_DEVICES[@]}"; do
        echo "  📱 $device"
    done
    echo ""
    echo "⚡ To flash firmware:"
    echo "  ./flash.sh ${FOUND_DEVICES[0]}"
fi