#!/bin/bash

# Firefly Pixie Flash Script
# Usage: ./flash.sh [device_path]

set -e

# Default device path (update this with your actual device)
DEFAULT_DEVICE="/dev/tty.usbmodem*"

# Use provided device or default
DEVICE=${1:-$DEFAULT_DEVICE}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}🔥 Firefly Pixie Flash Tool${NC}"
echo "=========================="

# Auto-detect device if using wildcard
if [[ "$DEVICE" == *"*"* ]]; then
    echo "🔍 Auto-detecting ESP32 device..."
    DETECTED_DEVICES=$(ls $DEVICE 2>/dev/null || true)
    
    if [ -z "$DETECTED_DEVICES" ]; then
        echo -e "${RED}❌ No ESP32 device found matching: $DEVICE${NC}"
        echo ""
        echo "Available devices:"
        ls /dev/tty.* 2>/dev/null | grep -E "(usbmodem|usbserial|SLAB)" || echo "  No USB devices found"
        echo ""
        echo "Usage: $0 /dev/tty.your_device"
        exit 1
    fi
    
    # Use first detected device
    DEVICE=$(echo $DETECTED_DEVICES | head -n1)
    echo -e "${GREEN}✅ Found device: $DEVICE${NC}"
fi

# Check if device exists
if [ ! -e "$DEVICE" ]; then
    echo -e "${RED}❌ Device not found: $DEVICE${NC}"
    echo ""
    echo "Available devices:"
    ls /dev/tty.* 2>/dev/null | grep -E "(usbmodem|usbserial|SLAB)" || echo "  No USB devices found"
    exit 1
fi

echo "📱 Target device: $DEVICE"
echo ""

# Build firmware
echo "🔨 Building firmware..."
docker run --rm -v $PWD:/project -w /project -e HOME=/tmp espressif/idf idf.py build

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Build failed${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Build successful${NC}"
echo ""

# Flash firmware
echo "⚡ Flashing to device: $DEVICE"

# Check if device still exists before flashing
if [ ! -e "$DEVICE" ]; then
    echo -e "${RED}❌ Device disappeared: $DEVICE${NC}"
    echo "This sometimes happens with ESP32 devices."
    echo ""
    echo "Try these solutions:"
    echo "1. Run ./flash-native.sh (uses native esptool)"
    echo "2. Unplug and reconnect the device"
    echo "3. Try a different USB cable"
    exit 1
fi

docker run --device=$DEVICE:$DEVICE --rm -v $PWD:/project -w /project -e HOME=/tmp espressif/idf idf.py -p $DEVICE flash

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}🎉 Flash successful!${NC}"
    echo ""
    echo "📋 Quick Start:"
    echo "  • Main menu: Navigate with North/South, select with OK"
    echo "  • Games: Snake, Tetris, Pong"
    echo "  • Wallet: Generates Ethereum addresses"
    echo "  • Back: West button in any panel"
    echo ""
    echo "🔧 Monitor output:"
    echo "  docker run --device=$DEVICE:$DEVICE --rm -v \$PWD:/project -w /project espressif/idf idf.py -p $DEVICE monitor"
else
    echo -e "${RED}❌ Flash failed${NC}"
    exit 1
fi